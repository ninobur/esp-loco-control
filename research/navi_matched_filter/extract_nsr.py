#!/usr/bin/env python3
"""Decode NSR captures into native-Hall sample arrays (research only).

Each capture becomes one .npz holding every CRC-valid native Hall sample in
recorded order, plus the boot id and the action records (commands). Nothing is
repaired or interpolated; serial gaps stay visible as gaps.

usage: extract_nsr.py OUT_DIR CAPTURE.nsr [...]
"""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import navi_sync_format as nsf  # noqa: E402

NATIVE = np.dtype([('serial', '<u4'), ('t_us', '<u8'), ('raw', '<i2'),
                   ('pwm', 'u1'), ('direction', 'u1')])


def extract(path, out_dir):
    chunks, boots, actions = [], {}, []
    bad = counts = 0
    counts = {}
    for _recv_us, data in nsf.iter_capture(path):
        if len(data) < nsf.HDR_LEN or data[:4] != nsf.MAGIC or not nsf.crc_ok(data):
            bad += 1
            continue
        f = nsf.struct.unpack_from(nsf.HDR_FMT, data, 0)
        rec_type, n_items, boot = f[2], f[3], f[10]
        counts[rec_type] = counts.get(rec_type, 0) + 1
        payload = data[nsf.HDR_LEN:]
        if rec_type == nsf.REC_HALL_NATIVE:
            a = np.frombuffer(payload, NATIVE, count=n_items)
            chunks.append(a)
            boots[boot] = boots.get(boot, 0) + n_items
        elif rec_type == nsf.REC_ACTION:
            act = nsf.parse_action(payload)
            actions.append({k: (v.decode('latin1').rstrip('\0') if isinstance(v, bytes) else v)
                            for k, v in act.items()})
    native = np.concatenate(chunks) if chunks else np.zeros(0, NATIVE)
    order = np.argsort(native['serial'], kind='stable')
    native = native[order]
    _, first = np.unique(native['serial'], return_index=True)
    dup = len(native) - len(first)
    native = native[first]
    out = Path(out_dir) / (Path(path).stem + '.npz')
    np.savez_compressed(out, native=native)
    meta = {'source': str(path), 'bytes': Path(path).stat().st_size,
            'bad_records': bad, 'record_counts': {nsf.REC_NAME.get(k, k): v for k, v in counts.items()},
            'native_samples': int(len(native)), 'duplicate_serials_dropped': int(dup),
            'boots': {('%016x' % b): n for b, n in boots.items()},
            'serial_gaps': int(np.count_nonzero(np.diff(native['serial'].astype(np.int64)) > 1)) if len(native) else 0,
            'duration_s': float((native['t_us'][-1] - native['t_us'][0]) / 1e6) if len(native) else 0.0,
            'actions': actions}
    (Path(out_dir) / (Path(path).stem + '.json')).write_text(json.dumps(meta, indent=1, default=str))
    return meta


if __name__ == '__main__':
    out_dir = sys.argv[1]
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    for p in sys.argv[2:]:
        m = extract(p, out_dir)
        print('%s samples=%d dur=%.1fs boots=%s bad=%d gaps=%d' % (
            Path(p).name, m['native_samples'], m['duration_s'], list(m['boots']), m['bad_records'], m['serial_gaps']))
