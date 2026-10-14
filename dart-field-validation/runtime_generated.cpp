
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
static constexpr uint8_t NAVI_BASE_CRUISE_PWM = 90;
static constexpr uint16_t AUTO_STEP_UP_MS = 150, AUTO_STEP_DOWN_MS = 31;
static constexpr uint16_t STOP_RESTART_STEP_UP_MS = 150;
static constexpr uint16_t STOP_FINAL_STEP_DOWN_MS = 300;
static constexpr uint16_t BRAKE_STEP_COAST_MS = 400, BRAKE_STEP_HARD_MS = 15;
static void writePwm(int value) {
  ledcWrite(MOTOR_PWM_PIN, value);
  static int lastRecorded = -1;
  if (value != lastRecorded) {
    if(dartAwaitDeparture && lastRecorded==0 && value>0) {
      dartDepartureUs=esp_timer_get_time(); dartAwaitDeparture=false;
      navi.noteOperatingContext(3,dartDepartureUs);
    }
    lastRecorded = value;
    recordAction(navi_sync::ActionKind::AppliedPwm);
  }
}
static uint16_t brakeStepMs() {
  return uint16_t(BRAKE_STEP_COAST_MS -
    (uint32_t(BRAKE_STEP_COAST_MS - BRAKE_STEP_HARD_MS) * brakeValue) / 255U);
}
static void requestPwm(int target, uint16_t up, uint16_t down,
                       bool manual = false) {
  target = constrain(target, 0, manual ? 255 : int(NAVI_MAX_OPERATING_PWM));
  const bool changed = target != rampTarget || up != rampUpMs || down != rampDownMs;
  rampTarget = target;
  commandedPwm = target;
  rampUpMs = up;
  rampDownMs = down;
  if (changed) recordAction(navi_sync::ActionKind::RequestedPwm);
}
static void serviceRamp() {
  digitalWrite(MOTOR_DIR_PIN, motorDirection ? HIGH : LOW);
  if (estopped || estopAsserted) {
    estopped = true;
    autoRunning = false;
    actualPwm = 0; commandedPwm = 0; rampTarget = 0;
    writePwm(0);
    return;
  }
  if (lowVoltage && rampTarget != 0) {
    rampTarget = 0; commandedPwm = 0;
    rampDownMs = AUTO_STEP_DOWN_MS;
  }
  if (actualPwm == rampTarget) return;
  const bool down = rampTarget < actualPwm;
  const uint16_t step = down ? (rampDownMs ? rampDownMs : brakeStepMs())
                             : (rampUpMs ? rampUpMs : 1);
  const uint32_t now = millis();
  if (now - lastRampStepMs < step) return;
  lastRampStepMs = now;
  actualPwm += down ? -1 : 1;
  writePwm(actualPwm);
}
static void withdraw(const char* why) {
  autoRunning = autoEnrolled = false;
  requestPwm(0, 0, AUTO_STEP_DOWN_MS);
  pub("state/auto", "0", true);
  pub("state/nav_ready", "0", true);
  warn(why, true);
}
static void serviceNavigationStop() {
  static bool reported=false;
  if(!navi.stopRequested()) { reported=false;return; }
  if(!reported || autoRunning || autoEnrolled) {
    withdraw(navi.stopReasonText());reported=true;
  }
}
static void serviceAutoCruise() {
  const uint64_t nowUs = esp_timer_get_time();
  const uint32_t nowMs = millis();
  static const int32_t targetUm = stopMarkerUm(45) + 150000;
  if (!autoRunning) {
    tileTiming.suspend(navi.selectedOpeningId());
    const StopView view = stopView(navi, targetUm, nowUs);
    const StopCommand command = stopOverlay.command(view.geography, view.available,
        navi.direction(), NAVI_BASE_CRUISE_PWM, actualPwm, rampTarget, nowMs, false);
    recordStop(view, command, nowUs, targetUm);
    navi.noteOperatingContext(0);
    return;  // Observe execution progress, never command the motor under Manual.
  }
  if(navi.stopRequested()) { serviceNavigationStop();return; }
  if(estopped || estopAsserted || lowVoltage) return;
  if (!navi.declared() || !navi.positionReliable()) {
    withdraw("Position relationship unreliable; AUTO withdrawn. Manual available.");
    return;
  }

  // Ordinary cruise is 90; retain the established location/direction-dependent
  // grade settings (David, 2026-10-09). They describe real track conditions.
  // Select one active overlay instruction before the single actuator request.
  const uint8_t cruise = cruisePwmAt(navi.mm(), navi.direction(), NAVI_BASE_CRUISE_PWM);
  const StopView view = stopView(navi, targetUm, nowUs);
  const auto priorExecution=stopOverlay.execution();
  const uint8_t next=navi_one::nextMarker(navi.mm(),navi.direction());
  lastTileTiming=tileTiming.command(view,navi.selectedOpeningId(),navi.selectedDecisionUs(),nowUs,
      targetUm,navi.travelHistory(navi.mm(),navi.direction()),cruise,
      cruisePwmAt(next,navi.direction(),NAVI_BASE_CRUISE_PWM),uint8_t(commandedPwm));
  // Once final braking/dwell/departure is underway, its existing executor
  // retains authority; losing IR must not restart or interrupt it.
  if(lastTileTiming.missingHistory && priorExecution==StopExecution::Geographic) {
    navi.requestStop(DartStop::NoTimingHistory);serviceNavigationStop();return;
  }
  const StopCommand command = stopOverlay.command(lastTileTiming.geography,lastTileTiming.available,
      navi.direction(),cruise,actualPwm,rampTarget,nowMs,true);
  const bool entry=lastTileTiming.entryNow && priorExecution==StopExecution::Geographic;
  const bool timed=lastTileTiming.mode==TileTimingMode::Ramp &&
      command.execution==StopExecution::Geographic && command.overlay;
  // Apply the authorized entry or time-interpolated PWM directly, still below
  // manual, E-stop and electrical protection. No PWM-derived distance exists.
  if((entry || timed) && !estopped && !estopAsserted && !lowVoltage) {
    const int value=constrain(int(entry?lastTileTiming.entryPwm:command.pwm),0,int(NAVI_MAX_OPERATING_PWM));
    const bool changed=entry || value!=actualPwm;
    rampTarget=value;commandedPwm=value;actualPwm=value;
    if(changed) { writePwm(value);recordAction(navi_sync::ActionKind::RequestedPwm); }
  }
  if(rampTarget!=command.pwm) {
    const uint16_t upMs=command.execution==StopExecution::Released?STOP_RESTART_STEP_UP_MS:AUTO_STEP_UP_MS;
    const uint16_t downMs=command.execution==StopExecution::Braking?STOP_FINAL_STEP_DOWN_MS:AUTO_STEP_DOWN_MS;
    requestPwm(command.pwm,upMs,downMs);
  }
  dartContext=command.releasedNow ? 3 : command.overlay ? 2 : 1;
  if(command.releasedNow) dartAwaitDeparture=true;
  navi.noteOperatingContext(dartContext);
  recordStop(view, command, nowUs, targetUm);
}

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
