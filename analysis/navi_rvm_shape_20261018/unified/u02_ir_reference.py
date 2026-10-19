"""Independent research reference for movement (NOT an input to any NAVI mechanism).
  u02_ir_reference.py <boots_dir> <ir_log> [<ir_log> ...]
Raw 1 kHz IR waveform (type-1 packets, tools/ir_scope_espnow_analyze.py) mapped to Otto's clock:
IR sample -> Pi time by a per-segment linear fit, offset at the 1st percentile of packet delay
(the minimum is an outlier, see the A0 report); Pi -> Otto by the boot's NSR status offset.
Per 20 ms Otto block, over the surrounding 1 s of IR samples (centred):
  ir_state 1 = MOVING: a stripe edge is flagged, or the 50 ms IR means span >= 60 counts;
  ir_state 0 = STATIONARY: no edge and the 50 ms means span <= 15 counts;
  ir_state -1 = UNKNOWN: anything else, or under half of the IR samples present.
A stationary wheel resting on a stripe plateau is insensitive to sub-stripe creep: STATIONARY
means 'no motion visible to the IR', the limit stated in the A0 report."""
import sys, pathlib, json, numpy as np
sys.path.insert(0, '/Users/davidbrown/esp-loco-control/tools')
import ir_scope_espnow_analyze as A
BD = pathlib.Path(sys.argv[1]); logs = sys.argv[2:]
rows = []
for lg in logs: rows += A.parse_file(lg)
pt = np.array([r['pi_t'] for r in rows]); fsa = np.array([r['firstSample'] for r in rows], np.int64); sid = np.array([r['sid'] for r in rows], np.int64)
smp = np.array([r['samples'] for r in rows], np.int32)
# segments: same sid, firstSample increasing
order = np.lexsort((fsa, sid)); pt, fsa, sid, smp = pt[order], fsa[order], sid[order], smp[order]
seg = np.concatenate([[0], np.cumsum((np.diff(sid) != 0) | (np.diff(fsa) <= 0) | (np.diff(pt) > 30))])
T, RAW, EDG = [], [], []
for s in np.unique(seg):
    m = seg == s
    if m.sum() < 20: continue
    a = np.polyfit(fsa[m], pt[m], 1)[0]; b = np.percentile(pt[m] - (fsa[m] + 96) * a, 1)
    i = (fsa[m][:, None] + np.arange(96)[None, :]).ravel()
    T.append(a * i + b); v = smp[m].ravel(); RAW.append(v & 0x0FFF); EDG.append((v & 0x6000) > 0)
T = np.concatenate(T); RAW = np.concatenate(RAW).astype(float); EDG = np.concatenate(EDG); o = np.argsort(T); T, RAW, EDG = T[o], RAW[o], EDG[o]
print('IR samples', len(T), 'Pi span', T[0], T[-1])
info = {}
for f in sorted(BD.glob('boot_*.npz')):
    z = dict(np.load(f)); k = f.stem[5:]
    tp = z['t_us'] / 1e6 + float(z['pi_offset_s'])                    # block centre in Pi time
    lo = np.searchsorted(T, tp - 0.5); hi = np.searchsorted(T, tp + 0.5)
    st = np.full(len(tp), -1, np.int8)
    # 50 ms means on a fixed Pi grid for speed
    g0 = np.floor(T[0] / 0.05); gi = (np.floor(T / 0.05) - g0).astype(np.int64)
    gc = np.bincount(gi); gs = np.bincount(gi, RAW); gm = np.where(gc > 10, gs / np.maximum(gc, 1), np.nan)
    ge = np.bincount(gi, EDG.astype(float)) > 0
    cum_e = np.concatenate([[0], np.cumsum(ge)])
    for j in np.where(hi - lo > 500)[0]:
        a_, b_ = int(np.floor(tp[j] - 0.5 - g0 * 0.05) / 0.05), int((tp[j] + 0.5) / 0.05 - g0)
        a_ = int(np.floor((tp[j] - 0.5) / 0.05) - g0); b_ = int(np.floor((tp[j] + 0.5) / 0.05) - g0) + 1
        if a_ < 0 or b_ > len(gm): continue
        w = gm[a_:b_]; w = w[np.isfinite(w)]
        if len(w) < 10: continue
        edges = cum_e[b_] - cum_e[a_]; span = w.max() - w.min()
        st[j] = 1 if (edges > 0 or span >= 60) else (0 if span <= 15 else -1)
    np.save(BD / f'ir_{k}.npy', st)
    info[k] = dict(blocks=len(st), moving=int((st == 1).sum()), stationary=int((st == 0).sum()), unknown=int((st == -1).sum()))
json.dump(info, open(BD / 'ir_reference_info.json', 'w'), indent=1); print(json.dumps(info, indent=1))
