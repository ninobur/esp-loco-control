// Replays the 2026-09-23 R2 bench captures through the R3 REVIEW PROPOSAL
// detector (tools/ir_r3_proposal/IrMovementDetector.h). Build only via
// tools/run_ir_r2_bench_review.sh, which stages the proposal header.
//
// Uses the same envelope-update schedule reconstruction as the R2 replay
// (tools/ir_bench_replay.h); that schedule is fitted to transmitted envelope
// observables and does not depend on the detector under test. What R3 would
// have reported is a PREDICTION from recorded ADC samples, not an observation.
//
// Gates:
//  - never stationary-ready (SIGNAL_STALE) within 500 ms of a motion edge the
//    ESP32 itself transmitted (RISE/FALL bit), anywhere in either capture;
//    drift edges over a still signal are reported separately;
//  - count 99 and count 462 stops (rest near a learned level) become
//    SIGNAL_STALE after continuity loss (openAborts increments first);
//  - count 627 (mid-band rest) stays unavailable;
//  - R3 never counts more completions than R2 observed in any range.
#include "ir_bench_replay.h"
#include "IrMovementDetector.h"
using namespace ir_movement;
using namespace ir_bench;

static int failures=0;
static void check(bool ok,const char* what){ std::printf("%s %s\n",ok?"PASS":"FAIL",what); if(!ok)++failures; }

struct StopProbe { uint32_t k; const char* label; bool expectReady; bool certified; };

int main(int argc,char** argv){
  if(argc!=3){std::fprintf(stderr,"usage: %s bench.fix three_stops.fix\n",argv[0]);return 2;}
  // Last completion of each documented stop, from tools/test_ir_r2_bench_replay.cpp.
  const StopProbe probes[2][3]={
    {{125880,"count 99 (first short roll)",true,true},{0,nullptr,false,false},{0,nullptr,false,false}},
    {{2226471,"count 462 (roll 1)",true,true},{2242065,"count 532 (roll 2, radio gap in roll)",false,false},
     {2263601,"count 627 (roll 3)",false,true}}};
  for(int f=0;f<2;++f){
    Capture c=load(argv[1+f]); const long t0=fitSnapshotOffsetMs(c);
    std::printf("\n== %s\n",f?"three stops":"bench");
    uint64_t driftEdges=0, readyNearEdge=0, readySamples=0, overCount=0, underCount=0;
    for(const Range& r:ranges(c)){
      if(r.end-r.first<3000) continue;
      auto seg=replay<Detector>(c,r,[]{return Detector(false,true);},NoHook<Detector>{},t0);
      // Transmitted edges (observed by the ESP32 running R2). An edge is a
      // DRIFT edge when the raw signal is still around it (5th-95th
      // percentile of the surrounding +-100 samples within 64 counts): the
      // live threshold moved across a resting level; no wheel motion.
      std::vector<uint32_t> edges;
      for(uint32_t k=seg.start;k<r.end;++k) if(seg.at(k)->flagsObs&(F_RISE|F_FALL)){
        if(k<seg.start+100 || k+100>=r.end){ edges.push_back(k); continue; }
        std::vector<uint16_t> v; for(uint32_t j=k-100;j<=k+100;++j) v.push_back(seg.at(j)->raw);
        std::sort(v.begin(),v.end());
        if(v[v.size()*95/100]-v[v.size()*5/100]<64){ ++driftEdges;
          if(driftEdges<=10) std::printf("  drift %s at %u: raw 5-95%% %u-%u, completed %llu (replay count)\n",
            seg.at(k)->flagsObs&F_FALL?"fall":"rise",k,v[v.size()*5/100],v[v.size()*95/100],(unsigned long long)seg.at(k)->completed);
        } else edges.push_back(k);
      }
      const uint32_t from=seg.start+3000;  // fresh detector: ignore its own acquisition
      for(uint32_t k=from;k<r.end;++k){
        const Rec& x=*seg.at(k);
        if(x.reason!=SIGNAL_STALE) continue;
        ++readySamples;
        auto it=std::lower_bound(edges.begin(),edges.end(),k>500?k-500:0);
        if(it!=edges.end() && *it<=k+500){ ++readyNearEdge;
          if(std::getenv("IR_REPLAY_VERBOSE") && (readyNearEdge<5 || readyNearEdge%100==0)) std::printf("  ready k=%u, ESP edge at %+d ms, raw %u, span %u, aborts %llu\n",k,(int)*it-(int)k,x.raw,x.high-x.low,(unsigned long long)x.aborts); }
      }
      // Completions: R3 vs the ESP32's FALL bits over the same samples.
      if(r.end>from+1){
        uint64_t obs=0; for(uint32_t k=from;k<r.end;++k) obs+= (seg.at(k)->flagsObs&F_FALL)!=0;
        const uint64_t mine=seg.at(r.end-1)->completed-seg.at(from-1)->completed;
        if(mine>obs) overCount+=mine-obs; else underCount+=obs-mine;
      }
      for(const StopProbe& p:probes[f]){
        if(!p.label || p.k<from || p.k+3000>=r.end) continue;
        unsigned ready=0,snaps=0; uint32_t firstReady=0;
        for(uint32_t k=p.k;k<p.k+3000 && k<r.end;k+=100){ ++snaps; if(seg.at(k)->reason==SIGNAL_STALE){ ++ready; if(!firstReady)firstReady=k-p.k; } }
        const Rec& end=*seg.at(p.k+3000); const Rec& at=*seg.at(p.k); const Rec& before=*seg.at(p.k-1000);
        std::printf("%s: +3 s reason %s, stationary-ready %u/%u snapshots (first at +%u ms), aborts +%llu from 1 s before the last completion, completions after stop %llu\n",
          p.label,reasonName(end.reason),ready,snaps,firstReady,(unsigned long long)(end.aborts-before.aborts),(unsigned long long)(end.completed-at.completed));
        char t[160];
        if(p.expectReady){
          std::snprintf(t,sizeof t,"%s: R3 reports a stationary zero after visible continuity loss",p.label);
          check(end.reason==SIGNAL_STALE && end.aborts>before.aborts && end.completed==at.completed,t);
        } else if(p.certified){
          std::snprintf(t,sizeof t,"%s: mid-band rest stays unavailable",p.label);
          check(ready==0,t);
        } else std::printf("INFO %s is not replay-certified; shown for information only\n",p.label);
      }
    }
    std::printf("transmitted drift edges (threshold crossed a still signal): %llu\n",(unsigned long long)driftEdges);
    std::printf("R3 stationary-ready samples: %llu; within 500 ms of a transmitted motion edge: %llu\n",
      (unsigned long long)readySamples,(unsigned long long)readyNearEdge);
    std::printf("R3 completions vs transmitted falls: %llu fewer, %llu more\n",(unsigned long long)underCount,(unsigned long long)overCount);
    check(readyNearEdge==0,"never stationary-ready within 500 ms of a motion edge the ESP32 observed");
    check(overCount==0,"never counts a completion the ESP32 did not observe");
  }
  std::printf(failures?"\nFAILED %d\n":"\nALL PASS\n",failures);
  return failures?1:0;
}
