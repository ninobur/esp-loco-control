// Adversarial checks for the IR TX 1.6 stop-retention candidate (1de06ae).
// Written by the independent review, docs/IR_TX_1_6_STOP_RETENTION_INDEPENDENT_REVIEW_20260923.md.
//
// Each case states the truthful outcome. A case FAILS when the detector
// reports measurement-ready (TRACKING or SIGNAL_STALE) while completed pulses
// fall short of the synthetic truth, i.e. an unavailable measurement is
// presented as a valid count or a valid zero. Exit status is non-zero while any
// hard case fails; INFO cases only print what happened.
//
// c++ -std=c++17 -fsanitize=address,undefined -Ifirmware/common \
//   -Ifirmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH \
//   tools/test_ir_stationary_adversarial.cpp -o /tmp/ir_adv && /tmp/ir_adv
#include "IrSpeedTelemetry.h"
#include <cstdio>
#include <vector>
using namespace ir_movement;

static bool ready(Reason r){return r==TRACKING || r==SIGNAL_STALE;}
static const char* name(Reason r){
  static const char* n[]={"PRIMING","INADEQUATE_CONTRAST","SATURATION","SAMPLE_GAP","SIGNAL_STALE","REACQUIRING","TRACKING"};
  return r<=TRACKING?n[r]:"?";
}

struct Rig {
  explicit Rig(bool retain):m(123,0,9.652,retain){}
  Measurement m;
  uint64_t us=0,truth=0;
  bool level=false;          // truth phase: true = high plateau
  uint64_t readySnaps=0,snaps=0;
  Snapshot lastSnap{};
  std::vector<Snapshot> history;
  void sample(uint16_t raw){
    us+=1000;m.sample(us,raw);
    if(us%100000==0){auto s=m.snapshot();++snaps;if(ready(s.reason))++readySnaps;history.push_back(s);}
  }
  void hold(uint16_t raw,unsigned ms){for(unsigned i=0;i<ms;++i)sample(raw);}
  // Square wave; truth counts one completion per high->low transition.
  void wave(uint16_t lo,uint16_t hi,unsigned halfMs,unsigned cycles){
    for(unsigned c=0;c<cycles;++c){hold(hi,halfMs);level=true;hold(lo,halfMs);level=false;++truth;}
  }
  void prime(){wave(1000,2000,100,20);}
  const Detector& d() const {return m.detector();}
};

static int failures=0;
static void verdict(bool hard,bool ok,const char* id,const char* text){
  std::printf("%s %-28s %s\n",ok?"PASS":hard?"FAIL":"INFO",id,text);
  if(hard && !ok)++failures;
}

// One-second windows (ten 10 Hz snapshots) whose two ends are both
// measurement-ready, with no epoch-checked counter change, yet which count at
// least two fewer completions than the synthetic truth.
static uint64_t concealedSeconds(const Rig& r,size_t from,uint64_t truthPerSecond){
  uint64_t n=0;
  for(size_t i=from+10;i<r.history.size();++i){
    const auto&a=r.history[i-10];const auto&b=r.history[i];
    if(ready(a.reason)&&ready(b.reason)&&a.sampleGaps==b.sampleGaps&&a.saturatedSamples==b.saturatedSamples&&
       a.openAborts==b.openAborts&&b.completedPulses-a.completedPulses+2<=truthPerSecond)++n;
  }
  return n;
}

int main(){
  char t[320];
  // 1. Dark level rises (e.g. ambient/sun) while the wheel keeps turning. The
  // new dark level (1400) is inside the learned outer envelope [750,2250] but
  // above the frozen low threshold (1333), so no completion is possible.
  // The default detector itself loses ~2 pulses while adapting to the step
  // (pre-existing, reported INFO). The hard check is: no worse than default.
  {
    uint64_t hidden[2]={0,0};
    for(int retain=1;retain>=0;--retain){
      Rig r(retain);r.prime();const auto c0=r.d().completed;const size_t h0=r.history.size();
      r.wave(1400,2100,100,25);
      const auto got=r.d().completed-c0;hidden[retain]=concealedSeconds(r,h0,5);
      std::snprintf(t,sizeof t,"%s: truth 25, counted %llu, final %s, ready 1 s windows missing >=2 pulses %llu",
        retain?"1.6":"default",(unsigned long long)got,name(r.d().reason),(unsigned long long)hidden[retain]);
      verdict(false,hidden[retain]==0,retain?"dark_level_rise_moving":"dark_level_rise_default",t);
    }
    std::snprintf(t,sizeof t,"1.6 concealed windows %llu vs default %llu",(unsigned long long)hidden[1],(unsigned long long)hidden[0]);
    verdict(true,hidden[1]<=hidden[0],"dark_level_rise_vs_default",t);
  }
  // 2. Contrast compresses toward mid-band while turning (span 200 >= 120,
  // so the flat mid-band guard does not fire). Truthful: unavailable.
  for(int retain=1;retain>=0;--retain){
    Rig r(retain);r.prime();const auto c0=r.d().completed;const size_t h0=r.history.size();
    r.wave(1400,1600,100,25);
    const auto got=r.d().completed-c0;const auto hidden=concealedSeconds(r,h0,5);
    std::snprintf(t,sizeof t,"%s: truth 25, counted %llu, final %s, ready 1 s windows missing >=2 pulses %llu",
      retain?"1.6":"default",(unsigned long long)got,name(r.d().reason),(unsigned long long)hidden);
    verdict(retain,hidden==0,retain?"contrast_compression_moving":"contrast_compression_default",t);
  }
  // 3. Ripple of 110 counts near the learned dark level while turning. Whether
  // this is motion or noise depends on the unmeasured stationary noise floor.
  {
    Rig r(true);r.prime();const auto c0=r.d().completed;
    r.wave(1000,1110,100,25);
    std::snprintf(t,sizeof t,"1.6: 110-count ripple at dark level, counted %llu, final %s (noise floor unmeasured)",
      (unsigned long long)(r.d().completed-c0),name(r.d().reason));
    verdict(false,!ready(r.d().reason),"low_band_ripple",t);
  }
  // 4. One non-saturating ADC glitch (2600 > learned 2000 + 250 margin) while
  // turning fast. 1.6 revokes retention and drops the open pulse; no counter
  // checked by IrOdometryEpoch changes, so continuity depends on a 10 Hz
  // snapshot happening to catch REACQUIRING. Default mode never revokes.
  for(int retain=1;retain>=0;--retain){
    unsigned lost=0,invisible=0,trials=0;
    for(unsigned offset=0;offset<100;offset+=3){
      Rig r(retain);r.prime();
      const size_t h0=r.history.size();const auto c0=r.d().completed;const auto t0=r.truth;
      r.wave(1000,2000,15,offset/30+1);
      r.hold(2000,5+offset%10);r.sample(2600);r.hold(2000,10-offset%10);r.hold(1000,15);++r.truth;
      r.wave(1000,2000,15,10);
      while(r.us%100000)r.sample(r.level?2000:1000);
      r.sample(1000);
      ++trials;
      const auto missing=(r.truth-t0)-(r.d().completed-c0);
      if(missing){++lost;
        bool allReady=true;for(size_t i=h0-1;i<r.history.size();++i)allReady&=ready(r.history[i].reason);
        const auto&a=r.history[h0-1];const auto&b=r.history.back();
        if(allReady&&a.openAborts==b.openAborts&&a.sampleGaps==b.sampleGaps&&a.saturatedSamples==b.saturatedSamples)++invisible;
      }
    }
    std::snprintf(t,sizeof t,"%s: %u trials, %u lost a pulse, %u of those with every 10 Hz snapshot ready and no epoch-checked counter change",
      retain?"1.6":"default",trials,lost,invisible);
    verdict(retain,invisible==0,retain?"glitch_while_moving":"glitch_while_moving_default",t);
  }
  // 5. The same glitch at a stop: retention is revoked and cannot return
  // until motion. Truthful, but shows retention's sensitivity to one sample.
  {
    Rig r(true);r.prime();r.hold(1000,2000);const auto before=r.d().reason;
    r.sample(2600);r.hold(1000,2000);
    std::snprintf(t,sizeof t,"1.6: stop %s -> one 2600 sample -> %s for the rest of the stop",
      name(before),name(r.d().reason));
    verdict(false,ready(r.d().reason),"glitch_at_stop",t);
  }
  // 6. Cold start slower than the 512 ms window: never acquires, never
  // reports zero. Also find the slowest half-period that acquires.
  {
    Rig r(true);r.wave(1000,2000,4000,5);
    std::snprintf(t,sizeof t,"1.6: 4 s half-periods from cold, counted %llu of 5, ready snaps %llu",
      (unsigned long long)r.d().completed,(unsigned long long)r.readySnaps);
    verdict(true,r.readySnaps==0,"slow_cold_start_unavailable",t);
    unsigned slowest=0;
    for(unsigned half=100;half<=1000;half+=20){Rig q(true);q.wave(1000,2000,half,6);if(ready(q.d().reason)&&q.d().completed>=4)slowest=half;}
    std::snprintf(t,sizeof t,"1.6: slowest cold-start half-period acquiring = %u ms (~%.1f mm/s at 9.652 mm/cycle)",
      slowest,slowest?9.652*1000.0/(2*slowest):0.0);
    verdict(false,true,"cold_start_bound",t);
  }
  // 7. Stop mid-transition at 1500, then continue the same half-cycle down.
  {
    Rig r(true);r.prime();r.hold(2000,100);const auto c0=r.d().completed;const size_t h0=r.history.size();
    r.hold(1500,3000);
    uint64_t readyStop=0;for(size_t i=h0;i<r.history.size();++i)readyStop+=ready(r.history[i].reason);
    const auto stopReason=r.d().reason;
    r.hold(1000,100);r.wave(1000,2000,100,5);
    std::snprintf(t,sizeof t,"1.6: 3 s stop at mid-band -> %s (ready for %llu of 30 snaps), counted %llu of truth 6 after resume, final %s",
      name(stopReason),(unsigned long long)readyStop,(unsigned long long)(r.d().completed-c0),name(r.d().reason));
    verdict(true,!ready(stopReason),"midband_stop",t);
  }
  // 8. NAV impact via the older MovementEvidence path: two TRACKING points
  // one second apart while the wheel turns under the case-1 dark-level rise.
  {
    Rig r(true);r.prime();
    r.wave(1400,2100,100,3);
    auto a=r.m.snapshot();const auto ta=r.truth;
    r.wave(1400,2100,100,5);
    auto b=r.m.snapshot();
    ngr_nav::MotionPoint pa,pb;pa.wire=encode(a,1,0);pb.wire=encode(b,2,0);pa.issue=pb.issue=ngr_nav::MotionIssue::None;
    const auto iv=ngr_nav::between(pa,pb);
    std::snprintf(t,sizeof t,"1.6: %s->%s, NAV interval %s, nominal %.1f mm vs truth %.1f mm",
      name(a.reason),name(b.reason),iv.usable()?"USABLE":ngr_nav::issueName(iv.issue),iv.nominalMm,(r.truth-ta)*9.652);
    verdict(true,!iv.usable() || iv.nominalMm>=(r.truth-ta-1)*9.652,"nav_interval_undercount",t);
  }
  // 9. Speed pipeline under case 2 during an unpowered hand roll (the bench
  // test itself): does the dashboard show a valid zero while turning?
  {
    Rig r(true);ngr_nav::IrHealthMonitor rx;ngr_nav::IrSpeedTelemetry speed;ngr_nav::IrSpeedQualification q;
    static const uint8_t MAC[6]={2,3,4,5,6,7};rx.pair(MAC,0);uint32_t seq=0;
    unsigned validZero=0,seconds=0;bool inCase=false;
    auto step=[&](uint16_t raw){
      r.sample(raw);
      if(r.us%100000==0){auto w=encode(r.m.snapshot(),++seq,r.d().high-r.d().low);
        w.crc=ngr_nav::movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(WireSnapshot,crc));
        rx.receive(MAC,reinterpret_cast<const uint8_t*>(&w),sizeof(w),r.us+10000);}
      if(r.us%1000000==0){auto s=q.assess(speed.sample(rx,r.us+10000,true,true),r.us+10000,true,false,false);
        if(inCase){++seconds;if(s.valid&&s.mmps==0)++validZero;}}
    };
    for(unsigned c=0;c<20;++c){for(int i=0;i<100;++i)step(2000);for(int i=0;i<100;++i)step(1000);}
    inCase=true;
    for(unsigned c=0;c<50;++c){for(int i=0;i<100;++i)step(1600);for(int i=0;i<100;++i)step(1400);}
    std::snprintf(t,sizeof t,"1.6 pipeline, unpowered: wheel turning 5 cycles/s, dashboard valid 0 pKPH for %u of %u s",validZero,seconds);
    verdict(true,validZero==0,"speed_valid_zero_while_turning",t);
  }
  std::printf("%s: %d hard failure(s)\n",failures?"CANDIDATE NOT TRUTHFUL":"ALL HARD CASES PASS",failures);
  return failures?1:0;
}
