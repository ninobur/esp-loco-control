"""Compare on-loco recognizer events with the Python replay of the same NSR capture, and print the run tail.
Usage: python3 replay_vs_firmware.py <capture.npz from extract_nsr.py> <mqtt log> <BOOT_ID>"""
import sys, json, numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[2] / 'navi_matched_filter'))
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[1]))
import mf
from design_analysis import PROPOSED
npz, log, boot = sys.argv[1:4]
nat = np.load(npz)['native']; tr = mf.condition(nat)
ev, _ = mf.recognize(tr, PROPOSED)
fw = [json.loads(l.split('\t',2)[2]) for l in open(log) if 'diag/navi_encounter' in l]
fw = [e for e in fw if e['boot'] == boot]
t0 = tr.t0_us
fwt = np.array([(e['passage_us'] - t0)/1e6 for e in fw])
pyt = np.array([e['t_peak'] for e in ev])
print('firmware events', len(fw), 'python events', len(ev), 'nsr span %.1f-%.1f s' % (0, tr.t[-1]))
# match
unmatched_py = [t for t in pyt if np.min(np.abs(fwt - t)) > 0.05] if len(fwt) else list(pyt)
unmatched_fw = [t for t in fwt if np.min(np.abs(pyt - t)) > 0.05]
print('python-only', np.round(unmatched_py,2), 'firmware-only', np.round(unmatched_fw,2))
dt = np.abs(np.array([np.min(np.abs(pyt - t)) for t in fwt]))
print('passage agreement ms: median %.1f max %.1f' % (1e3*np.median(dt), 1e3*dt.max()))
# tail
last = fwt[-1]
print('last firmware event at %.2f s, pol %s' % (last, fw[-1]['polarity']))
med = np.nanmedian(tr.x[(tr.t > last - 20) & (tr.t < last - 1)])
print('background (20 s before last event, median) %.0f' % med)
for a in np.arange(last - 4, tr.t[-1], 1.0):
    m = (tr.t >= a) & (tr.t < a + 1)
    if not m.any(): break
    x = tr.x[m]
    print('  t %6.1f  pwm %3d  hall mean %6.0f (dev %+5.0f) min %5.0f max %5.0f' % (a, tr.pwm[m].max(), np.nanmean(x), np.nanmean(x) - med, np.nanmin(x), np.nanmax(x)))
    if a > last + 40: break
