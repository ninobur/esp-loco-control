#include "../NaviStopOverlay.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace navi_eyes;
using namespace navi_one;
namespace navi_sync { enum class ActionKind { RequestedPwm }; }
static NaviIntegratedCore navi;
static NaviStopOverlay stopOverlay;
static bool autoEnrolled = true, autoRunning = true, estopped = false;
static bool estopAsserted = false, lowVoltage = false;
static int actualPwm = 90, commandedPwm = 90, rampTarget = 90;
static uint16_t rampUpMs = 150, rampDownMs = 31;
static uint8_t brakeValue = 0, motorDirection = 1;
static uint32_t clockMs = 1000, lastRampStepMs = 1000, requests = 0;
static int physicalPwm = 90;
static constexpr int NAVI_MAX_OPERATING_PWM = 110;
static constexpr int MOTOR_DIR_PIN = 2, HIGH = 1, LOW = 0;
static std::string lastStopJson;
static int constrain(int n, int lo, int hi) { return std::min(hi, std::max(lo, n)); }
static uint32_t millis() { return clockMs; }
static uint64_t esp_timer_get_time() { return uint64_t(clockMs) * 1000; }
static void digitalWrite(int, int) {}
static void writePwm(int value) { physicalPwm = value; }
static void recordAction(navi_sync::ActionKind) { ++requests; }
static void pub(const char* topic, const char* payload, bool = false) {
  if (!std::strcmp(topic, "telem/stop")) lastStopJson = payload;
}
static void warn(const char*, bool) {}
// Extracted verbatim by test_stop_handoff.py; only hardware outputs are stubbed.
#include "stop_handoff_shell.h"

static uint32_t hallSerial = 0, irSequence = 0;
static uint64_t pulses = 100;
static int8_t direction = 1;
static const int32_t targetUm = stopMarkerUm(45) + 150000;
static void hall(int raw) {
  HallSample h{};
  h.sampleSerial = ++hallSerial; h.timestampUs = uint64_t(++clockMs) * 1000;
  h.raw = raw; h.pwm = actualPwm; h.direction = direction > 0 ? 1 : 2;
  navi.observeHall(h);
}
static void neutral() { hall(navi.activeReference()); hall(navi.activeReference()); }
static void ir() {
  ir_movement::WireSnapshot w{};
  w.bootId = 42; w.sequence = ++irSequence; w.capturedUs = esp_timer_get_time();
  w.completedPulses = w.observedRises = pulses; w.nominalUm = pulses * w.pitchUm;
  w.opticalReason = ir_movement::TRACKING;
  navi.observeIr(w, w.capturedUs, actualPwm);
}
static void start(uint8_t mm, int8_t dir, bool withIr = false) {
  direction = dir; hall(1000);
  if (withIr) ir();
  navi.declare(mm, dir, uint64_t(++clockMs) * 1000);
  neutral(); serviceAutoCruise();
}
static void field(uint8_t mm, bool withIr = false) {
  neutral(); clockMs += 700;
  if (withIr) { pulses += 31; ir(); }
  const int raw = navi.activeReference() + (polarityAt(mm) ? 70 : -70);
  hall(raw); hall(raw);
}
static StopView view() { return stopView(navi, targetUm, esp_timer_get_time()); }
static void drainEvents() { EwoEvent e; while (navi.takeEvent(e)) {} }

static void finishStop() {
  assert(rampTarget == 0 && rampDownMs == 300);
  assert(lastStopJson.find("\"execution\":\"BRAKING\"") != std::string::npos);
  lastRampStepMs = clockMs;
  const uint32_t before = requests;
  // No additional Hall or IR input is needed to complete final braking.
  for (int pwm = actualPwm - 1; pwm >= 0; --pwm) {
    clockMs += 299; serviceRamp(); serviceAutoCruise(); assert(actualPwm == pwm + 1);
    ++clockMs; serviceRamp(); serviceAutoCruise();
    assert(actualPwm == pwm && physicalPwm == pwm && rampTarget == 0);
  }
  assert(requests == before);
  assert(lastStopJson.find("\"execution\":\"DWELL\"") != std::string::npos);
  clockMs += 4999; serviceAutoCruise(); assert(rampTarget == 0 && actualPwm == 0);
  ++clockMs; serviceAutoCruise(); assert(rampTarget == 90 && actualPwm == 0);
  assert(rampUpMs == 150);
  assert(lastStopJson.find("DWELL_5S_PWM_ZERO") != std::string::npos);
  serviceRamp(); serviceAutoCruise(); assert(actualPwm == 1);
  for (int pwm = 2; pwm <= 90; ++pwm) {
    clockMs += 149; serviceRamp(); assert(actualPwm == pwm - 1);
    ++clockMs; serviceRamp(); serviceAutoCruise();
    assert(actualPwm == pwm && rampTarget == 90); // no repeat stop in this visit
  }
  std::puts("PASS: final ramp 300 ms/count, applied-zero dwell 5000 ms, restart 150 ms/count");
}

static void journey(const char* name, int8_t dir, bool stale, bool fine) {
  start(dir > 0 ? 33 : 58, dir, stale || fine);
  if (stale) clockMs += 1001;
  // Independent established geography: CW 034..045 and CCW 057..046.
  const int expected[] = {90,85,75,65,55,50,50,50,50,50,50,0};
  unsigned mismatches = 0;
  for (unsigned i = 0; i < 12; ++i) {
    const uint8_t mm = dir > 0 ? 34 + i : 57 - i;
    field(mm, fine);
    assert(navi.mm() == mm && navi.positionReliable());
    assert(navi.confirmedCount() == i + 1 && navi.dnaCheck().contradictions == 0);
    const auto& accepted = navi.dnaHistory().at(navi.dnaHistory().size() - 1);
    assert(accepted.identified && accepted.mm == mm);
    assert(navi.physicalPositionAnchored());
    const auto geographic = view();
    serviceAutoCruise();
    std::printf("%s MM%03u reliable=%d anchored=%d relationship=%d available=%d fine=%d target=%d\n",
                name, mm, navi.positionReliable(), navi.physicalPositionAnchored(),
                navi.relationshipReliable(), geographic.available, geographic.fine, rampTarget);
    if (!geographic.available || geographic.fine != fine || rampTarget != expected[i])
      ++mismatches;
    assert(autoEnrolled && autoRunning);
    if (i != 11) {
      // Check actual ordinary deceleration, not only requested commands.
      while (actualPwm > rampTarget) {
        const int prior = actualPwm; lastRampStepMs = clockMs;
        clockMs += 30; serviceRamp(); assert(actualPwm == prior);
        ++clockMs; serviceRamp(); serviceAutoCruise(); assert(actualPwm == prior - 1);
      }
    }
    drainEvents();
  }
  assert(navi.mm() == (dir > 0 ? 45 : 46));
  assert(mismatches == 0); // baseline reaches MM045 with target=90, then fails here
  if (!fine) {
    assert(lastStopJson.find("\"source\":\"NAVI_MM\"") != std::string::npos);
    assert(lastStopJson.find("\"position_um\":null") != std::string::npos);
  }
  finishStop();
}

static void manual() {
  autoRunning = autoEnrolled = false;
  start(44, 1); field(45); assert(navi.mm() == 45);
  for (int requested : {0,35,90,200,255}) {
    requestPwm(requested, MANUAL_STEP_UP_MS, 0, true);
    const auto before = requests;
    serviceAutoCruise();
    assert(rampTarget == requested && commandedPwm == requested && requests == before);
  }
  requestPwm(0, 0, AUTO_STEP_DOWN_MS); serviceAutoCruise(); assert(rampTarget == 0);
  std::puts("PASS: manual STOP and full-range throttle unaffected at accepted MM045");
}

static void rejected() {
  start(43, 1); field(44); serviceAutoCruise();
  const int acceptedCommand = rampTarget;
  // Neither sustained samples nor a re-entry inside 650 ms invent an MM045.
  for (unsigned i = 0; i < 10; ++i) hall(navi.activeReference() + (polarityAt(44) ? 70 : -70));
  neutral();
  const int raw = navi.activeReference() + (polarityAt(45) ? 70 : -70);
  hall(raw); hall(raw); serviceAutoCruise();
  assert(navi.mm() == 44 && navi.confirmedCount() == 1 && rampTarget == acceptedCommand);
  neutral(); clockMs += 700;
  hall(navi.activeReference() - (polarityAt(45) ? 70 : -70));
  hall(navi.activeReference() - (polarityAt(45) ? 70 : -70));
  assert(navi.mm() == 44 && !navi.positionReliable() && navi.geographicJudgmentPending());
  assert(!view().available); serviceAutoCruise();
  assert(autoRunning && autoEnrolled && rampTarget == acceptedCommand);
  std::puts("PASS: unqualified samples and pending DNA cannot become STOP geography");
}

static void fineToMm() {
  start(43, 1, true); field(44, true);
  pulses += 8; ++clockMs; ir();
  assert(view().available && view().fine && view().positionUm > stopMarkerUm(44));
  clockMs += 1001;
  // Lost fine measurement retains NAVI's accepted MM, without claiming an offset.
  assert(navi.mm() == 44 && view().available && !view().fine);
  field(45); assert(navi.mm() == 45 && view().available && !view().fine);
  serviceAutoCruise(); assert(rampTarget == 0);
  std::puts("PASS: fine IR preferred; stale IR still exposes accepted MM and next Hall advances it");
}

int main(int argc, char** argv) {
  std::setbuf(stdout, nullptr);
  assert(argc == 2);
  if (!std::strcmp(argv[1], "no_ir_cw")) journey(argv[1], 1, false, false);
  else if (!std::strcmp(argv[1], "no_ir_ccw")) journey(argv[1], -1, false, false);
  else if (!std::strcmp(argv[1], "stale_ir_cw")) journey(argv[1], 1, true, false);
  else if (!std::strcmp(argv[1], "fine_ir_cw")) journey(argv[1], 1, false, true);
  else if (!std::strcmp(argv[1], "manual")) manual();
  else if (!std::strcmp(argv[1], "rejected")) rejected();
  else if (!std::strcmp(argv[1], "fine_to_mm")) fineToMm();
  else assert(false);
}
