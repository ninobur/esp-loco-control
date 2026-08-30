// NAVI_ONE contract gate. Drives the real Navigator and MagnetRecognizer.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../Navigator.h"
using namespace navi_one;

static int checks=0, failures=0;
static void ck(bool c,const char* what){ checks++; if(!c){ failures++; printf("  FAIL  %s\n",what);} }
static void ckEq(long g,long w,const char* what){ checks++;
  if(g!=w){ failures++; printf("  FAIL  %s (got %ld want %ld)\n",what,g,w);} }

// A synthetic passage that the recognizer will accept: full amplitude, clean
// Gaussian arc, opened well clear of the previous close.
static std::vector<int16_t> gauss(int peak,int n,int pre){
  std::vector<int16_t> v(n,0);
  double c=(n+pre)/2.0, s=(n-pre)/6.0;
  for(int i=pre;i<n;i++){ double d=(i-c)/s; v[i]=(int16_t)(peak*exp(-0.5*d*d)); }
  return v;
}
struct Rig {
  RecognizerConfig cfg; MagnetRecognizer rec; Navigator nav;
  uint32_t t=100000;
  std::vector<int16_t> buf;
  Rig():rec(cfg),nav(rec){}
  Ruling feed(uint8_t pol,int peak,uint32_t gapMs,uint16_t dur=180){
    t += gapMs;
    buf = gauss(peak,140,12);
    Passage p; p.openedAtMs=t; p.closedAtMs=t+dur; p.peakCounts=(uint16_t)peak;
    p.polarity=pol; p.oriented=buf.data(); p.sampleCount=(uint16_t)buf.size();
    p.preSamples=12;
    t += dur;
    Verdict v=rec.examine(p);
    return nav.judge(p,v);
  }
  // the polarity the map expects next
  uint8_t expected(){ return polarityAt(nextMarker(nav.status().navMm,nav.status().navDir)); }
};

int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  printf("NAVI_ONE contract gate\n");

  printf("\nT1  UNSET: nothing advances, whatever arrives\n");
  { Rig r;
    for(int i=0;i<20;i++) r.feed(i&1,200,900);
    ckEq(r.nav.status().navMm,0,"navMm untouched while unset");
    ck(r.nav.status().state==NavState::Unset,"still unset"); }

  printf("\nT2  declaration seeds position; first magnet on polarity alone\n");
  { Rig r; r.nav.declare(40,+1);
    ckEq(r.nav.status().navMm,40,"declared at 40");
    ckEq(r.nav.status().target,41,"target is 41");
    // only 50 ms since the declaration -- the guard must ABSTAIN (no anchor)
    Ruling g=r.feed(r.expected(),200,50);
    ck(g==Ruling::Advanced,"first magnet advanced with no guard evidence");
    ckEq(r.nav.status().navMm,41,"now at 41"); }

  printf("\nT3  a clean lap advances exactly one per magnet\n");
  { Rig r; r.nav.declare(0,+1);
    for(int i=0;i<60;i++) ck(r.feed(r.expected(),200,900)==Ruling::Advanced,"advanced");
    ckEq(r.nav.status().navMm,60,"60 magnets -> marker 60");
    ckEq((long)r.nav.status().refusals,0,"no refusals"); }

  printf("\nT4  wrong polarity: refuse, hold position, one strike\n");
  { Rig r; r.nav.declare(60,+1); r.feed(r.expected(),200,900);
    uint8_t before=r.nav.status().navMm;
    Ruling g=r.feed(r.expected()?0:1,200,900);
    ck(g==Ruling::WrongMagnet,"ruled WRONG_MAGNET");
    ckEq(r.nav.status().navMm,before,"navMm unchanged");
    ckEq((long)r.nav.status().refusals,1,"refusal counted"); }

  printf("\nT5  a recognizer refusal is not an identity failure\n");
  { Rig r; r.nav.declare(60,+1); r.feed(r.expected(),200,900);
    uint8_t before=r.nav.status().navMm;
    Ruling g=r.feed(r.expected(),20,900);          // far too weak
    ck(g==Ruling::NotAMagnet,"ruled NOT_A_MAGNET, not WRONG_MAGNET");
    ckEq(r.nav.status().navMm,before,"navMm unchanged");
    ckEq((long)r.nav.status().refusals,0,"identity refusals still zero"); }

  printf("\nT5b the guard anchors on ACCEPTANCE, not on arrival\n");
  { Rig r; r.nav.declare(60,+1);
    r.feed(r.expected(),200,900);                  // accepted -- anchors here
    uint8_t at=r.nav.status().navMm;
    Ruling g=r.feed(r.expected(),200,10);          // 10 ms later: a re-read
    ck(g==Ruling::NotAMagnet,"event inside the guard is NOT_A_MAGNET");
    ckEq(r.nav.status().navMm,at,"and did not advance");
    // A REFUSED event must not move the anchor. Constructed so the two
    // hypotheses give opposite answers: event B is 330 ms after the
    // ACCEPTANCE (passes) but only 160 ms after the refused event A (would
    // fail). B advancing is proof the anchor stayed where it belongs.
    Rig q; q.nav.declare(60,+1);
    q.feed(q.expected(),200,900,20);               // accepted -- anchor here
    uint8_t at2=q.nav.status().navMm;
    Ruling a1=q.feed(q.expected(),200,150,20);     // 150 ms: refused by guard
    ck(a1==Ruling::NotAMagnet,"event A refused inside the guard");
    ckEq(q.nav.status().navMm,at2,"A did not advance");
    Ruling b1=q.feed(q.expected(),200,160,20);     // 330 ms from acceptance
    ck(b1==Ruling::Advanced,"a refused event did not re-anchor the guard"); }

  printf("\nT6  no event of any kind advances navMm by more than one\n");
  { Rig r; r.nav.declare(0,+1);
    uint8_t prev=r.nav.status().navMm; long jumps=0;
    for(int i=0;i<800;i++){
      uint8_t pol=(uint8_t)((i*7+i/3)&1);
      int pk=10+(i*37)%420;
      uint32_t gap=5+(i*53)%1600;
      r.feed(pol,pk,gap,(uint16_t)(60+(i*17)%300));
      int step=(int)r.nav.status().navMm-(int)prev; if(step<0) step+=ROUTE_N;
      if(step>1) jumps++;
      prev=r.nav.status().navMm;
    }
    ckEq(jumps,0,"800 adversarial events, no multi-marker jump"); }

  printf("\nT7  reversal lands on the marker about to be met again\n");
  { Rig r; r.nav.declare(40,+1);
    for(int i=0;i<5;i++) r.feed(r.expected(),200,900);   // now at 45
    ckEq(r.nav.status().navMm,45,"at 45 CW");
    r.nav.setDirection(-1);
    ckEq(r.nav.status().navMm,46,"stepped back along the OLD direction");
    ckEq(r.nav.status().target,45,"next marker going CCW is 45");
    ck(r.feed(r.expected(),200,900)==Ruling::Advanced,"first magnet after reversal");
    ckEq(r.nav.status().navMm,45,"landed on 45"); }

  printf("\nT8  the ten-magnet word proves, and contradicts\n");
  { Rig r; r.nav.declare(0,+1);
    for(int i=0;i<12;i++) r.feed(r.expected(),200,900);
    ck(r.nav.status().trust==Trust::Proven,"PROVEN after ten clean markers");
    ckEq(r.nav.status().seqAt,r.nav.status().navMm,"and it agrees with navMm"); }

  printf("\n%d checks, %d failures\n",checks,failures);
  return failures?1:0;
}
