"""Validate the fixed local NSR slice; export native bytes and causal Hall bins.

No network access, writes only this research directory. See README for acquisition.
"""
import csv
import hashlib
import json
import mmap
from pathlib import Path
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'sources'))
import nsr_format as F

NATIVE = np.dtype([('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'),
                   ('pwm', 'u1'), ('direction', 'u1')])
BOOTS = {'D004D9A3888E8394': 'otto_cw', '3B1BE1D923CB81FD': 'otto_ccw',
         '6A31DD995673D123': 'toby_cw'}


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def blocks(a, out):
    """Never sort native samples to conceal ordering loss."""
    us = a['us'].astype(np.int64)
    if np.any(np.diff(us) <= 0):
        raise ValueError('non-monotone native clock: adjudicate before replay')
    b = us // 20000
    first, last = int(b[0]), int(b[-1])
    bins = np.arange(first, last + 1, dtype=np.int64)
    lo = np.searchsorted(b, bins, 'left')
    hi = np.searchsorted(b, bins, 'right')
    n = hi - lo
    sums = np.bincount(b - first, weights=a['adc'], minlength=len(bins))
    hall = sums / np.maximum(n, 1)
    missing = n < 50
    ser = a['serial'].astype(np.int64)
    jumps = np.flatnonzero(((ser[1:] - ser[:-1]) % (1 << 32)) != 1)
    missing[b[jumps] - first] = True
    missing[b[jumps + 1] - first] = True
    hall[missing] = np.nan
    # Timestamps are BIN ENDS (causal availability), not bin centres.
    np.savez_compressed(out, t=(bins + 1) * .02, hall=hall, n=n, lo=lo, hi=hi)
    return dict(reads=len(a), bins=len(bins), missing_bins=int(missing.sum()),
                serial_jumps=len(jumps), start_s=first * .02, end_s=(last + 1) * .02)


def main():
    src = ROOT / 'raw/pi_tail.nsr'
    info = dict(pi_path='/home/david/NGR/navi_sync/navi_sync_20261015_100403.nsr',
                slice_start=8652695604, slice_end_exclusive=9281594685,
                slice_bytes=src.stat().st_size, slice_sha256=sha(src),
                bad_frames=0, validated_frames=0, boots={}, discarded_prefix_bytes=0)
    outputs = {k: open(ROOT / f'raw/{name}.bin', 'wb') for k, name in BOOTS.items()}
    offsets = {k: [] for k in BOOTS}
    with open(src, 'rb') as f:
        buf = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        p = buf.find(F.MAGIC) - F.FRAME_LEN
        info['discarded_prefix_bytes'] = p
        while 0 <= p and p + F.FRAME_LEN <= len(buf):
            recv, n = struct.unpack_from(F.FRAME_FMT, buf, p)
            data = buf[p + F.FRAME_LEN:p + F.FRAME_LEN + n]
            try:
                h, payload = F.parse_record(data)
            except F.BadRecord:
                info['bad_frames'] += 1
                q = buf.find(F.MAGIC, p + F.FRAME_LEN + 1)
                if q < 0:
                    break
                p = q - F.FRAME_LEN
                continue
            info['validated_frames'] += 1
            key = f'{h.loco_boot_id:016X}'
            if key in outputs:
                if h.rec_type == 5:
                    outputs[key].write(payload)
                elif h.rec_type == 3:
                    offsets[key].append(recv / 1e6 - F.parse_status(payload)['t_us'] / 1e6)
            p += F.FRAME_LEN + n
        info['trailing_bytes'] = len(buf) - p
        buf.close()
    for k, out in outputs.items():
        out.close()
        name = BOOTS[k]
        path = ROOT / f'raw/{name}.bin'
        a = np.memmap(path, dtype=NATIVE, mode='r')
        row = blocks(a, ROOT / f'data/{name}.npz')
        row.update(boot=k, native_sha256=sha(path),
                   pi_offset_s=min(offsets[k]) if offsets[k] else None)
        info['boots'][name] = row
    earlier = Path('/Users/davidbrown/NGR/NGR-Files/hall-ground-20261015/derived/native_15549e9385faa4d5.bin')
    a = np.memmap(earlier, dtype=NATIVE, mode='r')
    # Only previously assigned construction material; do not read reserved boot.
    lo = int(np.searchsorted(a['us'], 100_000_000))
    hi = int(np.searchsorted(a['us'], 1_800_000_000))
    a[lo:hi].tofile(ROOT / 'raw/otto_train.bin')
    info['boots']['otto_train'] = blocks(a[lo:hi], ROOT / 'data/otto_train.npz')
    info['boots']['otto_train'].update(boot='15549e9385faa4d5', source=str(earlier),
        original_sha256=sha(earlier), native_sha256=sha(ROOT / 'raw/otto_train.bin'),
        source_native_rows=[lo, hi])
    # Copy only October 15 construction annotations, retaining uncertain activity.
    ann = ROOT.parents[1] / 'analysis/navi_rvm_shape_20261018/derived/annotation_boot1.csv'
    rows = list(csv.DictReader(open(ann)))
    rows = [r for r in rows if float(r['start_us']) >= 1e8 and float(r['end_us']) < 1.8e9]
    with open(ROOT / 'data/train_annotations.csv', 'w') as f:
        w = csv.DictWriter(f, fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
    info['annotations'] = dict(source=str(ann.relative_to(ROOT.parents[1])), sha256=sha(ann),
        status='ADC-derived boundaries; historical opening/sequence identity; not physical ground truth')
    for name in BOOTS.values():
        # Recognition NEVER reads these; retained for evaluation-only comparison.
        sub = {'otto_cw': 'run10_continuity', 'otto_ccw': 'run12_otto_ccw', 'toby_cw': 'run12_toby_cw'}[name]
        sources = list((ROOT.parents[1] / f'analysis/navi_rvm_shape_20261018/field_20261019/{sub}').glob('ensembles_*.csv'))
        target = ROOT / f'data/{name}_historical.csv'
        target.write_bytes(sources[0].read_bytes())
        info['boots'][name]['comparison_source'] = str(sources[0].relative_to(ROOT.parents[1]))
        info['boots'][name]['comparison_sha256'] = sha(sources[0])
        if name == 'otto_cw':
            # Run 11 continues the SAME boot; preserve its comparison rows too.
            extra = ROOT.parents[1] / 'analysis/navi_rvm_shape_20261018/field_20261019/run11_slow/ensembles_D004D9A3888E8394.csv'
            target.write_bytes(target.read_bytes() + extra.read_bytes().split(b'\n', 1)[1])
            info['boots'][name]['continuation_comparison_source'] = str(extra.relative_to(ROOT.parents[1]))
            info['boots'][name]['continuation_comparison_sha256'] = sha(extra)
    (ROOT / 'sources/data_manifest.json').write_text(json.dumps(info, indent=2))
    print(json.dumps(info, indent=2))


if __name__ == '__main__':
    main()
