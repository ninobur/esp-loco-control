"""Evidence-accounting and mathematical checks; no physical-truth claim."""
import json, hashlib
import numpy as np
import pandas as pd
from evaluate import ROOT, features, canonical, scores, metric
from prepare import sha

def main():
    models=json.loads((ROOT/'models/models.json').read_text())
    ann=pd.read_csv(ROOT/'data/annotations_tail_0.001.csv').fillna('')
    d=pd.read_csv(ROOT/'results/interval_decisions.csv')
    training=set(models['reference_ids'])
    evaluation=set(ann[ann.split=='eval'].id)
    assert not training.intersection(evaluation)
    assert set(ann[ann.id.isin(training)&ann.kind.eq('MM')].mm)==set(range(171))
    assert set(map(int,models['B']))==set(range(50,71))
    expected=ann[ann.split=='eval']
    assert set(d[d.model=='A'].id)==set(expected.id)
    assert len(d[d.model=='A'])==len(expected)
    focus=expected[expected.mm.between(50,70)]
    for name in ['B_oracle','B_union']:
        assert set(d[d.model==name].id)==set(focus.id)
        assert not d[d.model==name].mm.lt(50).any()
        assert not d[d.model==name].mm.gt(70).any()
    manifest=json.loads((ROOT/'sources/input_manifest.json').read_text())
    for i in manifest['inputs']:
        if 'portable_file' in i:
            assert sha(ROOT/i['portable_file'])==i['portable_sha256']
    for r in d.itertuples():
        if isinstance(r.reason,str) and r.reason:
            assert r.decision=='unresolved'
    c=pd.read_csv(ROOT/'results/continuous_summary.csv')
    all_scan=c[~c.focus]
    assert int(all_scan.MM_annotation_total.sum())==len(expected[expected.kind=='MM'])
    assert int(all_scan.MM_annotation_unresolved.sum())==len(expected[(expected.kind=='MM')&(expected.reason!='')])
    for name in ['A','B_union']:
        z=c[c.focus&c.model.eq(name)]
        assert int(z.MM_annotation_total.sum())==len(focus[focus.kind=='MM'])
    check=json.loads((ROOT/'results/checks.json').read_text())
    assert check['positive_gain_and_offset_max_error']<1e-9
    assert check['negative_gain_shape_max_error']<1e-9
    assert check['uniform_width_resampling_max_error']<1e-9
    assert check['every_source_bin_affects_profile']
    # The actual source interval must also be affine invariant, before canonicalization.
    y=np.r_[np.zeros(7),np.sin(np.linspace(0,7,80)),np.zeros(9)]
    x,_,_=features(y,[0],[len(y)]);z,_,_=features(y*4+1800,[0],[len(y)])
    assert np.max(abs(x-z))<1e-9
    assert np.max(abs(scores(x,models['A'])-scores(z,models['A'])))<1e-9
    summary=json.loads((ROOT/'results/interval_summary.json').read_text())
    for model in ['A','B_oracle','B_union']:
        assert metric(d[d.model==model])==summary[f'{model}/all_available']
    result={'status':'PASS','checks':['portable evidence hashes','reference/evaluation ID disjointness',
        '171 reference labels','21 B models and geographic scope','all unresolved intervals retained',
        'full and focus continuous denominators','gain/offset and uniform-width arithmetic invariance',
        'every source bin contributes','reported confusion counts recomputed'],
        'model_sha256':sha(ROOT/'models/models.json'),
        'physical_ground_truth_validated':False}
    (ROOT/'results/validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
