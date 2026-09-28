#include <cassert>
#include <cstring>
#include <iostream>
#include "Navigator.h"
using namespace navi_one;
static ngr_nav::IrOdometryEpoch owner;
static ngr_nav::IrOdometryPoint point(uint64_t mm,uint64_t t,uint64_t epoch=1) {
  return {&owner,epoch,1,t,mm,0,1000};
}
static void appendWord(ProximalRecovery& r,int start,int shift,unsigned mask=0) {
  for(int i=0;i<10;++i)r.append(routeMod(start+i),1,true,
    polarityAt(routeMod(start+i+shift))^((mask>>i)&1));
}
int main() {
  // Exact opening-polarity word at events408..430 on the Pi. The old global
  // exact-match search selected MM166 from incumbentMM65 on this word.
  const uint8_t fieldPoles[10]={1,0,0,1,0,1,0,0,0,0};
  int matches=0,global=-1;
  for(int k=1;k<ROUTE_N;++k) {
    bool all=true;for(int i=0;i<10;++i)all=all && fieldPoles[i]==polarityAt(routeMod(56+i+k));
    if(all){++matches;global=routeMod(65+k);}
  }
  assert(matches==1 && global==166);
  ProximalRecovery field;
  for(int i=0;i<10;++i)field.append(uint8_t(46+i),1,true,polarityAt(46+i),point(300*i,1000+i));
  assert(field.evaluate(55,1).offset==0);
  for(int i=0;i<10;++i) {
    field.append(uint8_t(56+i),1,true,fieldPoles[i]);
    assert(field.evaluate(uint8_t(56+i),1).offset==0);
  }
  assert(!strcmp(field.decision().reason,"TRAVEL_UNAVAILABLE_HOLD"));
  assert(field.decision().incumbent==8);
  // The actual word cannot even nominate the -70 offset at startup.
  ProximalRecovery startup;
  for(int i=0;i<10;++i)startup.append(uint8_t(56+i),1,true,fieldPoles[i]);
  const auto fd=startup.evaluate(65,1);assert(fd.offset>=-10 && fd.offset<=10);
  assert(routeMod(65+fd.offset)!=166);

  // Physical history can contain eight observations and two UNKNOWNs.
  ProximalRecovery unknown;
  for(int i=0;i<10;++i)unknown.append(uint8_t(40+i),1,i!=3 && i!=6,polarityAt(41+i));
  auto u=unknown.evaluate(49,1);assert(u.positions==10 && u.observations==8);
  assert(!unknown.entry(3).observed && !unknown.entry(6).observed);
  const auto unknownPole=unknown.entry(3).polarity;
  if(u.offset)assert(!unknown.entry(3).observed && unknown.entry(3).polarity==unknownPole);

  // A ten-position window, not ten successful observations, gates startup.
  ProximalRecovery warm;
  for(int i=0;i<9;++i)warm.append(uint8_t(40+i),1,true,polarityAt(42+i));
  assert(!warm.evaluate(48,1).offset);
  warm.append(49,1,true,polarityAt(51));assert(warm.evaluate(49,1).offset==2);
  assert(warm.count()==10); // correction keeps history, not an empty window.

  // Construct competing proximal explanations; score improvement need not be
  // perfect or exceed an arbitrary margin. Ties must retain the incumbent.
  unsigned ties=0,oneVote=0;
  for(unsigned mask=0;mask<1024;++mask) {
    ProximalRecovery r;appendWord(r,40,0,mask);const auto d=r.evaluate(49,1);
    if(d.ties>1){assert(d.offset==0);++ties;}
    if(d.offset && d.best==d.incumbent+1 && d.best<d.observations)++oneVote;
  }
  assert(ties && oneVote);

  // Established reference: measured travel admits +1 but not +4 or +7.
  ProximalRecovery bounded;
  for(int i=0;i<10;++i)bounded.append(uint8_t(30+i),1,true,polarityAt(30+i),point(300*i,1000+i));
  bounded.evaluate(39,1);
  for(int i=0;i<3;++i)bounded.append(uint8_t(40+i),1,true,!polarityAt(40+i),point(2700+400*(i+1),2000+i));
  const auto b=bounded.evaluate(42,1);
  assert(b.possibleMask&(1u<<11));
  assert(!(b.possibleMask&(1u<<14)) && !(b.possibleMask&(1u<<17)));
  // Recorded reference points before the discrepancy cannot translate with it.
  assert(bounded.entry(0).mm==33);
  for(int dir=-1;dir<=1;dir+=2) {
    ProximalRecovery r;
    for(int i=0;i<10;++i) {
      uint8_t mm=routeMod(50+dir*i);
      r.append(mm,int8_t(dir),true,polarityAt(mm),point(uint64_t(ProximalRecovery::mapped(50,int8_t(dir),i)),1000+i));
    }
    const uint8_t ref=routeMod(50+9*dir);
    r.evaluate(ref,int8_t(dir));
    const uint64_t base=uint64_t(ProximalRecovery::mapped(50,int8_t(dir),9));
    for(int i=0;i<3;++i) {
      uint8_t mm=routeMod(50+dir*(10+i));
      r.append(mm,int8_t(dir),true,!polarityAt(mm),point(base+uint64_t(ProximalRecovery::mapped(ref,int8_t(dir),i+2)),2000+i));
    }
    auto d=r.evaluate(routeMod(50+12*dir),int8_t(dir));
    assert(d.possibleMask&(1u<<(10+dir)));
    assert(!(d.possibleMask&(1u<<(10+4*dir))));
    assert(!(d.possibleMask&(1u<<(10+7*dir))));
    const auto retained=r.count();r.reverse();
    assert(r.count()==retained && !r.evaluate(routeMod(50+12*dir),int8_t(-dir)).offset);
  }

  // Same pole word with discontinuous IR never gains travel authority.
  for(uint64_t epoch=2;epoch<=3;++epoch) {
    ProximalRecovery gap;
    for(int i=0;i<10;++i)gap.append(uint8_t(30+i),1,true,polarityAt(30+i),point(300*i,1000+i));
    gap.evaluate(39,1);
    for(int i=0;i<3;++i)gap.append(uint8_t(40+i),1,true,!polarityAt(40+i),point(3000+300*i,2000+i,epoch));
    assert(!gap.evaluate(42,1).offset);
    assert(!strcmp(gap.decision().reason,"TRAVEL_UNAVAILABLE_HOLD"));
  }
  // Startup +/-10 boundary and wrap; actual directions both tested by the
  // exhaustive navigator suite. No offset can exceed the outer limit.
  for(int shift=-10;shift<=10;++shift) {
    ProximalRecovery r;appendWord(r,165,shift);auto d=r.evaluate(3,1);
    assert(d.offset>=-10 && d.offset<=10);
    if(d.offset)assert(d.offset==shift);
  }
  // A single outlying distance cannot make the whole route globally lost.
  // 20Q3: under the exact +/-15% rule, 10 m of same-epoch travel traverses the
  // expected windows (POSITION_ADVANCED_SANS_MM, UNKNOWN history). NAVI stays
  // TRACKING, never LOST; AUTO's landmark limit is what ends automatic operation.
  Navigator outlier;outlier.declare(40,1);
  static ngr_nav::IrOdometryEpoch owner;
  NavObservation first;first.openedAtMs=1000;first.polarity=polarityAt(41);
  first.odometry={&owner,1,1,1000000,0,0,1000};
  outlier.judge(first);
  auto far=first;far.openedAtMs=2000;far.odometry.capturedUs=2000000;far.odometry.pulses=10000;
  const Ruling farRuling=outlier.judge(far);
  assert(farRuling==Ruling::NonLandmark || farRuling==Ruling::Advanced || farRuling==Ruling::AdvancedWithDiscrepancy);
  assert(outlier.positionKnown() && outlier.status().sansMmTotal>=SANS_MM_AUTO_LIMIT);
  assert(!outlier.status().corrections);
  char json[512];assert(bounded.format(json,sizeof(json),430)<int(sizeof(json)));
  std::cout<<json<<'\n';
  std::cout<<"PASS proximal recovery: field -70 rejection, UNKNOWN denominator, warmup, retained history, "
    <<ties<<" ties, "<<oneVote<<" one-vote improvements, travel filtering, epoch breaks, wrap\n";
}
