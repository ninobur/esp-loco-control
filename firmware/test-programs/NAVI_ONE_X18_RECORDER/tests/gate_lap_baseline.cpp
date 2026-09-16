#include <cstdio>
#include "../LapBaselineController.h"
using namespace navi_one;
static int checks=0, failures=0;
static void ok(bool c,const char* s){++checks;if(!c){++failures;printf("  FAIL %s\n",s);}}
static LapBaselineUpdate lap(LapBaselineController& c,uint8_t origin,int dir,int value,int base){
  LapBaselineUpdate u; uint8_t mm=origin;
  for(int i=0;i<ROUTE_N;++i){mm=nextMarker(mm,dir);u=c.advance(mm,value,base);}
  return u;
}
int main(){
  printf("gate 13 -- SET LOCATION whole-lap baseline controller\n");
  LapBaselineController c; c.declare(71);
  auto u=lap(c,71,+1,1960,1949);
  ok(u.complete&&u.valid,"171 accepted advances returning to declared origin complete a lap");
  ok(u.coverage==ROUTE_N,"every route position has one vote");
  ok(u.estimate==1960&&u.requested==11&&u.applied==2,"first and later corrections are capped at two");
  u=lap(c,71,+1,1948,1950); ok(u.applied==-2,"negative correction is symmetric");
  c.declare(40); uint8_t mm=40;
  for(int i=0;i<50;++i){mm=nextMarker(mm,+1);c.advance(mm,1935,1935);}
  c.directionChanged();
  while(nextMarker(mm,-1)!=40){mm=nextMarker(mm,-1);u=c.advance(mm,1935,1935);}
  ok(!u.complete&&c.seekingOrigin(),"reversal discards partial and waits for fixed origin");
  mm=nextMarker(mm,-1); c.advance(mm,1935,1935);
  ok(!c.seekingOrigin()&&c.advances()==0,"arrival at original SET LOCATION starts a fresh circuit");
  u=lap(c,40,-1,1936,1935); ok(u.complete&&u.valid&&u.origin==40,"direction does not redefine origin");
  c.invalidate(); u=c.advance(41,2000,1935);
  ok(!u.complete&&!c.declared(),"withdrawal prevents adjustment until declaration");
  printf("%d checks, %d failures\n",checks,failures); return failures?1:0;
}
