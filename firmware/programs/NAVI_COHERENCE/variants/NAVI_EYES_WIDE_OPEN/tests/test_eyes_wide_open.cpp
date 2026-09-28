// NAVI_EYES_WIDE_OPEN deterministic checks. They prove the software
// implements the specified rules; they are not behavioral evidence about the
// railway (AGENTS.md §8). The waveform rig below is synthetic: Gaussian-like
// magnet fields on a flat ordinary-track level. It is not a model of Toby's
// real Hall environment and must not be read as one.
//
// Build and run from the repository root:
//   V=firmware/programs/NAVI_COHERENCE/variants/NAVI_EYES_WIDE_OPEN
//   c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I $V
//       $V/tests/test_eyes_wide_open.cpp -o /tmp/t_ewo && /tmp/t_ewo $V
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include "Navigator.h"
#include "NaviHall.h"
using namespace navi_one;
using namespace navi_hall;

static std::string readFile(const std::string& p){std::ifstream f(p);std::stringstream s;s<<f.rdbuf();return s.str();}
static unsigned checks=0;
#define CHECK(x) do{assert(x);++checks;}while(0)

// ---------------------------------------------------------------------------
// 1. Source: no X22/X22R in the Hall -> NAVI path; the Hall task only acquires.
static void sourceChecks(const std::string& v){
  const std::string ino=readFile(v+"/NAVI_EYES_WIDE_OPEN.ino");
  CHECK(!ino.empty());
  for(const char* banned:{"ExcursionDetector","X22R.h","HallObserver.h","ngr_hall","hallConfig(",
                          "BaselineOutcome","takeDetection","NAVI_BASELINE_ADAPT_PWM"})
    CHECK(ino.find(banned)==std::string::npos);
  const auto a=ino.find("static void hallTask(void*){"),b=ino.find("static void onIr(");
  CHECK(a!=std::string::npos&&b!=std::string::npos&&a<b);
  const std::string task=ino.substr(a,b-a);
  // Acquisition only: read, ring, recorder. No reference, threshold or opening.
  CHECK(task.find("hallRing.push(")!=std::string::npos);
  for(const char* banned:{"hallReference","openingObserver","OPENING_DEPARTURE","judge"})
    CHECK(task.find(banned)==std::string::npos);
  for(const std::string h:{"Navigator.h","NaviHall.h","RouteDisplacement.h","LocoConfig.h"})
    CHECK(readFile(v+"/"+h).find("#include \"ExcursionDetector")==std::string::npos);
}

// ---------------------------------------------------------------------------
// 2-3. The 70x2 opening and opening polarity, against NAVI's reference.
static void openingChecks(){
  OpeningObserver o;Opening out;uint64_t t=0;const int16_t R=2000;
  auto s=[&](int16_t raw,uint8_t pwm=60){HallSample x;x.tUs=(++t)*1000;x.raw=raw;x.pwm=pwm;return o.sample(x,R,out);};
  for(int i=0;i<5;++i)CHECK(!s(R+69));                 // 69 never qualifies
  CHECK(!s(R));CHECK(!s(R+70));CHECK(!s(R));           // one qualifying sample is not an opening
  CHECK(!s(R+70));CHECK(s(R+70));                      // second consecutive qualifying sample opens
  CHECK(out.tUs==t*1000&&out.polarity==1&&out.departure==70&&out.reference==R);
  const Opening first=out;
  // The same excursion continues, then return flux dips to -60 (inside 70):
  // the opening polarity is fixed; nothing overwrites it.
  for(int i=0;i<30;++i)CHECK(!s(R+250));
  for(int i=0;i<40;++i)CHECK(!s(R-60));
  CHECK(first.polarity==1&&o.openings()==1);
  // A later opposite-sign departure >=70x2 is a NEW opening delivered to NAVI
  // (NAVI decides what it is); it does not revise the first.
  CHECK(!s(R-80));CHECK(s(R-80));CHECK(out.polarity==0&&o.openings()==2&&first.polarity==1);
  // Opposite signs are not one excursion.
  for(int i=0;i<5;++i)s(R);
  CHECK(!s(R+80));CHECK(!s(R-80));CHECK(!s(R+80));CHECK(s(R+80));CHECK(out.polarity==1);
  // No window, refractory or PWM rule: an opening 1 ms after rearm, and at
  // PWM 0, is still delivered, carrying the PWM it occurred at.
  s(R);CHECK(!s(R-90,0));CHECK(s(R-90,0));CHECK(out.pwm==0&&out.polarity==0);
}

// ---------------------------------------------------------------------------
// Judgment-level fixtures (1 um per pulse so window edges are exact).
static ngr_nav::IrOdometryEpoch ownerA;
static ngr_nav::IrOdometryPoint pt(uint64_t um,uint64_t ms,uint64_t epoch=1){return {&ownerA,epoch,1,ms*1000,um,0,1};}
static NavObservation obs(uint32_t ms,uint8_t pole,ngr_nav::IrOdometryPoint p={},bool pwm0=false){NavObservation o;o.openedAtMs=ms;o.polarity=pole;o.odometry=p;o.motivePwmZero=pwm0;return o;}
static uint8_t syncAt(Navigator& n,uint8_t start,int8_t dir){
  n.declare(start,dir,1000);const uint8_t m=nextMarker(start,dir);
  n.observeIr(pt(0,2000),false);
  assert(n.judge(obs(2000,polarityAt(m),pt(0,2000)))==Ruling::Advanced);
  return m;
}

static void judgmentChecks(){
  for(int dir=-1;dir<=1;dir+=2)for(unsigned start=0;start<ROUTE_N;++start){
    Navigator base;const uint8_t m1=syncAt(base,uint8_t(start),int8_t(dir));
    const uint8_t m2=nextMarker(m1,dir);const uint64_t D=spanMm(m1,dir);
    // Wrong polarity at a coherent distance: not the expected MM; HELD.
    {Navigator n=base;n.observeIr(pt(D*1000,2400),false);
     CHECK(n.judge(obs(2400,!polarityAt(m2),pt(D*1000,2400)))==Ruling::WrongPolarityHeld);
     CHECK(n.status().navMm==m1&&n.status().target==m2&&n.referenceMm()==m1);
     CHECK(n.status().reason==JudgmentReason::WrongPolarity&&n.status().contradictions==1);
     // The real MM (right polarity) later in the same window is accepted.
     n.observeIr(pt(D*1050,2500),false);
     CHECK(n.judge(obs(2500,polarityAt(m2),pt(D*1050,2500)))==Ruling::Advanced&&n.status().navMm==m2);
     CHECK(n.recovery().entry(n.recovery().count()-1).polarity==polarityAt(m2));}
    // Distance-incoherent (too early), right polarity: not the expected MM.
    {Navigator n=base;n.observeIr(pt(D*400,2300),false);
     CHECK(n.judge(obs(2300,polarityAt(m2),pt(D*400,2300)))==Ruling::NonLandmark);
     CHECK(n.status().reason==JudgmentReason::TooEarlyByDistance&&n.status().navMm==m1);}
    // Correct polarity + coherent distance + sequence: the expected MM.
    {Navigator n=base;n.observeIr(pt(D*980,2600),false);
     CHECK(n.judge(obs(2600,polarityAt(m2),pt(D*980,2600)))==Ruling::Advanced);
     CHECK(n.status().navMm==m2&&n.status().evidence==EvidenceClass::Confirmed&&n.referenceMm()==m2);}
    // Opening at motive PWM 0: retained, no authority, at any distance.
    {Navigator n=base;n.observeIr(pt(D*1000,2700),true);
     CHECK(n.judge(obs(2700,polarityAt(m2),pt(D*1000,2700),true))==Ruling::RetainedPwmZero);
     CHECK(n.status().navMm==m1&&n.status().reason==JudgmentReason::PwmZero&&n.status().pwmZeroOpenings==1);}
  }
}

// PWM 0 cannot manufacture route displacement; a held contradiction survives
// into history; one missing MM does not destroy continuity.
static void pwmZeroAndMissing(){
  const int8_t dir=1;const uint8_t start=40;
  Navigator n;const uint8_t m1=syncAt(n,start,dir);const uint8_t m2=nextMarker(m1,dir),m3=nextMarker(m2,dir),m4=nextMarker(m3,dir);
  const uint64_t D1=spanMm(m1,dir)*1000ull,D2=spanMm(m2,dir)*1000ull,D3=spanMm(m3,dir)*1000ull;
  // Powered to half of span 1, then 5 m of wheel rotation at PWM 0 (handling).
  n.observeIr(pt(D1/2,3000),false);
  n.observeIr(pt(D1/2+5000000,4000),true);
  CHECK(n.traverse(pt(D1/2+5000000,4000))==0);                     // no sans-MM advance
  uint64_t um=0;CHECK(n.travelUmAt(pt(D1/2+5000000,4000),um)&&um==D1/2);
  CHECK(n.route().excludedTotal()==5000000&&n.status().navMm==m1&&n.haveReference());
  const uint64_t X=5000000; // excluded offset carried in all later points
  // Resume powered: the real MM2 at 1.0*D1 route distance is accepted.
  n.observeIr(pt(X+D1,5000),false);
  CHECK(n.judge(obs(5000,polarityAt(m2),pt(X+D1,5000)))==Ruling::Advanced&&n.status().navMm==m2);
  CHECK(n.referenceExcluded()==X);
  // MM3 read with the wrong polarity (genuine magnet, F05/F06 kind): held.
  n.observeIr(pt(X+D1+D2,6000),false);
  CHECK(n.judge(obs(6000,!polarityAt(m3),pt(X+D1+D2,6000)))==Ruling::WrongPolarityHeld&&n.status().navMm==m2);
  // Its window is traversed: sans-MM advance carrying the held polarity.
  n.observeIr(pt(X+D1+D2+D2*16/100,6500),false);
  CHECK(n.traverse(pt(X+D1+D2+D2*16/100,6500))==1&&n.status().navMm==m3&&n.positionKnown());
  {SansMmAdvance s;CHECK(n.takeSansAdvance(s)&&s.mm==m3&&s.contradictory&&s.polarity==uint8_t(!polarityAt(m3)));}
  {const auto& e=n.recovery().entry(n.recovery().count()-1);CHECK(e.mm==m3&&e.observed&&e.polarity==uint8_t(!polarityAt(m3)));}
  CHECK(n.status().evidence==EvidenceClass::MissedWithPolarityDiscrepancy);
  // MM4 accepted from the same synchronization (cumulative D2+D3): continuity.
  n.observeIr(pt(X+D1+D2+D3,7000),false);
  CHECK(n.judge(obs(7000,polarityAt(m4),pt(X+D1+D2+D3,7000)))==Ruling::Advanced&&n.status().navMm==m4);
  CHECK(n.status().sansMmConsecutive==0&&n.status().state==NavState::Tracking);
  // Operator redeclaration remains authoritative and clears the frame.
  n.declare(100,-1,8000);CHECK(n.status().navMm==100&&n.status().navDir==-1&&!n.haveReference());
  CHECK(n.status().trust==Trust::OperatorDeclared&&n.takeResetRequest());
}

// Hall-only degraded path (no MM-referenced distance) is 20Q3's, unchanged:
// 650 ms, and a wrong polarity advances WITH DISCREPANCY (review item).
static void hallOnlyUnchanged(){
  Navigator n;n.declare(40,1,1000);const uint8_t m=nextMarker(40,1);
  CHECK(n.judge(obs(1649,polarityAt(m)))==Ruling::NonLandmark&&n.status().reason==JudgmentReason::HallOnlyGuard);
  CHECK(n.judge(obs(1650,!polarityAt(m)))==Ruling::AdvancedWithDiscrepancy&&n.status().navMm==m);
}

// ---------------------------------------------------------------------------
// Waveform rig: the real OpeningObserver, SpatialReference, RouteDisplacement
// and Navigator, called in the sketch's loop order: IR point -> route account
// -> reference collection; then every native sample -> opening -> judge();
// then traversal. 1 kHz Hall, 100 ms IR points, 9.652 mm pulses.
struct Rig{
  HallSampleRing<1024> ring;OpeningObserver open;SpatialReference ref;Navigator nav;
  ngr_nav::IrOdometryEpoch owner;uint64_t epoch=1;
  uint64_t ms=0;double x=0,wheel=0,v=0;uint8_t pwm=0;bool pwmZeroSeen=true;
  std::vector<double> magX;std::vector<int> magSign;std::vector<double> magAmp;
  std::function<double(double)> level=[](double){return 1825.0;};
  double stationaryOffset=0; // Hall change while standing still (fringe/handling)
  std::vector<ngr_nav::IrOdometryPoint> hist;
  std::vector<Ruling> rulings;std::vector<JudgmentReason> reasons;std::vector<uint8_t> acceptedMm;
  std::vector<int16_t> refAtClose;std::vector<uint64_t> routeAtClose;uint32_t serial=0;
  uint64_t collectedBefore100=0;
  static constexpr uint32_t PITCH=9652;
  void buildMap(uint8_t start,int8_t dir,unsigned count){
    double at=0;uint8_t mm=start;
    for(unsigned i=0;i<=count;++i){magX.push_back(at);magSign.push_back(polarityAt(mm)?1:-1);magAmp.push_back(260);at+=spanMm(mm,dir);mm=nextMarker(mm,dir);}
  }
  double field(double pos)const{double f=0;for(size_t i=0;i<magX.size();++i){const double d=(pos-magX[i])/12.0;f+=magSign[i]*magAmp[i]*std::exp(-d*d);}return f;}
  ngr_nav::IrOdometryPoint point()const{return {&owner,epoch,1,ms*1000,uint64_t(wheel/(PITCH/1000.0)),0,PITCH};}
  ngr_nav::IrOdometryPoint nearest(uint64_t tUs)const{
    ngr_nav::IrOdometryPoint best{};uint64_t bd=UINT64_MAX;
    for(auto it=hist.rbegin();it!=hist.rend()&&it-hist.rbegin()<64;++it){const uint64_t d=it->capturedUs>tUs?it->capturedUs-tUs:tUs-it->capturedUs;if(d<bd){bd=d;best=*it;}}
    return bd<=150000?best:ngr_nav::IrOdometryPoint{};
  }
  void tick(){
    ++ms;
    if(pwm>0){x+=v/1000.0;wheel+=v/1000.0;}
    if(pwm==0)pwmZeroSeen=true;
    if(ms%100==0){                                   // IR point (onIrPoint)
      const auto p=point();hist.push_back(p);
      nav.observeIr(p,pwmZeroSeen);pwmZeroSeen=(pwm==0);
      const auto ev=ref.irPoint(p,p.capturedUs,nav.route(),ring,uint32_t(ms));
      if(ref.status().routeUm<REF_CLEARANCE_END_UM)collectedBefore100+=ref.status().collected;
      if(ev==RefEvent::Closed){refAtClose.push_back(ref.value());routeAtClose.push_back(ref.status().routeUm);}
    }
    HallSample s;s.tUs=ms*1000;s.pwm=pwm;                  // Hall task
    s.raw=int16_t(std::lround(level(x)+field(x)+(v==0||pwm==0?stationaryOffset:0)));
    ring.push(s);
    drain();                                           // serviceHall
    if(!hist.empty())nav.traverse(hist.back());        // serviceTraversal
  }
  uint32_t cursor=0;
  void drain(){
    HallSample s;
    while(cursor!=ring.head()){
      assert(ring.at(cursor,s));++cursor;
      if(!ref.ready()){ref.bootSample(s.raw,uint32_t(s.tUs/1000));continue;}
      Opening o;if(!open.sample(s,ref.value(),o))continue;
      ++serial;NavObservation n;n.openedAtMs=o.tMs;n.polarity=o.polarity;n.motivePwmZero=o.pwm==0;n.odometry=nearest(o.tUs);
      const uint32_t before=nav.synchronizations();
      const Ruling r=nav.judge(n);rulings.push_back(r);reasons.push_back(nav.status().reason);
      if(r==Ruling::Advanced||r==Ruling::AdvancedWithDiscrepancy){
        acceptedMm.push_back(nav.status().navMm);
        const bool synced=nav.synchronizations()!=before;
        ref.acceptedOpening(serial,synced?nav.referencePoint():ngr_nav::IrOdometryPoint{},nav.referenceExcluded(),nav.referencePoint().capturedUs,ring);
      }
    }
  }
  void run(uint64_t n){for(uint64_t i=0;i<n;++i)tick();}
  void runTo(double pos){while(x<pos)tick();}
};

// A normal run: boot reference, then every MM accepted, and after each the
// reference is re-established from 100-200 mm of IR route travel only.
static void rigNormal(){
  Rig r;r.buildMap(40,1,8);
  // Each interval has its own ordinary-track level (a real reference change).
  r.level=[&r](double pos){int k=0;for(size_t i=0;i<r.magX.size();++i)if(pos>=r.magX[i])k=int(i);return 1825.0+5.0*k;};
  // Boot stationary and clear of magnets; after declaring, stand 1 s so the
  // first MM is not inside the 650 ms Hall-only guard measured from it.
  r.x=150;r.run(2100);
  CHECK(r.ref.ready()&&r.ref.value()==1825&&r.ref.status().lastCount==BOOT_REFERENCE_SAMPLES);
  r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);
  r.pwm=60;r.v=220;r.runTo(r.magX[7]+250);
  // Every mapped MM 41..47 accepted, in order, with no rejection.
  CHECK(r.acceptedMm.size()==7);for(unsigned i=0;i<7;++i)CHECK(r.acceptedMm[i]==40+1+i);
  for(auto q:r.rulings)CHECK(q==Ruling::Advanced);
  // A reference closed after each accepted MM, at >=200 mm route travel, and
  // equals that interval's own level (the level after MM k is 1825+5k).
  CHECK(r.refAtClose.size()==7);
  for(unsigned i=0;i<7;++i){CHECK(r.routeAtClose[i]>=REF_COLLECT_END_UM);CHECK(r.refAtClose[i]==1825+5*int(i+1));}
  CHECK(r.collectedBefore100==0);                    // nothing collected in 0-100 mm
  // Collection spans 100-200 mm of travel: at 220 mm/s that is ~455 samples.
  CHECK(r.ref.status().lastCount>400&&r.ref.status().lastCount<500);
}

// The reference measured in 100-200 mm is the one used for the next MM: a
// level shift after MM41 is absorbed, so MM42 opens at the right place and
// a departure that would have looked like an opening against the old
// reference is not one.
static void rigReferenceUsedForNext(){
  Rig r;r.buildMap(40,1,4);
  r.level=[&r](double pos){return pos>r.magX[1]+50?1905.0:1825.0;}; // +80 after MM41: a 70-count "event" against the old reference
  r.x=150;r.run(2100);r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);r.pwm=60;r.v=220;
  r.runTo(r.magX[1]+30);CHECK(r.acceptedMm.size()==1&&r.acceptedMm[0]==41);
  const size_t before=r.rulings.size();
  // The shelf begins at 50 mm (inside clearance): against the incumbent 1825
  // it is a +80 departure, and NAVI receives it as an opening and judges it
  // too early by distance. Nothing upstream hid it.
  r.runTo(r.magX[1]+99);
  CHECK(r.rulings.size()==before+1&&r.rulings.back()==Ruling::NonLandmark&&r.reasons.back()==JudgmentReason::TooEarlyByDistance);
  r.runTo(r.magX[1]+201);
  CHECK(r.ref.value()==1905&&r.refAtClose.size()==1);  // new reference from 100-200 mm
  r.runTo(r.magX[2]+30);
  CHECK(r.acceptedMm.size()==2&&r.acceptedMm[1]==42);   // MM42 judged against 1905
}

// A long stationary dwell inside the collection interval does not advance
// the interval and cannot swamp the population, even with PWM > 0 (the MM109
// station case) and a Hall level shift while standing.
static void rigDwell(){
  for(int pwmDuringDwell:{0,40}){
    Rig r;r.buildMap(40,1,4);r.x=150;r.run(2100);r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);r.pwm=60;r.v=220;
    r.runTo(r.magX[1]+150);                            // halfway through collection
    r.v=0;r.pwm=uint8_t(pwmDuringDwell);r.run(200);    // stopped; last moving span reported
    const auto st=r.ref.status();CHECK(st.phase==RefPhase::Collecting);
    const uint32_t c0=st.collected;const uint64_t d0=st.routeUm;
    r.stationaryOffset=180;                           // standing: Hall +180
    r.run(60000);                                     // 60 s dwell
    const auto sd=r.ref.status();
    CHECK(sd.phase==RefPhase::Collecting&&sd.collected==c0&&sd.routeUm==d0);
    CHECK(sd.stationarySkipped>59000);
    if(pwmDuringDwell==0){                            // hand rotation of the wheel at PWM 0
      r.wheel+=200;r.run(500);
      CHECK(r.ref.status().routeUm==d0&&r.nav.status().navMm==41&&r.nav.route().excludedTotal()>=20);
    }
    r.stationaryOffset=0;r.pwm=60;r.v=220;r.runTo(r.magX[1]+230);
    CHECK(r.refAtClose.size()==1&&r.refAtClose[0]==1825); // the +180 standing level did not move the median
    r.runTo(r.magX[2]+30);CHECK(r.acceptedMm.size()==2&&r.acceptedMm[1]==42);
  }
}

// A genuine MM whose field is missing: NAVI advances sans MM and the next MM
// is accepted; no reference collection starts from a sans-MM advance.
static void rigMissing(){
  Rig r;r.buildMap(40,1,5);r.magAmp[2]=0;              // MM42 absent
  r.x=150;r.run(2100);r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);r.pwm=60;r.v=220;
  r.runTo(r.magX[3]+30);
  CHECK(r.acceptedMm.size()==2&&r.acceptedMm[0]==41&&r.acceptedMm[1]==43);
  CHECK(r.nav.status().navMm==43&&r.nav.status().state==NavState::Tracking&&r.nav.status().sansMmTotal==1);
  SansMmAdvance s;CHECK(r.nav.takeSansAdvance(s)&&s.mm==42&&!s.contradictory);
  CHECK(r.refAtClose.size()==1);                     // only after MM41 (none after the sans-MM advance)
}

// A genuine MM read with the wrong polarity (F05/F06): held, window
// traversed, contradiction recorded, continuity kept.
static void rigWrongPolarity(){
  Rig r;r.buildMap(40,1,5);r.magSign[2]=-r.magSign[2];
  r.x=150;r.run(2100);r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);r.pwm=60;r.v=220;
  r.runTo(r.magX[3]+30);
  CHECK(r.acceptedMm.size()==2&&r.acceptedMm[1]==43);
  bool held=false;for(auto q:r.rulings)held|=q==Ruling::WrongPolarityHeld;CHECK(held);
  SansMmAdvance s;CHECK(r.nav.takeSansAdvance(s)&&s.mm==42&&s.contradictory);
}

// Declaration abandons a collection in progress and keeps the reference.
static void rigDeclaration(){
  Rig r;r.buildMap(40,1,4);r.x=150;r.run(2100);r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);r.pwm=60;r.v=220;
  r.runTo(r.magX[1]+150);CHECK(r.ref.collecting());
  const int16_t incumbent=r.ref.value();
  r.pwm=0;r.v=0;r.run(10);r.nav.declare(41,1,uint32_t(r.ms));r.ref.frameChanged();
  CHECK(!r.ref.collecting()&&r.ref.value()==incumbent&&r.ref.status().lastEnd==RefEnd::FrameChanged);
  CHECK(r.nav.status().navMm==41);
}

// Median of the spatial collection: a level that differs across the
// 100-200 mm interval gives the median of the samples collected across it.
static void rigMedian(){
  Rig r;r.buildMap(40,1,3);
  // 100-130 mm: 1900; 130-200 mm: 1830 (70% of the traversed distance).
  r.level=[&r](double pos){const double d=pos-r.magX[1];return (d>=100&&d<130)?1900.0:1830.0;};
  r.x=150;r.run(2100);r.nav.declare(40,1,uint32_t(r.ms));r.run(1000);r.pwm=60;r.v=220;
  r.runTo(r.magX[1]+230);
  CHECK(r.refAtClose.size()==1&&r.refAtClose[0]==1830);
}

int main(int argc,char** argv){
  const std::string v=argc>1?argv[1]:".";
  sourceChecks(v);openingChecks();judgmentChecks();pwmZeroAndMissing();hallOnlyUnchanged();
  rigNormal();rigReferenceUsedForNext();rigDwell();rigMissing();rigWrongPolarity();rigDeclaration();rigMedian();
  std::printf("PASS NAVI_EYES_WIDE_OPEN: %u checks (source, 70x2/polarity, judgment all MMs both directions, "
              "PWM 0, missing MM, Hall-only unchanged, waveform rig: normal run, next-MM reference, dwell, "
              "missing, wrong polarity, declaration, median)\n",checks);
}
