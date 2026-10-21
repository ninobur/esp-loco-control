"""Frozen CPR construction/calibration/evaluation runner. No geographic inputs to CPR."""
import argparse
import csv
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import platform
import time

import numpy as np

from cpr import Config, NativeRule, Population, Segmenter, describe
from prepare import NATIVE, ROOT, sha


def jsonable(obj):
    if isinstance(obj, np.ndarray): return obj.tolist()
    if isinstance(obj, np.generic): return obj.item()
    raise TypeError(type(obj).__name__)


def dump(path, data):
    Path(path).write_text(json.dumps(data, indent=2, default=jsonable, allow_nan=False))


def read_annotations(name, a=-np.inf, b=np.inf):
    path = ROOT / ('data/train_annotations.csv' if name == 'otto_train' else f'data/{name}_historical.csv')
    rows = []
    for r in csv.DictReader(open(path)):
        if name == 'otto_train' and r['source'] not in ('OPENING', 'SEQUENCE'):
            continue
        s, e = float(r['start_us']) / 1e6, float(r['end_us']) / 1e6
        if s < a or e >= b or e <= s:
            continue
        rows.append(dict(start=s, end=e, source=r.get('source', r.get('result', '')),
                         historical_polarity=r.get('polarity', ''),
                         historical_mm=r.get('mm', ''), seq=r.get('seq', ''),
                         amplitude=float(r.get('amplitude', 0))))
    return sorted(rows, key=lambda r: r['start'])


def run_record(name, cfg, a=-np.inf, b=np.inf, data_path=None, native_path=None):
    with np.load(data_path or ROOT / f'data/{name}.npz') as source:
        z = dict(source)
    native = np.memmap(native_path or ROOT / f'raw/{name}.bin', dtype=NATIVE, mode='r')
    indices = np.flatnonzero((z['t'] >= a) & (z['t'] < b))
    journal_rows = []
    s, gate = Segmenter(cfg, journal_rows.append), NativeRule()
    events, baseline, noise, traces, witness_rows, timings = [], [], [], [], [], []
    started = time.perf_counter()
    for i in indices:
        ti = time.perf_counter()
        t, x = float(z['t'][i]), float(z['hall'][i])
        proof = None
        if s.warm >= cfg.warmup and np.isfinite(x):
            # Available baseline before any raw acquisition sample in this block.
            hits = gate.feed(native[int(z['lo'][i]):int(z['hi'][i])], s.base.predict(t-cfg.dt))
            for hit in hits:
                witness_rows.append(dict(kind='native70_detection', **hit))
            proof = hits[0] if hits else None
        else:
            gate.last = None; gate.latched = False
        event, base, ns = s.feed(t, x, proof)
        if event:
            d = describe(event)
            if d is not None:
                event['features'] = d
                events.append(event)
        baseline.append(base); noise.append(ns); traces.append(x)
        timings.append(time.perf_counter()-ti)
    seconds = time.perf_counter() - started
    s.finish()
    # Journal is a sink outside bounded online state; append-only JSONL in a device.
    journal = sorted(journal_rows + witness_rows, key=lambda r: r['t'])
    return dict(name=name, events=events, journal=journal, t=z['t'][indices],
                hall=np.array(traces), baseline=np.array(baseline), noise=np.array(noise),
                resource=dict(wall_s=seconds, bins=len(indices),
                    ms_per_bin_p50=float(np.median(timings)*1000),
                    ms_per_bin_p99=float(np.quantile(timings, .99)*1000),
                    ms_per_bin_max=float(max(timings)*1000),
                    max_event_bins=s.max_buffer, baseline_points=cfg.history,
                    exposure_s=float(len(indices)*cfg.dt),
                    observed_s=float(np.isfinite(traces).sum()*cfg.dt)))


def iou(x, y):
    overlap = max(0., min(x['end'], y['end'])-max(x['start'], y['start']))
    return overlap / max(1e-9, max(x['end'],y['end'])-min(x['start'],y['start']))


def match(events, truth):
    # Descending-IoU one-to-one assignment; not an oracle for recognition.
    candidates = sorted([(iou(e, r), i, j) for i, e in enumerate(events) for j, r in enumerate(truth) if iou(e, r) >= .3], reverse=True)
    ei, tj, pairs = set(), set(), []
    for score, i, j in candidates:
        if i not in ei and j not in tj:
            pairs.append((i, j, score)); ei.add(i); tj.add(j)
    counts = dict(events=len(events), annotations=len(truth), matched=len(pairs),
        missed_annotation_proxy=len(truth)-len(pairs), unmatched_objects=len(events)-len(pairs),
        recall_proxy=len(pairs)/len(truth) if truth else None,
        precision_proxy=len(pairs)/len(events) if events else None,
        f1_proxy=2*len(pairs)/max(1, len(events)+len(truth)),
        split_proxy=sum(sum(iou(e, r) >= .1 for e in events) > 1 for r in truth),
        merge_proxy=sum(sum(iou(e, r) >= .1 for r in truth) > 1 for e in events))
    for field in ('start', 'end'):
        err = [events[i][field]-truth[j][field] for i,j,_ in pairs]
        counts[f'{field}_error_abs_median_s'] = float(np.median(np.abs(err))) if err else None
        counts[f'{field}_error_signed_median_s'] = float(np.median(err)) if err else None
    latency = [events[i]['completed']-truth[j]['end'] for i,j,_ in pairs]
    counts['latency_vs_historical_end_median_s'] = float(np.median(latency)) if latency else None
    counts['completion_before_historical_end_count'] = sum(x < 0 for x in latency)
    counts['baseline_return_latency_median_s'] = float(np.median([e['completed']-e['end'] for e in events])) if events else None
    return counts, pairs


def positives(replay, truth):
    _, pairs = match(replay['events'], truth)
    # Require unambiguous association. Keep rejected/censored annotations in reports.
    return [replay['events'][i]['features'] for i,j,_ in pairs
            if sum(iou(replay['events'][i], r) >= .1 for r in truth) == 1]


def persist_replay(replay, model=None, output_dir=None):
    output_dir = Path(output_dir) if output_dir else ROOT / 'results'
    output_dir.mkdir(parents=True, exist_ok=True)
    name = replay['name']
    rows = []
    decisions = []
    for e in replay['events']:
        d = e['features']
        row = {k: v for k,v in e.items() if k not in ('samples', 'features')}
        row.update({k: v for k,v in d.items() if k != 'shape'})
        row['shape64'] = d['shape']
        row['scores'] = model.decide(d) if model else {}
        rows.append(row)
        for kind, score in row['scores'].items():
            decisions.append(dict(kind='recognized' if score['recognized'] else 'unrecognized',
                                  t=e['completed'], id=e['id'], model=kind, **score))
    dump(output_dir / f'{name}_events.json', rows)
    with open(output_dir / f'{name}_journal.jsonl', 'w') as f:
        for row in sorted(replay['journal']+decisions, key=lambda r: r['t']):
            f.write(json.dumps(row, default=jsonable)+'\n')
    np.savez_compressed(output_dir / f'{name}_trace.npz',
                        **{k:replay[k] for k in ('t','hall','baseline','noise')})


def assess(replay, truth, model):
    seg, pairs = match(replay['events'], truth)
    out = dict(segmentation=seg, resource=replay['resource'],
               incomplete=sum(r['kind']=='incomplete' for r in replay['journal']),
               recognition={}, physical_false_positive_rate=None,
               physical_missed_recognition_rate=None,
               independent_completion_accuracy=None)
    for kind in ('pca', 'template', 'gaussian'):
        chosen = [e for e in replay['events'] if model.decide(e['features'])[kind]['recognized']]
        metrics, _ = match(chosen, truth)
        metrics['unmatched_recognitions_per_observed_hour'] = metrics['unmatched_objects']/(replay['resource']['observed_s']/3600)
        metrics['recognized_count'] = len(chosen)
        out['recognition'][kind] = metrics
    return out


def calibrate():
    comparison = []
    calz = np.load(ROOT / 'data/otto_cw.npz')
    cut = float(calz['t'][0] + .6*(calz['t'][-1]-calz['t'][0]))
    train_truth = read_annotations('otto_train')
    cal_truth = read_annotations('otto_cw', b=cut)
    best = None
    for baseline in ('linear', 'ema'):
        for ret in (.12, .24, .48):
            cfg = Config(baseline=baseline, return_s=ret)
            tr = run_record('otto_train', cfg)
            ca = run_record('otto_cw', cfg, b=cut)
            tm,_ = match(tr['events'], train_truth); cm,_ = match(ca['events'], cal_truth)
            row = dict(config=asdict(cfg), training=tm, calibration=cm)
            comparison.append(row)
            criterion = (cm['f1_proxy'], ret == .24, baseline == 'linear')
            if best is None or criterion > best[0]:
                best = (criterion, cfg, tr, ca)
            print('component', baseline, ret, 'train', tm['matched'], '/', tm['annotations'], 'cal', cm['matched'], '/', cm['annotations'], flush=True)
    _, cfg, tr, ca = best
    pos = positives(tr, train_truth); cp = positives(ca, cal_truth)
    model = Population(pos); model.calibrate(cp)
    dump(ROOT / 'results/component_comparison.json', comparison)
    dump(ROOT / 'models.json', dict(config=asdict(cfg), population=model.serialize(),
        calibration_cut_s=cut, training_objects=len(pos), calibration_objects=len(cp),
        frozen_before_evaluation=True))
    persist_replay(tr, model)
    ca['name'] = 'otto_cw_calibration'
    persist_replay(ca, model)
    dump(ROOT / 'results/calibration.json', dict(train=assess(tr, train_truth, model),
        calibration=assess(ca, cal_truth, model), training_objects=len(pos), calibration_objects=len(cp),
        frozen_model_sha256=sha(ROOT / 'models.json'), cut_s=cut))
    print('frozen', asdict(cfg), 'objects', len(pos), len(cp), 'thresholds', model.thresholds, flush=True)


def load_model():
    frozen = json.loads((ROOT / 'models.json').read_text())
    model = Population.__new__(Population)
    for k,v in frozen['population'].items():
        setattr(model, k, np.array(v) if isinstance(v, list) else v)
    return Config(**frozen['config']), model, frozen


def evaluate():
    cfg, model, frozen = load_model()
    modelhash = sha(ROOT / 'models.json')
    results = dict(model_sha256=modelhash, environment=dict(python=platform.python_version(),
        numpy=np.__version__, machine=platform.machine(), platform=platform.platform()), recordings={})
    for name in ('otto_cw', 'otto_ccw', 'toby_cw'):
        lower = frozen['calibration_cut_s'] if name == 'otto_cw' else -np.inf
        replay = run_record(name, cfg, a=lower)
        truth = read_annotations(name, a=lower)
        results['recordings'][name] = assess(replay, truth, model)
        persist_replay(replay, model)
        print(name, results['recordings'][name]['segmentation'], flush=True)
    if sha(ROOT / 'models.json') != modelhash:
        raise AssertionError('evaluation altered frozen model')
    dump(ROOT / 'results/evaluation.json', results)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('stage', choices=('calibrate', 'evaluate'))
    args = parser.parse_args()
    calibrate() if args.stage == 'calibrate' else evaluate()
