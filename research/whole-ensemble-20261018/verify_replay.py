"""Deterministic actual-recording replay checks at already frozen parameters."""
from pathlib import Path
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','1')
import json
import numpy as np
import pandas as pd
from functional import infer
ROOT=Path(__file__).resolve().parent

def main():
    spans=pd.read_csv(ROOT/'data/eligible_spans.csv')
    selected=spans[(spans.partition=='eval')&((spans.hi-spans.lo)>=200)&((spans.hi-spans.lo)<=500)].iloc[:2]
    waves={b:pd.read_csv(ROOT.parent/f'statistical-shape-20261018/data/hall20_{b}.csv.gz') for b in selected.boot.unique()}
    checks=[]
    for name in ('A','B'):
        model=json.loads((ROOT/f'models/{name}.json').read_text())
        obs=json.loads((ROOT/f'results/evaluation_{name}.json').read_text())
        fits=np.load(ROOT/f'results/fits_{name}_0.npz')
        for s in selected.itertuples(index=False):
            out=infer(waves[s.boot].adc.iloc[s.lo:s.hi].to_numpy(float),model)
            expected=[o for o in obs if o['span_id']==s.id]
            assert len(expected)==len(out['objects'])
            for a,b in zip(expected,out['objects']):
                assert a['state']==b['state'] and a['lo']==s.lo+b['lo'] and a['hi']==s.lo+b['hi']
                np.testing.assert_array_equal(a['mean'],b['mean']);np.testing.assert_array_equal(a['cov'],b['cov'])
            for key in ('baseline','field','precision'): np.testing.assert_array_equal(fits[s.id+'__'+key],out[key])
            checks.append(dict(model=name,span_id=s.id,observations=s.hi-s.lo,states=len(expected),exact_array_reproduction=True))
    (ROOT/'results/replay_validation.json').write_text(json.dumps(checks,indent=2)+'\n');print(json.dumps(checks,indent=2))
if __name__=='__main__':main()
