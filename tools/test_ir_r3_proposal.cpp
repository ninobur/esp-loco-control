// Synthetic acceptance cases for the IR TX R3 REVIEW PROPOSAL
// (tools/ir_r3_proposal/IrMovementDetector.h). Uses the actual Measurement,
// wire/CRC, IrHealthMonitor epoch and IrSpeedTelemetry code. Build only via
// tools/run_ir_r2_bench_review.sh, which stages the proposal header in place
// of firmware/common/IrMovementDetector.h in a temporary copy.
//
// Waveforms are smooth (cosine) rather than square, decelerate with a
// lengthening period, and carry stationary noise and single-conversion ADC
// outliers at roughly the rate measured on the 2026-09-23 bench (>100 counts:
// ~3 per 1000 quiet samples, runs of 1-2). These are MODELS of the capture,
// not the capture; tools/test_ir_r3_bench_replay.cpp replays the capture.
#include "IrSpeedTelemetry.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <initializer_list>
using namespace ir_movement;

static const uint8_t MAC[6]={2,3,4,5,6,7};
static const double PI=3.14159265358979323846;
static int failures=0;
static void check(bool hard,bool ok,const char* id,const char* text){
  std::printf("%s %-30s %s\n",ok?"PASS":hard?"FAIL":"INFO",id,text); if(hard&&!ok)++failures;
}
static bool ready(Reason r){return r==TRACKING||r==SIGNAL_STALE;}

struct Rig {
  Measurement m{123,0,9.652,true};
  ngr_nav::IrHealthMonitor rx; ngr_nav::IrSpeedTelemetry speed;
  uint64_t us=0; uint32_t seq=0, rng=12345;
  double phase=0;            // radians; level = 1500 - 500 cos(phase)
  unsigned readyZeroWhileTurning=0, readySnapsInHold=0;
  bool turning=false;
  Rig(){rx.pair(MAC,0);}
  int noise(){ rng=rng*1664525u+1013904223u; int n=int(rng>>28)-8; // +-8 counts
    if((rng>>8)%1000<3) n+= ((rng>>20)&1)?260:-260;                // single-sample outlier
    return n; }
  const Detector& d() const {return m.detector();}
  void sample(double level){
    us+=1000; int raw=int(std::lround(level))+noise(); if(raw<1)raw=1; if(raw>4094)raw=4094;
    m.sample(us,(uint16_t)raw);
    if(us%100000==0){
      auto w=encode(m.snapshot(),++seq,d().high-d().low);
      w.crc=ngr_nav::movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(WireSnapshot,crc));
      rx.receive(MAC,reinterpret_cast<const uint8_t*>(&w),sizeof(w),us+10000);
      if(turning && d().reason==SIGNAL_STALE) ++readyZeroWhileTurning;
    }
  }
  double level() const { return 1500-500*std::cos(phase); }
  // Turn at `periodMs` for `ms`.
  void turn(double periodMs,unsigned ms){ turning=true; for(unsigned i=0;i<ms;++i){ phase+=2*PI/periodMs; sample(level()); } }
  // Decelerate from 150 ms to ~1.5 s periods, then continue slowly until the
  // phase reaches `stopPhase` (mod 2 pi) and stop there.
  void decelerateTo(double stopPhase){
    turning=true; double period=150;
    for(unsigned i=0;i<3000;++i){ period*=1.00077; phase+=2*PI/period; sample(level()); }
    const double target=std::floor(phase/(2*PI))*2*PI+stopPhase+2*PI;
    while(phase<target){ phase=std::min(target,phase+2*PI/period); sample(level()); }
    turning=false;
  }
  void hold(unsigned ms){ turning=false; for(unsigned i=0;i<ms;++i) sample(level()); }
  ngr_nav::IrSpeedReading read(){ return speed.sample(rx,us+10000,true,true); }
};

// Stop at a plateau or mid-band after a smooth deceleration.
static void stopCase(const char* id,double stopPhase,bool expectReady){
  Rig r; r.turn(150,4000);
  const auto referenceHeld=r.d().referenceHigh()-r.d().referenceLow();
  const auto epoch=r.rx.odometry().epochId();
  ngr_nav::MmDistanceReference ref;
  const bool synced=ref.synchronize(41,0,r.rx.odometry().point(),r.rx.odometry());
  const auto aborts=r.d().aborts;
  r.decelerateTo(stopPhase);
  const uint64_t countAtStop=r.d().completed;
  const unsigned refSpanAtStop=r.d().referenceHigh()-r.d().referenceLow();
  unsigned readyInHold=0, snaps=0; ngr_nav::IrSpeedReading last;
  for(unsigned s=0;s<30;++s){ r.hold(100); ++snaps; if(ready(r.d().reason))++readyInHold; if(s%10==9) last=r.read(); }
  char text[200];
  std::snprintf(text,sizeof text,"reason=%s ready %u/%u snaps, aborts +%llu, epoch %s, old MM %s, speed %s %.1f, ref span %u->%u",
    r.d().reason==SIGNAL_STALE?"SIGNAL_STALE":r.d().reason==INADEQUATE_CONTRAST?"INADEQUATE":"other",readyInHold,snaps,
    (unsigned long long)(r.d().aborts-aborts),r.rx.odometry().epochId()>epoch?"new":"same",
    synced&&ref.validFor(r.rx.odometry())?"VALID":"invalid",last.valid?"valid":last.reason,last.mmps,referenceHeld,refSpanAtStop);
  bool ok;
  if(expectReady) ok= r.d().reason==SIGNAL_STALE && last.valid && last.mmps==0 && r.d().completed==countAtStop &&
                      (r.d().aborts==aborts || (r.rx.odometry().epochId()>epoch && !(synced&&ref.validFor(r.rx.odometry()))));
  else ok= readyInHold==0 && !last.valid;
  check(true,ok,id,text);
  check(true,!expectReady || refSpanAtStop+32>=referenceHeld,(std::string(id)+"_no_ratchet").c_str(),
        "deceleration did not narrow the optical reference by more than two buckets");
}

int main(){
  // 1. Stops after a smooth deceleration (the physical failure).
  stopCase("stop_dark_plateau",0,true);
  stopCase("stop_bright_plateau",PI,true);
  stopCase("stop_mid_band",PI/2,false);
  stopCase("stop_quarter_band",PI/3,false);

  // 2. Outliers during a retained hold do not revoke; a sustained departure does.
  { Rig r; r.turn(150,4000); r.decelerateTo(0); r.hold(3000);
    const auto a=r.d().aborts, out=r.d().holdOutliers;
    r.hold(60000);
    char t[160]; std::snprintf(t,sizeof t,"60 s hold with ~3/1000 single outliers: reason %s, aborts +%llu, outliers counted %llu",
      r.d().reason==SIGNAL_STALE?"SIGNAL_STALE":"not ready",(unsigned long long)(r.d().aborts-a),(unsigned long long)(r.d().holdOutliers-out));
    check(true,r.d().reason==SIGNAL_STALE && r.d().aborts==a && r.d().holdOutliers>out,"hold_survives_outliers",t);
    for(int i=0;i<3;++i){ r.us+=1000; r.m.sample(r.us,1500); }
    check(true,r.d().reason==INADEQUATE_CONTRAST && r.d().aborts>a && !r.d().referenceKnown(),"sustained_departure_revokes",
          "3 consecutive mid-band samples revoke retention and count continuity loss");
  }

  // 3. Restart from a retained stop. On a smooth waveform the restarting
  //    wheel spends many samples between plateaus, so retention is revoked
  //    as a sustained departure: continuity loss is counted (new epoch) and
  //    at most the first cycle is lost. Never an over-count or a credited tail.
  for(int bright=0;bright<2;++bright){
    Rig r; r.turn(150,4000); r.decelerateTo(bright?PI:0); r.hold(3000);
    const bool held=r.d().reason==SIGNAL_STALE; const auto a=r.d().aborts;
    const auto c0=r.d().completed; r.turn(200,2000);  // exactly 10 cycles from the stop phase
    const auto n=r.d().completed-c0; const unsigned full=bright?9:10;
    char t[160]; std::snprintf(t,sizeof t,"held=%d, 10 cycles from the %s plateau: counted %llu of %u full cycles, aborts +%llu",
      held,bright?"bright":"dark",(unsigned long long)n,full,(unsigned long long)(r.d().aborts-a));
    check(true,held && n<=full && n+1>=full && (n==full || r.d().aborts>a),bright?"restart_bright_no_tail":"restart_dark",t);
  }

  // 4. Modulation fading while the wheel keeps turning (sun/shade compression
  //    toward the bright plateau). Must never be reported as a stationary zero
  //    while the remaining modulation exceeds the stillness bound.
  for(double endAmp: {400.0,100.0,70.0,40.0}){
    Rig r; r.turn(150,4000); r.turning=true;
    for(unsigned i=0;i<6000;++i){
      const double a=500-(500-endAmp/2)*std::min(1.0,i/4000.0);
      r.phase+=2*PI/200; r.sample(2000-a*(1+std::cos(r.phase)));
    }
    char id[40],t[160]; std::snprintf(id,sizeof id,"fade_turning_%g",endAmp);
    std::snprintf(t,sizeof t,"final modulation %g counts at the bright plateau: %u stationary-ready snapshots while turning",endAmp,r.readyZeroWhileTurning);
    // Bound: StillSpan 63 plus bucket quantisation. Below it one optical channel
    // cannot tell a still wheel from a turning one (documented limit).
    check(endAmp>=80,r.readyZeroWhileTurning==0,id,t);
  }

  std::printf(failures?"FAILED %d\n":"ALL PASS\n",failures);
  return failures?1:0;
}
