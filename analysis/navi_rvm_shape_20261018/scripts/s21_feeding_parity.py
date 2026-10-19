"""Bounded feeding (field fix): the firmware path must give exactly the same
recognitions whether released blocks are fed at once or one per incoming block.
Boot 1, firmware headers via tests/navi_rvm_shape/replay.cpp, compared with the
Python reference (stopgo.firmware_mask + rvm_local) from s17."""
import numpy as np, pandas as pd, json, subprocess, tempfile, pathlib
from common import *
import continuation as K
import s10_local_experiment as X
REPO = ROOT.parents[1]; OUT = ROOT / 'results' / 'local_pause'
tmp = pathlib.Path(tempfile.mkdtemp(prefix='rvm_feed_'))
SK = REPO / 'firmware' / 'test-programs' / 'NAVI_RVM_SHAPE_MANUAL'
subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', f'-I{SK}', str(REPO / 'tests' / 'navi_rvm_shape' / 'replay.cpp'), '-o', str(tmp / 'replay')], check=True)
pwm = np.load(D / 'block_pwm_boot1.npy')
np.where(K.commanded_stop_proxy(pwm), 0, 255).astype(np.uint8).tofile(tmp / 'cmd.bin')
sd = pd.read_csv(RP / 'declarations.csv'); sd = sd[sd.boot == BOOT1]
dir_at = lambda us: (1 if sd[(sd.t_us <= us) & sd.topic.str.contains('session_direction')].payload.iloc[-1] == 'CW' else -1)
with open(tmp / 'decl.csv', 'w') as fh:
    for r, i in zip(X.decl.itertuples(), X.decl_i):
        a_, b_ = map(int, r.payload.split('-')); d = dir_at(r.t_us)
        fh.write('%d,%d,%d,%d\n' % ((i,) + ((a_, b_) if d > 0 else (b_, a_)) + (d,)))
py = pd.read_csv(OUT / 'python_reference_detections.csv')
out = {}
for drain in (-1, 1):
    od = tmp / f'out{drain}'; od.mkdir()
    res = subprocess.run([str(tmp / 'replay'), str(HG / f'native_{BOOT1}.bin'), str(tmp / 'decl.csv'), str(od), '--cmd', str(tmp / 'cmd.bin'), '--drain', str(drain)],
                         capture_output=True, text=True, check=True)
    st = json.loads(res.stdout.strip().splitlines()[-1]); ce = pd.read_csv(od / 'ensembles.csv'); nav = pd.read_csv(od / 'nav.csv')
    same = len(ce) == len(py) and (ce.c_id.values == py.c_id.values).all() and (ce.pol.values == py.pol.values).all()
    out[f'drain_{drain}'] = dict(cpp=len(ce), python=len(py), identical=bool(same),
                                 max_llr_diff=float(np.abs(ce.llr.values - py.llr.values).max()) if len(ce) == len(py) else None,
                                 nav_events=nav.event.value_counts().to_dict(), backlog_max=st['backlog_max'], push_max_us_host=st['push_max_us'])
json.dump(out, open(OUT / 'feeding_parity.json', 'w'), indent=1); print(json.dumps(out, indent=1))
