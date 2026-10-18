"""Preservation and portability checks; no model retuning or field-truth invention."""
from pathlib import Path
import hashlib,json,tempfile,shutil,subprocess,sys
import numpy as np
import pandas as pd
ROOT=Path(__file__).resolve().parent

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    # Compare every file tracked at both preserved experimental roots to base commit.
    repo=ROOT.parents[1]; roots=['research/statistical-shape-20261018','research/statistical-shape-70adc-20261018']
    checked=0
    for root in roots:
        paths=subprocess.check_output(['git','ls-tree','-r','--name-only','8c40d6d','--',root],cwd=repo,text=True).splitlines()
        for path in paths:
            blob=subprocess.check_output(['git','show',f'8c40d6d:{path}'],cwd=repo)
            assert hashlib.sha256(blob).hexdigest()==sha(repo/path),path
            checked+=1
    with tempfile.TemporaryDirectory(prefix='ngr-eligibility-') as tmp:
        research=Path(tmp);dest=research/'whole-ensemble-20261018';dest.mkdir()
        for name in ('sources','data','results'): (dest/name).mkdir()
        shutil.copy2(ROOT/'eligibility.py',dest/'eligibility.py')
        shutil.copy2(ROOT/'sources/movement_snapshots.csv.gz',dest/'sources/movement_snapshots.csv.gz')
        (research/'statistical-shape-20261018').symlink_to(ROOT.parent/'statistical-shape-20261018',target_is_directory=True)
        subprocess.run([sys.executable,str(dest/'eligibility.py')],check=True,stdout=subprocess.DEVNULL)
        portable=[]
        for rel in ['data/eligible_spans.csv','data/eligibility_15549e9385faa4d5.csv.gz','data/eligibility_4d8083de79a0e299.csv.gz',
                    'sources/movement_brackets.csv.gz','results/eligibility_intervals.csv','results/eligibility_summary.csv','results/seed_eligibility_diagnostic.csv']:
            assert sha(ROOT/rel)==sha(dest/rel),rel
            portable.append(rel)
    # Identical ADC coverage for both models and exact partition of every eligible span.
    spans=pd.read_csv(ROOT/'data/eligible_spans.csv');paths={}
    for name in ('A','B'):
        obs=json.loads((ROOT/f'results/evaluation_{name}.json').read_text());paths[name]=obs
        for s in spans[spans.partition=='eval'].itertuples(index=False):
            objects=[o for o in obs if o['span_id']==s.id]
            assert objects[0]['lo']==s.lo and objects[-1]['hi']==s.hi
            assert all(o['hi']>o['lo'] for o in objects)
            for a,b in zip(objects[:-1],objects[1:]):
                assert a['hi']==b['lo'] and a['state']!=b['state']
        units=pd.read_csv(ROOT/f'results/units_{name}.csv')
        ids={o['id'] for o in obs}
        assert len(ids)==len(obs)
        assert set(units.left_mm)|set(units.right_mm)<=ids
        for s,group in units.groupby('span_id',sort=False):
            for a,b in zip(group.itertuples(index=False),list(group.itertuples(index=False))[1:]):
                assert a.right_mm==b.left_mm
    result=dict(preserved_control_files=checked,portable_eligibility_byte_identical=portable,
                eval_spans_both=int((spans.partition=='eval').sum()),all_ADC_bins_exactly_once=True,
                no_cross_exclusion_states=True,shared_overlapping_MM_ids=True)
    (ROOT/'results/integrity.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__': main()
