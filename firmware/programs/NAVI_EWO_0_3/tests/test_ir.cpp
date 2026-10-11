#include "../NaviIrEvidence.h"
#include <cassert>
using namespace ngr_evidence;
int main(){
 IrPulseEvidence p;
 assert(p.observe(1,100000,90).finding==PulseFinding::Baseline);
 assert(p.observe(2,160000,90).finding==PulseFinding::Baseline);
 assert(p.observe(3,220000,90).finding==PulseFinding::Normal);
 auto a=p.observe(4,400000,90);
 assert(a.finding==PulseFinding::TimingAnomaly && a.intervalUs==180000);
 assert(p.observe(5,460000,90).finding==PulseFinding::Normal);
 assert(p.observe(8,640000,90).finding==PulseFinding::ObservationGap);
 assert(p.observe(9,700000,90).finding==PulseFinding::Baseline);
 assert(p.observe(10,760000,95).finding==PulseFinding::Baseline);
 assert(p.observe(11,750000,95).finding==PulseFinding::ClockError);
}
