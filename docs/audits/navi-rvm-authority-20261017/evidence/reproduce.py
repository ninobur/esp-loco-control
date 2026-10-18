#!/usr/bin/env python3
"""Read candidate sources; write only audit evidence and temporary host binaries."""
from pathlib import Path
import hashlib,json,subprocess,tempfile,csv,platform,re
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
H=ROOT/'firmware/test-programs/NAVI_RVM'
T=ROOT/'tests/navi_rvm'
def run(args):
    return subprocess.run(args,capture_output=True,text=True)
manifest={'candidate':'d18fe67e52deda91720be4384cd77c8f53c62cf0','host':platform.platform(),
          'compiler':run(['c++','--version']).stdout,'files':{},'results':{}}
for f in sorted(H.iterdir()):
    if f.is_file():manifest['files'][str(f.relative_to(ROOT))]={'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'lines':len(f.read_text().splitlines())}
text=(H/'NAVI_RVM.ino').read_text()
services=text[text.index('static void withdraw('):text.index('// CRC/length checks')]
assert services.count('static void ')==3
with tempfile.TemporaryDirectory(prefix='rvm-authority-audit-') as temp:
    temp=Path(temp)
    generated=(HERE/'probes.cpp').read_text().replace('// AUDIT_INSERT_VERBATIM_SERVICES',services)
    (temp/'probes.cpp').write_text(generated)
    for name,source in [('deterministic',T/'test_recognizer.cpp'),('replay',T/'replay.cpp'),('authority-probes',temp/'probes.cpp')]:
        exe=temp/name
        result=run(['c++','-std=c++17','-O2','-Wall','-Wextra','-pedantic','-I'+str(H),str(source),'-o',str(exe)])
        (HERE/(name+'-compile.txt')).write_text(result.stdout+result.stderr or 'Compiled without diagnostics.\n')
        assert result.returncode==0,result.stderr
        if name!='replay':
            result=run([str(exe)]);(HERE/(name+'.txt')).write_text(result.stdout+result.stderr)
            manifest['results'][name]={'exit':result.returncode};assert result.returncode==0,result.stderr
        else:
            for meta in sorted((T/'fixtures').glob('*.json')):
                m=json.loads(meta.read_text());fixture=meta.with_suffix('.csv')
                result=run([str(exe),str(fixture),str(m['declared_mm']),str(m['direction'])])
                (HERE/(meta.stem+'-decisions.csv')).write_text(result.stdout)
                (HERE/(meta.stem+'-runtime.txt')).write_text(result.stderr)
                rows=list(csv.DictReader(result.stdout.splitlines()))
                old=list(csv.DictReader((T/'results'/(meta.stem+'-decisions.csv')).open()))
                manifest['results'][meta.stem]={'exit':result.returncode,'all_decision_fields_match_candidate_evidence':rows==old,'runtime':result.stderr.strip(),'fixture_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest()}
                assert result.returncode==0
(HERE/'verification.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest['results'],indent=2))
