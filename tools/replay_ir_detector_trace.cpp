// Diagnostic only. Includes actual detector, exposing state to explain changes.
// Input from ir_raw_replay_input.py; optional begin/end sample indices restrict
// printed events, not warmup. Missing radio samples become synthetic sample
// gaps: this replay is NOT an exact reconstruction or a count-accuracy test.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#define private public
#include "IrMovementDetector.h"
#undef private

int main(int argc,char** argv) {
  if(argc!=3){std::fprintf(stderr,"usage: replay_ir_detector_trace begin_sample end_sample < samples\n");return 2;}
  const uint64_t begin=std::strtoull(argv[1],nullptr,10),end=std::strtoull(argv[2],nullptr,10);
  ir_movement::Detector d(false,true);
  unsigned index,raw,flags;uint64_t total=0,mismatches=0,missing=0;
  unsigned previous=0;bool have=false;
  while(std::scanf("%u %u %u",&index,&raw,&flags)==3) {
    if(have && index<=previous)return 2;
    if(index>end)break;
    if(have && index!=previous+1)missing+=index-previous-1;
    have=true;previous=index;
    const auto before=d;
    d.sample(uint64_t(index)*1000+1,raw);
    if(index<begin)continue;
    ++total;
    const unsigned replayFlags=(d.inPulse()?1:0)|(d.rise?2:0)|(d.fall?4:0)|(d.high-d.low>=120?8:0);
    if(replayFlags!=flags)++mismatches;
    if(d.aborts!=before.aborts || d.proven_!=before.proven_) {
      std::printf("sample=%u raw=%u reason=%u->%u abort=%llu->%llu env=%u/%u prior_reference=%u/%u proven=%u->%u\n",
        index,raw,before.reason,d.reason,(unsigned long long)before.aborts,(unsigned long long)d.aborts,
        d.low,d.high,before.provenLow_,before.provenHigh_,before.proven_,d.proven_);
    }
  }
  std::printf("DIAGNOSTIC_ONLY flags_differ=%llu compared=%llu missing_input_samples=%llu\n",
    (unsigned long long)mismatches,(unsigned long long)total,(unsigned long long)missing);
  return total?0:2;
}
