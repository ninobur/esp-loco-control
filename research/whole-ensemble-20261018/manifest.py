"""Hash the complete new research package without changing the controls."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parent
frozen=json.loads((ROOT/'models/FREEZE.json').read_text())
for rel,expected in frozen.items():
    assert hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()==expected, f'Frozen input changed: {rel}'
files=[]
for p in sorted(ROOT.rglob('*')):
    if not p.is_file() or '__pycache__' in p.parts or p.name=='PACKAGE_MANIFEST.json': continue
    files.append(dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
manifest=dict(base_commit='8c40d6dab0b957e6884eb81b2836c5a5fa2062cd',
              source_review_commit='be4c6a3',model_freeze_commit='1c2d14d',
              file_count=len(files),files=files)
(ROOT/'PACKAGE_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(f'Hashed {len(files)} package files')
