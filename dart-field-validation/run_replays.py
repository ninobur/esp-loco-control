from pathlib import Path
import subprocess,json
root=Path(__file__).resolve().parents[1];p=root/'dart-field-validation'; source=root/'dart-validation'
for arm in ['baseline','experiment']:
 args=['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror']+(['-DBASELINE'] if arm=='baseline' else [])
 subprocess.run(args+[str(p/'production_replay.cpp'),'-o','/tmp/navi-05-'+arm+'-replay'],check=True)
summary=[];traffic=[]
cases=['sep28_254_257','sep28_425_426','sep28_425_427_continued','oct09_early_synthetic','oct13_mm017_1028000_synthetic','oct13_mm017_1179000_synthetic']
for case in cases:
 for arm in ['baseline','experiment']:
  with (source/(case+'.txt')).open() as inp:
   args=['/tmp/navi-05-'+arm+'-replay']+([str(p/f'{case}-diagnostics.jsonl')] if arm=='experiment' else [])
   r=subprocess.run(args,stdin=inp,capture_output=True,text=True,check=True)
  (p/f'{case}-{arm}.csv').write_text(r.stdout);(p/f'{case}-{arm}-summary.txt').write_text(r.stderr)
  summary.append(dict(case=case,arm=arm,summary=r.stderr.strip(),events=r.stdout.strip().splitlines()))
 f=p/f'{case}-diagnostics.jsonl';lines=f.read_text().splitlines();data=[json.loads(x) for x in lines]
 traffic.append(dict(case=case,records=len(lines),payload_bytes=sum(len(x.encode()) for x in lines),max_payload_bytes=max(map(len,lines),default=0),selections=[{'mm':x['target'],'id':x['id'],'open_us':x['open_us'],'decision_us':x['decision_us'],'ir_um':x['ir_um'],'anchor_um':x['anchor_um']} for x in data if x['reason'] in (8,9)]))
 for x in data:
  if x['reason'] in (8,9) and not x['inferred']:assert x['ir_um']==x['anchor_um']
(p/'differential-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
(p/'diagnostic-traffic.json').write_text(json.dumps(traffic,indent=2)+'\n')
print('PASS six paired production replays; JSON decoded; selected measured coordinates equal saved anchors.')
