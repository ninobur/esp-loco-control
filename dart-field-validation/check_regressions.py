from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];p=r/'firmware/programs/NAVI_EWO_0_5/tests';out=r/'dart-field-validation'
results=[]
for name in ['test_integrated_core','test_review_regressions','test_ir_authority','test_first_target_after_declare','test_pwm_zero_localization','test_dart','test_dart_invariants','test_operational_adapters','test_measured_station_stop']:
 binary='/tmp/navi-05-'+name
 c=subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/(name+'.cpp')),'-o',binary],capture_output=True,text=True)
 if c.returncode==0:
  v=subprocess.run([binary],capture_output=True,text=True);result={'test':name,'compile_exit':0,'run_exit':v.returncode,'output':v.stdout+v.stderr}
 else:result={'test':name,'compile_exit':c.returncode,'output':c.stdout+c.stderr}
 (out/(name+'.log')).write_text(result['output']);results.append(result)
 print(name, 'PASS' if result.get('run_exit')==0 else 'FAIL',result['output'].splitlines()[:1],flush=True)
(out/'regression-results.json').write_text(json.dumps(results,indent=2)+'\n')
