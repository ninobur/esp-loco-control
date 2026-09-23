#!/usr/bin/env python3
"""Decode type-5 movement snapshots from the existing raw Pi recorder."""
import argparse
import json
import struct
from ir_scope_espnow_analyze import crc16

WIRE = struct.Struct('<HBBI10QIIQBBHH')
assert WIRE.size == 110
FIELDS = ('magic version type sequence boot_id captured_us observed_rises '
          'completed_pulses inferred_added inferred_removed unreliable_samples '
          'saturated_samples sample_gaps open_aborts calibration_id pitch_um '
          'nominal_um optical_reason distance_validated span crc').split()
REASONS = ('PRIMING', 'INADEQUATE_CONTRAST', 'SATURATION', 'SAMPLE_GAP',
           'SIGNAL_STALE', 'REACQUIRING', 'TRACKING')


def decode(data):
    if len(data) != WIRE.size:
        raise ValueError('movement packet length')
    row = dict(zip(FIELDS, WIRE.unpack(data)))
    if (row['magic'], row['version'], row['type']) != (0x4952, 1, 5):
        raise ValueError('movement schema')
    if crc16(data[:-2]) != row['crc']:
        raise ValueError('movement CRC')
    if not row['boot_id'] or row['optical_reason'] >= len(REASONS):
        raise ValueError('movement identity/reason')
    if row['distance_validated'] != 0:
        raise ValueError('unsupported validated-distance claim')
    if row['nominal_um'] != row['completed_pulses'] * row['pitch_um']:
        raise ValueError('nominal distance mismatch')
    row['reason'] = REASONS[row['optical_reason']]
    row['nominal_mm'] = row['nominal_um'] / 1000
    row['distance_min_mm'] = 0
    row['distance_max_mm'] = None
    row['distance_range_reason'] = 'UNVALIDATED'
    return row


def records(path):
    with open(path, errors='replace') as source:
        for line in source:
            fields = line.split()
            if len(fields) != 7 or fields[1] != 'RX':
                continue
            try:
                data = bytes.fromhex(fields[6])
            except ValueError:
                continue
            if len(data) < 4 or data[:4] != b'RI\x01\x05':
                continue
            if int(fields[4]) != len(data) or crc16(data) != int(fields[5], 16):
                raise ValueError('recorder transport CRC/length')
            row = decode(data)
            row['received_epoch'] = float(fields[0])
            yield row


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    args = parser.parse_args()
    for row in records(args.capture):
        print(json.dumps(row))
