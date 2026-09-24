"""Compile the real PWM request function with hardware-free stubs."""
from pathlib import Path
import re
import subprocess
import tempfile

sketch = Path(__file__).resolve().parents[1] / 'firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/NAVI_COHERENCE_0_6_IR_HEALTH.ino'
source = sketch.read_text()
fn = source[source.index('static void requestPwm('):source.index('static void serviceRamp()')]
manual = source.split('} else if (!strcmp(leaf,"throttle")) {', 1)[1].split('} else if', 1)[0]
assert 'admitThrottle(o)' in manual
assert 'requestPwm(n, MANUAL_STEP_UP_MS, 0, StopCause::Controlled, true);' in manual
assert 'NAVI_MAX_OPERATING_PWM' not in manual
assert source.count('StopCause::Controlled, true)') == 1
stub = '''
#include <cassert>
#include <cstdint>
#include <algorithm>
#define NAVI_MAX_OPERATING_PWM 120
enum class StopCause {Controlled, Safety};
struct Log {template<class... T> void printf(const char*,T...) {}} Serial;
int constrain(int n,int lo,int hi){return std::max(lo,std::min(n,hi));}
int rampTarget=0,commandedPwm=0;
uint16_t stepUpMs=0,stepDownMs=0;
'''
main = '''
int main(){
  for(int n=-10;n<=300;++n){
    requestPwm(n,7,0,StopCause::Controlled,true);
    assert(rampTarget==constrain(n,0,255) && commandedPwm==rampTarget);
    assert(stepUpMs==7 && stepDownMs==0);
    requestPwm(n,8,9);
    assert(rampTarget==constrain(n,0,120));
    requestPwm(0,0,1,StopCause::Safety);assert(rampTarget==0);
  }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp)/'test.cpp'; exe = Path(tmp)/'test'
    cpp.write_text(stub+fn+main)
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-Wno-unused-parameter','-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS real requestPwm: 311 manual/AUTO/safety cases; manual handler bypass verified')
