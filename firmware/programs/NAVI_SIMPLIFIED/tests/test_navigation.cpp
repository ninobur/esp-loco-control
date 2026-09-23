#include <cassert>
#include <cstdint>
#include <iostream>
#include "SimpleHall.h"
#include "SimpleNavigator.h"
#include "Reachability.h"
#define NAVI_APPROACH_MARKER_MS {1213,1340,1496,1694,1952}
#include "Stations.h"
using namespace navi_one;

int main() {
  SimpleHall h;
  for (uint32_t t=0;t<2000;++t) h.sample(t,1900,0);
  assert(h.ready());
  assert(h.sample(2000,1970,90).event==HallEvent::None);
  HallDecision d=h.sample(2001,1970,90);
  assert(d.event==HallEvent::Open && d.openedMs==2001 && d.polarity==1);
  assert(physicallyUnreachable(279,280,1000));
  assert(!physicallyUnreachable(280,280,1000));
  assert(physicallyUnreachable(299,300,1000));
  assert(!physicallyUnreachable(300,300,1000));
  assert(!physicallyUnreachable(1,280,0)); // absent bound fails open
  // The actual route, both travel directions: every ten-opening word unique.
  for (int dir : {-1,+1}) {
    for (int end=0;end<ROUTE_N;++end) {
      int matches=0;
      for (int other=0;other<ROUTE_N;++other) {
        bool same=true;
        for (int i=0;i<10;++i)
          if (polarityAt(routeMod(end-dir*(9-i)))!=
              polarityAt(routeMod(other-dir*(9-i)))) same=false;
        if (same) ++matches;
      }
      assert(matches==1);
    }
  }

  Navigator n;
  n.declare(0,+1);
  assert(n.takeResetRequest());
  Passage p{};
  p.polarity=polarityAt(1);
  assert(n.judge(p)==Ruling::Advanced && n.status().navMm==1);

  // A shifted clean word must reestablish the position from observed polarity,
  // without writing a guessed position on the first contradiction.
  int start=0;
  for (int i=0;i<ROUTE_N;++i) if (polarityAt(i)!=polarityAt(2)) {start=i;break;}
  const uint8_t old=n.status().navMm;
  for (int i=0;i<10;++i) {
    p.polarity=polarityAt(routeMod(start+i));
    Ruling r=n.judge(p);
    if (i==0) {assert(r==Ruling::Ambiguous); assert(n.status().navMm==old);}
    if (i==9) {assert(r==Ruling::Reestablished); assert(n.status().navMm==routeMod(start+9));}
  }
  assert(n.positionKnown() && n.status().unresolvedCount==0);

  // An unmatched word runs to ten SUBSEQUENT observations, then stops.
  n.declare(0,+1);
  p.polarity=1-polarityAt(1);
  assert(n.judge(p)==Ruling::Ambiguous && n.status().unresolvedCount==0);
  for (int i=1;i<=10;++i) {
    p.polarity=1-polarityAt(1);
    Ruling r=n.judge(p);
    if (i<10) assert(r==Ruling::Ambiguous && n.status().unresolvedCount==i);
    else assert(r==Ruling::UnresolvedLimitStop);
  }
  assert(!n.positionKnown());
  n.declare(0,+1);
  assert(n.positionKnown());
  assert(STATION_DWELL_MS==5000UL);
  std::cout << "NAVI_SIMPLIFIED navigation checks passed\n";
}
