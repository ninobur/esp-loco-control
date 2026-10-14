from pathlib import Path
import re,subprocess,json
root=Path(__file__).resolve().parents[1]; p=root/'firmware/programs/NAVI_EWO_0_5'
s=(p/'NAVI_EWO_0_5.ino').read_text()
def fn(name):
 m=re.search(r'^static [^\n]*\b'+name+r'\(',s,re.M);start=s.index('{',m.start());i=start+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[m.start():i]
head=r'''
#define main old_dart_main
#include "../firmware/programs/NAVI_EWO_0_5/tests/test_dart.cpp"
#undef main
#include "../firmware/programs/NAVI_EWO_0_5/NaviTileTiming.h"
using namespace navi_one;
namespace navi_sync { enum class ActionKind { AppliedPwm, RequestedPwm }; }
static NaviIntegratedCore navi;static NaviStopOverlay stopOverlay;
static NaviTileTiming tileTiming;static TileTimingCommand lastTileTiming;
static uint64_t fakeUs=0,dartDepartureUs=0;static bool dartAwaitDeparture=false;
static uint8_t dartContext=0;
static unsigned millis(){return unsigned(fakeUs/1000);}
static uint64_t esp_timer_get_time(){return fakeUs;}
static int actualPwm=0,commandedPwm=0,rampTarget=0,written=-1;
static uint16_t rampUpMs=0,rampDownMs=0;static uint32_t lastRampStepMs=0;
static uint8_t brakeValue=0,motorDirection=1;
static bool estopped=false,estopAsserted=false,lowVoltage=false,autoRunning=true,autoEnrolled=true;
static constexpr int MOTOR_PWM_PIN=4,MOTOR_DIR_PIN=2,HIGH=1,LOW=0,NAVI_MAX_OPERATING_PWM=110;
static int constrain(int n,int a,int b){return std::min(b,std::max(a,n));}
static void digitalWrite(int,int){}
static void ledcWrite(int,int value){written=value;}
static void recordAction(navi_sync::ActionKind){}
static bool pub(const char*,const char*,bool=false){return false;}
static void warn(const char*,bool){}
static void recordStop(const StopView&,const StopCommand&,uint64_t,int32_t){}
'''
constants='\n'.join(m.group(0) for name in ['NAVI_BASE_CRUISE_PWM','AUTO_STEP_UP_MS','STOP_RESTART_STEP_UP_MS','STOP_FINAL_STEP_DOWN_MS','BRAKE_STEP_COAST_MS'] for m in [re.search(r'^static constexpr [^\n]*\b'+name+r'\b[^\n]*;',s,re.M)])
body=r'''
static void seed(uint8_t mm,bool fine=true) {
 Fixture f;f.start();navi=f.n;fakeUs=f.t;navi.mm_=mm;navi.chooseTarget();
 navi.expectedCumulativeUm_=uint64_t(spanMm(mm,1))*1000;
 navi.metric_=fine?DartMetric::Distance:DartMetric::Time;
 navi.selectedDecisionUs_=fakeUs;navi.priorSelection_.id=1;
 navi.travel_.begin(mm,1);uint8_t at=mm;
 for(int i=0;i<171;++i){DartTravel t;t.valid=true;t.intervalUs=20000000;
  if(at==45)t.departureOffsetUs=15500000;
  auto next=nextMarker(at,1);navi.travel_.record(at,next,1,t);at=next;}
 stopOverlay.reset();tileTiming.reset();actualPwm=commandedPwm=rampTarget=80;
 estopped=estopAsserted=lowVoltage=false;autoRunning=autoEnrolled=true;
 dartAwaitDeparture=false;lastRampStepMs=millis();serviceNavigationStop();
}
int main(){
 seed(35);serviceAutoCruise();CHECK(rampTarget==85);
 fakeUs+=2000000;serviceAutoCruise();CHECK(rampTarget==85);CHECK(lastTileTiming.mode==TileTimingMode::HoldUntilOpening);
 navi.mm_=36;navi.chooseTarget();navi.priorSelection_.id=2;navi.metric_=DartMetric::Time;navi.selectedDecisionUs_=fakeUs;
 serviceAutoCruise();CHECK(actualPwm==75 && commandedPwm==75);CHECK(written==75);
 fakeUs+=10000000;serviceAutoCruise();CHECK(actualPwm==70 && commandedPwm==70);
 fakeUs+=15000000;serviceAutoCruise();CHECK(actualPwm==65);CHECK(navi.mm()==36);
 estopAsserted=true;serviceAutoCruise();serviceRamp();CHECK(actualPwm==0 && written==0);
 seed(45,false);serviceAutoCruise();CHECK(actualPwm==35);CHECK(rampTarget==0);CHECK(rampDownMs==300);
 for(int i=0;i<35;++i){fakeUs+=300000;serviceRamp();serviceAutoCruise();CHECK(actualPwm==34-i);}
 CHECK(stopOverlay.execution()==StopExecution::Dwell);const auto zero=fakeUs;
 fakeUs=zero+4999000;serviceAutoCruise();CHECK(rampTarget==0);
 fakeUs=zero+5000000;serviceAutoCruise();CHECK(stopOverlay.execution()==StopExecution::Released);
 CHECK(rampTarget==90);CHECK(rampUpMs==150);serviceRamp();CHECK(actualPwm==1);CHECK(dartDepartureUs==fakeUs);
 // Operator STOP/manual authority suppresses automatic writes.
 autoRunning=false;const auto pwm=actualPwm;serviceAutoCruise();CHECK(actualPwm==pwm);
 seed(36,false);navi.requestStop(DartStop::NoHallBySuccessor);serviceNavigationStop();
 CHECK(!autoRunning && !autoEnrolled && rampTarget==0);CHECK(rampDownMs==31);
 seed(36,false);lowVoltage=true;serviceAutoCruise();serviceRamp();CHECK(rampTarget==0);
 std::printf("PASS %u actual .ino motor/overlay adapter assertions\n",checks);
}
'''
out=root/'dart-field-validation/runtime_generated.cpp'
out.write_text(head+constants+'\n'+'\n'.join(fn(n) for n in ['writePwm','brakeStepMs','requestPwm','serviceRamp','withdraw','serviceNavigationStop','serviceAutoCruise'])+'\n'+body)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out),'-o','/tmp/navi-05-runtime'],check=True)
r=subprocess.run(['/tmp/navi-05-runtime'],capture_output=True,text=True)
(root/'dart-field-validation/runtime-test.log').write_text(r.stdout+r.stderr)
print(r.stdout+r.stderr);raise SystemExit(r.returncode)
