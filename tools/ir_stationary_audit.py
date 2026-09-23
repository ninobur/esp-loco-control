#!/usr/bin/env python3
"""Report detector activity in a user-confirmed stationary capture window."""
import argparse
import json
import statistics
from ir_scope_espnow_analyze import parse_file


def audit(rows, seconds):
    last = max(rows, key=lambda row: row['pi_t'])
    rows = sorted((row for row in rows if row['sid'] == last['sid']
                   and row['pi_t'] >= last['pi_t'] - seconds),
                  key=lambda row: row['firstSample'])
    if len(rows) < 2:
        raise ValueError('need at least two packets from the latest boot')
    values = sorted(s & 4095 for row in rows for s in row['samples'][:row['count']])
    spans = [row['runMax'] - row['runMin'] for row in rows]
    dt = (rows[-1]['firstSample'] - rows[0]['firstSample']) / 1000
    delta = rows[-1]['pulses'] - rows[0]['pulses']
    return dict(boot_id=hex(last['sid']), start_epoch=rows[0]['pi_t'],
                end_epoch=rows[-1]['pi_t'], sample_seconds=dt,
                packets=len(rows), observed_count_delta=delta,
                counts_per_second=delta / dt if dt > 0 else None,
                raw_percentiles={str(p): values[int((len(values)-1)*p/100)]
                                 for p in (0, 1, 5, 50, 95, 99, 100)},
                envelope_span_min=min(spans),
                envelope_span_median=statistics.median(spans),
                envelope_span_max=max(spans),
                stationary_assertion='requires independent operator confirmation',
                distance_valid=False,
                reason='UNVALIDATED_PULSE_MEASUREMENT')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--seconds', type=float, default=60)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error('--seconds must be positive')
    print(json.dumps(audit(parse_file(args.capture), args.seconds), indent=2))
