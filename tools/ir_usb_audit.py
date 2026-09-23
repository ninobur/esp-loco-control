#!/usr/bin/env python3
"""Audit a USB bench CSV without treating pulse count as physical ground truth."""
import argparse
import csv
import json


def audit(path):
    result = dict(records=0, boots=0, sequence_missing=0, sequence_nonforward=0,
                  missed_slots=0, queue_drops=0, crc_errors=0, gap_rows=0,
                  saturated_samples=0, duration_s=0., demo=False)
    previous = None
    with open(path, newline='') as stream:
        for row in csv.DictReader(stream):
            # A live file may end with one not-yet-complete CSV row.
            if row.get('demo') not in ('0', '1'):
                continue
            result['records'] += 1
            result['demo'] |= row['demo'] == '1'
            result['gap_rows'] += int(row['gap'])
            result['saturated_samples'] += int(row['raw']) in (0, 4095)
            result['crc_errors'] = max(result['crc_errors'], int(row['bad_crc']))
            if previous is None or row['boot'] != previous['boot']:
                result['boots'] += 1
            else:
                delta = (int(row['seq']) - int(previous['seq'])) & 0xffffffff
                if 0 < delta < 0x80000000:
                    result['sequence_missing'] += delta-1
                else:
                    result['sequence_nonforward'] += 1
                result['duration_s'] += (int(row['us'])-int(previous['us']))/1e6
                result['missed_slots'] += (int(row['missed'])-int(previous['missed'])) & 0xffffffff
                result['queue_drops'] += (int(row['dropped'])-int(previous['dropped'])) & 0xffffffff
            previous = row
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv')
    print(json.dumps(audit(parser.parse_args().csv), indent=2))
