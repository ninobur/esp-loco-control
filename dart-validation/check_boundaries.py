from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];b=r/'firmware/programs/NAVI_EWO_0_1_MM045_STOP';e=r/'firmware/programs/NAVI_EWO_0_4'
a=(b/'NAVI_EWO_0_1_MM045_STOP.ino').read_text();z=(e/'NAVI_EWO_0_4.ino').read_text()
def function(text,name):
 m=re.search(r'^static [^\n]*\b'+name+r'\(',text,re.M);assert m,name
 start=text.index('{',m.start());level=1;i=start+1
 while level:
  level+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[m.start():i]
rows=[]
for name in ['hallTask','onIr','serviceIrIngress','handleCommand','serviceRamp','serviceBattery','requestPwm','brakeStepMs']:
 aa=function(a,name);zz=function(z,name);assert aa==zz,name
 rows.append({'function':name,'unchanged':True,'sha256':hashlib.sha256(aa.encode()).hexdigest()})
for name in ['NaviMapAdapter.h','NaviEstop.h','NaviCompatibility.h','NaviPulseObservation.h','NaviPulseTelemetry.h','NaviSyncRecorder.h']:
 assert (b/name).read_bytes()==(e/name).read_bytes(),name
 rows.append({'file':name,'unchanged':True})
x=(e/'NaviIntegratedCore.h').read_text()
assert 'evaluateMissing(' not in x and 'tolerance' not in x and '15 / 100' not in x
# Every tracked baseline file, map, calibration and other firmware remains intact.
subprocess.run(['git','diff','--exit-code','85a2cf2','--','firmware/programs/NAVI_EWO_0_1_MM045_STOP','firmware/programs/NAVI_COHERENCE','firmware/common','firmware/config'],cwd=r,check=True)
(r/'dart-validation/source-boundaries.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS unchanged acquisition, Type-5/Type-6 ingress, commands, ramp/protection and recording components; no distance gate/missed advance in experimental core.')
