#!/usr/bin/env python3
"""Evaluate independently measured travel between explicit snapshot IDs."""
import argparse
import json
import math
from ir_movement_decode import records

COUNTERS = ('observed_rises', 'completed_pulses', 'inferred_added',
            'inferred_removed', 'unreliable_samples', 'saturated_samples',
            'sample_gaps', 'open_aborts')


def evaluate(rows, trial):
    # Explicit packet IDs avoid pretending MQTT/USB receipt time is wheel time.
    boot = trial['boot_id']
    selected = sorted((r for r in rows if r['boot_id'] == boot and
                       trial['start_sequence'] <= r['sequence'] <= trial['end_sequence']),
                      key=lambda r: r['sequence'])
    if not selected or selected[0]['sequence'] != trial['start_sequence'] or selected[-1]['sequence'] != trial['end_sequence']:
        raise ValueError('missing explicit endpoint snapshot')
    a,b = selected[0],selected[-1]
    if a['sequence'] >= b['sequence'] or a['captured_us'] >= b['captured_us']:
        raise ValueError('non-increasing endpoints')
    for left,right in zip(selected,selected[1:]):
        if right['sequence'] <= left['sequence'] or right['captured_us'] <= left['captured_us']:
            raise ValueError('duplicate/reordered sensor time')
        if any(right[k]<left[k] for k in COUNTERS):
            raise ValueError('counter regression within boot')
        if right['pitch_um']!=left['pitch_um'] or right['calibration_id']!=left['calibration_id']:
            raise ValueError('calibration changed during trial')
    lo,hi = trial['truth_min_mm'],trial['truth_max_mm']
    if not all(isinstance(v,(int,float)) and math.isfinite(v) for v in (lo,hi)) or lo<0 or hi<lo:
        raise ValueError('invalid independent travel bounds')
    if not trial.get('truth_method'):
        raise ValueError('independent truth method required')
    delta={k:b[k]-a[k] for k in COUNTERS}
    nominal=delta['completed_pulses']*a['pitch_um']/1000
    result=dict(label=trial['label'],boot_id=boot,
                duration_s=(b['captured_us']-a['captured_us'])/1e6,
                observed=delta, nominal_mm=nominal,
                nominal_error_min_mm=nominal-hi,nominal_error_max_mm=nominal-lo,
                received_snapshots=len(selected),
                missing_snapshots=b['sequence']-a['sequence']+1-len(selected),
                reason_counts={reason:sum(r['reason']==reason for r in selected)
                               for reason in sorted({r['reason'] for r in selected})},
                unreliability_detected=bool(delta['unreliable_samples'] or
                    delta['saturated_samples'] or delta['sample_gaps'] or delta['open_aborts'] or
                    any(r['reason']!='TRACKING' for r in selected)),
                field_distance_validated=False,
                truth_method=trial['truth_method'])
    if 'truth_pulses' in trial:
        expected=trial['truth_pulses']
        if not isinstance(expected,int) or expected<0:
            raise ValueError('truth_pulses must be a nonnegative integer')
        result['net_count_error']=delta['completed_pulses']-expected
        result['net_count_error_fraction']=(result['net_count_error']/expected if expected else None)
    # Net error cannot separate compensating missed and doubled pulses.
    result['individual_missed_doubled_counts']='not identifiable from endpoint totals'
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture');p.add_argument('trials_json');a=p.parse_args()
    with open(a.trials_json) as source:trials=json.load(source)
    rows=list(records(a.capture))
    for trial in trials:print(json.dumps(evaluate(rows,trial)))
