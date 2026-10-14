#define main original_dart_scenarios
#include "test_dart.cpp"
#undef main
#include "../NaviTileTiming.h"

static void previousLap(Fixture& f,uint64_t duration=1000000) {
  uint8_t mm=f.n.mm(); const auto start=mm;const auto dir=f.n.direction();
  f.n.travel_.begin(start,dir);
  for(unsigned i=0;i<171;++i) {
    const auto next=navi_one::nextMarker(mm,dir);
    DartTravel t{};t.intervalUs=duration;t.fromId=i+1;t.toId=i+2;t.valid=true;t.context=1;
    f.n.travel_.record(mm,next,dir,t);mm=next;
  }
  CHECK(f.n.travel_.completedLaps()==1);
}
// Exact-coordinate arithmetic fixture through actual production selection.
// Native ADC/packet tests below exercise physical acquisition separately.
static void exact(Fixture& f,uint64_t coordinate,uint64_t after=1000000,int pole=0,bool measured=true) {
  DartOpening o{};o.id=++f.n.nextOpeningId_;o.serial=f.serial;
  o.timeUs=f.n.selectedTimeUs_+after;f.t=o.timeUs;
  o.timingApplicable=f.n.runClock_.removedBefore(o.timeUs,o.manualPauseUs);
  o.pole=pole?pole:(f.n.target().polarity==HallOpeningPolarity::AboveReference?1:-1);
  o.pwm=40;o.direction=f.n.direction()>0?1:2;
  o.ir=f.n.irAt(f.t);o.ir.um=coordinate;o.ir.epoch=f.n.epochId_;o.ir.boot=1;
  o.ir.applicable=measured;o.ir.pulses=coordinate/ir_movement::kInstalledPitchUm;
  f.n.latestIrReceivedUs_=f.t;f.n.latestIr_.nominalUm=coordinate;
  f.n.beginDecision(f.t);f.n.pending_[f.n.pendingCount_++]=o;f.n.evaluateOpening(o);
  if(f.n.replayMetricPending_) f.n.replayPending();
}
static uint64_t spacing(const Fixture& f) {return uint64_t(navi_one::spanMm(f.n.mm(),f.n.direction()))*1000;}
int main() {
  // STOP time is removed without changing raw acquisition timestamps. Queued
  // pre-STOP/during-STOP samples still associate after GO, including repeats.
  {NaviRunClock clock;clock.stopped(true,100);CHECK(clock.now(150)==100);
   clock.stopped(true,160);clock.stopped(false,200);
   CHECK(clock.now(90)==90);CHECK(clock.now(150)==100);CHECK(clock.now(230)==130);
   clock.stopped(true,250);clock.stopped(false,400);CHECK(clock.now(450)==200);
   CHECK(clock.now(150)==100);CHECK(clock.now(300)==150);
   for(unsigned i=0;i<17;++i){clock.stopped(true,500+i*100);clock.stopped(false,550+i*100);}
   uint64_t removed=0;CHECK(!clock.removedBefore(90,removed));CHECK(clock.now(2200)==1100);}
  // Complete lap is immutable while the next lap is collected; no mixed laps.
  {DartTravelHistory h;h.begin(170,1);DartTravel t{};t.valid=true;t.intervalUs=1000000;
   uint8_t mm=170;for(int i=0;i<170;++i){auto to=navi_one::nextMarker(mm,1);h.record(mm,to,1,t);mm=to;CHECK(!h.get(170,1).valid);}
   h.record(mm,170,1,t);CHECK(h.completedLaps()==1);CHECK(h.get(170,1).intervalUs==1000000);
   t.intervalUs=2000000;h.record(170,0,1,t);CHECK(h.get(170,1).intervalUs==1000000);
   CHECK(!h.get(170,-1).valid);mm=0;
   for(int i=1;i<171;++i){auto to=navi_one::nextMarker(mm,1);h.record(mm,to,1,t);mm=to;}
   CHECK(h.get(170,1).intervalUs==2000000);CHECK(h.get(170,1).lap==2);
   h.begin(20,-1);h.record(20,19,-1,t);CHECK(!h.get(20,-1).valid);}
  // Dart replacement and midpoint remain the actual production comparator.
  {Fixture f;syntheticSeed(f);previousLap(f);exact(f,250000,100000);CHECK(f.n.candidateBoundaryUm()==350000);
   exact(f,280000,200000);CHECK(f.n.candidateBoundaryUm()==320000);
   exact(f,295000,300000);CHECK(f.n.candidateBoundaryUm()==305000);
   exact(f,303000,400000);CHECK(f.n.mm()==18);CHECK(f.n.confirmedCount()==2);CHECK(f.n.openingIrUm()==303000);}
  {Fixture f;syntheticSeed(f);exact(f,290000,100000);exact(f,310000,200000);
   CHECK(f.n.inferredAnchor());CHECK(f.n.openingIrUm()==300000);CHECK(f.n.anchorMismatchCount()==0);}
  // Both signs, exact provisional boundaries. Geographic selection is retained.
  for(int sign:{-1,1})for(int64_t error:{10000LL,10001LL,48259LL,48260LL}) {
    Fixture f;f.start();previousLap(f);const auto d=spacing(f);
    exact(f,uint64_t(int64_t(d)+sign*error),500000);
    if(f.n.candidateAvailable()) f.n.selectBest(DartReason::Boundary);
    CHECK(f.n.mm()==18);CHECK(f.n.confirmedCount()==2);
    CHECK((f.n.irAuthority()==IrGeographicAuthority::Withdrawn)==(error>=48260));
    CHECK(f.n.irCredibility()==(error<=10000?IrIntervalClassification::Consistent:
      error<48260?IrIntervalClassification::InconsistentPossible:IrIntervalClassification::ProvisionalImpossible));
    CHECK(f.n.openingIrUm()==uint64_t(int64_t(d)+sign*error));
    CHECK(!f.n.stopRequested());
  }
  // Restoration waits for a NEW reference, then the next independently timed
  // landmark. A good first interval alone cannot restore withdrawn authority.
  {Fixture f;f.start();previousLap(f);uint64_t x=spacing(f)+48260;exact(f,x);
   CHECK(f.n.irAuthority()==IrGeographicAuthority::Withdrawn);CHECK(f.n.comparisonMetric()==DartMetric::Time);
   CHECK(!f.n.restorationReferenceValid_);f.drain();x+=spacing(f);exact(f,x);
   CHECK(f.n.mm()==19);CHECK(f.n.irAuthority()==IrGeographicAuthority::Withdrawn);
   CHECK(f.n.restorationReferenceValid_);auto fresh=f.n.selectedOpeningId();f.drain();
   x+=spacing(f)+30000;exact(f,x);CHECK(f.n.irAuthority()==IrGeographicAuthority::Retained);
   CHECK(f.n.comparisonMetric()==DartMetric::Distance);CHECK(f.n.irCredibility()==IrIntervalClassification::InconsistentPossible);
   bool restored=false;DartRecord r;while(f.n.takeDartRecord(r))if(r.transition==IrCredibilityTransition::Restore){restored=true;CHECK(r.restorationStartId==fresh);CHECK(r.metric==DartMetric::Time);CHECK(r.anchorUm==r.opening.ir.um);}
   // Small diagnostic rings may drop records; never use them as authority.
   CHECK(f.n.anchorMismatchCount()==0);CHECK(restored);}
  // No previous lap: inability to compare requests stop, never Hall-free advance.
  {Fixture f;f.start();f.t+=2000000;f.tick(2000);f.complete();CHECK(f.n.stopReason()==DartStop::NoTimingHistory);CHECK(f.n.mm()==17);}
  // The first increment's 250 -> 610 fixture now withdraws IR at the first
  // selected interval (50 mm short). Without a previous lap it must stop at
  // MM018. With that lap, the preserved later opening is judged by TIME.
  for(bool haveLap:{false,true}) {
    Fixture f;syntheticSeed(f);if(haveLap)previousLap(f);
    exact(f,250000,1000000);exact(f,610000,2000000,-1);
    CHECK(f.n.irAuthority()==IrGeographicAuthority::Withdrawn);
    CHECK(f.n.confirmedCount()==(haveLap?3:2));CHECK(f.n.mm()==(haveLap?19:18));
    CHECK(f.n.stopRequested()!=haveLap);
    if(haveLap){CHECK(f.n.openingIrUm()==610000);CHECK(f.n.comparisonMetric()==DartMetric::Time);}
    else CHECK(f.n.stopReason()==DartStop::NoTimingHistory);
  }
  // Two individually possible errors remain measurements: compensation
  // changes the cumulative discrepancy, not the rule or geographic identities.
  for(int secondError:{-30000,30000}) {
    Fixture f;f.start();auto x=spacing(f)-30000;exact(f,x);
    f.n.selectBest(DartReason::Boundary);CHECK(f.n.cumulativeResidualUm()==-30000);
    x=uint64_t(int64_t(x+spacing(f))+secondError);exact(f,x);
    if(f.n.candidateAvailable())f.n.selectBest(DartReason::Boundary);
    CHECK(f.n.mm()==19);CHECK(f.n.cumulativeResidualUm()==-30000+secondError);
    CHECK(f.n.irAuthority()==IrGeographicAuthority::Retained);CHECK(!f.n.stopRequested());
  }
  // A wrong-polarity event never advances; no opening by expected MM+2 stops.
  {Fixture f;f.start();previousLap(f);const auto next=navi_one::nextMarker(f.n.target().sequence,1);
   const auto frontier=spacing(f)+uint64_t(navi_one::spanMm(f.n.target().sequence,1))*1000;
   exact(f,spacing(f),500000,1);CHECK(f.n.mm()==17);
   f.ir((frontier+9651)/9652);f.tick(2000);f.complete();
   CHECK(f.n.stopReason()==DartStop::NoHallBySuccessor);CHECK(f.n.mm()==17);CHECK(f.n.target().sequence!=next);}
  {Fixture f;f.start();previousLap(f);const auto next=navi_one::nextMarker(f.n.target().sequence,1);
   exact(f,spacing(f)+uint64_t(navi_one::spanMm(f.n.target().sequence,1))*1000,1000000,navi_one::polarityAt(next)?1:-1);
   CHECK(f.n.stopReason()==DartStop::MissedTarget);CHECK(f.n.mm()==17);CHECK(f.n.confirmedCount()==1);}
  // Time reference uses the same dart comparison; metric switch replay cannot
  // consume one opening twice. Missing measurement is not an invented anchor.
  {Fixture f;f.start();previousLap(f);f.n.irAuthority_=IrGeographicAuthority::Withdrawn;
   exact(f,0,1000000,0,false);CHECK(f.n.mm()==18);CHECK(f.n.confirmedCount()==2);
   CHECK(!f.n.selectedAnchorAvailable());CHECK(f.n.comparisonMetric()==DartMetric::Time);}
  {Fixture f;f.start();previousLap(f);f.n.irAuthority_=IrGeographicAuthority::Withdrawn;
   f.n.prepareMetric();f.n.replayPending();f.t=f.n.selectedTimeUs_+2000000;
   f.tick(2000);f.complete();CHECK(f.n.stopReason()==DartStop::NoHallBySuccessor);CHECK(f.n.mm()==17);}
  // Actual departure replaces the historical dwell portion, preserving actual
  // time from throttle application to the next landmark.
  {Fixture f;f.start();previousLap(f,10000000);auto& record=f.n.travel_.banks_[1-f.n.travel_.writeBank_][f.n.mm()];
   record.departureOffsetUs=6000000;f.n.irAuthority_=IrGeographicAuthority::Withdrawn;
   f.n.prepareMetric();f.n.replayPending();f.n.noteOperatingContext(3,f.n.selectedTimeUs_+7000000);
   CHECK(f.n.timingExpectedUs()==4000000);CHECK(f.n.metricOriginUs_==f.n.selectedTimeUs_+7000000);}
  // Manual STOP/GO preserves the candidate, MM, selected coordinate and
  // comparison origin. A long stop cannot trigger the MM+2 time deadline.
  {Fixture f;f.start();previousLap(f);f.n.irAuthority_=IrGeographicAuthority::Withdrawn;
   f.n.prepareMetric();f.n.replayPending();const auto origin=f.n.selectedTimeUs_;
   exact(f,100000,250000);CHECK(f.n.candidateAvailable());const auto boundary=f.n.candidateBoundaryUm();
   const auto identity=f.n.mm();const auto anchor=f.n.openingIrUm();
   f.n.setManualStopped(true,origin+400000);f.n.noteOperatingContext(0);
   f.t=origin+10000000;f.tick(2000);f.complete();
   CHECK(!f.n.stopRequested());CHECK(f.n.mm()==identity);CHECK(f.n.openingIrUm()==anchor);
   CHECK(f.n.candidateBoundaryUm()==boundary);CHECK(f.n.manualTimingPaused());
   f.n.setManualStopped(false,origin+10400000);f.n.noteOperatingContext(1);
   exact(f,300000,11000000);CHECK(f.n.mm()==18);CHECK(!f.n.stopRequested());
   CHECK(f.n.selectedTimeUs()==origin+11000000);CHECK(f.n.selectedClockUs_==origin+1000000);
   const auto& stored=f.n.travel_.banks_[f.n.travel_.writeBank_][identity];
   CHECK(stored.valid);CHECK(stored.intervalUs==1000000);}
  // A stopped interval can cross from healthy IR to timing without turning
  // stopped wall time into movement. Existing PWM-zero movement latch wins.
  {Fixture f;f.start();previousLap(f);const auto start=f.n.selectedTimeUs_;
   f.n.setManualStopped(true,start+100000);f.n.noteOperatingContext(0);
   f.t=start+10000000;f.pwm=0;f.tick(2000);f.complete();CHECK(!f.n.stopRequested());
   f.n.setManualStopped(false,start+10100000);f.pwm=40;f.t=start+10100000;f.tick(2000);f.complete();
   exact(f,0,11000000,0,false);
   CHECK(f.n.mm()==18);CHECK(f.n.comparisonMetric()==DartMetric::Time);CHECK(!f.n.stopRequested());}
  {Fixture f;f.start();previousLap(f);f.n.setManualStopped(true,f.t);f.pwm=0;f.ir(1);
   CHECK(f.n.pwmZeroMovementRequiresDeclaration());f.n.setManualStopped(false,f.t+1000000);
   f.pwm=40;f.field(-1);CHECK(f.n.mm()==17);CHECK(f.n.pwmZeroMovementRequiresDeclaration());}
  // Native MM017 repeated-opening evidence is retained without advancement.
  for(uint64_t delay:{1028000ULL,1179000ULL}){Fixture f;f.start();f.t+=delay;f.ir(3);f.field(1);f.complete();
    CHECK(f.n.mm()==17);CHECK(f.n.confirmedCount()==1);CHECK(f.n.openingCount()==2);}
  for(int dir:{-1,1}){Fixture f;f.start(dir>0?170:1,dir);CHECK(f.n.mm()==0);}
  // Existing PWM-zero movement intervention remains in charge.
  {Fixture f;f.start();f.pwm=0;f.ir(1);CHECK(f.n.pwmZeroMovementRequiresDeclaration());f.pwm=40;f.field(-1);CHECK(f.n.mm()==17);}
  // Throttle-only fallback: mid-tile command holds, next accepted opening
  // resets entry, elapsed time moves PWM, and expiry cannot advance identity.
  for(int dir:{-1,1}) {
    NaviTileTiming execution;StopView v{};v.identityAvailable=v.available=v.fine=true;
    v.identifiedDirection=dir;v.identifiedMm=dir>0?35:56;
    DartTravel history{};history.valid=true;history.intervalUs=10000000;
    const auto target=stopMarkerUm(45)+150000;
    execution.command(v,1,1000000,1000000,target,history,90,90,85);
    v.fine=v.available=false;
    auto c=execution.command(v,1,1000000,5000000,target,history,90,90,82);
    CHECK(c.mode==TileTimingMode::HoldUntilOpening);CHECK(!c.available);CHECK(!c.entryNow);
    v.identifiedMm=dir>0?36:55;
    c=execution.command(v,2,10000000,10000000,target,history,90,90,82);
    CHECK(c.entryNow && c.entryPwm==75);CHECK(c.geography.pwm==75);
    c=execution.command(v,2,10000000,15000000,target,history,90,90,75);CHECK(c.geography.pwm==70);
    c=execution.command(v,2,10000000,25000000,target,history,90,90,70);CHECK(c.geography.pwm==65);
    CHECK(v.identifiedMm==(dir>0?36:55));CHECK(!c.entryNow);
    v.fine=v.available=true;c=execution.command(v,2,10000000,26000000,target,history,90,90,65);
    CHECK(c.mode==TileTimingMode::Geographic);
  }
  // Endpoint/provenance JSON fits the bounded transport buffer and truncation
  // is detectable; formatting cannot change the navigation decision.
  {Fixture f;f.start();previousLap(f);f.drain();exact(f,spacing(f)+48260);DartRecord r;
   while(f.n.takeDartRecord(r)){char text[2048];auto count=formatDart(text,sizeof(text),r,1);CHECK(count>0 && count<int(sizeof(text)));CHECK(std::strstr(text,"\"ir_authority\":")!=nullptr);}
   CHECK(f.n.mm()==18);CHECK(f.n.irAuthority()==IrGeographicAuthority::Withdrawn);}
  // Exercise the formatter's widest numeric values, including signed minimum,
  // and its required-length contract rather than merely strlen(truncated).
  {DartRecord r{};auto& o=r.opening;
   o.timeUs=o.firstTickUs=o.manualPauseUs=UINT64_MAX;o.id=o.serial=o.referenceVersion=UINT32_MAX;
   o.median=o.reference=INT16_MIN;o.pole=-1;o.pwm=255;o.direction=2;
   auto& ir=o.ir;ir.um=ir.receivedUs=ir.capturedUs=ir.pulses=ir.epoch=ir.boot=UINT64_MAX;
   ir.sequence=UINT32_MAX;ir.applicable=true;r.intervalStart=ir;
   r.decisionUs=r.anchorUm=r.errorUm=r.boundaryUm=r.intervalUs=r.departureUs=UINT64_MAX;
   r.residualUm=r.cumulativeUm=r.priorResidualUm=INT64_MIN;
   r.partnerId=r.generation=r.intervalStartId=r.restorationStartId=UINT32_MAX;
   r.timingFromId=r.timingToId=r.timingLap=UINT32_MAX;
   r.target=r.previousMm=r.successorTarget=170;r.expectedPole=-1;r.context=3;
   r.inferred=r.intervalValid=r.residualValid=r.anchorAvailable=r.independentIdentity=true;
   r.measuredIntervalUm=r.expectedIntervalUm=r.expected=r.successorExpected=UINT64_MAX;
   r.metricCoordinate=r.timingOriginUs=UINT64_MAX;
   char json[2048],tiny[16];const int bytes=formatDart(json,sizeof(json),r,UINT64_MAX);
   std::printf("widest_diagnostic_bytes=%d\n",bytes);
   CHECK(bytes>0 && bytes<int(sizeof(json)));CHECK(json[bytes-1]=='}');
   CHECK(formatDart(tiny,sizeof(tiny),r,UINT64_MAX)>=int(sizeof(tiny)));
  }
  std::printf("PASS %u integrated EWO 0_5 assertions; core=%zu history=%zu record=%zu bytes\n",checks,sizeof(NaviIntegratedCore),sizeof(DartTravelHistory),sizeof(DartRecord));
  return 0;
}
