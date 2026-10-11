#include "../NaviIrEvidence.h"
#include <cassert>
using namespace ngr_evidence;
int main(){
 IrPulseEvidence p;
 assert(p.observe(1,60000,90).finding==PulseFinding::Baseline);
 for(unsigned i=2;i<=6;++i)assert(p.observe(i,i*60000,90).finding==PulseFinding::Baseline);
 assert(p.observe(7,420000,90).finding==PulseFinding::Normal);
 auto a=p.observe(8,600000,90);
 assert(a.finding==PulseFinding::TimingAnomaly && a.intervalUs==180000 && !a.questionable);
 a=p.observe(9,660000,90);
 assert(a.finding==PulseFinding::BracketedMissingCompatible && a.questionable);
 assert(p.observe(12,840000,90).finding==PulseFinding::ObservationGap);
 assert(p.observe(13,900000,90).finding==PulseFinding::Baseline);
 assert(p.observe(14,960000,95).finding==PulseFinding::Baseline);
 assert(p.observe(15,950000,95).finding==PulseFinding::ClockError);
 // A prolonged slowdown never supplies a returning normal bracket.
 p.reset();for(unsigned i=1;i<=7;++i)p.observe(i,i*60000,90);
 assert(!p.observe(8,600000,90).questionable);
 assert(!p.observe(9,780000,90).questionable);
}
