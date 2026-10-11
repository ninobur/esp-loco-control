#include "../NaviEvidenceCore.h"
#include <cassert>
using namespace ngr_evidence;
int main(){
 HallQualifier h; Encounter e{};
 assert(!h.observe(170,100,1000000,1,500,e));
 assert(h.observe(171,100,1001000,2,510,e));
 assert(e.openedUs==1000000 && e.detectedUs==1001000 && e.irUm==500);
 assert(!h.observe(100,100,1010000,3,600,e));
 assert(!h.observe(180,100,1020000,4,700,e));
 assert(!h.observe(180,100,1021000,5,710,e));
 assert(!h.observe(100,100,1700000,6,900,e));
 assert(!h.observe(20,100,1701000,7,910,e));
 assert(h.observe(20,100,1702000,8,920,e));
 DnaHistory d; d.append(e,1); assert(d.size()==1);
 assert(d.identify(0,24)); assert(d.at(0).mm==24);
 for(int i=0;i<15;i++){ Encounter n=e; n.serial+=i+1; d.append(n,1); }
 assert(d.size()==12); assert(!d.at(11).identified);
}
