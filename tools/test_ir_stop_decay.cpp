// Desired stop-retention behavior: intentionally fails on reviewed R2.
// --expect-r2-failure verifies the precise known baseline, not release readiness.
#include "IrSpeedTelemetry.h"
#include <cmath>
#include <cstdio>
#include <cstring>

struct Outcome { unsigned reason;uint64_t aborts;bool zero;bool referenceValid; };
static Outcome run(bool smooth,unsigned resting) {
  ir_movement::Measurement tx(123,0,9.652,true);
  ngr_nav::IrHealthMonitor rx;ngr_nav::IrSpeedTelemetry speed;
  ngr_nav::MmDistanceReference ref;
  const uint8_t mac[6]={2,3,4,5,6,7};rx.pair(mac,0);
  ngr_nav::IrSpeedReading reading;
  uint32_t seq=0;
  uint64_t before=0;
  for(unsigned i=0;i<6000;++i) {
    const uint64_t us=uint64_t(i+1)*1000;
    unsigned raw=resting;
    if(i<4000)raw=smooth?unsigned(1500-500*std::cos(double(i%200)/200*6.283185307179586)):
                                (i%200<100?1000:2000);
    tx.sample(us,raw);
    if(us%100000==0) {
      const auto& d=tx.detector();auto w=ir_movement::encode(tx.snapshot(),++seq,d.high-d.low);
      w.crc=ngr_nav::movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(ngr_nav::WireSnapshot,crc));
      rx.receive(mac,reinterpret_cast<const uint8_t*>(&w),sizeof(w),us+10000);
    }
    if(us%1000000==0)reading=speed.sample(rx,us+10000,true,true);
    if(i==3999) {
      if(!ref.synchronize(41,4000,rx.odometry().point(),rx.odometry()))return {255,0,false,false};
      before=tx.detector().aborts;
    }
  }
  return {unsigned(tx.detector().reason),tx.detector().aborts-before,
          reading.valid && reading.mmps==0,ref.validFor(rx.odometry())};
}
int main(int argc,char** argv) {
  const bool baseline=argc==2 && !std::strcmp(argv[1],"--expect-r2-failure");
  if(argc>1 && !baseline)return 2;
  unsigned failures=0;bool baselineMatches=true;
  for(unsigned caseId=0;caseId<5;++caseId) {
    const bool smooth=caseId!=0;
    const unsigned resting=caseId<2?1200:caseId==2?1000:caseId==3?2000:1500;
    const auto out=run(smooth,resting);
    const bool wantZero=caseId!=4;
    bool pass=(out.zero==wantZero) && out.reason!=255;
    // A continuity break may not silently restore an earlier MM reference.
    pass=pass && (!out.aborts || !out.referenceValid);
    if(!pass)++failures;
    baselineMatches=baselineMatches && (caseId==1?!out.zero && out.aborts==2 && out.reason==1:pass);
    std::printf("%s smooth=%u resting=%u final_reason=%u abort_delta=%llu valid_zero=%u old_mm_valid=%u\n",
      pass?"PASS":"FAIL",smooth,resting,out.reason,(unsigned long long)out.aborts,out.zero,out.referenceValid);
  }
  if(baseline){std::puts("Known-R2 reproduction mode: NOT an acceptance pass");return baselineMatches&&failures==1?0:1;}
  return failures?1:0;
}
