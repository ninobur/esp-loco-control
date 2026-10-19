import sys, struct, numpy as np
sys.path.insert(0, sys.argv[1]); import navi_sync_format as F
buf = open(sys.argv[2], 'rb').read(); boot_want = int(sys.argv[3], 16)
p = buf.find(F.MAGIC) - F.FRAME_LEN
NATIVE = np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
out = []; bad = 0
while p + F.FRAME_LEN <= len(buf):
    recv, n = struct.unpack_from(F.FRAME_FMT, buf, p); data = buf[p+F.FRAME_LEN:p+F.FRAME_LEN+n]
    try:
        h, pl = F.parse_record(data)
    except Exception:
        bad += 1; q = buf.find(F.MAGIC, p + F.FRAME_LEN + 1)
        if q < 0: break
        p = q - F.FRAME_LEN; continue
    if h.rec_type == 5 and h.loco_boot_id == boot_want: out.append(np.frombuffer(pl, dtype=NATIVE))
    p += F.FRAME_LEN + n
a = np.concatenate(out); np.save(sys.argv[4], a)
print('reads', len(a), 'bad', bad, 'us', a['us'][0]/1e6, a['us'][-1]/1e6)
