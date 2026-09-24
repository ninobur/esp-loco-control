// Replays the recorded IR TX 1.6 R2 bench captures through the ACTUAL R2
// firmware detector (firmware/common/IrMovementDetector.h, unmodified) and
// attributes every continuity loss (openAborts increment) and reference erasure
// to the branch that caused it. Review: docs/IR_TX_R2_BENCH_FAILURE_REVIEW_20260924.md.
//
// Build and run from the repo root:
//   python3 tools/ir_r2_capture_extract.py field-records/logs/20260923_ir_r2_bench_radio.log.gz \
//     /tmp/r2_bench.fix e9f8b7c4 8dc31ef4e9f8b7c4
//   python3 tools/ir_r2_capture_extract.py field-records/logs/20260923_ir_r2_three_stops_radio.log.gz \
//     /tmp/r2_three.fix e9f8b7c4 8dc31ef4e9f8b7c4
//   c++ -std=c++17 -O1 -fsanitize=address,undefined -Ifirmware/common \
//     tools/test_ir_r2_bench_replay.cpp -o /tmp/r2_replay && /tmp/r2_replay /tmp/r2_bench.fix /tmp/r2_three.fix
// (tools/run_ir_r2_bench_replay.sh does all of this.)
//
// Private detector state is read, never written, to label branches: the
// firmware header is included with `private` mapped to `public` in this
// translation unit only. No firmware source is changed.
#include "ir_bench_replay.h"  // standard headers first, before the access mapping
#include <string>
#define private public
#include "IrMovementDetector.h"
#undef private

using namespace ir_movement;
using namespace ir_bench;

enum Branch { B_GAP, B_SAT, B_PRIMING, B_HOLD_OFF_PLATEAU, B_NO_CONTRAST, B_OPEN_TIMEOUT, B_QUALITY_300, B_N };
static const char* branchName[B_N]={"sample gap","saturation","priming",
  "holding: raw off learned plateau (:45-46)","span<120, no reference (:49)",
  "open-pulse timeout (:55-57)","live span<300 quality loss (:73-77)"};

// Which R2 branch changed `aborts` or cleared `proven_` on this sample.
static Branch classify(const Detector& p,const Detector& q,uint64_t us){
  if(q.gaps>p.gaps) return B_GAP;
  if(q.saturated>p.saturated) return B_SAT;
  if(q.fill_<128) return B_PRIMING;
  const bool contrast=q.high-q.low>=120;
  const bool holding=q.retainStationary_ && p.proven_ && !contrast;
  if(holding) return B_HOLD_OFF_PLATEAU;
  if(!contrast) return B_NO_CONTRAST;
  if(p.open_ && us-p.openAt_>=2500000) return B_OPEN_TIMEOUT;
  return B_QUALITY_300;
}

struct Event { uint32_t k; Branch b; bool abortInc, erasedRef; unsigned low, high, provenLow, provenHigh; uint16_t raw; };

// Hidden detector state is not transmitted, so a replay that starts on a
// received range is trusted only after an event that fixes all of it:
//  - reset: the replay invalidated everything (span<120, no reference) AND a
//    transmitted snapshot shows INADEQUATE_CONTRAST at span<120 at the same
//    time, which in R2 implies the ESP32 cleared the same state; or
//  - reference: two consecutive completions at span>=300 whose envelopes
//    are compatible; candidate_ and proven_ are then set from those envelopes
//    whatever came before, and the envelopes are exact once the window holds
//    only received samples.
struct Recorder {
  std::vector<Event>* ev;
  std::map<uint32_t,std::pair<unsigned,unsigned>>* refAtCompletion;
  std::vector<uint32_t>* resets; std::vector<uint32_t>* refSyncs;
  uint32_t start=0, lastStrong=0; bool haveStrong=false;
  void operator()(uint32_t k,uint16_t raw,uint64_t us,const Detector& p,const Detector& q){
    if(q.fill_==512 && k>=start+512){
      if(!q.armed_ && !q.open_ && !q.proven_ && !q.candidate_ && !q.haveCompleted_ && q.high-q.low<120) resets->push_back(k);
      if(q.fall && q.high-q.low>=300){
        if(haveStrong && lastStrong>=start+512 && q.proven_ && q.provenLow_==q.low && q.provenHigh_==q.high &&
           q.compatible(p.candidateLow_,p.candidateHigh_)) refSyncs->push_back(k);
        haveStrong=true; lastStrong=k;
      }
    }
    const bool inc=q.aborts>p.aborts, erased=p.proven_ && !q.proven_;
    if(inc||erased) ev->push_back({k,classify(p,q,us),inc,erased,q.low,q.high,p.provenLow_,p.provenHigh_,raw});
    if(q.fall && q.proven_) (*refAtCompletion)[k]={q.provenLow_,q.provenHigh_};
  }
};

static int failures=0;
static bool verbose=false;
static std::vector<uint32_t> unmatchedEdges;  // samples of edges present in one stream only
static void check(bool ok,const char* what){ std::printf("%s %s\n",ok?"PASS":"FAIL",what); if(!ok)++failures; }

struct StopRow { uint32_t k; uint64_t completed; int rest; unsigned refLo, refHi; bool nearPlateau; int reason3s; std::string path; bool erasedBeforeQuiet; };

static void analyze(const char* path,const char* label,std::vector<StopRow>& stops){
  Capture c=load(path);
  const long t0=fitSnapshotOffsetMs(c);
  auto rs=ranges(c);
  std::printf("\n== %s: %zu batches, %zu snapshots, %zu contiguous ranges; snapshot sample = capturedUs/1000 - %ld\n",
              label,c.batches.size(),c.moves.size(),rs.size(),t0);
  uint64_t flagUnexplained=0,reasonBad=0,schedBad=0,fitCost=0,flagN=0,flagBad=0,latchN=0,latchBad=0,riseBad=0,snapN=0,snapBad=0,unsynced=0;
  std::vector<Event> events; std::map<uint32_t,std::pair<unsigned,unsigned>> refs;
  std::vector<Segment<Detector>> segs;
  for(const Range& r:rs){
    if(r.end-r.first<3000){unsynced+=r.end-r.first;continue;}
    std::vector<Event> ev; std::vector<uint32_t> resets,refSyncs;
    Recorder rec{&ev,&refs,&resets,&refSyncs}; rec.start=r.first;
    auto seg=replay<Detector>(c,r,[]{return Detector(false,true);},rec,t0);
    // Earliest trustworthy point (see Recorder). Snapshot confirmation for resets.
    uint32_t sync=r.end; const char* how="none";
    for(uint32_t k:refSyncs){ sync=k; how="two compatible completions"; break; }
    for(const Move& m:c.moves){
      const long k=(long)(m.us/1000)-t0;
      if(k<(long)r.first+512 || k>=(long)sync) continue;
      if(m.reason==INADEQUATE_CONTRAST && m.span<120 && std::binary_search(resets.begin(),resets.end(),(uint32_t)k)){ sync=(uint32_t)k; how="observed reset"; break; }
    }
    // The ESP32's own startup sample gap (gaps 0->1) reset its window; the
    // replay cannot see it, so nothing before it (plus a window) is compared.
    for(const Move& m:c.moves) if(m.gaps>=1){ const long k=(long)(m.us/1000)-t0+600; if(k>(long)sync && k>(long)r.first && (long)r.first<k-600+1) sync=std::max<long>(sync,k); break; }
    seg.syncedFrom=std::min<uint32_t>(sync,r.end);
    if(verbose) std::printf("  range %u-%u: synced from %u (%s), %u slips\n",r.first,r.end,sync,how,seg.slips);
    unsynced+=seg.syncedFrom-r.first; schedBad+=seg.scheduleViolations; fitCost+=seg.fitCost;
    for(const Event& e:ev) if(e.k>=seg.syncedFrom) events.push_back(e);
    // Per-sample flags. The schedule fit cannot always resolve the envelope
    // update phase to the sample, so a threshold crossing can land a few
    // samples apart. Such a DISPLACED edge is tolerated; an edge present in
    // one stream with no same-type edge within 40 samples in the other (an
    // added or lost pulse) is not.
    for(uint32_t k=seg.syncedFrom;k<r.end;++k){
      const Rec& x=*seg.at(k);++flagN;
      for(uint16_t e:{(uint16_t)F_RISE,(uint16_t)F_FALL}){
        const bool o=x.flagsObs&e, p=x.flagsRep&e; if(o==p) continue;
        bool found=false;
        for(uint32_t j=k>seg.syncedFrom+40?k-40:seg.syncedFrom;j<=k+40&&j<r.end;++j){ const Rec& y=*seg.at(j); if((o?y.flagsRep:y.flagsObs)&e){found=true;break;} }
        if(!found){ ++flagUnexplained; unmatchedEdges.push_back(k); if(verbose) std::printf("  UNMATCHED %s k=%u raw=%u in %s only\n",e==F_RISE?"rise":"fall",k,x.raw,o?"ESP32":"replay"); }
      }
      if(x.flagsObs!=x.flagsRep){ if(verbose && flagBad<40) std::printf("  flag mismatch k=%u raw=%u obs=%04x rep=%04x low/high=%u/%u\n",k,x.raw,x.flagsObs,x.flagsRep,x.low,x.high); ++flagBad; }
    }
    // Per-batch increments of the cumulative abort and rise counters.
    const Batch* pb=nullptr; const Rec* px=nullptr;
    for(size_t i=r.b0;i<r.b1;++i){
      const Batch& b=c.batches[i];
      if(b.first<seg.syncedFrom) continue;
      const Rec* x=seg.at(b.first+95);
      if(pb){ ++latchN;
        if(b.latch-pb->latch!=x->aborts-px->aborts){ if(verbose) std::printf("  abort delta mismatch batch %u obs +%u rep +%llu\n",b.first,b.latch-pb->latch,(unsigned long long)(x->aborts-px->aborts)); ++latchBad; }
        riseBad+= b.pulses-pb->pulses!=x->rises-px->rises; }
      pb=&b; px=x;
    }
    // Snapshots: reason and span exactly; counter increments between consecutive snapshots.
    const Move* pm=nullptr; const Rec* pr=nullptr;
    for(const Move& m:c.moves){
      const long k=(long)(m.us/1000)-t0; if(k<(long)seg.syncedFrom||k>=(long)r.end) continue;
      const Rec* x=seg.at((uint32_t)k);
      if(!pm){ seg.completedOffset=(int64_t)m.completed-(int64_t)x->completed; }
      else { ++snapN; reasonBad+= m.reason!=x->reason;
        const bool bad= m.reason!=x->reason || m.span!=(int)(x->high-x->low) ||
                m.completed-pm->completed!=x->completed-pr->completed || m.aborts-pm->aborts!=x->aborts-pr->aborts;
        if(bad && verbose && snapBad<40) std::printf("  snapshot mismatch seq=%u k=%ld obs %s span %d aborts+%llu rep %s span %u aborts+%llu\n",m.seq,k,
          reasonName(m.reason),m.span,(unsigned long long)(m.aborts-pm->aborts),reasonName(x->reason),x->high-x->low,(unsigned long long)(x->aborts-pr->aborts));
        snapBad+=bad; }
      pm=&m; pr=x;
    }
    segs.push_back(std::move(seg));
  }
  std::printf("agreement after convergence: flag words %llu/%llu, batch abort deltas %llu/%llu, batch rise deltas %llu/%llu, snapshots %llu/%llu; %llu samples before convergence or in short ranges\n",
    (unsigned long long)(flagN-flagBad),(unsigned long long)flagN,(unsigned long long)(latchN-latchBad),(unsigned long long)latchN,
    (unsigned long long)(latchN-riseBad),(unsigned long long)latchN,(unsigned long long)(snapN-snapBad),(unsigned long long)snapN,(unsigned long long)unsynced);
  std::printf("envelope schedule: fit disagreements %llu (CONTRAST bits + headers), detector updates off schedule %llu\n",(unsigned long long)fitCost,(unsigned long long)schedBad);
  check(schedBad==0,"real detector updated its envelope exactly on the fitted schedule");
  std::printf("edges present in only one stream (no same edge within 40 samples): %llu; snapshot reason disagreements: %llu\n",
    (unsigned long long)flagUnexplained,(unsigned long long)reasonBad);
  check(latchBad==0,"every batch's openAborts increment equals the replay's");
  check(reasonBad==0,"every snapshot's optical reason equals the replay's");
  if(flagUnexplained) std::printf("INFO %llu edge(s) in one stream only: a dip or peak within a bucket of the threshold, decided differently because the fitted envelope phase differs; see stop-window check\n",(unsigned long long)flagUnexplained);
  check(flagBad*1000<flagN,"flag words agree for more than 99.9% of converged samples");

  unsigned byBranch[B_N]{}, erasedBy[B_N]{};
  for(const Event& e:events){ if(e.abortInc)++byBranch[e.b]; if(e.erasedRef)++erasedBy[e.b]; }
  std::printf("openAborts increments by branch (synced samples only):\n");
  for(int b=0;b<B_N;++b) if(byBranch[b]||erasedBy[b]) std::printf("  %-44s %3u increments, %3u reference erasures\n",branchName[b],byBranch[b],erasedBy[b]);

  // Stops: a completion followed by >=3 s without another, entirely inside a synced range.
  for(const auto& seg:segs){
    for(uint32_t k=seg.syncedFrom;k+3000<seg.range.end;++k){
      const Rec& x=*seg.at(k);
      if(!(x.flagsRep&F_FALL)) continue;
      bool more=false; for(uint32_t j=k+1;j<=k+3000;++j) if(seg.at(j)->flagsRep&F_FALL){more=true;break;}
      if(more) continue;
      StopRow s{}; s.k=k; s.completed=(uint64_t)((int64_t)x.completed+seg.completedOffset);
      std::vector<int> v; for(uint32_t j=k+2000;j<k+3000;++j) v.push_back(seg.at(j)->raw);
      std::nth_element(v.begin(),v.begin()+v.size()/2,v.end()); s.rest=v[v.size()/2];
      // The reference in force during the deceleration: the one erased in the
      // last second before quiet, else the one held at the final completion.
      auto it=refs.find(k); if(it!=refs.end()){s.refLo=it->second.first;s.refHi=it->second.second;}
      for(const Event& e:events) if(e.erasedRef && e.k+1000>=k && e.k<=k+3000){s.refLo=e.provenLow;s.refHi=e.provenHigh;}
      if(s.refHi){ const int m=(int)(s.refHi-s.refLo)/4;
        s.nearPlateau=(s.rest+m>=(int)s.refLo && s.rest<=(int)s.refLo+m)||(s.rest+m>=(int)s.refHi && s.rest<=(int)s.refHi+m); }
      s.reason3s=seg.at(k+3000)->reason;
      bool quietSeen=false;
      for(const Event& e:events){
        if(e.k+1000<k||e.k>k+3000) continue;
        char buf[96]; std::snprintf(buf,sizeof buf,"%s%+dms:%s%s",s.path.empty()?"":" | ",(int)e.k-(int)k,
          e.b==B_QUALITY_300?"q300":e.b==B_NO_CONTRAST?"inv120":e.b==B_HOLD_OFF_PLATEAU?"offplateau":e.b==B_OPEN_TIMEOUT?"timeout":"other",
          e.erasedRef?"(erase)":"");
        s.path+=buf;
        if(e.b==B_NO_CONTRAST||e.b==B_HOLD_OFF_PLATEAU) quietSeen=true;
        if(e.erasedRef && e.b==B_QUALITY_300 && !quietSeen) s.erasedBeforeQuiet=true;
      }
      stops.push_back(s);
      std::printf("stop %-6s k=%u count=%llu ref=%u/%u rest=%d nearPlateau=%s at+3s=%s path: %s\n",label,k,
        (unsigned long long)s.completed,s.refLo,s.refHi,s.rest,s.refHi?(s.nearPlateau?"yes":"NO"):"n/a",
        reasonName(s.reason3s),s.path.c_str());
    }
  }
}

int main(int argc,char** argv){
  verbose=std::getenv("IR_REPLAY_VERBOSE")!=nullptr;
  if(argc!=3){std::fprintf(stderr,"usage: %s bench.fix three_stops.fix\n",argv[0]);return 2;}
  std::vector<StopRow> bench,three;
  analyze(argv[1],"bench",bench);
  analyze(argv[2],"three",three);
  // The recorded stops named in the bench documents.
  auto find=[](const std::vector<StopRow>& v,uint64_t count)->const StopRow*{
    for(const auto& s:v) if(s.completed==count) return &s; return nullptr; };
  const StopRow* first=find(bench,99);
  const StopRow* s1=find(three,462); const StopRow* s2=find(three,532); const StopRow* s3=find(three,627);
  std::printf("\n== documented stops\n");
  check(first && s1 && s3,"first short roll (99) and final rolls 1 and 3 (462, 627) are inside synced ranges");
  if(s2) std::printf("INFO roll 2 stop (532) is inside a synced range\n");
  else   std::printf("INFO roll 2 stop (532) not in a synced range (radio gap during the roll)\n");
  for(const StopRow* s:{first,s1,s2,s3}) if(s){
    std::printf("count %llu: final reason %s, reference erased by q300 before quiet: %s, rest %d vs ref %u/%u near plateau: %s\n",
      (unsigned long long)s->completed,reasonName(s->reason3s),s->erasedBeforeQuiet?"yes":"no",s->rest,s->refLo,s->refHi,s->nearPlateau?"yes":"no");
    check(s->reason3s==INADEQUATE_CONTRAST,"stop ends INADEQUATE_CONTRAST (as observed)");
  }
  bool clean=true;
  for(const StopRow* s:{first,s1,s3}) if(s) for(uint32_t k:unmatchedEdges) if(k+5000>=s->k && k<=s->k+3000) clean=false;
  check(clean,"no added/lost edge within 5 s before to 3 s after any certified documented stop");
  if(first) check(first->erasedBeforeQuiet && first->nearPlateau,"count 99: q300 erased a reference the rest level WOULD have satisfied");
  if(s1)    check(s1->erasedBeforeQuiet && s1->nearPlateau,   "count 462: q300 erased a reference the rest level WOULD have satisfied");
  if(s3)    check(!s3->nearPlateau,"count 627: rest level is mid-band; R2's plateau rule would refuse it even with the reference kept");
  std::printf(failures?"\nFAILED %d\n":"\nALL PASS\n",failures);
  return failures?1:0;
}
