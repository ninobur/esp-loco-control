#include "IrRouteEvidence.h"
#include "RouteMap.h"
#include <cassert>
#include <cstdio>
using namespace ir_movement;
int main(){
  unsigned cases=0;
  for(unsigned anchor=0;anchor<navi_one::ROUTE_N;++anchor){
    for(int dir=-1;dir<=1;dir+=2){
      const double next=navi_one::spanMm(anchor,dir);
      DistanceRange r;r.issues=0;r.minMm=next-10;r.maxMm=next+10;
      RouteCandidate candidates[3];
      assert(routeCandidates(r,navi_one::ROUTE_SPACING_MM,navi_one::ROUTE_N,
          anchor,dir,2,20,candidates,3)==3);
      assert(candidates[0].feasibility==EXCLUDED);
      assert(candidates[1].feasibility==POSSIBLE);
      assert(candidates[1].marker==navi_one::nextMarker(anchor,dir));
      assert(candidates[2].feasibility==EXCLUDED);
      const double later=candidates[2].travelMm;
      r.minMm=0;r.maxMm=10;
      routeCandidates(r,navi_one::ROUTE_SPACING_MM,navi_one::ROUTE_N,
          anchor,dir,2,20,candidates,3);
      assert(candidates[0].feasibility==POSSIBLE);
      assert(candidates[1].feasibility==EXCLUDED && candidates[2].feasibility==EXCLUDED);
      r.minMm=later-10;r.maxMm=later+10;
      routeCandidates(r,navi_one::ROUTE_SPACING_MM,navi_one::ROUTE_N,
          anchor,dir,2,20,candidates,3);
      assert(candidates[0].feasibility==EXCLUDED && candidates[1].feasibility==EXCLUDED);
      assert(candidates[2].feasibility==POSSIBLE);
      r.issues=UNVALIDATED;
      routeCandidates(r,navi_one::ROUTE_SPACING_MM,navi_one::ROUTE_N,
          anchor,dir,2,20,candidates,3);
      for(auto c:candidates)assert(c.feasibility==UNKNOWN);
      r.issues=0;
      routeCandidates(r,navi_one::ROUTE_SPACING_MM,navi_one::ROUTE_N,
          anchor,dir,2,NAN,candidates,3);
      for(auto c:candidates)assert(c.feasibility==UNKNOWN);
      ++cases;
    }
  }
  std::printf("PASS %u real-map anchor/direction cases; synthetic error windows only\n",cases);
}
