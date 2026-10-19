import sys, struct, numpy as np
sys.path.insert(0, '/Users/davidbrown/NGR/NGR-Files/navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/sources/decoder_support')
import navi_sync_format as F
buf = open(sys.argv[1], 'rb').read(); p = buf.find(F.MAGIC) - F.FRAME_LEN; R = []; H = []
while p + F.FRAME_LEN <= len(buf):
    recv, n = struct.unpack_from(F.FRAME_FMT, buf, p); h, pl = F.parse_record(buf[p + F.FRAME_LEN:p + F.FRAME_LEN + n]); p += F.FRAME_LEN + n
    if h.loco_boot_id != 0xB29BDF0E37C0CDA1: continue
    if h.rec_type == 3: R.append((recv, F.parse_status(pl)['t_us']))
    if h.rec_type == 5: H.append((recv, h.t0_us))
for name, X in (('status', R), ('native batch t0', H)):
    X = np.array(X, float); recv_s, otto_s = X[:, 0] / 1e6, X[:, 1] / 1e6
    # lower envelope: transport delay only adds; fit offset on the 5th percentile of (recv - otto)
    d = recv_s - otto_s; slope = np.polyfit(otto_s, recv_s, 1)[0]
    print(name, 'n', len(X), 'recv epoch range', recv_s.min(), recv_s.max(), 'slope', slope, 'delay p1/p50/p99 rel. min: %.3f %.3f %.3f' % tuple(np.percentile(d - d.min(), [1, 50, 99])), 'offset(min)', d.min())
np.save('otto_to_pi.npy', np.array(H, float))
