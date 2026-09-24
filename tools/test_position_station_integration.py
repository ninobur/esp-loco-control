"""Compile the actual GO, station service and PWM ramp with host hardware stubs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
variant = root / "firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH"
source = (variant / "NAVI_COHERENCE_0_6_IR_HEALTH.ino").read_text()


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


go = source.split('} else if (!strcmp(leaf,"go") ||', 1)[1]
go = go[go.index("{") + 1:go.index('} else if (!strcmp(leaf,"stop")')]
loop = source[source.index("void loop(){"):]
assert loop.index("handleCommand(c)") < loop.index("stationService(millis())") < loop.index("serviceRamp();")
stub = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#define NAVI_APPROACH_MARKER_MS {1390,1539,1725,1960,2271}
#include "RecoveryControl.h"
using namespace navi_one;
constexpr int NAVI_MAX_OPERATING_PWM=120, AUTO_CRUISE_PWM=90;
constexpr int AUTO_STEP_UP_MS=80, AUTO_STEP_DOWN_MS=31;
constexpr int MOTOR_DIR_PIN=2, HIGH=1, LOW=0, T_STATION=1;
constexpr bool NGR_ENABLE_EXPERIMENTAL_AUTO=true;
bool hallReady=true, autoRunning=false, estopped=false, estopAsserted=false;
bool lowVoltage=false, motorDirection=false;
int actualPwm=0,commandedPwm=0,rampTarget=0,writtenPwm=0;
uint16_t stepUpMs=0,stepDownMs=0;
unsigned long lastStepMs=0,clockMs=0;
unsigned withdraws=0;
std::vector<std::string> events;
Navigator navigator;
StationMachine stationMachine;
enum class StopCause {Controlled,Safety};
struct Log {template<class... T> void printf(const char*,T...) {}} Serial;
int constrain(int n,int lo,int hi){return std::max(lo,std::min(n,hi));}
unsigned long millis(){return clockMs;}
void digitalWrite(int,int){}
void writePwm(int pwm){writtenPwm=pwm;}
uint16_t brakeStepMs(){return 31;}
void pub(int,const char* json){events.emplace_back(json);}
void warn(const char*){}
void warnClear(){}
using Refusal=const char*;
Refusal refusal=nullptr;
Refusal admitGo(int){return refusal;}
void refuse(Refusal){}
void withdraw(const char*);
'''
main = r'''
void withdraw(const char*) {
  ++withdraws; autoRunning=false; requestPwm(0,0,AUTO_STEP_DOWN_MS);
}
void prepare(int mm,int dir) {
  stationMachine=StationMachine{};navigator.declare(mm,dir,clockMs);
  actualPwm=commandedPwm=rampTarget=writtenPwm=0;autoRunning=false;
  events.clear();refusal=nullptr;
}
int main() {
  // In-zone resume, direct cold start and repeated GO all choose 60, never 90.
  prepare(20,-1);clockMs=1000;stationService(clockMs);goCommand();
  assert(autoRunning && rampTarget==0);
  stationService(clockMs);assert(commandedPwm==60 && rampTarget==60);
  serviceRamp();assert(actualPwm==1);
  const auto count=events.size();
  for(int i=0;i<100;++i){clockMs+=2;stationService(clockMs);serviceRamp();}
  assert(events.size()==count && actualPwm==2); // repeated orders do not reset ramp clock
  autoRunning=false;requestPwm(0,0,AUTO_STEP_DOWN_MS);stationService(clockMs);
  clockMs+=300000;actualPwm=0;stationService(clockMs);
  assert(rampTarget==0); // no AUTO target while paused
  goCommand();stationService(clockMs);assert(rampTarget==60 && autoRunning);
  goCommand();stationService(++clockMs);assert(rampTarget==60);
  // Exact field regression: pause before crossing MM25 and resume at MM24.
  prepare(26,-1);stationService(clockMs);navigator.declare(24,-1,clockMs);
  goCommand();stationService(clockMs);assert(rampTarget==78);
  // At a stop trigger, GO must not cause a motor step before the zero order.
  prepare(15,-1);goCommand();stationService(++clockMs);serviceRamp();
  assert(actualPwm==0 && rampTarget==0);
  stationService(++clockMs);assert(stationMachine.phase()==StPhase::Dwell);
  clockMs+=4999;stationService(clockMs);assert(rampTarget==0);
  ++clockMs;stationService(clockMs);assert(rampTarget==90);
  // Existing section routing and exceptional Grillers CW departure are retained.
  prepare(70,1);goCommand();stationService(++clockMs);assert(rampTarget==110);
  prepare(30,-1);goCommand();stationService(++clockMs);assert(rampTarget==105);
  prepare(62,1);goCommand();stationService(++clockMs);stationService(++clockMs);
  clockMs+=5000;stationService(clockMs);assert(rampTarget==110);
  // Unknown position withdraws AUTO without launching; existing failure handling stays.
  prepare(0,0);goCommand();stationService(++clockMs);
  assert(!autoRunning && rampTarget==0 && withdraws==1);
  // GO admission remains effective.
  prepare(20,-1);refusal="blocked";goCommand();assert(!autoRunning && rampTarget==0);
  refusal=nullptr;hallReady=false;goCommand();assert(!autoRunning);hallReady=true;
  // Real ramp still gives E-stop and low voltage priority over a station request.
  goCommand();actualPwm=30;stationService(++clockMs);estopAsserted=true;serviceRamp();
  assert(actualPwm==0 && writtenPwm==0 && rampTarget==0);estopAsserted=false;
  actualPwm=30;lowVoltage=true;clockMs+=1000;stationService(clockMs);serviceRamp();
  assert(rampTarget==0 && actualPwm==29);lowVoltage=false;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / "test.cpp"
    exe = Path(tmp) / "test"
    cpp.write_text(stub + function("static void requestPwm(")
                   + function("static void serviceRamp()")
                   + function("static void stationService(")
                   + "\nvoid goCommand(){int o=0;" + go + "}\n" + main)
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-parameter", "-fsanitize=address,undefined",
                    "-I", str(variant), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS actual GO/stationService/requestPwm/serviceRamp: position, pause, dwell, routing, admission and protection")
