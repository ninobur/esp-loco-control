// ============================================================================
// harness_navi2.cpp — host regression suite for NAVI 2.0.
//
//   Starting from a known navMm and direction, reliably find and identify
//   the next magnet, then advance navMm by exactly one. Nothing else
//   belongs in the first build.
//
// Compiles the REAL sketch (arduino-cli --preprocess output; one code path,
// no test copy) against tests/stubs/, then drives synthetic MarkerEvents
// through the genuine drainMarkers() -> naviIdentify() -> navOnMarker() path
// with a simulated clock.
//
// T2 is the position-contract regression carried over from 0.3B/0045: R3
// identifies, the inherited conservation gate refuses the SAME passage,
// and repeated refusals must produce ZERO unsupported drift — navMm is
// written in exactly one place (acceptEvent()), never by identification
// itself. T9 is 0.4's own structural guarantee: no event, of any kind this
// suite can produce, may ever advance navMm by more than one — there is no
// offset in the contract to misuse. It is the direct answer to the field
// failure that ended 0.3B: "if the pattern disagrees, it says there must
// have been a silent magnet."
//
// Build and run:  ./run_tests.sh   (from tests/)
// ============================================================================
unsigned long hostNowMs = 100000;
int hostAnalogValue = 1800;
#include "Arduino.h"
HostSerial Serial;
#include "WiFi.h"
HostWiFi WiFi;
#include "Wire.h"
HostWire Wire;
#include "PubSubClient.h"
std::vector<HostPub> hostPublished;

// The real sketch, prototypes inserted by arduino-cli --preprocess.
#include "gen/NAVI_pp.cpp"

// ---------------------------------------------------------------------------
// test infra
// ---------------------------------------------------------------------------
static int failures = 0, checks = 0;
static void ck(bool cond, const char* what){
  checks++;
  if(!cond){ failures++; printf("  FAIL  %s\n", what); }
}
static void ckEq(long got, long want, const char* what){
  checks++;
  if(got != want){ failures++; printf("  FAIL  %s (got %ld, want %ld)\n", what, got, want); }
}

// Drain the sketch's publish queues into a flat capture list.
struct Cap { std::string topic, payload; };
static std::vector<Cap> caps;
static void drainPubs(){
  PubMsg pm;
  while(pubQueue && xQueueReceive(pubQueue,&pm,0)==pdTRUE)
    caps.push_back({pm.topic?pm.topic:"", pm.payload});
  while(markerPubQueue && xQueueReceive(markerPubQueue,&pm,0)==pdTRUE)
    caps.push_back({pm.topic?pm.topic:"", pm.payload});
}
static const Cap* lastOn(const char* topicSub){
  for(int i=(int)caps.size()-1;i>=0;i--)
    if(caps[i].topic.find(topicSub)!=std::string::npos) return &caps[i];
  return nullptr;
}
static const Cap* lastExact(const char* topic){
  for(int i=(int)caps.size()-1;i>=0;i--)
    if(caps[i].topic==topic) return &caps[i];
  return nullptr;
}
static bool payloadHas(const Cap* c, const char* sub){
  return c && c->payload.find(sub)!=std::string::npos;
}


// ---------------------------------------------------------------------------
// event helpers — simulated clock, real queue, real drain
// ---------------------------------------------------------------------------
// Advance the simulated clock and let the moratorium see it. The motion clock
// accrues (now - lastTick) only while actualPwm > NAVI_MOVE_PWM_FLOOR, so
// this is how the harness spends either MOVING time or STOPPED time.
static void spend(unsigned long ms, uint8_t pwm){
  actualPwm = pwm; commandedPwm = pwm;
  hostNowMs += ms;
  naviNoteMotion();
}

static void inject(uint8_t pol, int peak, uint16_t durMs, unsigned long dtMs,
                   uint8_t pwm){
  spend(dtMs, pwm);
  MarkerEvent e{};
  e.polarity=pol; e.peak=peak; e.durationMs=durMs; e.baselineDrift=0;
  e.detectedAtMs=hostNowMs;
  e.pwmActualAtDetect=pwm; e.pwmCommandedAtDetect=pwm;
  ck(xQueueSend(eventQueue,&e,0)==pdTRUE, "event queued");
  drainMarkers();
  drainPubs();
}
// an event carrying the polarity the map says the NEXT marker has
static void injectCorrect(unsigned long dtMs, uint8_t pwm=90){
  inject(dnaAt(nextMm(navMm,navDir)), 190, 120, dtMs, pwm);
}
// an event carrying the WRONG polarity for the next marker
static void injectWrong(unsigned long dtMs, uint8_t pwm=90){
  inject(dnaAt(nextMm(navMm,navDir))?0:1, 190, 120, dtMs, pwm);
}
static void reset(uint8_t mm, int8_t dir){
  navState=NAV_UNSET; sessionDir=dir; navDir=dir;
  autoRunning=false; autoEnrolled=false;
  naviMotionMs=0; naviMotionAtAnchor=0; naviMotionTickMs=hostNowMs;
  naviIdentified=naviNotIdentified=naviBypassed=0;
  caps.clear();
  navDeclare(mm);
  drainPubs();
}

int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  printf("NAVI 2.0 contract suite\n");
  buildTopics();
  eventQueue=xQueueCreate(256,sizeof(MarkerEvent));
  pubQueue=xQueueCreate(32,sizeof(PubMsg));
  markerPubQueue=xQueueCreate(128,sizeof(PubMsg));
  cmdQueue=xQueueCreate(16,sizeof(CmdMsg));
  waveQueue=xQueueCreate(4,sizeof(NaviWave));

  // -----------------------------------------------------------------------
  printf("\nT1  UNSET: nothing advances, whatever arrives\n");
  navState=NAV_UNSET; navDir=MAP_CW; navMm=40;
  naviMotionTickMs=hostNowMs;
  for(int i=0;i<20;i++) inject(i&1, 200, 120, 900, 90);
  ckEq(navMm, 40, "navMm frozen while UNSET");
  ckEq((long)navState, (long)NAV_UNSET, "state still UNSET");

  // -----------------------------------------------------------------------
  printf("\nT2  declaration seeds position; first magnet needs polarity only\n");
  reset(40, MAP_CW);
  ckEq(navMm, 40, "declared at 40");
  ckEq((long)navState, (long)NAV_NORMAL, "NORMAL after declaration");
  // Only 50 ms of motion since the declaration -- far under the debounce.
  // The debounce must ABSTAIN (no anchor), so polarity alone decides.
  injectCorrect(50, 90);
  ckEq(navMm, 41, "first magnet advanced on polarity alone");

  // -----------------------------------------------------------------------
  printf("\nT3  a clean lap advances exactly one per magnet\n");
  reset(0, MAP_CW);
  for(int i=0;i<40;i++) injectCorrect(900, 90);
  ckEq(navMm, 40, "40 magnets -> 40 markers, no more, no less");
  ckEq((long)naviNotIdentified, 0, "no refusals on a clean run");

  // -----------------------------------------------------------------------
  printf("\nT4  wrong polarity: refuse, hold position, stop in AUTO\n");
  reset(60, MAP_CW);
  injectCorrect(900,90);                       // anchor
  autoRunning=true; autoEnrolled=true;
  uint8_t before=navMm;
  injectWrong(900, 90);
  ckEq(navMm, before, "navMm unchanged on a polarity failure");
  ck(!autoRunning, "AUTO dropped on a polarity failure");
  const Cap* c=lastExact(T_NAVI);
  ck(payloadHas(c,"\"why\":\"POLARITY\""), "reason published as POLARITY");
  ck(payloadHas(c,"\"id\":0"), "published as not identified");

  // -----------------------------------------------------------------------
  printf("\nT5  rebound inside the debounce: refused, and NOT a stop\n");
  reset(60, MAP_CW);
  injectCorrect(900,90);                       // anchor
  autoRunning=true;
  before=navMm;
  // the rebound: opposite pole, ~10% of peak, arriving 60 ms later
  inject(dnaAt(navMm)?0:1, 20, 40, 60, 90);
  ckEq(navMm, before, "rebound did not advance position");
  c=lastExact(T_NAVI);
  ck(payloadHas(c,"\"why\":\"REBOUND\""), "reason published as REBOUND");
  ck(autoRunning, "a rebound refusal does NOT stop the train");

  // -----------------------------------------------------------------------
  printf("\nT6  THE MORATORIUM: stop on the magnet, wait, restart\n");
  // The operator's own edge case: 'Debounce may not work depending on the
  // stopping point... We are measuring it by time.' Wall-clock debounce fails
  // here; motion-time debounce must not.
  reset(60, MAP_CW);
  injectCorrect(900,90);                       // anchor
  spend(200, 90);                              // 200 ms of travel past it
  spend(60000, 0);                             // then stand for a full minute
  before=navMm;
  inject(dnaAt(navMm)?0:1, 20, 40, 100, 90);   // restart into the rebound
  ckEq(navMm, before, "rebound after a 60 s dwell still refused");
  c=lastExact(T_NAVI);
  ck(payloadHas(c,"\"why\":\"REBOUND\""), "still REBOUND, not admitted");

  // and the counter-check: a genuine magnet after the dwell IS accepted
  spend(500, 90);
  injectCorrect(500, 90);
  ckEq(navMm, (uint8_t)nextMm(before,MAP_CW), "real magnet after dwell accepted");

  // -----------------------------------------------------------------------
  printf("\nT7  no event of any kind advances navMm by more than one\n");
  reset(0, MAP_CW);
  uint8_t prev=navMm; long jumps=0;
  for(int i=0;i<600;i++){
    uint8_t pol = (i*7+i/3)&1;                 // arbitrary, often wrong
    int pk = 20 + (i*37)%400;
    unsigned long dt = 20 + (i*53)%1500;
    uint8_t pwm = (i%5==0)?0:(uint8_t)(40+(i*11)%70);
    inject(pol, pk, (uint16_t)(40+(i*17)%200), dt, pwm);
    int step = (int)navMm - (int)prev;
    if(step<0) step += DNA_N;
    if(step>1) jumps++;
    prev=navMm;
  }
  ckEq(jumps, 0, "600 adversarial events produced no multi-marker jump");

  // -----------------------------------------------------------------------
  printf("\nT8  repeated refusals produce ZERO drift\n");
  reset(100, MAP_CW);
  injectCorrect(900,90);
  before=navMm;
  for(int i=0;i<50;i++){ autoRunning=true; injectWrong(900,90); }
  ckEq(navMm, before, "50 refusals moved position not at all");

  // -----------------------------------------------------------------------
  printf("\nT9  the ten-magnet word reports\n");
  reset(0, MAP_CW);
  for(int i=0;i<12;i++) injectCorrect(900,90);
  ck(naviSeqChecked>0, "sequence witness ran");
  ckEq((long)naviSeqFailed, 0, "and agreed with the map on a clean run");

  // -----------------------------------------------------------------------
  printf("\nT10 a declaration restores normal operation after a refusal\n");
  reset(60, MAP_CW);
  injectCorrect(900,90);
  autoRunning=true; injectWrong(900,90);
  ck(!autoRunning, "stopped");
  navDeclare(70); drainPubs();
  ckEq(navMm, 70, "re-declared");
  injectCorrect(900,90);
  ckEq(navMm, 71, "advancing again after re-declaration");

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures?1:0;
}
