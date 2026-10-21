"""Additional evidence accounting; does not change any fitted model or decision."""
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np
from experiment import ROOT, dump, load_model, match, read_annotations
from prepare import NATIVE, sha


def spans(t, mask, dt=.02):
    inds = np.flatnonzero(mask)
    groups = np.split(inds, np.flatnonzero(np.diff(inds)>1)+1)
    return [[float(t[g[0]]-dt), float(t[g[-1]])] for g in groups if len(g)]


def main():
    _, model, _ = load_model()
    out = dict(native_roundtrip={}, case={}, errors={}, resources={})
    manifest = json.loads((ROOT/'sources/data_manifest.json').read_text())
    for name, meta in manifest['boots'].items():
        with np.load(ROOT/f'data/{name}_native.npz') as z:
            a = np.empty(len(z['adc']), dtype=NATIVE)
            a['serial'] = np.cumsum(z['serial_delta']); a['us'] = np.cumsum(z['us_delta'])
            for key in ('adc','pwm','direction'):
                a[key] = z[key]
            digest = hashlib.sha256(a.tobytes()).hexdigest()
        out['native_roundtrip'][name] = dict(sha256=digest, byte_identical=digest==meta['native_sha256'])
        if digest != meta['native_sha256']:
            raise AssertionError(name)
    z = dict(np.load(ROOT/'results/otto_ccw_trace.npz'))
    mask = (z['t']>=965.43)&(z['t']<=967.05)
    gaps = spans(z['t'],~np.isfinite(z['hall']))
    journal = [json.loads(l) for l in (ROOT/'results/otto_ccw_journal.jsonl').read_text().splitlines()]
    events = json.loads((ROOT/'results/otto_ccw_events.json').read_text())
    out['case'] = dict(historical_false_window_s=[965.43,967.05],
        historical_false_amplitude_adc=12.4,
        overlapping_native_gaps_s=[g for g in gaps if g[0]<=967.05 and g[1]>=965.43],
        baseline_ready_nearby=[r for r in journal if r['kind']=='baseline_ready' and 964<r['t']<970],
        native70_nearby=[r for r in journal if r['kind']=='native70_detection' and 964<r['t']<973],
        following_south=[{k:v for k,v in e.items() if k!='shape64'} for e in events if 969<e['peak_t']<972],
        interpretation='INCONCLUSIVE: native gap and subsequent warmup censor the false-event window; no demonstrated rejection')
    # Preserve the Pi MQTT episode, including the actual observer declaration.
    mqtt = ROOT/'raw/otto_ccw_mqtt.log'
    if mqtt.exists():
        lines = [l for l in mqtt.read_text().splitlines() if '2026-10-19T18:19:' in l
                 and ('CONTRADICTION' in l or 'ENSEMBLE' in l or '"seq":624' in l or '"seq":625' in l)]
        (ROOT/'sources/mm101_mqtt_excerpt.log').write_text('\n'.join(lines)+'\n')
        out['case']['mqtt_source'] = '/home/david/NGR/telemetry/runs/9950011_20261019_180350.log'
        out['case']['mqtt_source_sha256'] = sha(mqtt)
    # Descriptive failure audit: no edits to primary frozen evaluation metrics.
    evals = json.loads((ROOT/'results/evaluation.json').read_text())
    timings = []
    for name in ('otto_cw','otto_ccw','toby_cw'):
        es = json.loads((ROOT/f'results/{name}_events.json').read_text())
        truth = read_annotations(name)
        if name=='otto_cw':
            cut = json.loads((ROOT/'models.json').read_text())['calibration_cut_s']
            truth = [r for r in truth if r['start']>=cut]
        _, pairs = match(es, truth)
        associated = {j for _,j,_ in pairs}
        matched_e = {i for i,_,_ in pairs}
        out['errors'][name] = dict(
            first_unmatched_historical_objects=[r for j,r in enumerate(truth) if j not in associated][:12],
            first_unmatched_cpr_objects=[{k:v for k,v in r.items() if k!='shape64'} for i,r in enumerate(es) if i not in matched_e][:12])
        for e in es:
            d = dict(e); d['shape'] = np.array(d.pop('shape64'))
            before = time.perf_counter(); model.decide(d); timings.append(time.perf_counter()-before)
    out['resources'] = dict(scoring_calls=len(timings),
        all_three_models_ms_p50=float(np.median(timings)*1000),
        all_three_models_ms_p99=float(np.quantile(timings,.99)*1000),
        all_three_models_ms_max=float(max(timings)*1000),
        max_stored_event_bins=1519,
        proposed_c_compact_event_bytes_per_bin=16,
        proposed_c_compact_event_bytes=1519*16,
        target_runtime_measured=False)
    # Known background is construction-only, so never call this a held-out FPR.
    background_source = ROOT.parents[1]/'analysis/navi_rvm_shape_20261018/derived/ground_boot1.csv'
    bg=[]
    for r in csv.DictReader(open(background_source)):
        a,b=float(r['start_us'])/1e6,float(r['end_us'])/1e6
        a,b=max(a,100),min(b,1800)
        if b>a:
            bg.append((a,b))
    train = json.loads((ROOT/'results/otto_train_events.json').read_text())
    out['training_background'] = dict(exposure_s=sum(b-a for a,b in bg),
        positive_events_wholly_inside_background=sum(
            e['scores']['pca']['recognized'] and any(a<=e['start'] and e['end']<=b for a,b in bg) for e in train),
        status='construction-only ADC background labels; not a test false-positive rate',
        source_sha256=sha(background_source))
    dump(ROOT/'results/evidence_audit.json', out)
    print(json.dumps(out['case'],indent=2))


if __name__=='__main__':
    main()
