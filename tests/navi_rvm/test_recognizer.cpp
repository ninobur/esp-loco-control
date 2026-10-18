#include "RvmRecognizer.h"
#include "NaviIntegratedCore.h"
#include "NaviEstop.h"
#include "Ops.h"
#include "NaviStations.h"
#include <array>
#include <vector>
#include <iostream>
#include <cassert>

struct Board {
 static constexpr int N=12,L=3600,G=5;
 std::array<rvm::Cell,L/G> cells{};
 std::array<uint16_t,N> peaks{},seams{};
 std::array<int16_t,N> base{};
 std::array<uint16_t,156> speeds{};
 rvm::Model model;
 explicit Board(bool flat=false):model{cells.data(),peaks.data(),seams.data(),base.data(),speeds.data(),L/G,L,G,N,7}{
  for(int i=0;i<N;++i){peaks[i]=i*300;seams[i]=(i*300+L-150)%L;base[i]=8000;}
  speeds.fill(150);
  for(int i=0;i<L/G;++i)cells[i]={int16_t(std::lround(value(i*G,flat)*4)),8};
 }
 static float value(float x,bool flat=false){
  if(flat)return 2000;
  float y=2000;
  for(int j=0;j<N;++j){float d=std::remainder(x-j*300.f,float(L));
   float sign=j%3==0?-1.f:1.f;
   y+=sign*(90*std::exp(-d*d/450.f)+25*std::exp(-(d+40)*(d+40)/120.f)-18*std::exp(-(d-43)*(d-43)/120.f));
  }return y;
 }
};
struct Run {std::vector<rvm::Token> tokens,qualities;bool stopped=false;};
Run drive(int start=2,int pieces=6,int badFrom=-1,int badCount=0,bool pause=false,bool flat=false,bool spike=false){
 Board board(flat);rvm::Recognizer e(board.model);e.setBoot(123);e.declare(start,1,1);
 Run r;uint64_t us=1;uint32_t serial=0;float x=start*300+90;
 for(int i=0;i<pieces*100;++i,x+=3){
  rvm::Block b;b.firstSerial=b.lastSerial=++serial;b.firstUs=b.lastUs=us+=20000;b.hall=Board::value(x,flat);b.pwm=80;b.irValid=true;b.irSpeed=150;
  for(int k=0;k<badCount;++k){const float d=std::remainder(x-(badFrom+k)*300.f,float(Board::L));if(std::fabs(d)<90)b.hall=2000+(b.hall-2000)*0.55f;}
  if(spike && i>=15 && i<25)b.hall+=800;
  if(pause && i==120){
   const auto phase=e.phase();const auto ord=e.ordinal();const auto offset=e.offset();
   for(int t=0;t<180;++t){auto zero=b;zero.firstUs=zero.lastUs=us+=20000000;zero.firstSerial=zero.lastSerial=++serial;zero.pwm=0;zero.hall=1500+(t%2)*1200;rvm::Token tok;assert(!e.observe(zero,tok));}
   assert(e.phase()==phase && e.ordinal()==ord && e.offset()==offset);
   b.firstUs=b.lastUs=us+=20000;b.firstSerial=b.lastSerial=++serial;
  }
  rvm::Token t;if(e.observe(b,t)){if(t.finalQuality)r.qualities.push_back(t);else r.tokens.push_back(t);}
 }
 r.stopped=e.lossOfPattern();return r;
}
int main(){
 for(int start:{0,2,7,10}){
  auto r=drive(start,4);assert(r.tokens.size()==4);
  for(size_t i=0;i<r.tokens.size();++i){assert(r.tokens[i].committed==(start+int(i)+1)%12);assert(r.tokens[i].ordinal==i+1);assert(std::isfinite(r.tokens[i].evidence));}
 }std::cout<<"PASS geographically separated declarations and wrap; every token checked\n";
 assert(drive(2,6,-1,0,false,true).tokens.empty());std::cout<<"PASS flat map plus movement prior never emits\n";
 auto normal=drive();auto paused=drive(2,6,-1,0,true);assert(paused.tokens.size()==normal.tokens.size());
 for(size_t i=0;i<normal.tokens.size();++i)assert(paused.tokens[i].committed==normal.tokens[i].committed);
 std::cout<<"PASS 60-minute PWM-zero waveform changes preserve phase, offset and token identity\n";
 auto corrupted=drive(2,6,-1,0,false,false,true);assert(corrupted.tokens.size()==normal.tokens.size());
 for(size_t i=0;i<normal.tokens.size();++i){
  auto a=normal.tokens[i],b=corrupted.tokens[i];
  std::cout<<"CORRUPTION ordinal="<<a.ordinal<<" expected="<<a.committed<<" actual="<<b.committed<<" normal_us="<<a.lastUs<<" corrupt_us="<<b.lastUs<<'\n';
  assert(b.committed==a.committed && b.ordinal==a.ordinal && b.lastUs>=a.lastUs);
  // Synthetic physical leading ensemble cannot appear before center-70 mm.
  const float x=690+float(b.lastUs-20001)*0.00015f;
  assert(x >= 900+300*i-70);
 }
 std::cout<<"PASS gross 10-block corruption: every identity/time checked, no premature token\n";
 auto anomaly=drive(2,6,4,1);
 for(auto t:anomaly.qualities)std::cout<<"ANOMALY mm="<<t.committed<<" q="<<t.quality<<" bad="<<t.atypical<<" trusted="<<t.trusted<<" streak="<<int(t.mismatches)<<'\n';
 assert(anomaly.tokens.size()==normal.tokens.size());
 bool bad=false,good=false;
 for(auto t:anomaly.qualities){if(t.committed==4){bad=t.atypical;assert(t.trusted!=4);assert(t.mismatches==1);}if(t.committed==5){good=!t.atypical;assert(t.mismatches==0);}}
 assert(bad&&good);std::cout<<"PASS one weak MM counts once, does not anchor, next normal MM resets quality run\n";
 auto five=drive(2,9,4,5);assert(five.stopped);std::cout<<"PASS five new-magnet incoherent assessments stop; shared MM not double counted\n";
 navi_eyes::OrderedEstop estop;auto release=estop.received(false);estop.received(true);assert(estop.asserted());assert(estop.apply(release,false));
 std::cout<<"PASS independent E-STOP assertion survives an older queued release\n";
 navi_eyes::NaviIntegratedCore core;core.declare(108,1,1);assert(core.mm()==108&&core.target().sequence==109);core.declare(157,-1,2);assert(core.mm()==157&&core.target().sequence==156);
 ir_movement::WireSnapshot w;w.bootId=3;w.sequence=1;w.capturedUs=100000;w.opticalReason=ir_movement::TRACKING;
 core.observeIr(w,100000,80);assert(core.irApplicable(100000));
 ++w.sequence;w.capturedUs+=100000;++w.sampleGaps;core.observeIr(w,200000,80);assert(!core.irApplicable(200000));
 ++w.sequence;w.capturedUs+=100000;core.observeIr(w,300000,80);assert(core.irApplicable(300000));
 assert(core.mm()==157);std::cout<<"PASS IR integrity judged from own counters; next clean packet immediately eligible\n";
 {
  Board board(true);rvm::Recognizer engine(board.model);engine.declare(2,1,1);
  rvm::Block b;b.pwm=80;b.irValid=false;b.hall=2000;
  for(int i=1;i<=200;++i){b.firstSerial=b.lastSerial=i;b.firstUs=b.lastUs=i*20000+1;rvm::Token t;assert(!engine.observe(b,t));}
  assert(!engine.stalled());b.irValid=true;b.irSpeed=0;
  int stops=0;for(int i=201;i<=400;++i){b.firstSerial=b.lastSerial=i;b.firstUs=b.lastUs=i*20000+1;rvm::Token t;if(engine.observe(b,t)){assert(t.stopDecision && t.committed==2 && t.ordinal==0);++stops;}}
  assert(stops==1);
  assert(engine.stalled());
  std::cout<<"PASS stationary Hall plus fresh stationary IR triggers stall; missing IR alone does not\n";
 }
 {
  using namespace navi_one;
  Ops o;assert(admitAuto(o));assert(!admitThrottle(o));
  o.positionKnown=true;o.enrolled=true;assert(!admitGo(o));assert(admitThrottle(o));
  o.estopped=true;assert(admitGo(o));o.enrolled=false;assert(admitThrottle(o));
  bool stop=false;assert(!parseEstop("",stop)&&stop);
  o.estopped=false;o.actualPwm=40;assert(admitDeclaration(o));assert(admitMotorDirection(o));
  o.actualPwm=0;o.commandedPwm=90;assert(admitMotorDirection(o));
  using namespace navi_eyes;
  assert(kStationTargets[0].mm==15&&kStationTargets[1].mm==63&&kStationTargets[2].mm==108&&kStationTargets[3].mm==157);
  NaviStopOverlay overlay;StopGeography geo;geo.section=StopSection::Final;
  auto c=overlay.command(geo,true,1,90,35,35,1,true);assert(c.pwm==0&&c.execution==StopExecution::Braking);
  c=overlay.command(geo,true,1,90,0,0,1000,true);assert(c.execution==StopExecution::Dwell);
  c=overlay.command(geo,true,1,90,0,0,5999,true);assert(c.pwm==0);
  c=overlay.command(geo,true,1,90,0,0,6000,true);assert(c.pwm==90&&c.releasedNow);
  c=overlay.command(geo,true,1,90,30,42,7000,false);assert(c.pwm==42);
  std::cout<<"PASS manual/AUTO admission, direction lockout, unchanged station landmarks, braking/dwell/release\n";
 }
 std::cout<<"recognizer_bytes="<<sizeof(rvm::Recognizer)<<" adapter_bytes="<<sizeof(core)<<'\n';
}
