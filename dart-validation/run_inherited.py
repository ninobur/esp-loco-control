from pathlib import Path
import subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'dart-validation';rows=[]
tests=['test_map_adapter','test_integrated_core','test_recorder','test_review_regressions','test_ir_authority','test_stop_overlay','test_first_target_after_declare','test_pwm_zero_localization','test_ir_configuration','test_pulse_observation']
for arm in ['NAVI_EWO_0_1_MM045_STOP','NAVI_EWO_0_4']:
 for name in tests:
  src=root/'firmware/programs'/arm/'tests'/f'{name}.cpp';exe=Path('/tmp')/f'dart-{arm}-{name}'
  c=subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],capture_output=True,text=True)
  r=subprocess.run([str(exe)],capture_output=True,text=True) if c.returncode==0 else c
  (out/f'{arm}-{name}.log').write_text(c.stdout+c.stderr+(r.stdout+r.stderr if r is not c else ''))
  rows.append(dict(arm=arm,test=name,compiled=c.returncode==0,exit=r.returncode))
(out/'inherited-results.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(rows,indent=2))
