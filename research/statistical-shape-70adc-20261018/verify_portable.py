"""Reproduce the revised experiment in a separate copy and compare outputs."""
from pathlib import Path
import tempfile,shutil,subprocess,sys,json
from repeat import ROOT,ORIGINAL,sha

def main():
    dest=Path(tempfile.mkdtemp(prefix='ngr-70adc-portable-'))
    shutil.copytree(ORIGINAL,dest/ORIGINAL.name,ignore=shutil.ignore_patterns('__pycache__'))
    run=dest/ROOT.name;run.mkdir()
    for name in ['repeat.py','compare.py','checks.py']:shutil.copy2(ROOT/name,run/name)
    for name in ['results','figures']:(run/name).mkdir()
    with open(dest/'run.txt','w') as f:
        for name in ['repeat.py','compare.py','checks.py']:
            subprocess.run([sys.executable,str(run/name)],check=True,cwd=run,stdout=f,stderr=subprocess.STDOUT)
    identical=[];numeric_equivalent=[]
    for folder in ['results','figures']:
        for path in sorted((run/folder).iterdir()):
            original=ROOT/folder/path.name
            assert original.exists(),str(original)
            assert sha(path)==sha(original),f'Reproduction differs: {folder}/{path.name}'
            identical.append(f'{folder}/{path.name}')
    result={'status':'PASS','native_archive_used':False,'models_refitted':False,
            'prior_revised_outputs_used':False,'identical_artifacts':identical,
            'original_model_sha256':sha(ORIGINAL/'models/models.json')}
    (ROOT/'results/portable_reproduction.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
