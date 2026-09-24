#!/usr/bin/env python3
"""Extract CRC-checked type-1 ADC samples for the C++ diagnostic replay.

Output: sample_index raw_adc recorded_flags. No samples are interpolated.
Sample indices are NOT exact timestamps; receiver times are not ADC times.
"""
import argparse
import gzip
import sys
from ir_scope_espnow_analyze import HDR, SAMPLES, crc16


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture')
    p.add_argument('--sid', required=True, type=lambda s: int(s, 16))
    args = p.parse_args()
    opener = gzip.open if args.capture.endswith('.gz') else open
    packets = corrupt = gaps = missing = 0
    previous_end = None
    with opener(args.capture, 'rt') as source:
        for line in source:
            fields = line.split()
            if len(fields) != 7 or fields[1] != 'RX':
                continue
            try:
                data = bytes.fromhex(fields[-1])
                transport_crc = int(fields[-2], 16)
            except ValueError:
                continue
            if len(data) != 250 or data[3] != 1:
                continue
            header = HDR.unpack(data[:HDR.size])
            if header[3] != args.sid:
                continue
            if (header[:3] != (0x4952, 1, 1) or int(fields[4]) != len(data)
                    or crc16(data) != transport_crc
                    or crc16(data[:-2]) != int.from_bytes(data[-2:], 'little')
                    or not 0 < header[6] <= 96):
                corrupt += 1
                continue
            start, count = header[5], header[6]
            if previous_end is not None:
                if start < previous_end:
                    raise ValueError('Duplicate/out-of-order samples: inspect capture before replay')
                if start != previous_end:
                    gaps += 1
                    missing += start - previous_end
            for i, value in enumerate(SAMPLES.unpack(data[56:248])[:count]):
                print(start + i, value & 4095, value >> 12)
            previous_end = start + count
            packets += 1
    print(f'packets={packets} corrupt={corrupt} missing_ranges={gaps} missing_samples={missing}', file=sys.stderr)
    if not packets or corrupt:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
