"""Hash the delivered package, excluding caches and the manifest itself."""
import json
from prepare import ROOT, sha

def main():
    artifacts=[]
    for p in sorted(ROOT.rglob('*')):
        if not p.is_file() or p.name=='PACKAGE_MANIFEST.json':continue
        if any(x in p.parts for x in ['__pycache__','.venv','.DS_Store']):continue
        artifacts.append(dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=sha(p)))
    result={'base_commit':'6aea978ae45e7164d176dfdda6d38ff41e8f272b',
            'research_branch':'research/navi-rvm-statistical-shape-comparison-20261018',
            'artifact_count':len(artifacts),'artifacts':artifacts}
    (ROOT/'PACKAGE_MANIFEST.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f"Hashed {len(artifacts)} research artifacts")

if __name__=='__main__':main()
