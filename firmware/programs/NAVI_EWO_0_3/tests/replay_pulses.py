"""Replay real completed timestamps; never invent pulses or Hall DNA."""
from pathlib import Path
import gzip,hashlib,json,subprocess,tempfile
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'replay'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(HERE/'replay_timing.cpp'),'-o',str(exe)],check=True)
    results=[]
    for fixture in json.loads((HERE/'fixtures/manifest.json').read_text()):
        path=HERE/'fixtures'/(fixture['name']+'.txt.gz')
        assert hashlib.sha256(path.read_bytes()).hexdigest()==fixture['fixture_sha256']
        data=gzip.decompress(path.read_bytes())
        result=json.loads(subprocess.check_output([str(exe)],input=data))
        assert result['events']==fixture['events']
        assert result['proven_physical_missed_pulses']==0 and not result['dna_replay']
        if fixture['name']=='oct09_run2_onset':assert result['timing_questions']>0
        result['name']=fixture['name'];results.append(result)
    print(json.dumps(results,indent=2))
