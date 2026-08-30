// GATE 4 — the layer that actually fails on the railway.
//
// Every host gate NAVI_ONE had covered the headers. But both of 0.1's field
// bugs and most of its review findings were in the .ino: travelDir() ignoring
// motor direction ("He complied and died"), a 400-byte buffer that silently
// destroyed every telemetry field, a missing subscription, commands accepted
// at times they must not be, and atoi() answering 0 for input it did not
// understand. That layer had no coverage, so that is the layer that failed in
// front of the operator.
//
// Ops.h exists so this gate can drive the policy directly, and HallCapture is
// driven here for the first time as well -- gate 1 replays waveforms straight
// into the recognizer, so nothing until now had ever exercised the code that
// assembles a passage.
#include <cstdio>
#include <vector>
#include <cmath>
#include "../Ops.h"
#include "../HallCapture.h"
using namespace navi_one;

static int checks=0, failures=0;
static void ck(bool c,const char* what){ checks++; if(!c){ failures++; printf("  FAIL  %s\n",what);} }
static void ckEq(long g,long w,const char* what){ checks++;
  if(g!=w){ failures++; printf("  FAIL  %s (got %ld want %ld)\n",what,g,w);} }

int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  printf("NAVI_ONE gate 4 — commands, interlocks, capture\n");

  printf("\nP1  a payload that is not understood is REFUSED, never interpreted\n");
  { int n;
    ck(parseInt("0",n)&&n==0,"\"0\" parses");
    ck(parseInt("-12",n)&&n==-12,"\"-12\" parses");
    ck(parseInt(" 45 ",n)&&n==45,"surrounding spaces tolerated");
    ck(!parseInt("",n),"empty string refused");
    ck(!parseInt("abc",n),"\"abc\" refused — atoi() would have said 0");
    ck(!parseInt("12x",n),"trailing rubbish refused");
    ck(!parseInt("1.5",n),"\"1.5\" refused");
    ck(!parseInt("9999999999999",n),"absurd length refused");
    bool b;
    ck(parseBool("1",b)&&b,"\"1\" is true");
    ck(parseBool("OFF",b)&&!b,"\"OFF\" is false, case-insensitively");
    ck(!parseBool("maybe",b),"\"maybe\" refused"); }

  printf("\nP2  on an emergency topic, ambiguity STOPS\n");
  { bool stop=false;
    ck(parseEstop("1",stop)&&stop,"\"1\" asserts");
    ck(parseEstop("0",stop)&&!stop,"\"0\" clears");
    stop=false;
    ck(!parseEstop("",stop),"empty payload is not understood");
    ck(stop,"...AND IT STOPPED. 0.1 computed atoi(\"\")!=0 == false and stood down");
    stop=false;
    ck(!parseEstop("garbage",stop) && stop,"unreadable payload stops"); }

  printf("\nP3  garbage on cmd/direction does not mean REVERSE\n");
  { bool fwd=true;
    ck(parseMotorDir("2",fwd)&&fwd,"2 is FORWARD");
    ck(parseMotorDir("0",fwd)&&!fwd,"0 is REVERSE");
    ck(!parseMotorDir("",fwd),"empty refused — 0.1 reversed the locomotive");
    ck(!parseMotorDir("banana",fwd),"noise refused");
    ck(!parseMotorDir("1",fwd),"1 has never meant anything and is refused");
    int8_t d;
    ck(parseSessionDir("ccw",d)&&d==-1,"CCW parses, case-insensitively");
    ck(!parseSessionDir("clockwise",d),"anything else refused");
    int a,b;
    ck(parseInterval("040-041",a,b)&&a==40&&b==41,"an interval parses");
    ck(!parseInterval("040",a,b),"a bare marker is not an interval");
    ck(!parseInterval("040-",a,b),"a missing end is refused");
    ck(!parseInterval("040-0x1",a,b),"rubbish after the dash is refused"); }

  printf("\nP4  position is declared standing still\n");
  { Ops o; o.sessionDir=+1;
    ck(admitStartMarker(o)==nullptr,"stopped and oriented: admitted");
    o.running=true;  ck(admitStartMarker(o)!=nullptr,"refused while AUTO runs");
    o.running=false; o.actualPwm=40;
    ck(admitStartMarker(o)!=nullptr,"refused while the wheels are turning");
    o.actualPwm=0; o.commandedPwm=90;
    ck(admitStartMarker(o)!=nullptr,"refused while a ramp is on its way up");
    o.commandedPwm=0; o.sessionDir=0;
    ck(admitStartMarker(o)!=nullptr,"refused with no session direction"); }

  printf("\nP5  AUTO and GO interlocks, including the one bought with a dead locomotive\n");
  { Ops o; o.positionKnown=true; o.forward=true;
    ck(admitAuto(o)==nullptr,"AUTO admitted");
    o.forward=false;
    ck(admitAuto(o)!=nullptr,"AUTO refused in REVERSE — \"He complied and died\"");
    o.forward=true; o.positionKnown=false;
    ck(admitAuto(o)!=nullptr,"AUTO refused with no position");
    o.positionKnown=true; o.lowVoltage=true;
    ck(admitAuto(o)!=nullptr,"AUTO refused on low voltage");
    Ops g; g.positionKnown=true; g.enrolled=true; g.forward=true;
    ck(admitGo(g)==nullptr,"GO admitted");
    g.enrolled=false; ck(admitGo(g)!=nullptr,"GO refused when not enrolled");
    g.enrolled=true;  g.positionKnown=false;
    ck(admitGo(g)!=nullptr,"GO refused after a strike has withdrawn position");
    g.positionKnown=true; g.forward=false;
    ck(admitGo(g)!=nullptr,"GO refused in REVERSE"); }

  printf("\nP6  the throttle answers by hand, and says why when it does not\n");
  { Ops o;
    ck(admitThrottle(o)==nullptr,"throttle admitted when not enlisted");
    o.enrolled=true;
    ck(admitThrottle(o)!=nullptr,"throttle refused while enlisted -- WITH A REASON");
    o.enrolled=false; o.lowVoltage=true;
    ck(admitThrottle(o)!=nullptr,"throttle refused on low voltage");
    Ops d; d.safeDirPwm=15; d.actualPwm=0; d.commandedPwm=0;
    ck(admitMotorDirection(d)==nullptr,"direction change admitted at rest");
    d.actualPwm=40;  ck(admitMotorDirection(d)!=nullptr,"refused while moving");
    d.actualPwm=0; d.commandedPwm=90;
    ck(admitMotorDirection(d)!=nullptr,"refused mid-ramp — 0.1 checked only actualPwm");
    d.commandedPwm=0; d.enrolled=true;
    ck(admitMotorDirection(d)!=nullptr,"refused while enlisted"); }

  // ---- capture -----------------------------------------------------------
  // Drive HallCapture the way the Hall task does: one sample per millisecond.
  struct Rig {
    CaptureConfig cfg; HallCapture<512> cap; uint32_t t=0; int base=1834;
    Rig():cap(cfg){}
    void prime(){ for(int i=0;i<3000;i++) cap.sample(t++, (int16_t)base); }
    // A Gaussian bump of the given peak and full duration, in milliseconds.
    bool bump(int peak,int durMs,int railAt=-1){
      bool closed=false; const double c=durMs/2.0, s=durMs/6.0;
      for(int i=0;i<durMs;i++){
        double d=(i-c)/s; int v=base+(int)(peak*std::exp(-0.5*d*d));
        if(railAt>=0 && i==railAt) v=4095;
        if(cap.sample(t++, (int16_t)v)) closed=true;
      }
      for(int i=0;i<60;i++) if(cap.sample(t++,(int16_t)base)) closed=true;
      return closed;
    }
  };

  printf("\nC1  a passage is assembled, once, with its pre-roll\n");
  { Rig r; r.prime();
    ck(r.bump(200,180),"a 180 ms passage closes");
    const Passage& p=r.cap.passage();
    ckEq(p.polarity,1,"positive pole");
    ck(p.peakCounts>=190 && p.peakCounts<=205,"peak recovered");
    ck(!p.truncated,"not truncated");
    ck(!p.clipped,"not clipped");
    ckEq(p.decimation,1,"stored at full rate");
    // 0.1 wrote the entry-crossing sample into the pre-roll, replayed it, and
    // pushed it again, so it appeared twice at the boundary.
    ck(p.preSamples<=12,"pre-roll is at most PRE samples");
    bool dup=false;
    if(p.preSamples>0 && p.sampleCount>p.preSamples)
      dup = (p.oriented[p.preSamples-1]==p.oriented[p.preSamples]) &&
            (p.oriented[p.preSamples]>=38);
    ck(!dup,"the entry sample is stored exactly once"); }

  printf("\nC2  a long passage is DECIMATED, not truncated — the shape test survives\n");
  { Rig r; r.prime();
    ck(r.bump(200,4000),"a 4 s passage closes");     // 0.1: truncated at 512 ms
    const Passage& p=r.cap.passage();
    ck(!p.truncated,"NOT truncated — 0.1 set this and the shape test abstained");
    ck(p.decimation>1,"stored at a reduced rate instead");
    ck(p.sampleCount<=512,"and still inside the fixed buffer");
    float resid=0;
    ck(MagnetRecognizer::fitResidual(p,resid),"the arc is still fittable");
    ck(resid<0.13f,"and still reads as a Gaussian after decimation");
    // The whole arc must be there, not the first half of it.
    ck(p.peakCounts>=190,"the peak survived decimation"); }

  printf("\nC3  a rail between passages does not excuse the NEXT one\n");
  { Rig r; r.prime();
    // A supply transient far from any magnet, then a clean passage.
    for(int i=0;i<5;i++) r.cap.sample(r.t++, 4095);
    for(int i=0;i<200;i++) r.cap.sample(r.t++, (int16_t)r.base);
    ck(r.bump(200,180),"the next passage closes");
    ck(!r.cap.passage().clipped,
       "and is NOT marked clipped — 0.1 carried the rail forward for minutes");
    // A rail DURING the passage is still reported.
    Rig q; q.prime();
    ck(q.bump(200,180,90),"a passage containing a rail closes");
    ck(q.cap.passage().clipped,"and IS marked clipped"); }

  printf("\nC4  a declaration ends the frame under the sensor too\n");
  { Rig r; r.prime();
    // Open a passage and leave the sensor inside the field, as declaring
    // while stopped over a magnet does.
    for(int i=0;i<100;i++) r.cap.sample(r.t++,(int16_t)(r.base+200));
    r.cap.reset();                       // the declaration
    bool closed=false;
    for(int i=0;i<200;i++) if(r.cap.sample(r.t++,(int16_t)r.base)) closed=true;
    ck(!closed,"driving off does not emit the old frame's passage");
    ck(r.bump(200,180),"and the next real magnet still closes normally"); }

  printf("\nC5  the floor and the entry margin still hold\n");
  { Rig r; r.prime();
    ck(!r.bump(200,20),"a 20 ms excursion is refused by the 40 ms floor");
    ckEq((long)r.cap.floorRejects(),1,"and counted");
    ck(!r.bump(30,180),"a 30-count bump never reaches the 38-count entry margin");
    ck(r.bump(200,180),"a real passage still closes"); }

  printf("\n%d checks, %d failures\n",checks,failures);
  return failures?1:0;
}
