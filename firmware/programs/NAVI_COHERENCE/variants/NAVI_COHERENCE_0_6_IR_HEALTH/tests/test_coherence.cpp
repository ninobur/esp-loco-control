#include <cassert>
#include <iostream>
#include "Navigator.h"
using namespace navi_one;
static ngr_nav::MotionPoint mp(uint64_t pulses,uint64_t us){ngr_nav::MotionPoint p{};p.issue=ngr_nav::MotionIssue::None;p.frame=1;p.wire.bootId=1;p.wire.capturedUs=us;p.wire.completedPulses=pulses;p.wire.observedRises=pulses;p.wire.pitchUm=9652;p.wire.opticalReason=ir_movement::TRACKING;return p;}
int main(){
 Navigator n;
 // Operator declaration is authoritative, but a Hall lobe 100 ms later is still too early.
 n.declare(55,1,1000);NavObservation early{};early.openedAtMs=1100;early.polarity=polarityAt(56);early.movement.issue=ngr_nav::MotionIssue::NoSource;assert(n.judge(early)==Ruling::NonLandmark);assert(n.status().navMm==55);
 // First plausible Hall point establishes MM56 and the IR anchor.
 NavObservation a{};a.openedAtMs=1700;a.polarity=polarityAt(56);a.movement=mp(100,1700000);assert(n.judge(a)==Ruling::Advanced);assert(n.status().navMm==56);
 // Spurious field/lobe: valid IR says only ~48 mm; cannot be MM57 even with right polarity.
 NavObservation b{};b.openedAtMs=2400;b.polarity=polarityAt(57);b.movement=mp(105,2400000);assert(n.judge(b)==Ruling::NonLandmark);assert(n.status().navMm==56);
 // True MM57 near mapped distance, deliberately wrong polarity: continuity wins.
 uint64_t p1=(spanMm(56,1)+5)/10;NavObservation c{};c.openedAtMs=3200;c.polarity=!polarityAt(57);c.movement=mp(100+p1,3200000);assert(n.judge(c)==Ruling::AdvancedWithDiscrepancy);assert(n.status().navMm==57);
 // Stopped/repeated field: no wheel movement, cannot advance.
 NavObservation d{};d.openedAtMs=4000;d.polarity=polarityAt(58);d.movement=c.movement;d.movement.wire.capturedUs=4000000;assert(n.judge(d)==Ruling::NonLandmark);assert(n.status().navMm==57);
 // Deliberately miss MM58; a Hall event near cumulative MM57->59 accepts 59 and records one missed.
 uint32_t two=spanMm(57,1)+spanMm(58,1);uint64_t p2=(two+5)/10;NavObservation e{};e.openedAtMs=5500;e.polarity=polarityAt(59);e.movement=mp((100+p1)+p2,5500000);assert(n.judge(e)==Ruling::MissedAndAdvanced);assert(n.status().navMm==59);assert(n.status().missedSinceLast==1);assert(n.status().evidence==EvidenceClass::MissedObservation);
 // Missed-and-advance must not drop the final Hall polarity discrepancy.
 Navigator n2;n2.declare(55,1,1000);NavObservation f{};f.openedAtMs=1700;f.polarity=polarityAt(56);f.movement=mp(200,1700000);assert(n2.judge(f)==Ruling::Advanced);uint32_t twob=spanMm(56,1)+spanMm(57,1);uint64_t pb=(twob+5)/10;NavObservation g{};g.openedAtMs=3200;g.polarity=!polarityAt(58);g.movement=mp(200+pb,3200000);assert(n2.judge(g)==Ruling::MissedAndAdvanced);assert(n2.status().navMm==58);assert(n2.status().evidence==EvidenceClass::MissedWithPolarityDiscrepancy);
 // Reversal preserves location authority and expects the point just passed.
 // Recovery retains observations but cannot reuse unsigned travel across reversal.
 const auto historyBefore=n.recovery().count();
 n.setDirection(-1,6000);assert(n.positionKnown());assert(n.status().target==59);assert(n.recovery().count()==historyBefore);
 // ---- 0.5: IR window is +/-15% --------------------------------------------
 {Navigator w;w.declare(100,1,0);NavObservation a0{};a0.openedAtMs=1000;a0.polarity=polarityAt(101);a0.movement=mp(1000,1000000);assert(w.judge(a0)==Ruling::Advanced);
  const double span=spanMm(101,1);
  auto at=[&](double frac,uint32_t ms){NavObservation o{};o.openedAtMs=ms;o.polarity=polarityAt(102);o.movement=mp(1000+uint64_t(span*frac/9.652+0.5),uint64_t(ms)*1000);return o;};
  Navigator w2=w;assert(w2.judge(at(1.14,3000))==Ruling::Advanced);   // refused by 0.4, accepted now
  Navigator w3=w;assert(w3.judge(at(0.86,3000))==Ruling::Advanced);
  Navigator w4=w;assert(w4.judge(at(1.17,3000))==Ruling::NonLandmark);
  Navigator w5=w;assert(w5.judge(at(0.82,3000))==Ruling::NonLandmark);}
 // ---- 0.5: direction before declaration is not a position ------------------
 {Navigator u;u.setDirection(1,0);assert(!u.positionKnown());assert(u.status().state==NavState::Unset);
  u.declare(143,1,0);assert(u.positionKnown());assert(u.status().target==144);}
 // ---- 0.5: sequence authority ----------------------------------------------
 // Drive by timing fallback (no IR) so every Hall point is a single-step advance.
 auto run=[](Navigator& n,uint8_t physicalFirst,int8_t dir,int count,int flipAt,uint32_t& ms){
   uint8_t phys=physicalFirst;
   for(int i=0;i<count;++i){NavObservation o{};ms+=1000;o.openedAtMs=ms;o.polarity=polarityAt(phys);if(i==flipAt)o.polarity=!o.polarity;
     o.movement.issue=ngr_nav::MotionIssue::NoSource;Ruling r=n.judge(o);assert(r!=Ruling::NonLandmark&&r!=Ruling::NoPosition);phys=nextMarker(phys,dir);}
   return phys;};
 // Today's lap 1: physically in 040-041 CW, declared 041-042.
 {Navigator n;uint32_t ms=0;n.declare(41,1,ms);run(n,41,1,9,-1,ms);assert(n.status().corrections==0);
  run(n,50,1,1,-1,ms);assert(n.status().corrections==1);assert(n.status().navMm==50);assert(n.status().trust==Trust::SequenceRecovered);
  SequenceCorrection f;assert(n.takeCorrection(f));assert(f.fromMm==51&&f.toMm==50&&f.offset==-1);assert(!n.takeCorrection(f));
  run(n,51,1,171,-1,ms);assert(n.status().corrections==1);assert(n.status().navMm==50);assert(n.status().sequenceMatches==10);}
 // Every start, both directions, offsets +/-1..3: corrected on the 10th point, to the truth.
 int corrected=0;
 for(int dir=-1;dir<=1;dir+=2)for(int start=0;start<ROUTE_N;++start)for(int k=-3;k<=3;++k){if(!k)continue;
   Navigator n;uint32_t ms=0;uint8_t declared=routeMod(start-dir+k);n.declare(declared,int8_t(dir),ms);
   uint8_t phys=run(n,uint8_t(start),int8_t(dir),10,-1,ms);assert(n.status().corrections==1);assert(n.status().navMm==routeMod(int32_t(phys)-dir));++corrected;}
 // A correct declaration with any single misread never moves position.
 int held=0;
 for(int dir=-1;dir<=1;dir+=2)for(int start=0;start<ROUTE_N;++start)for(int flip=0;flip<20;++flip){
   Navigator n;uint32_t ms=0;n.declare(routeMod(start-dir),int8_t(dir),ms);uint8_t phys=run(n,uint8_t(start),int8_t(dir),20,flip,ms);
   assert(n.status().corrections==0);assert(n.status().navMm==routeMod(int32_t(phys)-dir));++held;}
 // A multi-step advance retains history: skipped points are UNKNOWN.
 {Navigator n;n.declare(55,1,1000);NavObservation f{};f.openedAtMs=1700;f.polarity=polarityAt(56);f.movement=mp(200,1700000);n.judge(f);
  uint32_t twob=spanMm(56,1)+spanMm(57,1);NavObservation g{};g.openedAtMs=3200;g.polarity=polarityAt(58);g.movement=mp(200+(twob+5)/10,3200000);
  assert(n.judge(g)==Ruling::MissedAndAdvanced);assert(n.status().sequenceLength==2);
  assert(n.recovery().count()==3 && !n.recovery().entry(1).observed);}
 std::cout<<"PROXIMAL_R1 navigator checks passed ("<<corrected<<" offset corrections, "<<held<<" single-misread holds)\n";
}
