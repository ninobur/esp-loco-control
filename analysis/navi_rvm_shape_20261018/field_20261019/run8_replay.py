"""Run 8 (boot B29BDF0E37C0CDA1, 2026-10-19) offline replay of Otto's native Hall.
  run8_replay.py <nsr_tail.bin> <run8 MQTT log (combined)> <out_dir>
Analysis only. Source: the tail of the Pi NAVI sync recording (read-only copy).
1. Decode native (5 kHz Hall + applied PWM) and legacy (commanded PWM) records of the boot.
2. Firmware path: tests/navi_rvm_shape/replay.cpp (firmware headers), real commanded
   target per block (legacy 'command', the firmware's commandedPwm), bounded feeding (--drain 1).
3. Python reference: s01 tracing + stopgo.firmware_mask (real commanded stop) + rvm_local.run.
4. Both compared with the 165 field recognitions (diag/rvm_ensemble midpoints).
5. Counterfactual (diagnostic only, not a proposal): the two stalls observed in the native Hall
   (Otto stationary over MM109 and MM023 with applied PWM 29-35) treated as STOPPED (PWM 0),
   to test whether the stall plateaus caused MM109-MM113 and MM023 to go unrecognized."""
import sys, struct, json, subprocess, tempfile, pathlib
import numpy as np, pandas as pd
HERE = pathlib.Path(__file__).resolve().parent
SC = HERE.parent / 'scripts'; sys.path.insert(0, str(SC))
from common import *
import rvm_shape as R, rvm_local as L, stopgo as G, continuation as K
sys.path.insert(0, str(NG / 'navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/sources/decoder_support'))
import navi_sync_format as F

NSR, LOG, OUT = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3]); OUT.mkdir(parents=True, exist_ok=True)
BOOT = 0xB29BDF0E37C0CDA1
DECL_US, DECL = 26_990_000, (23, 24, 1)          # state/nav DECLARED: baseline_start_us 26990000, last 23 next 24 CW
STALL_FLAT_S = [(606.0, 665.0), (903.5, 911.5)] # the stationary plateaus only

# ---- 1. decode ----
NATIVE = np.dtype([('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'), ('pwm', 'u1'), ('direction', 'u1')])
WIRE = np.dtype([('dt', '<u2'), ('raw', '<u2', (5,)), ('median', '<i2'), ('pwm', 'u1'), ('command', 'u1'), ('flags', 'u1'), ('reserved', 'u1')])
buf = open(NSR, 'rb').read(); p = buf.find(F.MAGIC) - F.FRAME_LEN
nat, leg_us, leg_cmd = [], [], []
while p + F.FRAME_LEN <= len(buf):
    recv, n = struct.unpack_from(F.FRAME_FMT, buf, p); h, pl = F.parse_record(buf[p + F.FRAME_LEN:p + F.FRAME_LEN + n]); p += F.FRAME_LEN + n
    if h.loco_boot_id != BOOT: continue
    if h.rec_type == 5: nat.append(np.frombuffer(pl, dtype=NATIVE))
    elif h.rec_type == 1:
        w = np.frombuffer(pl, dtype=WIRE); dt = w['dt'].astype(np.uint64); dt[0] = 0
        leg_us.append(h.t0_us + np.cumsum(dt)); leg_cmd.append(w['command'])
a = np.concatenate(nat); a = a[np.argsort(a['us'], kind='stable')]
lus = np.concatenate(leg_us); lcmd = np.concatenate(leg_cmd); o = np.argsort(lus, kind='stable'); lus, lcmd = lus[o], lcmd[o]
tmp = pathlib.Path(tempfile.mkdtemp(prefix='run8_')); a.tofile(tmp / 'native.bin')

# ---- tracing (s01 rule, identical grid to RvmTracing.h) ----
us = a['us'].astype(np.int64); adc = a['adc'].astype(float); ser = a['serial'].astype(np.int64)
B = DT_MS * 1000; t0 = us[0] // B * B; b = (us - t0) // B; nb = int(b[-1]) + 1
cnt = np.bincount(b, minlength=nb); hall = np.where(cnt > 0, np.bincount(b, adc, nb) / np.maximum(cnt, 1), np.nan)
jump = np.where(np.diff(ser) != 1)[0]; gap = cnt < 50; gap[b[jump]] = True; gap[b[jump + 1]] = True; hall[gap] = np.nan
pwm = np.zeros(nb, np.int16); np.maximum.at(pwm, b, a['pwm'].astype(np.int16))
ccw = np.zeros(nb, bool)                                   # declared CW
blk_end = t0 + (np.arange(nb) + 1) * B
cmd = lcmd[np.clip(np.searchsorted(lus, blk_end, side='right') - 1, 0, None)].astype(np.uint8)   # last commanded value in/before the block
decl_i = int((DECL_US - t0) // B)
centre_us = lambda i: t0 + np.asarray(i, float) * B + B / 2

# ---- field recognitions ----
E = []
for line in open(LOG, encoding='utf-8', errors='replace'):
    f = line.rstrip('\n').split('\t')
    if len(f) == 3 and f[1].endswith('/diag/rvm_ensemble'):
        d = json.loads(f[2])
        if d.get('boot') == 'B29BDF0E37C0CDA1': E.append(d)
fe = pd.DataFrame(E); fe['mid_s'] = fe.midpoint_us / 1e6

# ---- 2. firmware path (C++) ----
REPO = ROOT.parents[1]; SK = REPO / 'firmware/test-programs/NAVI_RVM_SHAPE_MANUAL'
subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', f'-I{SK}', str(REPO / 'tests/navi_rvm_shape/replay.cpp'), '-o', str(tmp / 'replay')], check=True)
def cpp(cmdarr, name):
    cmdarr.astype(np.uint8).tofile(tmp / f'{name}.cmd'); od = tmp / name; od.mkdir()
    (tmp / 'decl.csv').write_text('%d,%d,%d,%d\n' % ((decl_i,) + DECL))
    r = subprocess.run([str(tmp / 'replay'), str(tmp / 'native.bin'), str(tmp / 'decl.csv'), str(od), '--cmd', str(tmp / f'{name}.cmd'), '--drain', '1'],
                       capture_output=True, text=True, check=True)
    st = json.loads(r.stdout.strip().splitlines()[-1]); ce = pd.read_csv(od / 'ensembles.csv'); nv = pd.read_csv(od / 'nav.csv')
    assert st['first_block_us'] in (t0, t0 + B // 2), (st['first_block_us'], t0)
    ce['mid_s'] = centre_us(ce.c_id) / 1e6
    return st, ce, nv

# ---- 3. Python reference ----
mj = json.load(open(ROOT / 'results/local/model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(mj['E']), gmm(mj['N'])
def python(pwm_arr):
    keep = G.firmware_mask(hall, pwm_arr, [decl_i], cmd < G.STOPPED_BELOW_PWM)
    C = G.Compressed(hall, ccw, keep)
    py = L.run(L.Tracing(C.h, C.ccw), Em, Nm, [(C.start_of(decl_i), 'decl')])
    py['c_id'] = np.floor(py.c).astype(int).map(lambda i: int(C.ids[min(i, len(C.ids) - 1)]))
    py['mid_s'] = centre_us(C.to_ids(py.c)) / 1e6; py['bl_start_s'] = centre_us(C.to_ids(py.bl_start)) / 1e6
    return py, keep

def with_nav(ce, nv):
    """Attach the firmware navigator's event per recognition (nav.csv, in recognition order)."""
    ev = nv[nv.event != 'DECLARE'].reset_index(drop=True)
    return ce.assign(event=ev.event.values, mm=ev.mm.values, expected=ev.expected.values)

def match(x, y, tol_s=0.05):
    """Recognitions of x with a recognition of y within tol_s (midpoint) and the same polarity."""
    ym, yp = y.mid_s.values, np.asarray(y.pol)
    hit = [bool(len(ym)) and np.min(np.abs(ym - m)) <= tol_s and yp[np.argmin(np.abs(ym - m))] == q for m, q in zip(x.mid_s.values, np.asarray(x.pol))]
    return dict(n=len(x), matched=int(sum(hit)), unmatched=x[~np.array(hit, bool)][['mid_s', 'pol', 'llr']].round(3).to_dict('records'))

# recording gaps (serial jumps) in the NSR copy: reads the locomotive had but the recording lost
jumps = np.where(np.diff(ser) != 1)[0]
summary = dict(native_reads=int(len(a)), blocks=nb, legacy_samples=int(len(lus)), decl_block=decl_i, first_block_us=int(t0),
               recording_gaps=[dict(t_s=round(us[k] / 1e6, 3), missing_reads=int(ser[k + 1] - ser[k] - 1)) for k in jumps])
fe['pol'] = np.where(fe.polarity == 'N', 1, -1)

st, ce, nv = cpp(cmd, 'actual'); ce = with_nav(ce, nv)
py, keep = python(pwm)
summary['actual'] = dict(cpp_runtime=st, cpp=len(ce), python=len(py), field=len(fe),
                         field_in_cpp=match(fe, ce), cpp_in_field=match(ce, fe), python_in_cpp=match(py, ce), cpp_in_python=match(ce, py),
                         cpp_nav=nv.event.value_counts().to_dict())

# ---- 5. counterfactual (diagnostic only): the stall plateaus treated as STOPPED ----
pwm_cf = pwm.copy(); tb = centre_us(np.arange(nb)) / 1e6
for s0, s1 in STALL_FLAT_S: pwm_cf[(tb >= s0) & (tb < s1)] = 0
a2 = a.copy(); tread = a2['us'] / 1e6
for s0, s1 in STALL_FLAT_S: a2['pwm'][(tread >= s0) & (tread < s1)] = 0
a2.tofile(tmp / 'native.bin'); st2, ce2, nv2 = cpp(cmd, 'stall_stopped'); a.tofile(tmp / 'native.bin'); ce2 = with_nav(ce2, nv2)
py2, keep2 = python(pwm_cf)
summary['stall_as_stopped'] = dict(cpp=len(ce2), python=len(py2), python_in_cpp=match(py2, ce2), cpp_in_python=match(ce2, py2),
                                   cpp_nav=nv2.event.value_counts().to_dict(),
                                   new_vs_actual=match(ce2, ce)['unmatched'], lost_vs_actual=match(ce, ce2)['unmatched'])
cols = ['mid_s', 'pol', 'llr', 'level', 'event', 'mm', 'expected']
for nm, df in (('actual', ce), ('stall_as_stopped', ce2)):
    df.to_csv(OUT / f'replay_{nm}_ensembles.csv', index=False)
    summary[nm + '_585_712'] = df[(df.mid_s > 585) & (df.mid_s < 712)][cols].round(3).to_dict('records')
    summary[nm + '_880_end'] = df[df.mid_s > 880][cols].round(3).to_dict('records')
json.dump(summary, open(OUT / 'replay_summary.json', 'w'), indent=1, default=float)
print(json.dumps(summary, indent=1, default=float))
