#include "../NaviSequentialJudgment.h"
#include <cassert>
using namespace ngr_evidence;
int main(){
  Encounter e{}; e.polarity=Polarity::North;
  IdentityEvidence c{}; c.expectedMm=24;
  c.expectedPolarity=Polarity::North;
  c.expectedDirection=1; c.observedDirection=1;
  c.irCredible=false; c.irInsideWindow=false;
  auto r=judgeExpected(e,c);
  assert(r.ruling==IdentityRuling::ExpectedMagnet && r.mm==24);
  c.irCredible=true;
  r=judgeExpected(e,c);
  assert(r.ruling==IdentityRuling::ExpectedMagnet && r.irDisagrees);
  e.polarity=Polarity::South;
  r=judgeExpected(e,c);
  assert(r.ruling==IdentityRuling::ContradictoryPolarity);
  e.polarity=Polarity::North;c.observedDirection=-1;
  r=judgeExpected(e,c);
  assert(r.ruling==IdentityRuling::WrongDirection);
  c.expectedDirection=0;c.observedDirection=0;
  assert(judgeExpected(e,c).ruling==IdentityRuling::WrongDirection);
}
