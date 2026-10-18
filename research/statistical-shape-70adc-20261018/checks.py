"""Check the one changed condition, full-window preservation and paired accounting."""
import json
import numpy as np
import pandas as pd
from repeat import ROOT,ORIGINAL,MIN_EXCURSION_ADC,original_hashes,sha,raw_profiles,local_baseline,excursion,select,base

def main():
    assert MIN_EXCURSION_ADC==70.0
    e=excursion(np.array([0.,-70.,0.]),np.array([70.,0.,69.999999]),np.zeros(3))
    assert (e>=MIN_EXCURSION_ADC).tolist()==[True,True,False]
    # The disqualified high-scoring proposal must not block a qualifying proposal.
    proposals=np.array([[0,20,2,1,0,0,69.9,69.9],[0,20,1,1,0,0,70,70]],dtype=float)
    assert select(proposals,20).tolist()==[0]
    assert select(proposals,20,70).tolist()==[1]
    # A narrow principal excursion is measured before phase averaging dilutes it.
    signal=np.r_[np.zeros(100),[30,100,25,-15],np.zeros(100)]
    before=signal.copy();v=raw_profiles(signal,[0],[len(signal)]);b=local_baseline(v)
    assert excursion(signal.min(),signal.max(),b)[0]==100
    assert max(abs(v[0]-b[0]))<70
    z,_,_=base.canonical(v);x,_,_=base.features(signal,[0],[len(signal)])
    assert np.array_equal(x,z)
    assert np.array_equal(signal,before)
    assert sha(ROOT/'results/original_events_replay.csv')==sha(ORIGINAL/'results/continuous_events.csv')
    assert sha(ROOT/'results/original_summary_replay.csv')==sha(ORIGINAL/'results/continuous_summary.csv')
    expected=json.loads((ROOT/'results/original_hashes.json').read_text())
    assert original_hashes()==expected
    audit=pd.read_csv(ROOT/'results/candidate_audit.csv.gz')
    assert audit.score.gt(0).all()
    assert (audit.passes_70==(audit.excursion_adc>=70)).all()
    assert not (audit.selected_70adc&~audit.passes_70).any()
    events=pd.read_csv(ROOT/'results/continuous_events.csv')
    for condition,column in [('original','selected_original'),('70adc','selected_70adc')]:
        selected=audit[audit[column]]
        found=events[events.condition.eq(condition)&events.status.ne('missed')]
        key=['span','model','lo','hi']
        a=selected.sort_values(key).reset_index(drop=True)
        f=found.sort_values(key).reset_index(drop=True)
        pd.testing.assert_frame_equal(a[key],f[key],check_dtype=False,check_exact=True)
        assert np.array_equal(a.score.to_numpy(),f.score.to_numpy())
    summary=pd.read_csv(ROOT/'results/continuous_summary.csv')
    for _,z in summary.groupby(['span','model']):
        assert z.MM_annotation_total.nunique()==1
        assert z.MM_annotation_unresolved.nunique()==1
        assert z.candidate_windows.nunique()==1
        assert z.unresolved_windows.nunique()==1
        assert (z.matched+z.missed+z.MM_annotation_unresolved==z.MM_annotation_total).all()
    result={'status':'PASS','inclusive_70_and_both_polarities':True,'filter_before_overlap_suppression':True,
        'principal_measured_before_phase_averaging':True,'complete_candidate_features_unchanged':True,
        'original_event_and_summary_CSVs_byte_identical':True,'original_artifacts_unchanged':len(expected),
        'model_sha256':sha(ORIGINAL/'models/models.json'),'candidate_audit_rows':len(audit),
        'fixed_partitions_and_unresolved_denominators':True}
    (ROOT/'results/checks.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
