#!/usr/bin/env python3
"""Read-only measurements from the pre-RVM October-15 ADC-ground study."""
from __future__ import annotations
import csv, json
from pathlib import Path
import numpy as np

SOURCE=Path('/Users/davidbrown/NGR/NGR-Files/hall-ground-20261015/derived')
OUT=Path(__file__).resolve().parent/'independent_width_study'

def q(xs):
    a=np.asarray(xs,dtype=float)
    return {'n':len(a),'min':float(a.min()),'p05':float(np.quantile(a,.05)),'median':float(np.median(a)),'p95':float(np.quantile(a,.95)),'max':float(a.max())}

def main():
    OUT.mkdir(exist_ok=True)
    key=lambda x:str(int(float(x)))
    encounters={key(r['encounter_id']):r for r in csv.DictReader((SOURCE/'encounters_adc_only.csv').open())}
    ground={r['ground_id']:r for r in csv.DictReader((SOURCE/'ground_intervals.csv').open())}
    rows=[]
    for r in csv.DictReader((SOURCE/'selected_matches.csv').open()):
        # Record kind 4 is the preserved opening observation.  A match to a
        # supported ADC-only interval excludes uncertainty/censoring and failed
        # geographic association; no RVM decision is consulted here.
        if not (r['kind']=='4' and r['match_status']=='matched_supported_activity' and r['mm']==r['target']): continue
        e=encounters[key(r['encounter_id'])]; left=ground[e['left_ground_id']]
        if not (e['classification']=='supported_activity' and left['boot']==e['boot']): continue
        try: distance=abs(float(r['end_interp_mm'])-float(r['start_interp_mm']))
        except ValueError: distance=None
        rows.append({'boot':r['boot'],'mm':r['mm'],'direction':r['direction'],'start_us':r['start_us'],'end_us':r['end_us'],
          'ensemble_s':float(r['duration_s']),'ensemble_native_samples':int(e['samples']),'ensemble_ir_mm':distance,
          'ir_start_bracket_ms':r['start_ir_bracket_ms'],'ir_end_bracket_ms':r['end_ir_bracket_ms'],
          'baseline_before_s':float(left['duration_s']),'baseline_before_native_samples':int(left['samples']),
          'baseline_before_adc_std':float(left['adc_std']),'source_classification':e['classification']})
    with (OUT/'measured_supported_matched.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,rows[0].keys());w.writeheader();w.writerows(rows)
    summary={'inclusion':'kind=4, matched_supported_activity, mm==target; excludes uncertain, censored, unsupported and mismatched intervals',
      'recordings':sorted({r['boot'] for r in rows}),'ensembles':q([r['ensemble_s'] for r in rows]),
      'native_samples':q([r['ensemble_native_samples'] for r in rows]),'ir_distance_mm':q([r['ensemble_ir_mm'] for r in rows if r['ensemble_ir_mm'] is not None]),
      'baseline_before_s':q([r['baseline_before_s'] for r in rows]),'baseline_before_native_samples':q([r['baseline_before_native_samples'] for r in rows]),
      'by_direction':{d:q([r['ensemble_s'] for r in rows if r['direction']==d]) for d in ('CW','CCW')}}
    (OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    z=summary
    lines=['# Independent Hall-ground ensemble and baseline measurement','',
      'This supersedes the October-19 RVM-adjacent width estimate for requirements work. It uses the earlier October-15 **ADC-only** ground/encounter partition, frozen before opening decisions, geographic sequence and RVM work were read. Its ground intervals are observed stable, low-variation Hall segments; encounters are the bounded complements.','',
      '## Inclusion and exclusion','',f"Included **{z['ensembles']['n']}** preserved opening records only when the opening has `matched_supported_activity` and `mm == target`. Excluded: uncertain instability, censored edge/gap activity, unsupported activity, unmatched openings, and mismatched identity. The retained row-level reasons and bracket quality are in `measured_supported_matched.csv`.",'',
      '## Measurements','', '| Quantity | n | median | 5th–95th | min–max |','|---|---:|---:|---:|---:|']
    for label,key,unit in [('Ensemble width','ensembles','s'),('Ensemble width','native_samples','native Hall samples'),('Ensemble IR distance','ir_distance_mm','mm'),('Baseline before ensemble','baseline_before_s','s'),('Baseline before ensemble','baseline_before_native_samples','native Hall samples')]:
        a=z[key];lines.append(f"| {label} ({unit}) | {a['n']} | {a['median']:.3f} | {a['p05']:.3f}–{a['p95']:.3f} | {a['min']:.3f}–{a['max']:.3f} |")
    lines += ['','## Direction','', '| Direction | n | median ensemble width s | 5th–95th s |','|---|---:|---:|---:|']
    for d,a in z['by_direction'].items():lines.append(f"| {d} | {a['n']} | {a['median']:.3f} | {a['p05']:.3f}–{a['p95']:.3f} |")
    lines += ['', '## What RVM 5.x actually bounds', '',
      'RVM is **not** an independent physical-boundary measurement. `RvmShape.h` scores a whole window `[c−W/2, c+W/2]`, but publishes its `Ensemble.s/e` as only the middle half `[c−W/4, c+W/4]`. On acceptance it starts the next reference baseline at the **full window end**, not the published core end. Thus the accepted window can include baseline, its published core can truncate tails, and neither is a measured magnet width. This conclusion is from source inspection, not from using RVM to select the rows above.','',
      '## Limits','', 'The IR coordinate is interpolated and its per-boundary bracket time is retained. It is valid movement evidence, not a direct Hall boundary sensor. This earlier source is Otto-only; it does not establish Toby dimensions. It also does not claim that every supported interval is a physical magnet—only that it is a well-bounded, ADC-supported encounter associated with a matching recorded opening.']
    (OUT/'REPORT.md').write_text('\n'.join(lines)+'\n')
if __name__=='__main__':main()
