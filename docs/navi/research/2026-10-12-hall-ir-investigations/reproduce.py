#!/usr/bin/env python3
"""Verify the archive and reproduce analyses in a new, disposable directory.

Original scripts remain unchanged in Git. Only documented path substitutions
are made to working copies. No firmware, serial device, or network is used.
"""
from pathlib import Path
import argparse, csv, gzip, hashlib, importlib.util, json, math, os
import shutil, subprocess, sys

ARCHIVE = Path(__file__).resolve().parent

def verify():
    manifest = json.loads((ARCHIVE/'provenance/source-manifest.json').read_text())
    for row in manifest['files']:
        data=(ARCHIVE/row['path']).read_bytes()
        assert hashlib.sha256(data).hexdigest()==row['archived_sha256'], row['path']
        raw=gzip.decompress(data) if row['encoding']=='gzip' else data
        assert len(raw)==row['original_bytes'], row['path']
        assert hashlib.sha256(raw).hexdigest()==row['original_sha256'], row['path']
    print(f"PASS {len(manifest['files'])} original artifact hashes and lossless decompression", flush=True)
    return manifest

def module(path, name):
    spec=importlib.util.spec_from_file_location(name,path)
    mod=importlib.util.module_from_spec(spec); sys.modules[name]=mod
    spec.loader.exec_module(mod); return mod

def verify_circuits(hallroot, output):
    import numpy as np
    base=module(hallroot/'analyze_continuous_hall.py','circuit_base')
    odo=module(hallroot/'analyze_ir_odometry_calibration.py','circuit_odo')
    capture=hallroot/'sources/otto_navi_sync_20260928_101705.nsr'
    ir=odo.load_ir(capture,base); odo.prepare_ir(ir)
    hall,*_=base.load_capture(capture)
    tele=odo.load_telemetry(sorted((hallroot/'sources').glob('9950011_20260928_*.log')))
    events={e['event_serial']:e for e in json.loads((hallroot/'continuous_hall_analysis.json').read_text())['events']}
    seqix={int(s):i for i,s in enumerate(ir['sequence'])}
    endpoints=[]
    original=ARCHIVE/'originals/ir-pulse-count-circuits-20260928'
    for row in csv.DictReader((original/'endpoint_counts.csv').open()):
        serial=int(row['event']); e=events[serial]; t=int(hall['t_us'][e['sample_index']])
        selected=seqix[int(tele[serial]['discrepancy']['ir_seq'])]
        left=int(np.searchsorted(ir['aligned_online_us'],t,side='right')-1); right=left+1
        actual={'event':serial,'selected':int(ir['completed_pulses'][selected]),
                'before':int(ir['completed_pulses'][left]),'after':int(ir['completed_pulses'][right])}
        for a,b in [('selected','selected_completed_pulses'),('before','immediate_before_count'),('after','immediate_after_count')]:
            assert actual[a]==int(row[b]), (a,row,actual)
        for index,prefix in [(left,'immediate_before'),(right,'immediate_after')]:
            assert int(ir['sequence'][index])==int(row[prefix+'_sequence'])
            assert abs((int(ir['aligned_online_us'][index])-t)/1000-float(row[prefix+'_offset_ms']))<0.0011
        endpoints.append(actual)
    by={r['event']:r for r in endpoints}; counts=[]
    for row in csv.DictReader((original/'circuit_counts.csv').open()):
        a,b=by[int(row['start_event'])],by[int(row['end_event'])]
        count=b['selected']-a['selected']; counts.append(count)
        assert count==int(row['selected_snapshot_delta'])
        assert b['before']-a['after']==int(row['exact_opening_delta_low'])
        assert b['after']-a['before']==int(row['exact_opening_delta_high'])
        assert abs(52150/count-float(row['map_implied_mm_per_selected_pulse']))<1e-9
    assert counts==[5342,5385,5351]
    result={'endpoints':endpoints,'circuit_pulses':counts,'aggregate':sum(counts),
        'status':'Raw selected counts and aligned endpoint brackets reproduced; geographic endpoint judgments remain report interpretations, not automatic truth.'}
    (output/'circuit-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS raw circuit counts and endpoint brackets',flush=True)

def compare_json(expected, actual, key=''):
    # Stored source locations change when work is relocated; measurements do not.
    if key in ('capture','path','logs','source_capture'): return
    if isinstance(expected,dict):
        assert expected.keys()==actual.keys(), key
        for k,v in expected.items(): compare_json(v,actual[k],k)
    elif isinstance(expected,list):
        assert len(expected)==len(actual), key
        for x,y in zip(expected,actual): compare_json(x,y,key)
    elif isinstance(expected,(float,int)) and not isinstance(expected,bool):
        assert (math.isnan(expected) and math.isnan(actual)) or math.isclose(expected,actual,rel_tol=1e-9,abs_tol=1e-9), (key,expected,actual)
    else: assert expected==actual,(key,expected,actual)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--verify-only',action='store_true')
    ap.add_argument('--output-dir',type=Path)
    args=ap.parse_args(); manifest=verify()
    if args.verify_only:return
    if args.output_dir is None:ap.error('--output-dir must be a new directory outside the archive')
    out=args.output_dir.resolve()
    if out==ARCHIVE or ARCHIVE in out.parents:ap.error('output must be outside the archive')
    out.mkdir(parents=True,exist_ok=False)
    for row in manifest['files']:
        if not row['path'].startswith(('originals/','dependencies/')): continue
        dest=out/row['path']; data=(ARCHIVE/row['path']).read_bytes()
        if row['encoding']=='gzip':dest=dest.with_suffix('');data=gzip.decompress(data)
        dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
    h=out/'originals/hall-field-analysis-20260929'
    b=h/'analyze_continuous_hall.py'; od=h/'analyze_ir_odometry_calibration.py'
    old='REPO = Path("/Users/davidbrown/NGR/NGR-Files/navi-ewo-0-3-stop-integration")'
    s=b.read_text();assert old in s
    b.write_text(s.replace(old,'REPO = Path('+repr(str(out/'originals/navi-ewo-0-3-stop-integration'))+')',1))
    freq=out/'originals/ir-reporting-frequency-review-20261012/measure_sept28_ir_age.py'
    old='ROOT = Path("/Users/davidbrown/NGR/NGR-Files")';s=freq.read_text();assert old in s
    freq.write_text(s.replace(old,'ROOT = Path('+repr(str(out/'originals'))+')',1))
    cap=h/'sources/otto_navi_sync_20260928_101705.nsr';data=h/'continuous_hall_analysis.json'
    logs=sorted((h/'sources').glob('9950011_20260928_*.log'))
    jobs=[('continuous',[b,cap,'--logs',*logs,'--output',data,'--plots-dir',h/'plots']),
          ('opening-position',[h/'analyze_opening_positions.py',cap,data,'--base-script',b,'--output-dir',h/'opening-position']),
          ('opening-map',[od,cap,data,'--base-script',b,'--logs',*logs,'--output-dir',h/'opening-map']),
          ('morphology',[h/'analyze_morphology_candidates.py',cap,data,'--base-script',b,'--odometry-script',od,'--logs',*logs,'--output-dir',h/'morphology-candidate']),
          ('waveforms',[h/'plot_hall_intervals_254_257.py','--analysis-root',h]),
          ('reporting-age',[freq])]
    env=os.environ.copy();env.update(MPLBACKEND='Agg',MPLCONFIGDIR=str(out/'matplotlib-cache'),PYTHONDONTWRITEBYTECODE='1')
    os.environ.setdefault('MPLCONFIGDIR',str(out/'matplotlib-cache'))
    results=[]
    for name,cmd in jobs:
        print('Running '+name,flush=True)
        with (out/(name+'.log')).open('w') as log:
            subprocess.run([sys.executable,*map(str,cmd)],check=True,stdout=log,stderr=subprocess.STDOUT,env=env)
        results.append(name)
    sys.path.insert(0,str(out/'originals/navi-ewo-0-3-stop-integration/tools'))
    verify_circuits(h,out)
    review=out/'originals/navi-ir-sync-review-20261012/reproduce_review.py'
    s=review.read_text();old="REPO = Path('/Users/davidbrown/esp-loco-control')";assert old in s
    review.write_text(s.replace(old,'REPO = Path('+repr(str(out/'dependencies/review-baseline'))+')',1))
    with (out/'review-reproduction.log').open('w') as log:
        subprocess.run([sys.executable,str(review)],check=True,stdout=log,stderr=subprocess.STDOUT,env=env)
    jsons=['hall-field-analysis-20260929/continuous_hall_analysis.json',
           'hall-field-analysis-20260929/opening-position/opening_position_summary.json',
           'hall-field-analysis-20260929/opening-map/ir_calibration_summary.json',
           'hall-field-analysis-20260929/morphology-candidate/summary.json',
           'hall-field-analysis-20260929/waveform-254-257/waveform_metrics.json',
           'ir-reporting-frequency-review-20261012/sept28_ir_age_metrics.json']
    for rel in jsons:
        compare_json(json.loads((ARCHIVE/'originals'/rel).read_text()),json.loads((out/'originals'/rel).read_text()))
    csv_matches=[]
    for name in ['opening-position','opening-map','morphology-candidate','waveform-254-257']:
        for p in sorted((ARCHIVE/'originals/hall-field-analysis-20260929'/name).glob('*.csv')):
            rel=p.relative_to(ARCHIVE/'originals')
            assert p.read_bytes()==(out/'originals'/rel).read_bytes(),str(rel)
            csv_matches.append(str(rel))
    import numpy,matplotlib
    result={'analysis_jobs':results,'json_results_matched':jsons,'circuit_verification':'PASS','submitted_defects_reproduced':'PASS',
            'csv_results_byte_identical':csv_matches,
            'python':sys.version,'numpy':numpy.__version__,'matplotlib':matplotlib.__version__,
            'numerical_tolerance':'1e-9 relative/absolute; only relocated source paths ignored',
            'plots':'regenerated; original images preserved byte for byte; renderer differences not numerically compared'}
    (out/'reproduction-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS six numerical JSON results, circuits and four original defect reproductions',flush=True)

if __name__=='__main__':main()
