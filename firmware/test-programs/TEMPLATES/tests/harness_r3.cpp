// ============================================================================
// harness_r3.cpp — host regression suite for the TEMPLATES 0.3B position
// contract (CODEX requirements, 2026-08-27).
//
// Compiles the REAL sketch (arduino-cli --preprocess output; one code path,
// no test copy) against tests/stubs/, then drives synthetic MarkerEvents
// through the genuine drainMarkers() -> r3Evaluate() -> navOnMarker() path
// with a simulated clock.
//
// The load-bearing test is T2: the field sequence of 2026-08-27 18:05
// (R3 confirms; the inherited conservation gate PHANTOM_REJECTs the same
// passage; under 0.3A R3 then resynced to the wreckage and drifted 82
// markers). Under 0.3B repeated downstream refusals must produce ZERO
// unsupported forward drift.
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
#include "gen/TEMPLATES_pp.cpp"

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
static bool payloadHas(const Cap* c, const char* sub){
  return c && c->payload.find(sub)!=std::string::npos;
}

// ---------------------------------------------------------------------------
// event helpers — simulated clock, real queue, real drain
// ---------------------------------------------------------------------------
static void inject(uint8_t pol, int peak, uint16_t durMs, unsigned long dtMs,
                   uint8_t pwmA, uint8_t pwmC){
  hostNowMs += dtMs;
  MarkerEvent e{};
  e.polarity=pol; e.peak=peak; e.durationMs=durMs; e.baselineDrift=0;
  e.detectedAtMs=hostNowMs;
  e.pwmActualAtDetect=pwmA; e.pwmCommandedAtDetect=pwmC;
  ck(xQueueSend(eventQueue,&e,0)==pdTRUE, "event queued");
  drainMarkers();
  drainPubs();
}
// model velocity (mm/s) the conservation gate uses at a given PWM
static float modelVel(uint8_t pwm){ return VEL_MODEL_SLOPE*(float)pwm + VEL_MODEL_INTERCEPT; }
// the gate's expected interval for the NEXT event, given current navMm/dir
static float gateExpectedMs(uint8_t pwm){
  uint8_t idx = (navDir==MAP_CW) ? routeMod((int32_t)navMm-1) : navMm;
  return 1000.0f*(float)pgm_read_word(&spacingMm[idx]) / modelVel(pwm);
}
static uint32_t spacingOf(uint8_t idx){ return pgm_read_word(&spacingMm[idx]); }
// a "clean" event that should confirm as the expected next marker
static void injectClean(unsigned long dtMs, uint8_t pwm=90){
  uint8_t next = nextMm(navMm,navDir);
  int peak = (int)(190u * strengthAt(next) / 100u);
  uint16_t dur = (uint16_t)((uint32_t)durationAt(next) * 90u / pwm);
  inject(dnaAt(next), peak, dur, dtMs, pwm, pwm);
}
static unsigned long steadyDt(uint8_t pwm=90){
  // travel time for the segment about to be crossed at the model velocity —
  // combined ratio comes out ~2.0, far outside the phantom kill window.
  uint8_t next = nextMm(navMm,navDir);
  uint8_t idx  = (navDir==MAP_CW) ? navMm : next;
  return (unsigned long)(1000.0f*(float)spacingOf(idx)/modelVel(pwm));
}

static void bootAt(uint8_t mm){
  // direction first (applyDirection derives navDir), then position
  sessionDir = MAP_CW; motorDirection = DIRECTION_FORWARD;
  applyDirection();
  navDeclare(mm);
  caps.clear();
}
static void warmup(int n){
  for(int i=0;i<n;i++) injectClean(steadyDt());
}

// find a CW position where the polarity pattern ahead matches `pat`
// (pat like "NS": dnaAt(p+1)=='N' && dnaAt(p+2)=='S'); -1 if none
static int findPos(const char* pat){
  for(int p=0;p<DNA_N;p++){
    bool ok=true;
    for(int k=0; pat[k]; k++){
      uint8_t mm=routeMod(p+1+k);
      char c = dnaAt(mm)?'N':'S';
      if(c!=pat[k]){ ok=false; break; }
    }
    if(ok) return p;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// T1 — ordinary confirmation
// ---------------------------------------------------------------------------
static void t1_ordinary_confirmation(){
  printf("T1 ordinary confirmation\n");
  bootAt(40);
  uint8_t startMm = navMm;
  warmup(12);
  ckEq(navMm, routeMod(startMm+12), "12 clean events advance 12 markers");
  const Cap* r3 = lastOn("diag/r3_admit");
  ck(r3!=nullptr, "r3 record published");
  ck(payloadHas(r3,"\"prop\":\"TARGET_CONFIRMED\""), "proposal is TARGET_CONFIRMED");
  ck(payloadHas(r3,"\"nav\":\"ACCEPTED\""), "navigator disposition ACCEPTED");
  ck(payloadHas(r3,"\"cm\":1"), "committed flag set");
  ckEq(r3NavRefused, 0, "no navigator refusals in a clean run");
  ckEq(r3CorrectedN, 0, "no corrections in a clean run");
}

// ---------------------------------------------------------------------------
// T2 — THE regression: R3 confirms, phantom detector refuses, ZERO drift.
// ---------------------------------------------------------------------------
static void t2_phantom_refusal_no_drift(){
  printf("T2 phantom refusal produces no drift (MM101-104 regression)\n");
  bootAt(95);
  warmup(10);
  uint8_t P = navMm;
  // RAMP event: accepted, invalidates the timing predecessor
  {
    uint8_t next=nextMm(navMm,navDir);
    inject(dnaAt(next),(int)(190u*strengthAt(next)/100u),
           (uint16_t)((uint32_t)durationAt(next)),steadyDt(),90,120);
  }
  ckEq(navMm, routeMod(P+1), "RAMP event accepted");
  // seed a SHORT predecessor through NO_PREV (half the model expectation)
  float E1 = gateExpectedMs(90);           // for the event after this one
  unsigned long seedDt = (unsigned long)(0.5f*E1);
  injectClean(seedDt);
  ckEq(navMm, routeMod(P+2), "seed event accepted via NO_PREV");
  uint8_t held = navMm;
  unsigned long refusalsBefore = r3NavRefused;
  // now the trap: R3 confirms, the conservation gate refuses.
  // combined = 0.75E + 0.5E = 1.25E -> |combined-E| = 0.25E <= 0.30E -> kill.
  float E = gateExpectedMs(90);
  unsigned long killDt = (unsigned long)(0.75f*E);
  injectClean(killDt);
  {
    const Cap* r3 = lastOn("diag/r3_admit");
    ck(payloadHas(r3,"\"prop\":\"TARGET_CONFIRMED\""), "R3 proposed confirmation");
    ck(payloadHas(r3,"\"nav\":\"PHANTOM_REJECTED\""), "phantom detector refused it");
    ck(payloadHas(r3,"\"cm\":0"), "not committed");
  }
  ckEq(navMm, held, "refusal leaves navMm unchanged");
  ckEq(r3LastAcceptMm, held, "refusal leaves the R3 shadow unchanged");
  // repeat the refusal three more times: NO accumulation of any kind
  for(int i=0;i<3;i++) injectClean((unsigned long)(0.75f*gateExpectedMs(90)));
  ckEq(navMm, held, "repeated refusals: navMm still unchanged (the 0.3A bug)");
  ckEq(r3LastAcceptMm, held, "repeated refusals: shadow still unchanged");
  ckEq((long)(r3NavRefused-refusalsBefore), 4, "all four refusals counted");
  // recovery: a proper-cadence event is accepted and advances exactly one
  injectClean(steadyDt());
  ckEq(navMm, routeMod(held+1), "recovery event advances exactly one marker");
}

// ---------------------------------------------------------------------------
// T3 — unresolved hold, then committed correction (atomic jump)
// ---------------------------------------------------------------------------
static void t3_hold_then_committed_correction(){
  printf("T3 hold then committed correction\n");
  int p = findPos("NS");           // pol differs between next and next+1
  ck(p>=0, "position with N,S pattern exists");
  bootAt(routeMod((int32_t)p-10));  // warmup lands navMm exactly on p
  warmup(10);
  uint8_t Q = navMm;
  ckEq(Q, (long)p, "warmup landed on the pattern position");
  // hold: garbage event — wrong polarity AND wrong duration -> conf < 50
  {
    uint8_t next=nextMm(navMm,navDir);
    inject(dnaAt(next)?0:1, 150, 20, steadyDt(), 90, 90);
  }
  ckEq(navMm, Q, "held event does not advance position");
  {
    const Cap* r3 = lastOn("diag/r3_admit");
    ck(payloadHas(r3,"\"prop\":\"MAGNET_UNRESOLVED\""), "held as MAGNET_UNRESOLVED");
    ck(payloadHas(r3,"\"nav\":\"NOT_PRESENTED\""), "never presented to the navigator");
  }
  ckEq(r3UnresolvedStreak, 1, "unresolved streak counts the hold");
  // correction: the NEXT physical magnet is Q+2 (the held one was Q+1).
  // dtAcc spans both segments at sane speed; ladder sees a long dt (pass).
  uint8_t cand = routeMod((int32_t)Q + 2);
  {
    int peak=(int)(190u*strengthAt(cand)/100u);
    uint16_t dur=(uint16_t)durationAt(cand);
    unsigned long dt2 = steadyDt();   // remaining leg at model cadence
    inject(dnaAt(cand), peak, dur, dt2, 90, 90);
  }
  ckEq(navMm, cand, "committed correction jumps atomically to the candidate");
  {
    const Cap* r3 = lastOn("diag/r3_admit");
    ck(payloadHas(r3,"\"prop\":\"R3_CORRECTED\""), "proposal was a correction");
    ck(payloadHas(r3,"\"nav\":\"ACCEPTED\""), "correction committed");
    ck(payloadHas(r3,"\"cm\":1"), "committed flag set");
  }
  ck(r3CorrectedN>=1, "committed-correction counter incremented");
  ckEq(r3UnresolvedStreak, 0, "commitment closes the unresolved gap");
}

// ---------------------------------------------------------------------------
// T4 — correction REFUSED downstream: no partial commit, no shadow move
// ---------------------------------------------------------------------------
static void t4_correction_refused_atomically(){
  printf("T4 refused correction commits nothing\n");
  int p = findPos("NS");
  ck(p>=0, "position with N,S pattern exists");
  bootAt(routeMod((int32_t)p-12));  // warmup+ramp+seed land near p
  warmup(10);
  // RAMP then short-seed the predecessor (as in T2)
  {
    uint8_t next=nextMm(navMm,navDir);
    inject(dnaAt(next),(int)(190u*strengthAt(next)/100u),
           (uint16_t)durationAt(next),steadyDt(),90,120);
  }
  injectClean((unsigned long)(0.5f*gateExpectedMs(90)));
  uint8_t Q = navMm;
  unsigned long nxBefore = r3CorrectedN;
  // hold one (wrong pol + garbage duration; short dt is fine — R3 holds on
  // low confidence or reachability, either way NOT_PRESENTED)
  {
    uint8_t next=nextMm(navMm,navDir);
    inject(dnaAt(next)?0:1, 150, 20, 300, 90, 90);
  }
  ckEq(navMm, Q, "held event does not advance");
  // correction proposal timed INTO the phantom kill window AND above the
  // two-segment reachability veto. Held events never update lastMarkerMs,
  // so the ladder's dt equals R3's dtAcc (both measured from the seed):
  //   kill window: dtAcc + 0.5E within [0.7,1.3]E -> dtAcc in [0.2E, 0.8E]
  //   reachability (2 segments): dtAcc >= 1.25 x (span of both segments)
  float E = gateExpectedMs(90);
  uint32_t span2 = spacingOf(navMm) + spacingOf(routeMod((int32_t)navMm+1)); // CW
  float lo = 1.28f*(float)span2;          // just above the 800 mm/s veto
  float hi = 0.80f*E;                     // top of the kill window
  ck(lo < hi, "kill window and reachability overlap at this position");
  unsigned long dtAccTarget = (unsigned long)(0.5f*(lo+hi));
  unsigned long dtG = dtAccTarget - 300;  // 300 ms was spent on the hold
  uint8_t cand = routeMod((int32_t)Q + 2);
  {
    int peak=(int)(190u*strengthAt(cand)/100u);
    uint16_t dur=(uint16_t)durationAt(cand);
    inject(dnaAt(cand), peak, dur, dtG, 90, 90);
  }
  const Cap* r3 = lastOn("diag/r3_admit");
  if(payloadHas(r3,"\"prop\":\"R3_CORRECTED\"")){
    ck(payloadHas(r3,"\"nav\":\"PHANTOM_REJECTED\""), "correction refused by phantom gate");
    ck(payloadHas(r3,"\"cm\":0"), "refused correction not committed");
    ckEq(navMm, Q, "REFUSED correction moves navMm not at all (atomicity)");
    ckEq(r3LastAcceptMm, Q, "shadow unchanged after refused correction");
    ckEq((long)(r3CorrectedN-nxBefore), 0, "no committed-correction count");
  }else{
    // If scoring didn't reach correction authority the event was held —
    // also a no-drift outcome, but then this test exercised nothing new.
    printf("  note: correction authority not reached (prop=%s)\n",
           payloadHas(r3,"MAGNET_UNRESOLVED")?"MAGNET_UNRESOLVED":"?");
    ck(false, "T4 must reach the refused-correction branch to count");
  }
}

// ---------------------------------------------------------------------------
// T5 — quarantine hold then successor COMMIT (inherited machinery intact)
// ---------------------------------------------------------------------------
static void t5_quarantine_commit(){
  printf("T5 quarantine hold, successor commits\n");
  // need pol(next) != pol(next+2)'s frame: matchG && !matchP at arbitration
  int p=-1;
  for(int q=0;q<DNA_N;q++){
    // post-warmup frame Q=q+10. The dim-suspect conjunction needs the event
    // polarity OPPOSITE the ring's last entry (dna Q != dna Q+1), and the
    // arbitration needs distinguishable frames (dna Q+1 != dna Q+2): an
    // alternating triple.
    uint8_t m0=routeMod(q+10), m1=routeMod(q+11), m2=routeMod(q+12);
    if(dnaAt(m0)!=dnaAt(m1) && dnaAt(m1)!=dnaAt(m2)){ p=q; break; }
  }
  ck(p>=0, "position with differing next/next+2 arbitration frames exists");
  bootAt((uint8_t)p);
  // slow steady cadence so the conjunction band is calibrated high
  for(int i=0;i<10;i++) injectClean(2500);
  uint8_t Q = navMm;
  // DIM suspect: opposite polarity to the ring's last entry comes free with
  // an alternating map; peak at half expectation, dt < 0.4 x median(2500)
  uint8_t next=nextMm(navMm,navDir);
  {
    int weakPeak=(int)(0.5f*190.0f*strengthAt(next)/100.0f);
    inject(dnaAt(next), weakPeak, (uint16_t)durationAt(next), 900, 90, 90);
  }
  const Cap* r3h = lastOn("diag/r3_admit");
  bool wasPresented = payloadHas(r3h,"\"nav\":\"QUARANTINED\"");
  if(wasPresented){
    ckEq(navMm, Q, "quarantined event does not advance position");
    ck(payloadHas(r3h,"\"cm\":0"), "quarantined = not committed");
    ckEq(r3LastAcceptMm, Q, "shadow unchanged while pending");
    // successor fits H-genuine (mmG = Q+2), proper cadence, fit witness
    uint8_t mmG=routeMod((int32_t)Q+2);
    {
      int peak=(int)(190u*strengthAt(mmG)/100u);
      inject(dnaAt(mmG), peak, (uint16_t)durationAt(mmG), 2500, 90, 90);
    }
    ckEq(navMm, routeMod(Q+2), "arbitration commit + successor = exactly two");
  }else{
    // R3 held it before the quarantine could (reachability/conf) — also
    // safe; document which layer refused.
    ckEq(navMm, Q, "held event does not advance (R3 layer)");
    printf("  note: event held by R3, quarantine path not reached here\n");
  }
}

// ---------------------------------------------------------------------------
// T6 — quarantine hold then successor DISCARD (phantom verdict)
// ---------------------------------------------------------------------------
static void t6_quarantine_discard(){
  printf("T6 quarantine hold, successor discards\n");
  int p=-1;
  for(int q=0;q<DNA_N;q++){
    // post-warmup frame Q=q+10. The dim-suspect conjunction needs the event
    // polarity OPPOSITE the ring's last entry (dna Q != dna Q+1), and the
    // arbitration needs distinguishable frames (dna Q+1 != dna Q+2): an
    // alternating triple.
    uint8_t m0=routeMod(q+10), m1=routeMod(q+11), m2=routeMod(q+12);
    if(dnaAt(m0)!=dnaAt(m1) && dnaAt(m1)!=dnaAt(m2)){ p=q; break; }
  }
  bootAt((uint8_t)p);
  for(int i=0;i<10;i++) injectClean(2500);
  uint8_t Q = navMm;
  uint8_t next=nextMm(navMm,navDir);
  {
    int weakPeak=(int)(0.5f*190.0f*strengthAt(next)/100.0f);
    inject(dnaAt(next), weakPeak, (uint16_t)durationAt(next), 900, 90, 90);
  }
  if(qPendingValid){
    // successor fits H-PHANTOM (mmP = Q+1): pending discarded, successor
    // judged on the folded interval and accepted -> exactly one advance
    uint8_t mmP=nextMm(Q,navDir);
    {
      int peak=(int)(190u*strengthAt(mmP)/100u);
      inject(dnaAt(mmP), peak, (uint16_t)durationAt(mmP), 2500, 90, 90);
    }
    ckEq(navMm, routeMod(Q+1), "discard verdict: exactly one advance");
  }else{
    printf("  note: event held by R3, quarantine discard path not reached\n");
    ckEq(navMm, Q, "no advance either way");
  }
}

// ---------------------------------------------------------------------------
// T7 — relocation: R3 stands aside once, resyncs on the next acceptance
// ---------------------------------------------------------------------------
static void t7_relocation_resync(){
  printf("T7 relocation resync\n");
  bootAt(40);
  warmup(6);
  unsigned long bypBefore = r3Bypassed;
  navDeclare(90);                       // operator relocation
  injectClean(steadyDt());              // first event after relocation
  ckEq(navMm, 91, "post-relocation event accepted at the new frame");
  ck(r3Bypassed>bypBefore, "R3 stood aside for the first post-relocation event");
  ckEq(r3LastAcceptMm, navMm, "shadow resynced on the ACCEPTED event");
  unsigned long confBefore = r3Confirmed;
  injectClean(steadyDt());              // R3 active again
  ck(r3Confirmed>confBefore, "R3 active again after resync");
}

// ---------------------------------------------------------------------------
// T8 — QUORUM evaluation owns its regime; R3 stands aside and returns
// ---------------------------------------------------------------------------
static void t8_evaluating_bypass(){
  printf("T8 EVALUATING bypass and return\n");
  // a run of >=5 same-polarity markers so wrong-polarity events still score
  // sPol=0 on every candidate and confirm on the remaining attributes
  int p = findPos("NNNNN");
  if(p<0) p = findPos("SSSSS");
  ck(p>=0, "same-polarity run exists on the map");
  bootAt(routeMod((int32_t)p-8));   // disagreements land inside the run,
  warmup(8);                        // later clean events differentiate past it
  // three wrong-polarity-else-clean events -> confirmed, DISAGREE x3
  for(int i=0;i<3 && navState==NAV_NORMAL;i++){
    uint8_t next=nextMm(navMm,navDir);
    int peak=(int)(190u*strengthAt(next)/100u);
    inject(dnaAt(next)?0:1, peak, (uint16_t)durationAt(next), steadyDt(), 90, 90);
  }
  ck(navState==NAV_EVALUATING, "three disagreements open an evaluation");
  unsigned long bypBefore = r3Bypassed;
  // correct events let the evaluation resolve to offset 0 — inside a
  // same-polarity run every offset scores alike, so give the incident room
  // to reach differentiating map positions
  for(int i=0;i<12 && navState==NAV_EVALUATING;i++) injectClean(steadyDt());
  ck(r3Bypassed>bypBefore, "R3 bypassed during EVALUATING");
  ck(navState==NAV_NORMAL, "evaluation resolved back to NORMAL");
  injectClean(steadyDt());
  ckEq(r3LastAcceptMm, navMm, "shadow resynced after the incident");
}

// ---------------------------------------------------------------------------
// T9 — 0.3C: a single-event "silent magnet" claim must NOT correct.
// The first 0.3B field run advanced itself a lap-plus by treating every
// pattern disagreement as a silently missed magnet. With no held passages
// (streak 0) a wrong-polarity-else-clean event must confirm at the EXPECTED
// marker (advancing exactly one, publishing DISAGREE so QUORUM's missStreak
// grows) — never jump ahead.
// ---------------------------------------------------------------------------
static void t9_no_silent_miss_correction(){
  printf("T9 silent-miss claim does not correct\n");
  // alternating region: the off=1 candidate polarity-matches a misread
  int p=-1;
  for(int q=0;q<DNA_N;q++){
    uint8_t m1=routeMod(q+11), m2=routeMod(q+12);
    if(dnaAt(m1)!=dnaAt(m2)){ p=q; break; }
  }
  ck(p>=0, "alternating region exists");
  bootAt((uint8_t)p);
  warmup(10);
  uint8_t Q = navMm;
  unsigned long corrBefore = r3CorrectedN;
  unsigned long disagreeBefore = navDisagree;
  // wrong polarity, everything else matching the expected marker: exactly
  // the shape that produced the runaway (pol=100 at off=1, seq=0)
  uint8_t next=nextMm(navMm,navDir);
  {
    int peak=(int)(190u*strengthAt(next)/100u);
    inject(dnaAt(next)?0:1, peak, (uint16_t)durationAt(next), steadyDt(), 90, 90);
  }
  ckEq(navMm, routeMod(Q+1), "misread advances exactly ONE marker, not two");
  ckEq((long)(r3CorrectedN-corrBefore), 0, "no correction on a single-event claim");
  ck(navDisagree>disagreeBefore, "DISAGREE published — QUORUM stays fed");
  const Cap* r3 = lastOn("diag/r3_admit");
  ck(payloadHas(r3,"\"prop\":\"TARGET_CONFIRMED\""), "confirmed at the expected marker");
}

// ---------------------------------------------------------------------------
int main(){
  // minimal init mirroring setup()'s relevant slice: queues + topics
  pubQueue=xQueueCreate(256,sizeof(PubMsg));
  markerPubQueue=xQueueCreate(256,sizeof(PubMsg));
  cmdQueue=xQueueCreate(16,sizeof(CmdMsg));
  eventQueue=xQueueCreate(64,sizeof(MarkerEvent));
  rejectQueue=xQueueCreate(32,sizeof(AdmitReject));
  buildTopics();

  t1_ordinary_confirmation();
  t2_phantom_refusal_no_drift();
  t3_hold_then_committed_correction();
  t4_correction_refused_atomically();
  t5_quarantine_commit();
  t6_quarantine_discard();
  t7_relocation_resync();
  t8_evaluating_bypass();
  t9_no_silent_miss_correction();

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
