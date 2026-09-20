#include <cassert>
#include <iostream>
#include "SimpleNavigator.h"
using namespace navi_one;
int main() {
  Navigator n;
  Passage p;
  Verdict v;
  n.declare(10,1);
  assert(n.takeResetRequest());
  assert(!n.takeResetRequest());
  const auto next=nextMarker(10,1);
  p.polarity=polarityAt(next);
  assert(n.judge(p,v)==Ruling::Advanced);
  assert(n.status().navMm==next);
  p.polarity=1-polarityAt(nextMarker(next,1));
  assert(n.judge(p,v)==Ruling::WrongMagnet);
  assert(!n.positionKnown());
  assert(n.judge(p,v)==Ruling::NoPosition);
  n.declare(next,-1);
  assert(n.positionKnown());
  std::cout<<"simple navigator checks passed\n";
}
