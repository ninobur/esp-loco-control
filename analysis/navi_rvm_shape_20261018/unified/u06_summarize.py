"""Summarize u04 results: main comparison (R0, F, U1 primary, U0) per dataset, the U1 parameter grid,
and the F hold-back backlog (released-but-not-yet-fed blocks) from the feeding model.
  u06_summarize.py <out_dir> <boots_dir> <decl_events.tsv>"""
import sys, json, glob, pathlib, numpy as np, pandas as pd
OUT = pathlib.Path(sys.argv[1]); rows = []
for f in sorted(glob.glob(str(OUT / 'results_*_grid.json'))): rows += json.load(open(f))
def flat(r):
    rt = r.get('route', {}); tr = r.get('truth', {}); ir = r.get('ir', {}); rd = r.get('report_delay_s', {}); dz = r.get('report_delay_in_decel_s', {})
    miss = list(rt.get('missed', [])) + list(rt.get('tail_missed', []))
    return dict(dataset=r['dataset'], mech=r['mechanism'], recog=r['recognitions'], withheld_s=round(r['withheld'] * 0.02, 1),
                claims=r['nav'].get('MM', 0), contra=r['nav'].get('CONTRADICTION', 0), claims_wrong=r.get('claims_wrong'),
                route_missed=len(miss), route_missed_list=' '.join(miss)[:120], extras=len(rt.get('extras', [])),
                truth_once=tr.get('once'), truth_missed=tr.get('missed'), truth_split=tr.get('split'), truth_unmatched=tr.get('unmatched_detections'),
                llr_min=r.get('llr_min'), llr_p5=r.get('llr_p5'), delay_med=rd.get('median'), delay_max=rd.get('max'), never_fed=rd.get('never'),
                decel_n=dz.get('n'), decel_delay_max=dz.get('max'), withheld_while_ir_moving_s=round(ir.get('withheld_while_moving', 0) * 0.02, 1),
                stat_off_kept_s=round(ir.get('stationary_off_baseline_kept', 0) * 0.02, 1), stat_off_s=round(ir.get('stationary_off_baseline_blocks', 0) * 0.02, 1))
df = pd.DataFrame([flat(r) for r in rows]); df.to_csv(OUT / 'summary_all.csv', index=False)
main = df[df.mech.isin(['R0', 'F', 'U1_k50_b24_m16', 'U0_k50_b24'])]
pd.set_option('display.width', 250); pd.set_option('display.max_columns', 40); pd.set_option('display.max_colwidth', 60)
print(main.drop(columns=['route_missed_list']).to_string(index=False))
print(); print(main[['dataset', 'mech', 'route_missed_list']].to_string(index=False))
# grid: totals over datasets (excluding the counterfactual-direction duplicate of 326C)
# route metrics are meaningful only where the declared direction matches the travel and one declaration covers the run:
# exclude the field-declared Oct 18 run (declared CCW, travelled CW; its :CW counterfactual is kept) and BOOT1 (several declarations; scored by annotation truth)
ROUTE_OK = ~df.dataset.isin(['326C357476B624FC', 'BOOT1'])
df.loc[~ROUTE_OK, ['route_missed', 'extras']] = np.nan
g = df[df.mech.str.startswith('U1') & (df.dataset != '326C357476B624FC')].copy()
agg = g.groupby('mech').agg(recog=('recog', 'sum'), route_missed=('route_missed', 'sum'), extras=('extras', 'sum'), contra=('contra', 'sum'),
                            claims=('claims', 'sum'), truth_missed=('truth_missed', 'sum'), truth_split=('truth_split', 'sum'), truth_unmatched=('truth_unmatched', 'sum'),
                            withheld_moving_s=('withheld_while_ir_moving_s', 'sum'), stat_off_kept_s=('stat_off_kept_s', 'sum'), llr_min=('llr_min', 'min')).reset_index()
ref = df[df.mech.isin(['R0', 'F']) & (df.dataset != '326C357476B624FC')].groupby('mech').agg(recog=('recog', 'sum'), route_missed=('route_missed', 'sum'), extras=('extras', 'sum'),
      contra=('contra', 'sum'), claims=('claims', 'sum'), truth_missed=('truth_missed', 'sum'), truth_split=('truth_split', 'sum'), truth_unmatched=('truth_unmatched', 'sum'),
      withheld_moving_s=('withheld_while_ir_moving_s', 'sum'), stat_off_kept_s=('stat_off_kept_s', 'sum'), llr_min=('llr_min', 'min')).reset_index()
tot = pd.concat([ref, agg]); tot.to_csv(OUT / 'grid_totals.csv', index=False); print(); print(tot.to_string(index=False))
