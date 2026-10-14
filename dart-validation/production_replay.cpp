#include <algorithm>
#include <vector>
#include <cstdio>
#include <iostream>
#include <string>
#define private public
#ifdef BASELINE
#include "../firmware/programs/NAVI_EWO_0_1_MM045_STOP/NaviIntegratedCore.h"
#else
#include "../firmware/programs/NAVI_EWO_0_4/NaviIntegratedCore.h"
#endif
#undef private
#ifndef BASELINE
#include "../firmware/programs/NAVI_EWO_0_4/NaviDartTelemetry.h"
#endif
using namespace navi_eyes;
int main(int argc,char** argv){FILE* diag=argc>1?std::fopen(argv[1],"w"):nullptr; (void)diag; NaviIntegratedCore n;char kind;uint64_t t;uint32_t serial=0;uint8_t pwm=40;while(std::cin>>kind>>t){
 if(kind=='S'){unsigned mm;int dir,ref;uint64_t anchor;std::cin>>mm>>dir>>anchor>>ref;
  n.declare(mm,dir,t);n.firstTargetAfterDeclare_=false;n.physicalPositionAnchored_=true;n.initialReferenceReady_=true;
  n.activeReference_=ref;n.physicalOriginUm_=n.openingIrUm_=anchor;
  n.targetOriginValid_=n.relationshipReliable_=true;n.expectedCumulativeUm_=uint64_t(n.target().distanceMm)*1000;
 }else if(kind=='I'){ir_movement::WireSnapshot w{};unsigned reason;std::cin>>w.sequence>>w.bootId>>w.capturedUs>>w.completedPulses>>reason;
  w.observedRises=w.completedPulses;w.nominalUm=w.completedPulses*w.pitchUm;w.opticalReason=reason;n.observeIr(w,t,pwm);
 }else if(kind=='H'){int raw;unsigned power,dir;std::cin>>raw>>power>>dir;pwm=power;HallSample h{};h.sampleSerial=++serial;h.timestampUs=t;h.raw=raw;h.pwm=power;h.direction=dir;n.observeHall(h,t);
 }else if(kind=='F'){
#ifndef BASELINE
 n.completeComparison(n.processedHallUs(),t);
#endif
 }
 EwoEvent e;while(n.takeEvent(e)){if(e.kind==EwoEventKind::TargetConfirmed || e.kind==EwoEventKind::MissedMagnet)
 std::printf("%llu,%u,%u,%u,%llu,%llu,%u\n",(unsigned long long)e.timestampUs,unsigned(e.kind),unsigned(e.mm),unsigned(e.target),(unsigned long long)e.openingIrUm,(unsigned long long)e.irUm,e.openingSerial);}
#ifndef BASELINE
 DartRecord r;while(n.takeDartRecord(r)){if(diag){char json[1200];formatDart(json,sizeof(json),r,1);std::fprintf(diag,"%s\n",json);}}
#endif
 }

#ifndef BASELINE
 std::fprintf(stderr,"openings=%u candidate=%u boundary=%llu origin=%llu latest=%llu ref=%d incomplete=%u pending_hwm=%zu\n",n.openingCount(),unsigned(n.candidateAvailable()),(unsigned long long)n.candidateBoundaryUm(),(unsigned long long)n.physicalOriginUm_,(unsigned long long)n.latestIr().nominalUm,n.activeReference(),unsigned(n.comparisonIncomplete()),n.pendingHighWater());
#endif
 if(diag)std::fclose(diag);
 std::fprintf(stderr,"mm=%u confirmations=%u misses=%u core_bytes=%zu\n",n.mm(),n.confirmedCount(),n.missedCount(),sizeof(n));}
