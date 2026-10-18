#!/usr/bin/env python3
"""Reproduce deterministic checks and focused replays from repository files only.
Comparisons are against legacy selection timestamps, not physical ground truth.
No calibration or model changes are made by this command.
"""
from pathlib import Path
import csv, hashlib, json, os, platform, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[2]
D = ROOT / 'tests/navi_rvm'
H = ROOT / 'firmware/test-programs/NAVI_RVM'
OUT = D / 'results'
OUT.mkdir(exist_ok=True)
compiler = os.environ.get('CXX', 'c++')
def run(args):
    return subprocess.run(args, capture_output=True, text=True, check=True)
summary = {'compiler': run([compiler, '--version']).stdout, 'host': platform.platform(),
           'limits': 'Recorded comparisons use fallible legacy labels. Host timing excludes firmware acquisition, queues, and network tasks.',
           'frozen_sources': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in [H/'RvmRecognizer.h', H/'RvmReference.h', H/'RvmCalibration.h']},
           'cases': {}, 'behavioral_failures': []}
with tempfile.TemporaryDirectory(prefix='navi-rvm-check-') as temp:
    binaries = {}
    for name in ['test_recognizer', 'replay']:
        exe = str(Path(temp)/name)
        build = run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-pedantic',
                     '-I'+str(H), str(D/(name+'.cpp')), '-o', exe])
        (OUT/(name+'-build.txt')).write_text(build.stdout+build.stderr or 'Build passed without diagnostics.\n')
        binaries[name] = exe
    test = run([binaries['test_recognizer']])
    (OUT/'deterministic.txt').write_text(test.stdout+test.stderr)
    print(test.stdout, end='')
    for setup in sorted((D/'fixtures').glob('*.json')):
        meta = json.loads(setup.read_text())
        data = setup.with_suffix('.csv')
        replay = run([binaries['replay'], str(data), str(meta['declared_mm']), str(meta['direction'])])
        (OUT/(setup.stem+'-decisions.csv')).write_text(replay.stdout)
        (OUT/(setup.stem+'-runtime.txt')).write_text(replay.stderr)
        rows = list(csv.DictReader(replay.stdout.splitlines()))
        tokens = [r for r in rows if r['final_quality']=='0' and r['stop_decision']=='0']
        labels = meta['scoring_only_legacy_selections']
        reference = {int(r['target']): int(r['open_us']) for r in labels}
        comparison = []
        seen = set()
        for i, r in enumerate(tokens, 1):
            mm = int(r['committed'])
            assert mm == (meta['declared_mm']+i*meta['direction']) % 171
            assert int(r['ordinal']) == i and int(r['expected']) == mm
            assert int(r['time_last_us']) <= int(r['commit_us'])
            # Every geographic token is checked and retained, not only the total.
            seen.add(mm)
            old = reference.get(mm)
            comparison.append([i, mm, r['commit_us'], old,
                               int(r['commit_us'])-old if old is not None else None])
        with (OUT/(setup.stem+'-comparison.csv')).open('w') as f:
            writer = csv.writer(f)
            writer.writerow(['ordinal','mm','rvm_commit_us','legacy_open_us','rvm_minus_legacy_us'])
            writer.writerows(comparison)
        delta = [r[-1] for r in comparison if r[-1] is not None]
        case = {'fixture_sha256': hashlib.sha256(data.read_bytes()).hexdigest(),
                'runtime': replay.stderr.strip(), 'tokens': len(tokens), 'legacy_selections': len(labels),
                'missing_legacy_ids': sorted(set(reference)-seen),
                'ids_without_legacy_label': sorted(seen-set(reference)),
                'min_rvm_minus_legacy_us': min(delta) if delta else None,
                'max_rvm_minus_legacy_us': max(delta) if delta else None,
                'quality_assessments': sum(r['final_quality']=='1' for r in rows),
                'atypical_final_ids': [int(r['committed']) for r in rows if r['final_quality']=='1' and r['atypical']=='1']}
        if setup.stem == 'grillers_stall' and 'stall=0' in replay.stderr:
            summary['behavioral_failures'].append('Stationary Grillers excerpt: PWM=90 and recorded IR speed=0, but no stall E-STOP. Do not treat this as a passing behavioral test.')
        summary['cases'][setup.stem] = case
        print(setup.stem, json.dumps(case))
(OUT/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')

for failure in summary['behavioral_failures']:
    print('OBSERVED FAILURE:', failure)
raise SystemExit(1 if summary['behavioral_failures'] else 0)
