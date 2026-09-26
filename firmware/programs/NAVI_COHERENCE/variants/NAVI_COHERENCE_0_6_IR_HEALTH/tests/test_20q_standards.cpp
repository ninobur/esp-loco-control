// 20Q3 deterministic checks: the three previously decided Twenty Questions
// standards and the removed anachronisms. These prove the software implements
// the defined rules; they are not behavioral evidence (AGENTS.md §8).
//
// Build and run from the repository root (see the variant's CHANGES_20Q3.md):
//   V=firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH
//   c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I $V
//       $V/tests/test_20q_standards.cpp -o /tmp/test_20q && /tmp/test_20q $V
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include "Navigator.h"
#include "HallObserver.h"
using namespace navi_one;

// IR at 1 um per pulse, so the inclusive window boundaries are exact.
static ngr_nav::IrOdometryEpoch owner;
static ngr_nav::IrOdometryPoint pt(uint64_t um,uint64_t ms,uint64_t epoch=1){return {&owner,epoch,1,ms*1000,um,0,1};}
static NavObservation hall(uint32_t ms,uint8_t pole,ngr_nav::IrOdometryPoint p={}){NavObservation o;o.openedAtMs=ms;o.polarity=pole;o.odometry=p;return o;}

// Declared one interval back, then the first MM accepted Hall-only with an IR
// point: that strike is the MM/IR synchronization. Returns the synchronized MM.
static uint8_t sync(Navigator& n,uint8_t start,int8_t dir){
  n.declare(start,dir,1000);const uint8_t m=nextMarker(start,dir);
  assert(n.judge(hall(2000,polarityAt(m),pt(0,2000)))==Ruling::Advanced);
  assert(n.haveReference() && n.referenceMm()==m && n.status().navMm==m);
  return m;
}

static std::string readFile(const std::string& path){std::ifstream f(path);std::stringstream s;s<<f.rdbuf();return s.str();}

static unsigned windowBoundaries(){
  unsigned cases=0;
  for(int dir=-1;dir<=1;dir+=2)for(unsigned start=0;start<ROUTE_N;++start){
    Navigator base;const uint8_t m1=sync(base,uint8_t(start),int8_t(dir));
    const uint8_t m2=nextMarker(m1,dir),m3=nextMarker(m2,dir);
    const uint64_t D=spanMm(m1,dir);
    // 300 ms after the synchronized strike: valid IR is never vetoed by time.
    auto at=[&](uint64_t um){return hall(2300,polarityAt(m2),pt(um,2300));};
    {Navigator n=base;assert(n.judge(at(D*850-1))==Ruling::NonLandmark);                // just before 0.85D
     assert(n.status().navMm==m1 && n.status().target==m2);SansMmAdvance s;assert(!n.takeSansAdvance(s));}
    {Navigator n=base;assert(n.judge(at(D*850))==Ruling::Advanced && n.status().navMm==m2);}   // exactly 0.85D
    {Navigator n=base;assert(n.judge(at(D*1000))==Ruling::Advanced && n.status().navMm==m2);}  // inside
    {Navigator n=base;assert(n.judge(at(D*1150))==Ruling::Advanced && n.status().navMm==m2);   // exactly 1.15D
     SansMmAdvance s;assert(!n.takeSansAdvance(s));}
    {Navigator n=base;const auto before=n.recovery().count();                                  // just beyond 1.15D
     assert(n.judge(at(D*1150+1))==Ruling::NonLandmark);
     assert(n.status().navMm==m2 && n.status().target==m3 && n.status().sansMmConsecutive==1);
     SansMmAdvance s;assert(n.takeSansAdvance(s) && s.mm==m2 && s.referenceMm==m1 && s.consecutive==1);
     assert(n.recovery().count()==before+1 && !n.recovery().entry(n.recovery().count()-1).observed);}
    // An early extra Hall event followed by the real expected MM.
    {Navigator n=base;const auto before=n.recovery().count();
     assert(n.judge(hall(2100,polarityAt(m2),pt(D*300,2100)))==Ruling::NonLandmark && n.status().target==m2);
     assert(n.judge(hall(2900,polarityAt(m2),pt(D*1000,2900)))==Ruling::Advanced && n.status().navMm==m2);
     assert(n.recovery().count()==before+1 && n.recovery().entry(n.recovery().count()-1).observed);}
    // Opening polarity is the only Hall polarity with NAV authority.
    {Navigator n=base;assert(n.judge(hall(2300,!polarityAt(m2),pt(D*1000,2300)))==Ruling::AdvancedWithDiscrepancy);
     assert(n.status().navMm==m2 && n.status().evidence==EvidenceClass::PolarityDiscrepancy);}
    cases+=7;
  }
  return cases;
}

static unsigned hallOnlyFallback(){
  unsigned cases=0;
  for(int dir=-1;dir<=1;dir+=2){
    const uint8_t start=dir>0?170:0; // wraparound in both directions
    // No MM reference yet (after declaration at 1000 ms): 649 refuses, 650 accepts.
    {Navigator n;n.declare(start,int8_t(dir),1000);const uint8_t m=nextMarker(start,dir);
     assert(n.judge(hall(1649,polarityAt(m)))==Ruling::NonLandmark);
     assert(n.judge(hall(1650,polarityAt(m)))==Ruling::Advanced && n.status().navMm==m);
     assert(n.status().distance==DistanceBasis::NoReference);}
    Navigator base;const uint8_t m1=sync(base,start,int8_t(dir));const uint8_t m2=nextMarker(m1,dir);
    // Reference exists but no IR point at the Hall event: 650 ms from the previous accepted detection.
    {Navigator n=base;assert(n.judge(hall(2649,polarityAt(m2)))==Ruling::NonLandmark && n.status().distance==DistanceBasis::NoIrPoint);
     assert(n.judge(hall(2650,polarityAt(m2)))==Ruling::Advanced && n.status().navMm==m2);}
    // Epoch break: the new epoch's point cannot be measured from the old reference.
    {Navigator n=base;const uint64_t far=uint64_t(spanMm(m1,dir))*5000; // would be many windows in epoch 1
     assert(n.judge(hall(2649,polarityAt(m2),pt(far,2649,2)))==Ruling::NonLandmark);
     assert(n.status().distance==DistanceBasis::EpochBreak && !n.haveReference() && n.status().navMm==m1);
     SansMmAdvance s;assert(!n.takeSansAdvance(s)); // no distance manufactured across the break
     assert(n.judge(hall(2650,polarityAt(m2),pt(far,2650,2)))==Ruling::Advanced);
     // Immediate new synchronization in the new epoch; no probation.
     assert(n.haveReference() && n.referenceMm()==m2);
     const uint8_t m3=nextMarker(m2,dir);
     assert(n.judge(hall(2700,polarityAt(m3),pt(far+uint64_t(spanMm(m2,dir))*1000,2700,2)))==Ruling::Advanced);
     assert(n.status().navMm==m3 && n.status().distance==DistanceBasis::Epoch && n.status().distanceConfirmed);}
    cases+=3;
  }
  return cases;
}

static unsigned traversalAndLimit(){
  static_assert(SANS_MM_AUTO_LIMIT==10,"operator ruling 2026-09-25: ten consecutive MMs sans landmark");
  unsigned cases=0;
  for(int dir=-1;dir<=1;dir+=2)for(unsigned start=0;start<ROUTE_N;start+=17){
    Navigator n;const uint8_t m1=sync(n,uint8_t(start),int8_t(dir));
    uint64_t cum=0;uint8_t expect=m1;const auto history=n.recovery().count();
    for(unsigned k=1;k<=SANS_MM_AUTO_LIMIT;++k){
      cum+=spanMm(expect,dir);expect=nextMarker(expect,dir);
      assert(!n.traverse(pt(cum*1150,3000+k)));                // exactly 1.15D: not yet traversed
      assert(n.traverse(pt(cum*1150+1,3000+k))==1);            // complete window traversed, no Hall
      assert(n.status().navMm==expect && n.status().sansMmConsecutive==k && n.positionKnown());
      SansMmAdvance s;assert(n.takeSansAdvance(s) && s.mm==expect && s.referenceMm==m1 && s.consecutive==k);
      assert(!n.recovery().entry(n.recovery().count()-1).observed);
      assert(n.autoLandmarkLimitReached()==(k>=SANS_MM_AUTO_LIMIT));
    }
    // Limit: AUTO authority only. Not LOST; position, history and IR reference retained.
    assert(n.status().state==NavState::Tracking && n.haveReference() && n.referenceMm()==m1);
    assert(n.recovery().count()>=history);
    // An accepted Hall landmark (cumulative window from the same synchronization) resets the count.
    const uint8_t next=nextMarker(expect,dir);cum+=spanMm(expect,dir);
    assert(n.judge(hall(9000,polarityAt(next),pt(cum*1000,9000)))==Ruling::Advanced);
    assert(n.status().navMm==next && !n.status().sansMmConsecutive && !n.autoLandmarkLimitReached());
    assert(n.referenceMm()==next);
    // The new synchronization restarts cumulative distance: the next window is one span.
    {Navigator m=n;const uint8_t after=nextMarker(next,dir);
     assert(m.judge(hall(9100,polarityAt(after),pt(cum*1000+uint64_t(spanMm(next,dir))*1000,9100)))==Ruling::Advanced);
     assert(m.status().navMm==after);}
    // An ended epoch never reopens: a later-epoch point drops the reference,
    // and an old-epoch point afterwards produces no distance.
    assert(!n.traverse(pt(uint64_t(1)<<40,9100,2)) && !n.haveReference());
    assert(!n.traverse(pt(cum*1000+(uint64_t(1)<<30),9200,1)) && n.status().navMm==next);
    cases+=SANS_MM_AUTO_LIMIT+1;
  }
  return cases;
}

static unsigned detector(const std::string& variant){
  // The candidate configures the X22 detector with 70 counts.
  assert(readFile(variant+"/LocoConfig.h").find("#define NAVI_HALL_DEPART_COUNTS 70")!=std::string::npos);
  assert(readFile(variant+"/NAVI_COHERENCE_0_6_IR_HEALTH.ino").find("hallConfig(NAVI_HALL_DEPART_COUNTS)")!=std::string::npos);
  const auto cfg=ngr_nav::hallConfig(70);
  assert(cfg.departCounts==70 && cfg.persistSamples==2); // X22R: no refractory exists (see test_x22r_equivalence.cpp)
  ngr_hall::ExcursionDetector<> d(cfg);
  uint32_t t=0;const int16_t base=2000;ngr_hall::Detection det;
  auto feed=[&](int16_t raw){++t;d.sample(t,raw,false,false,0);};
  for(int i=0;i<2200;++i)feed(base);
  assert(d.ready() && d.baseline()==base);
  auto quiet=[&](int n){for(int i=0;i<n;++i)feed(base);};
  for(int i=0;i<5;++i)feed(base+69);
  assert(!d.takeDetection(det));                                      // 69: never qualifies
  quiet(5);
  feed(base+70);assert(!d.takeDetection(det));                        // first qualifying sample
  quiet(5);assert(!d.takeDetection(det));                             // a single sample is not an opening
  feed(base+70);assert(!d.takeDetection(det));
  feed(base+70);const uint32_t second=t;                              // second consecutive qualifying sample
  assert(d.takeDetection(det) && det.detectedAtMs==second && det.polarity==1 && det.departAtDetect==70);
  quiet(450);                                                         // acquisition window is telemetry
  // No refractory: a new opening ~460 ms after the previous detection is
  // declared: the detector has no refractory (X22's superseded 645 ms guard would have suppressed it).
  feed(base-70);assert(!d.takeDetection(det));
  feed(base-70);assert(d.takeDetection(det) && det.polarity==0 && det.detectedAtMs==t && t-second<645);
  quiet(450);
  feed(base+70);feed(base-70);assert(!d.takeDetection(det));          // opposite signs are not one excursion
  return 6;
}

int main(int argc,char** argv){
  const std::string variant=argc>1?argv[1]:".";
  const unsigned w=windowBoundaries(),h=hallOnlyFallback(),t=traversalAndLimit(),d=detector(variant);
  std::printf("PASS 20Q3 standards: %u window cases (all MMs, both directions, wrap), %u Hall-only/epoch cases, "
              "%u traversal/limit cases, %u detector cases\n",w,h,t,d);
}
