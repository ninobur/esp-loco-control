#!/usr/bin/env python3
"""Run original host regressions unchanged, then the new isolation regression."""
from pathlib import Path
import subprocess
import sys
import tempfile

variant=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(tempfile.mkdtemp(prefix='ir-sync-regression-'))
out.mkdir(parents=True,exist_ok=True)
fixture=out/'legacy-filenames'
fixture.mkdir(exist_ok=True)
# The unmodified 20Q3 test opens the old filename to check the detector setting.
# Map that name to the corrected sketch, so the test checks the actual build.
for name,target in [('LocoConfig.h',variant/'LocoConfig.h'),('NAVI_COHERENCE_0_6_IR_HEALTH.ino',variant/'NAVI_COHERENCE_0_6_IR_SYNC_TEST.ino')]:
    p=fixture/name
    if not p.exists():p.symlink_to(target)
for test in sorted((variant/'tests').glob('test_*.cpp')):
    exe=out/test.stem
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(variant),str(test),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe),str(fixture)],check=True,text=True,capture_output=True)
    (out/(test.stem+'.log')).write_text(result.stdout+result.stderr)
    print('PASS '+test.stem,flush=True)
subprocess.run([sys.executable,str(variant/'tests/test_health_json.py'),str(out/'test_ir_health_monitor')],check=True)
subprocess.run([sys.executable,str(variant/'tests/test_ir_sync.py'),str(out)],check=True)
print('PASS all 10 original C++ suites, original JSON checks, source equivalence and new isolation/timing regression',flush=True)
