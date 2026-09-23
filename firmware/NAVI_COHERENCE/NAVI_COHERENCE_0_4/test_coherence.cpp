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
 // Reversal preserves location authority/history and expects the point just passed.
 auto hist=n.status().sequenceLength;n.setDirection(-1,6000);assert(n.positionKnown());assert(n.status().target==59);assert(n.status().sequenceLength==hist);
 std::cout<<"NAVI_COHERENCE_0_3 focused checks passed\n";
}
