"""Frozen-parameter follow-ups: continuity through NAVI withdrawals, live-jump contexts, search width,
ensemble discovery, multi-lobe ablation, empirical atypicality, compute size."""
import sys, pathlib, json, copy, numpy as np, pandas as pd
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent)); import aligner, score, experiments as X
D = aligner.D; B1 = X.B1; R = {}
G = aligner.G

# ---- E1d: honour only declarations that follow PWM-0 relocation; skip the two post-withdrawal redeclarations
import aligner as A
orig = A.run
def run_honour(boot, label, honour):
    A.P.clear(); A.P.update(copy.deepcopy(X.FR['params']))
    dec = pd.read_csv(D / 'declarations.csv'); keep = dec[~(dec.topic.str.contains('start_interval') & ~dec.payload.isin(honour))]
    keep.to_csv(D / 'declarations_honoured.csv', index=False)
    src = pathlib.Path(D / 'declarations.csv'); bak = src.read_bytes(); src.write_bytes(open(D / 'declarations_honoured.csv', 'rb').read())
    try: o = A.run(boot, 'CW_L1', 'CCW_L1', declarations='all', label=label)
    finally: src.write_bytes(bak)
    return o
o = run_honour(B1, 'x_e1d', ['063-064', '071-072', '121-122', '143-144'])
df = X.scored(o, B1); df.to_csv(D / 'xs_e1d.csv', index=False)
win = lambda a, b: df[(df.t >= a) & (df.t < b)]
R['E1d_skip_021-022_and_123-124'] = dict(summary=X.summ(df), teleports=X.teleports(o),
    around_withdrawal_1=score.summary(win(860, 1100)), around_withdrawal_2=score.summary(win(1180, 1500)))

# ---- live-jump contexts (base run) -------------------------------------------------------------------
b = np.load(D / 'align_x_e1_CW_L1.npz'); x = b['x']; t = b['t']; s = b['seg']; pwm = b['pwm']; vir = b['vir']; cl = b['cost_local']
ok = np.isfinite(x[1:]) & np.isfinite(x[:-1]) & (s[1:] == s[:-1]) & (s[1:] >= 0)
j = np.where(ok & (np.abs(score.wrap(np.diff(x))) > 60))[0] + 1
ctx = []
for i in j:
    w = slice(max(0, i - 50), i + 1)
    ctx.append(dict(t=round(float(t[i]), 2), jump_mm=round(float(score.wrap(x[i] - x[i - 1]))), pwm=float(pwm[i]),
                    vir=round(float(vir[i])), pwm0_within_2s=bool((pwm[w] <= 0).any()), ir_zero_within_2s=bool((np.nan_to_num(vir[w]) < 5).any())))
cj = pd.DataFrame(ctx); cj.to_csv(D / 'live_jumps_base.csv', index=False)
R['live_jumps_base'] = dict(n=len(cj), with_pwm0_or_ir_stop_within_2s=int((cj.pwm0_within_2s | cj.ir_zero_within_2s).sum()),
                            while_cruising=int((~(cj.pwm0_within_2s | cj.ir_zero_within_2s)).sum()))

# ---- search width actually used: online incumbent vs 4 s retrospective at every 1 s -------------------
# (recomputed via the base run's retro function would need the in-memory backpointers; use the scored openings)
bd = pd.read_csv(D / 'xs_e1_CW_L1.csv'); dd = (bd.err_online - bd.err_retro).abs()
R['online_vs_retro_at_openings_mm'] = dict(q50=float(dd.quantile(.5)), q95=float(dd.quantile(.95)), q99=float(dd.quantile(.99)), max=float(dd.max()))

# ---- ensemble discovery from two independent CW passes (no detector, no spacing assumption) -------------
T1 = np.load(D / 'template_CW_L1.npy'); T2 = np.load(D / 'template_CW_L2.npy'); NC = len(T1)
pad = lambda a, n: np.concatenate([a[-n:], a, a[:n]])
base = (pd.Series(pad((T1 + T2) / 2, 1500)).rolling(3001, center=True).median().values[1500:-1500])
d1, d2 = T1 - base, T2 - base; noise = 1.4826 * np.median(np.abs((T1 - T2) - np.median(T1 - T2))) / np.sqrt(2)
res = {}
for k in [4, 6, 10]:
    struct = (np.abs(d1) > k * noise) & (np.abs(d2) > k * noise) & (np.sign(d1) == np.sign(d2))
    # runs of reproducible structure, then gaps between runs
    st = struct.astype(int); edges = np.flatnonzero(np.diff(np.concatenate([[0], st, [0]])))
    runs = list(zip(edges[::2], edges[1::2])); gaps = np.array([runs[i + 1][0] - runs[i][1] for i in range(len(runs) - 1)]) * G
    widths = np.array([b_ - a_ for a_, b_ in runs]) * G
    res[k] = dict(runs=len(runs), width_mm_q=[float(np.quantile(widths, q)) for q in (.05, .5, .95)],
                  gap_hist_mm={f'{lo}-{hi}': int(((gaps >= lo) & (gaps < hi)).sum()) for lo, hi in [(0, 10), (10, 25), (25, 50), (50, 100), (100, 150), (150, 200), (200, 300), (300, 2000)]})
    if k == 6: runs6, gaps6 = runs, gaps
# merge runs separated by gaps shorter than the empirical within/between valley (found from the histogram above)
R['ensemble_discovery'] = dict(lap_to_lap_noise_counts=round(float(noise), 2), by_k=res)
mp = pd.read_csv(D / 'map_positions.csv'); meta = json.load(open(D / 'template_meta.json'))['CW_L1']['opening_coord_mm']
op = np.array(sorted(meta.values()))
def ensembles(runs, merge_gap_mm):
    out = []
    for a_, b_ in runs:
        if out and (a_ - out[-1][1]) * G < merge_gap_mm: out[-1][1] = b_; out[-1][2] += 1
        else: out.append([a_, b_, 1])
    return out
for mg in [25, 50, 100]:
    E = ensembles(runs6, mg); w = np.array([(b_ - a_) * G for a_, b_, _ in E]); parts = np.array([p for *_, p in E])
    # how many ensembles contain exactly one NAVI-selected opening coordinate (scoring only)
    cnt = np.array([((op >= a_ * G - 20) & (op <= b_ * G + 20)).sum() for a_, b_, _ in E])
    R['ensemble_discovery'][f'merge_gap_{mg}mm'] = dict(ensembles=len(E), width_mm_q05_50_95=[float(np.quantile(w, q)) for q in (.05, .5, .95)],
        multi_part=int((parts > 1).sum()), parts_max=int(parts.max()), with_one_opening=int((cnt == 1).sum()), with_none=int((cnt == 0).sum()), with_two_plus=int((cnt >= 2).sum()))

# ---- multi-lobe value: template reduced to the main lobe per ensemble (sub-lobes, overshoots, steps removed)
E = ensembles(runs6, 50); Tm = {}
for name in ['CW_L1', 'CCW_L1']:
    T = np.load(D / f'template_{name}.npy'); b_ = pd.Series(pad(T, 1500)).rolling(3001, center=True).median().values[1500:-1500]
    dev = T - b_; out = b_.copy()
    # per region: keep only the contiguous lobe around the absolute extremum, down to 10% of its height
    regs = E if name == 'CW_L1' else None
    if regs is None:
        sd = 1.4826 * np.median(np.abs(dev)); st = (np.abs(dev) > 6 * max(noise, 0.5)).astype(int)
        ed = np.flatnonzero(np.diff(np.concatenate([[0], st, [0]]))); regs = ensembles(list(zip(ed[::2], ed[1::2])), 50)
    for a_, b2, _ in regs:
        seg = dev[a_:b2]; k = int(np.argmax(np.abs(seg))); pk = seg[k]; lo_ = k; hi_ = k
        while lo_ > 0 and np.sign(seg[lo_ - 1]) == np.sign(pk) and abs(seg[lo_ - 1]) > 0.1 * abs(pk): lo_ -= 1
        while hi_ < len(seg) - 1 and np.sign(seg[hi_ + 1]) == np.sign(pk) and abs(seg[hi_ + 1]) > 0.1 * abs(pk): hi_ += 1
        out[a_ + lo_:a_ + hi_ + 1] = T[a_ + lo_:a_ + hi_ + 1]
    np.save(D / f'template_{name}_MAINLOBE.npy', out); Tm[name] = out
    R.setdefault('mainlobe_template_rms_removed_counts', {})[name] = round(float(np.sqrt(np.mean((T - out) ** 2))), 2)
for nm in ['CW_L1', 'CCW_L1']:
    for f in [f'speed_model_{nm}.csv']:
        pathlib.Path(D / f'speed_model_{nm}_MAINLOBE.csv').write_bytes(pathlib.Path(D / f).read_bytes())
jm = json.load(open(D / 'template_meta.json'))
o = X.run(B1, 'x_e5_mainlobe', cw='CW_L1_MAINLOBE', ccw='CCW_L1_MAINLOBE')
# score against the full-template opening coordinates (same coordinate system)
df = score.score(o, B1, 'CW_L1', 'CCW_L1'); df = df[df.t < X.MANUAL_REVERSAL_S]
R['E5_mainlobe_only_template'] = dict(summary=score.summary(df), teleports=X.teleports(o))

# ---- empirical normal variation: 1 s local alignment cost, learned on CW lap 2 aligned by the CW_L1 template
c = pd.DataFrame({'t': t, 'c': cl, 'pwm': pwm}); c = c[c.pwm > 0]; c['s'] = c.t.astype(int)
per = c.groupby('s').c.sum(); m = json.load(open(D / 'template_meta.json'))
ref = per[(per.index >= m['CW_L2']['start_s']) & (per.index < m['CW_L2']['end_s'])]
qs = {q: float(ref.quantile(q)) for q in (.5, .9, .99, .999, 1.0)}
R['normal_variation_1s_cost_from_CW_lap2'] = dict(n_seconds=int(len(ref)), quantiles=qs)
out = per[(per > qs[1.0])]
R['beyond_max_of_normal'] = dict(n_seconds=int(len(out)), fraction=round(float(len(out) / len(per)), 4),
    clusters=[dict(t0=int(g.index.min()), t1=int(g.index.max()), max_cost=round(float(g.max()), 1))
              for _, g in out.groupby((out.index.to_series().diff() > 5).cumsum())])

json.dump(R, open(D / 'experiments2.json', 'w'), indent=1, default=str)
print(json.dumps(R, indent=1, default=str))
