#include "../NaviEvidenceBridge.h"
#include <cassert>
using namespace ngr_evidence;
int main(){
 NaviEvidenceBridge nav;
 IdentityEvidence c; c.expectedMm=24;c.expectedPolarity=Polarity::North;
 c.expectedDirection=1;c.irCredible=false;
 assert(!nav.observe(170,100,1000000,1,100,1,c).newEncounter);
 auto first=nav.observe(170,100,1001000,2,110,1,c);
 assert(first.newEncounter&&first.expectedMagnet&&first.proposedMm==24);
 assert(nav.history().size()==1);
 assert(!nav.observe(170,100,1002000,3,120,1,c).newEncounter);
 assert(!nav.observe(100,100,1003000,4,130,1,c).newEncounter);
 assert(!nav.observe(170,100,1004000,5,140,1,c).newEncounter);
 assert(!nav.observe(170,100,1005000,6,150,1,c).newEncounter);
 assert(nav.history().size()==1);
 assert(nav.recordConfirmed(0,24));
 assert(nav.verify([](uint8_t mm){return mm==24?Polarity::North:Polarity::South;}).matched==1);
 c.expectedMm=25;c.expectedPolarity=Polarity::South;
 assert(!nav.observe(100,100,1700000,7,170,1,c).newEncounter);
 assert(!nav.observe(20,100,1701000,8,180,1,c).newEncounter);
 auto second=nav.observe(20,100,1702000,9,190,1,c);
 assert(second.newEncounter&&second.expectedMagnet);
 assert(nav.history().size()==2);
 assert(nav.verify([](uint8_t mm){return mm==24?Polarity::North:Polarity::South;}).matched==1);
 nav.resetForDeclarationOrReversal();
 assert(nav.history().size()==0);
}
