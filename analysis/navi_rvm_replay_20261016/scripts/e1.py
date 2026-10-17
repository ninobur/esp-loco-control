import sys, pathlib, json, numpy as np, pandas as pd
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent)); import aligner, score
B='15549e9385faa4d5'; res={}
for cw in ['CW_L1','CW_L2']:
    o=aligner.run(B,cw,'CCW_L1',declarations=sys.argv[1],label=f'e1_{sys.argv[1]}_{cw}')
    df=score.score(o,B,cw,'CCW_L1'); df.to_csv(score.D/f'score_e1_{sys.argv[1]}_{cw}.csv',index=False)
    m=score.meta
    ins_cw=(m[cw]['start_s'],m[cw]['end_s']); ins_ccw=(m['CCW_L1']['start_s'],m['CCW_L1']['end_s'])
    oos=df[~(((df.t>=ins_cw[0])&(df.t<ins_cw[1]))|((df.t>=ins_ccw[0])&(df.t<ins_ccw[1])))]
    res[cw]={'out_of_sample_CW':score.summary(oos[oos.dir=='CW']),'out_of_sample_CCW':score.summary(oos[oos.dir=='CCW']),
             'in_sample':score.summary(df[~df.index.isin(oos.index)])}
print(json.dumps(res,indent=1)); json.dump(res,open(score.D/f'e1_{sys.argv[1]}.json','w'),indent=1)
