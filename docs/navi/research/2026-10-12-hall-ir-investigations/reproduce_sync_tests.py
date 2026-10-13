#!/usr/bin/env python3
"""Run the corrected sketch's original regression runner in an isolated tree."""
from pathlib import Path
import argparse, json, shutil, subprocess, sys
from reproduce import ARCHIVE, verify

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--output-dir',type=Path,required=True)
args=ap.parse_args();verify()
out=args.output_dir.resolve()
if out==ARCHIVE or ARCHIVE in out.parents:ap.error('output must be outside the archive')
out.mkdir(parents=True,exist_ok=False)
shutil.copytree(ARCHIVE/'dependencies/review-baseline/firmware',out/'firmware')
rel=Path('firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST')
shutil.copytree(ARCHIVE/'originals/navi-ir-sync-fix-20261012'/rel,out/rel)
cmd=[sys.executable,str(out/rel/'tests/run_all.py'),str(out/'results')]
with (out/'regression.log').open('w') as f:
    subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
print((out/'regression.log').read_text())
(out/'result.json').write_text(json.dumps({'status':'PASS','runner':'unchanged archived tests/run_all.py',
    'scope':'Ten inherited C++ suites, thirteen health JSON cases, source equivalence and 240-judgment Type-6 isolation/timing regression',
    'hardware_used':False,'firmware_built_or_flashed':False,'python':sys.version},indent=2)+'\n')
