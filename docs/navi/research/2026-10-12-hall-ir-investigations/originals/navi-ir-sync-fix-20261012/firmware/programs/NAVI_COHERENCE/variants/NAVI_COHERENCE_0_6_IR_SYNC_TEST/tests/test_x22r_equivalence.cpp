// X22R equivalence: the successor detector (X22 with the obsolete ~645 ms
// refractory removed) must behave exactly like the historical NAVI_ONE X22
// detector configured with refractoryMs=0, for every retained output:
// detections and their fields (opening polarity included), the 400 ms window
// record, the locked-baseline cycle and its outcomes, dwell/old-field handling,
// lost-lock recovery and the suppression counters.
//
// Deterministic software evidence only (AGENTS.md §8), not field evidence.
// Build from the repository root:
//   V=firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH
//   c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I $V
//       $V/tests/test_x22r_equivalence.cpp -o /tmp/x22r && /tmp/x22r
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../../../NAVI_ONE/variants/NAVI_ONE_X22/ExcursionDetector.h" // historical, unmodified
#include "ExcursionDetectorX22R.h"

// X22R must not carry the refractory as a dormant option.
template<class T,class=void> struct HasRefractory:std::false_type{};
template<class T> struct HasRefractory<T,decltype(void(std::declval<T>().refractoryMs))>:std::true_type{};
template<class T,class=void> struct HasGuardUntil:std::false_type{};
template<class T> struct HasGuardUntil<T,decltype(void(std::declval<T>().guardUntilMs))>:std::true_type{};
static_assert(!HasRefractory<ngr_hall::DetectorConfig>::value,"X22R has no refractoryMs");
static_assert(!HasGuardUntil<ngr_hall::Detection>::value,"X22R detections carry no guard deadline");
static_assert(HasRefractory<navi_one::DetectorConfig>::value,"historical X22 is untouched");

struct Rng{uint32_t s;uint32_t next(){s=s*1664525u+1013904223u;return s>>8;}int range(int lo,int hi){return lo+int(next()%uint32_t(hi-lo+1));}};

using Old=navi_one::ExcursionDetector<>;
using New=ngr_hall::ExcursionDetector<>;

static void sameExcursion(const navi_one::Excursion& a,const ngr_hall::Excursion& b){
  assert(a.detectedAtMs==b.detectedAtMs && a.windowEndMs==b.windowEndMs && a.rawAtDetect==b.rawAtDetect);
  assert(a.localRef==b.localRef && a.departAtDetect==b.departAtDetect && a.restRef==b.restRef);
  assert(a.reference==b.reference && a.shadowRef==b.shadowRef && a.baselineAgeMs==b.baselineAgeMs);
  assert(a.baselineStale==b.baselineStale && a.peakCounts==b.peakCounts && a.peakSigned==b.peakSigned);
  assert(a.polarity==b.polarity && a.openingDisagree==b.openingDisagree && a.excursionSum==b.excursionSum);
  assert(a.excursionCount==b.excursionCount && a.excursionFirst==b.excursionFirst && a.excursionLast==b.excursionLast);
  assert(a.widthCaliperMs==b.widthCaliperMs && a.widthFracMs==b.widthFracMs && a.sampleCount==b.sampleCount);
  assert(a.preSamples==b.preSamples && a.clipped==b.clipped && a.widthRejected==b.widthRejected);
  assert(a.stoppedShort==b.stoppedShort && a.measuredMs==b.measuredMs);
  assert((a.oriented==nullptr)==(b.oriented==nullptr) && (a.judged==nullptr)==(b.judged==nullptr));
  if(a.oriented)assert(!memcmp(a.oriented,b.oriented,sizeof(int16_t)*a.sampleCount));
  if(a.judged)assert(!memcmp(a.judged,b.judged,sizeof(int16_t)*a.sampleCount));
}

static void sameState(const Old& a,const New& b,uint32_t t){
  assert(a.baseline()==b.baseline() && a.shadowBaseline()==b.shadowBaseline() && a.ready()==b.ready());
  assert(a.acquiring()==b.acquiring() && a.collecting()==b.collecting() && a.baselineSetMs()==b.baselineSetMs());
  assert(a.baselineAgeMs(t)==b.baselineAgeMs(t) && a.baselineStale(t)==b.baselineStale(t));
  assert(a.baselineAccepted()==b.baselineAccepted() && a.baselineRejected()==b.baselineRejected());
  assert(a.baselineConsecutiveRejects()==b.baselineConsecutiveRejects());
  assert(a.lostLock()==b.lostLock() && a.lostLockTotal()==b.lostLockTotal());
  assert(a.suppressed()==b.suppressed() && a.rearmSuppressed()==b.rearmSuppressed());
  assert(a.armedElectrically()==b.armedElectrically() && a.dwellActive()==b.dwellActive());
  assert(a.oldFieldHold()==b.oldFieldHold() && a.dwellSuppressed()==b.dwellSuppressed());
  assert(a.holdSuppressed()==b.holdSuppressed() && a.widthRejects()==b.widthRejects());
}

struct Counts{unsigned samples=0,detections=0,windows=0,outcomes=0,accepted=0,dwells=0,lost=0;};

// One seeded trace: quiet intervals whose level drifts between magnets (so the
// baseline re-locks), N and S magnet passes of varied height/width including
// sub-threshold and two-lobed fields, station dwells (sometimes on a magnet),
// cruise-PWM stretches long enough for lost-lock, resets and baseline nudges.
// (Dwell exit exercises the detector's internal arm().)
static void run(uint32_t seed,int16_t depart,uint32_t lostMs,Counts& c){
  navi_one::DetectorConfig oc;oc.departCounts=depart;oc.refractoryMs=0;oc.lostMs=lostMs;
  ngr_hall::DetectorConfig nc;nc.departCounts=depart;nc.lostMs=lostMs;
  Old a(oc);New b(nc);Rng r{seed};
  uint32_t t=1; // t=0 is excluded: X22 maps a zero deadline to 1 ms (clock-wrap artefact)
  int level=2000;
  auto step=[&](int raw,bool moving,bool stopped,uint8_t pwm){
    raw=raw<0?0:raw>4095?4095:raw;
    const bool ra=a.sample(t,int16_t(raw),moving,stopped,pwm),rb=b.sample(t,int16_t(raw),moving,stopped,pwm);
    assert(ra==rb);
    if(ra){sameExcursion(a.excursion(),b.excursion());++c.windows;}
    navi_one::Detection da;ngr_hall::Detection db;
    const bool ta=a.takeDetection(da),tb=b.takeDetection(db);assert(ta==tb);
    if(ta){assert(da.detectedAtMs==db.detectedAtMs && da.rawAtDetect==db.rawAtDetect && da.localRef==db.localRef);
      assert(da.departAtDetect==db.departAtDetect && da.restRef==db.restRef && da.reference==db.reference);
      assert(da.shadowRef==db.shadowRef && da.baselineAgeMs==db.baselineAgeMs && da.baselineStale==db.baselineStale);
      assert(da.polarity==db.polarity);++c.detections;}
    navi_one::BaselineOutcome oa;ngr_hall::BaselineOutcome ob;
    const bool ba=a.takeBaselineOutcome(oa),bb=b.takeBaselineOutcome(ob);assert(ba==bb);
    if(ba){assert(oa.accepted==ob.accepted && int(oa.reject)==int(ob.reject) && oa.candidate==ob.candidate);
      assert(oa.previous==ob.previous && oa.delta==ob.delta && oa.spread==ob.spread && oa.atMs==ob.atMs);
      assert(oa.priorGapMs==ob.priorGapMs && oa.consecutiveRejects==ob.consecutiveRejects && oa.recovery==ob.recovery);
      ++c.outcomes;if(oa.accepted)++c.accepted;}
    assert(int(a.takeDwellEvent())==int(b.takeDwellEvent()));
    sameState(a,b,t);++t;++c.samples;
  };
  auto noise=[&]{return r.range(-6,6);};
  for(int i=0;i<2300;++i)step(level+noise(),false,false,0); // prime
  for(int seg=0;seg<220;++seg){
    const int kind=r.range(0,9);
    const uint8_t pwm=uint8_t(r.range(0,1)?90:r.range(30,120));
    if(kind<=5){ // quiet interval, then a magnet pass
      const int quiet=r.range(250,1400);
      for(int i=0;i<quiet;++i)step(level+noise(),true,false,pwm);
      const int sign=r.range(0,1)?1:-1,height=r.range(40,220),width=r.range(15,140);
      const bool twoLobes=r.range(0,4)==0;
      for(int i=0;i<width;++i){int shape=height*(i<width/2?i:width-i)/(width/2>0?width/2:1);
        if(twoLobes && i>width/2)shape=-shape/2;
        step(level+sign*shape+noise(),true,false,pwm);}
      if(r.range(0,2)==0)level+=r.range(-20,20); // the next interval's ordinary-track level differs
    } else if(kind==6){ // station dwell, sometimes standing in a field
      const int off=r.range(0,1)?0:r.range(80,160)*(r.range(0,1)?1:-1);
      for(int i=0,n=r.range(800,4000);i<n;++i)step(level+off+noise(),false,true,0);
      ++c.dwells;
    } else if(kind==7){ // long elevated shelf at cruise: exercises lost-lock when enabled
      const int off=r.range(80,150)*(r.range(0,1)?1:-1);
      for(int i=0,n=r.range(500,2600);i<n;++i)step(level+off+noise(),true,false,110);
      if(a.lostLock())++c.lost;
    } else if(kind==8){a.reset();b.reset();step(level+noise(),true,false,pwm);}
    else {const int8_t d=int8_t(r.range(-3,3));assert(a.adjustBaseline(d)==b.adjustBaseline(d));step(level+noise(),true,false,pwm);}
  }
}

int main(){
  Counts c;unsigned traces=0;
  for(uint32_t seed=1;seed<=24;++seed){
    run(seed,70,0,c);++traces;       // the candidate's configuration
    run(seed,70,2000,c);++traces;    // lost-lock recovery enabled (X22 default)
    run(seed,38,0,c);++traces;       // the superseded 38-count threshold, for breadth
  }
  assert(c.detections>1000 && c.windows>1000 && c.accepted>100 && c.dwells>100);
  std::printf("PASS X22R == X22(refractory=0): %u traces, %u samples, %u detections, %u windows, "
              "%u baseline outcomes (%u locks accepted), %u dwells, %u lost-lock segments\n",
              traces,c.samples,c.detections,c.windows,c.outcomes,c.accepted,c.dwells,c.lost);
}
