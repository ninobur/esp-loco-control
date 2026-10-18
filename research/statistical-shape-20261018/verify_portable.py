"""Run a clean portable reconstruction without native archives or prior outputs."""
from pathlib import Path
import shutil, subprocess, sys, tempfile, json
from prepare import ROOT, sha

def main():
    dest=Path(tempfile.mkdtemp(prefix='ngr-shape-portable-'))
    for name in ['prepare.py','evaluate.py','analyze.py','validate.py']:
        shutil.copy2(ROOT/name,dest/name)
    # Only the delivered portable observations, seeds/annotations and metadata.
    shutil.copytree(ROOT/'data',dest/'data')
    shutil.copytree(ROOT/'sources',dest/'sources')
    for name in ['results','models','figures']:(dest/name).mkdir()
    with open(dest/'run.txt','w') as f:
        for name in ['evaluate.py','analyze.py','validate.py']:
            subprocess.run([sys.executable,str(dest/name)],check=True,cwd=dest,stdout=f,stderr=subprocess.STDOUT)
    compared=[]
    for folder in ['models','results','figures']:
        for path in sorted((dest/folder).iterdir()):
            original=ROOT/folder/path.name
            if path.is_file() and original.exists():
                assert sha(path)==sha(original), f'Non-reproducible artifact: {folder}/{path.name}'
                compared.append(f'{folder}/{path.name}')
    result={'status':'PASS','clean_reproduction_directory':str(dest),
        'native_archive_used':False,'prior_models_or_results_used':False,
        'identical_artifacts':compared,'python':sys.version}
    (ROOT/'results/portable_reproduction.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
