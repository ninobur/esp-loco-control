"""Decode a read-only NSR slice into per-boot 20 ms block tracings (Otto 9950011).
  u01_decode_nsr.py <slice.nsr> <out_dir>
Per boot: t_us (block centre, Otto clock), hall (mean of native reads; NaN if < 50 reads or a
serial jump touches the block: the RvmTracing.h rule), pwm (highest applied PWM in the block),
cmd (last commanded PWM at/before the block end, from legacy records), dirb (declared direction
byte), and the Otto->Pi clock offset (lower envelope of status records). Also the raw native
records for the C++ replay."""
import sys, struct, json, pathlib, numpy as np
sys.path.insert(0, '/Users/davidbrown/NGR/NGR-Files/navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/sources/decoder_support')
import navi_sync_format as F
SRC, OUT = sys.argv[1], pathlib.Path(sys.argv[2]); OUT.mkdir(parents=True, exist_ok=True)
LOCOS = {int(x) for x in (sys.argv[3].split(',') if len(sys.argv) > 3 else ['9950011'])}   # e.g. 9950011,9950012
NATIVE = np.dtype([('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'), ('pwm', 'u1'), ('direction', 'u1')])
WIRE = np.dtype([('dt', '<u2'), ('raw', '<u2', (5,)), ('median', '<i2'), ('pwm', 'u1'), ('command', 'u1'), ('flags', 'u1'), ('reserved', 'u1')])
buf = np.memmap(SRC, mode='r', dtype=np.uint8); mv = memoryview(buf)
p = bytes(mv[:1 << 20]).find(F.MAGIC) - F.FRAME_LEN
nat, leg, st = {}, {}, {}; bad = 0
while p + F.FRAME_LEN <= len(buf):
    recv, n = struct.unpack_from(F.FRAME_FMT, mv, p)
    try:
        h, pl = F.parse_record(bytes(mv[p + F.FRAME_LEN:p + F.FRAME_LEN + n]))
    except Exception:
        bad += 1; q = bytes(mv[p + F.FRAME_LEN + 1:p + F.FRAME_LEN + 1 + (1 << 16)]).find(F.MAGIC)
        if q < 0: break
        p = p + F.FRAME_LEN + 1 + q - F.FRAME_LEN; continue
    p += F.FRAME_LEN + n
    if h.loco_id not in LOCOS: continue
    k = '%016X' % h.loco_boot_id
    if h.rec_type == 5: nat.setdefault(k, []).append(np.frombuffer(pl, dtype=NATIVE).copy())
    elif h.rec_type == 1:
        w = np.frombuffer(pl, dtype=WIRE); dt = w['dt'].astype(np.uint64); dt[0] = 0
        leg.setdefault(k, []).append((h.t0_us + np.cumsum(dt), w['command'].copy()))
    elif h.rec_type == 3: st.setdefault(k, []).append((recv / 1e6, F.parse_status(pl)['t_us'] / 1e6))
info = dict(bad_frames=bad, boots={})
for k, parts in nat.items():
    a = np.concatenate(parts); a = a[np.argsort(a['us'], kind='stable')]
    if len(a) < 5000 * 30: continue                                   # skip boots with < 30 s of Hall
    a.tofile(OUT / f'native_{k}.bin')
    us = a['us'].astype(np.int64); ser = a['serial'].astype(np.int64); B = 20000; t0 = us[0] // B * B; b = (us - t0) // B; nb = int(b[-1]) + 1
    cnt = np.bincount(b, minlength=nb); hall = np.where(cnt > 0, np.bincount(b, a['adc'].astype(float), nb) / np.maximum(cnt, 1), np.nan)
    j = np.where(np.diff(ser) != 1)[0]; gap = cnt < 50; gap[b[j]] = True; gap[b[j + 1]] = True; hall[gap] = np.nan
    pwm = np.zeros(nb, np.int16); np.maximum.at(pwm, b, a['pwm'].astype(np.int16))
    dirb = np.zeros(nb, np.int8); dirb[b] = a['direction']
    if k in leg:
        lu = np.concatenate([x[0] for x in leg[k]]); lc = np.concatenate([x[1] for x in leg[k]]); o = np.argsort(lu, kind='stable'); lu, lc = lu[o], lc[o]
        cmd = lc[np.clip(np.searchsorted(lu, t0 + (np.arange(nb) + 1) * B, side='right') - 1, 0, None)].astype(np.int16)
    else: cmd = np.full(nb, -1, np.int16)
    s = np.array(st.get(k, [(np.nan, np.nan)])); off = float(np.nanmin(s[:, 0] - s[:, 1])) if len(s) else np.nan
    np.savez_compressed(OUT / f'boot_{k}.npz', t_us=t0 + np.arange(nb) * B + B // 2, hall=hall, n=cnt, pwm=pwm, cmd=cmd, dirb=dirb, pi_offset_s=off, first_us=t0)
    info['boots'][k] = dict(reads=int(len(a)), blocks=nb, seconds=round(nb * 0.02, 1), gap_blocks=int(np.isnan(hall).sum()), serial_jumps=int(len(j)),
                            legacy=bool(k in leg), pi_offset_s=off, pi_start=off + t0 / 1e6, pi_end=off + us[-1] / 1e6)
json.dump(info, open(OUT / 'decode_info.json', 'w'), indent=1); print(json.dumps(info, indent=1))
