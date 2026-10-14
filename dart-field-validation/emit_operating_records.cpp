#define main original_dart_scenarios
#include "../firmware/programs/NAVI_EWO_0_5/tests/test_dart.cpp"
#undef main
#include "../firmware/programs/NAVI_EWO_0_5/NaviTileTiming.h"

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
  o.pole=pole?pole:(f.n.target().polarity==HallOpeningPolarity::AboveReference?1:-1);
  o.pwm=40;o.direction=f.n.direction()>0?1:2;
  o.ir=f.n.irAt(f.t);o.ir.um=coordinate;o.ir.epoch=f.n.epochId_;o.ir.boot=1;
  o.ir.applicable=measured;o.ir.pulses=coordinate/ir_movement::kInstalledPitchUm;
  f.n.latestIrReceivedUs_=f.t;f.n.latestIr_.nominalUm=coordinate;
  f.n.beginDecision(f.t);f.n.pending_[f.n.pendingCount_++]=o;f.n.evaluateOpening(o);
  if(f.n.replayMetricPending_) f.n.replayPending();
}
static uint64_t spacing(const Fixture& f) {return uint64_t(navi_one::spanMm(f.n.mm(),f.n.direction()))*1000;}

int main(){Fixture f;f.start();previousLap(f);f.drain();uint64_t x=spacing(f)+48260;
 for(int step=0;step<3;++step){if(step)x+=spacing(f);exact(f,x);DartRecord r;
 while(f.n.takeDartRecord(r)){if(r.reason!=DartReason::CredibilityEvidence)continue;
 char payload[2048];int bytes=formatDart(payload,sizeof(payload),r,0xBEEFC005);
 if(bytes<0 || bytes>=int(sizeof(payload)))return 1;std::puts(payload);}}
 return 0;}
