#include "../NaviEvidenceCore.h"
#include <cassert>
using namespace ngr_evidence;
int main() {
  HallQualifier h; Encounter e{};
  assert(!h.observe(170,100,1000000,1,500,e));
  assert(h.observe(171,100,1001000,2,510,e));
  assert(e.openedUs==1000000 && e.detectedUs==1001000 && e.irUm==500);
  // A continuous field cannot retrigger even after the 650 ms timer expires.
  assert(!h.observe(171,100,1700000,3,600,e));
  assert(!h.observe(100,100,1701000,4,610,e));
  assert(!h.observe(180,100,1702000,5,620,e));
  assert(h.observe(180,100,1703000,6,630,e));
  assert(!h.observe(100,100,1710000,7,640,e));
  assert(!h.observe(180,100,1711000,8,650,e));
  assert(!h.observe(180,100,1712000,9,660,e)); // repeat inside guard
  // A packet arriving between opening and detection must not make a duplicate
  // seem farther from the preceding encounter than its actual opening was.
  HallQualifier boundary;
  assert(!boundary.observe(170,100,1000,1,1000,e,true,850,true));
  assert(boundary.observe(170,100,2000,2,1000,e,true,850,true));
  assert(!boundary.observe(100,100,700000,3,1000,e,true,850,true));
  assert(!boundary.observe(170,100,701000,4,1800,e,true,850,true));
  assert(!boundary.observe(170,100,702000,5,1900,e,true,850,true));
  DnaHistory d; d.append(e,1); assert(d.identify(0,24));
  const auto result=verifyIdentified(d,[](uint8_t mm){
    return mm==24?Polarity::South:Polarity::North;
  });
  assert(result.compared==1 && result.contradictions==1);
  for(int i=0;i<12;i++){Encounter n=e;n.serial+=i+1;d.append(n,1);}
  assert(d.size()==12 && !d.at(11).identified);
}
