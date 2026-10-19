import sys, struct, numpy as np
sys.path.insert(0, '/Users/davidbrown/esp-loco-control/tools')
import ir_scope_espnow_analyze as A
rows = A.parse_file(sys.argv[1])
print('type-1 packets', len(rows), 'sids', sorted({r['sid'] for r in rows}))
fs = np.array([r['firstSample'] for r in rows]); cnt = np.array([r['count'] for r in rows]); pt = np.array([r['pi_t'] for r in rows])
o = np.argsort(fs); fs, cnt, pt = fs[o], cnt[o], pt[o]
print('count values', np.unique(cnt)[:10], 'firstSample span', fs[0], fs[-1], 'pi span', pt.min(), pt.max())
gaps = np.diff(fs) - cnt[:-1]; print('sample gaps: n', int((gaps > 0).sum()), 'missing samples', int(gaps[gaps > 0].sum()), 'overlaps', int((gaps < 0).sum()))
# sample-index -> Pi time: lower envelope of (pi_t - firstSample/1000)
off = pt - fs / 1000.0
print('rate check: slope of pi_t vs firstSample (s/sample):', np.polyfit(fs, pt, 1)[0])
N = int(fs[-1] + cnt[-1] - fs[0]); raw = np.full(N, -1, np.int32); flg = np.zeros(N, np.uint16)
rows_s = [rows[i] for i in o]
for r in rows_s:
    s = np.array(r['samples'][:r['count']], np.int32); i0 = r['firstSample'] - fs[0]
    raw[i0:i0 + len(s)] = s & 0x0FFF; flg[i0:i0 + len(s)] = s & 0xF000
np.savez_compressed('ir_raw_run8.npz', raw=raw, flags=flg, first_sample=fs[0], fs=fs, pt=pt, cnt=cnt)
p = np.polyfit(fs, pt, 1); res = pt - np.polyval(p, fs); print('pi_t residual vs linear fit: p5 %.3f median %.3f p95 %.3f' % tuple(np.percentile(res, [5, 50, 95])))
