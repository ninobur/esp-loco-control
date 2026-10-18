#!/usr/bin/env python3
"""One-pass empirical calibration; never imports evaluation fixtures."""
from pathlib import Path
import subprocess,csv,json,hashlib,tempfile
scratch=tempfile.TemporaryDirectory(prefix="rvm-calibration-")
calibration_exe=str(Path(scratch.name)/"calibrate")
quality_exe=str(Path(scratch.name)/"quality")
ROOT=Path(__file__).resolve().parents[2]; D=ROOT/'tests/navi_rvm'; H=ROOT/'firmware/test-programs/NAVI_RVM'
meta=json.loads((D/'fixtures/calibration_cw.json').read_text())
labels=meta['scoring_only_legacy_selections']
pairs=''.join(f"{b['target']} {a['open_us']} {b['open_us']}\n" for a,b in zip(labels[:-1],labels[1:]))
(D/'results/calibration_labels.txt').write_text(pairs)
subprocess.run(['g++','-std=c++17','-O2','-I'+str(H),str(D/'calibrate.cpp'),'-o',calibration_exe],check=True)
r=subprocess.run([calibration_exe,str(D/'fixtures/calibration_cw.csv'),str(D/'results/calibration_labels.txt')],capture_output=True,text=True,check=True)
(D/'results/calibration_scores.csv').write_text(r.stdout)
rows=list(csv.DictReader(r.stdout.splitlines()))
negative=max(float(r['quiet_max']) for r in rows);positive=min(float(r['appearance_max']) for r in rows)
quality=max(float(r['appearance_quality']) for r in rows)
threshold=(negative+positive)/2
# Retain an observed upper envelope, rounded up to one decimal for the candidate.
import math
quality=math.ceil(quality*10)/10
header=f'''#pragma once
namespace rvm_calibration {{
// Empirical CW_L2 excerpt separation and observed quality envelope. See
// tests/navi_rvm/results/calibration.json; these are not reliability guarantees.
inline constexpr float appearanceEvidence = {threshold:.6f}f;
inline constexpr float atypicalQuality = {quality:.6f}f;
}}
'''
(H/'RvmCalibration.h').write_text(header)
subprocess.run(['g++','-std=c++17','-O2','-I'+str(H),str(D/'replay.cpp'),'-o',quality_exe],check=True)
full=subprocess.run([quality_exe,str(D/'fixtures/calibration_cw.csv'),'124','1','--calibration'],capture_output=True,text=True,check=True)
(D/'results/calibration_full_quality.csv').write_text(full.stdout)
full_rows=list(csv.DictReader(full.stdout.splitlines()))
quality=math.ceil(max(float(r['quality']) for r in full_rows if r['final_quality']=='1')*10)/10
import re
header=re.sub(r'atypicalQuality = [0-9.]+f',f'atypicalQuality = {quality:.6f}f',header)
(H/'RvmCalibration.h').write_text(header)
report={'training':'calibration_cw.csv; CW lap 2, separate from CW lap 1 reference', 'pairs':len(rows),'quiet_max':negative,'appearance_min':positive,'separated':negative<positive,'threshold':threshold,'quality_upper_envelope':quality,'method':'Midpoint between largest quiet-interval evidence and smallest appearance evidence; quality is the rounded-up maximum finalized new-MM cost from a full calibration-only replay with classification disabled. One pass, no tuning against evaluation counts.','limitations':['Legacy opening labels are fallible timing comparators; calibration annotations are not independent field truth.','One evening, one locomotive, one CW excerpt. No held-out-day or CCW calibration.','Overlapping raw samples are averaged in disjoint blocks; integrated contrast is not a statistical probability.'],'header_sha256':hashlib.sha256(header.encode()).hexdigest()}
(D/'results/calibration.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
