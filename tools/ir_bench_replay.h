// Timing-reconstructed replay of recorded IR TX raw captures through a
// detector class (the actual firmware Detector, or a review proposal).
//
// What is observed vs assumed (see docs/IR_TX_R2_BENCH_FAILURE_REVIEW_20260924.md):
// - OBSERVED: every replayed ADC value, and per sample the transmitted
//   CONTRAST/RISE/FALL/INPULSE flag bits; per 96-sample batch the envelope
//   (runMin/runMax) at the batch's first sample and cumulative rises (`pulses`)
//   and aborts (`latch`) at its last sample; 10 Hz type-5 snapshots.
// - ASSUMED: sample timestamps. The TX sketch reported late=0 and missed=0,
//   so each sample is within 250 us of its 1 ms slot; the replay uses
//   us = 1000*k - slips. A "slip" delays one 50 ms envelope update by one
//   sample, which is what microsecond jitter does on the ESP32. The slip
//   schedule is FITTED to the envelope-dependent flag bits and batch headers;
//   it is the only fitted quantity. The detector's rise/fall/in-pulse
//   decisions and abort counts are then checked against observation, not fitted.
// - Detector state is not transmitted. Each contiguous raw range starts a
//   fresh detector; agreement is reported only after the replay has converged
//   (see Segment::syncedFrom).
#pragma once
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <utility>

namespace ir_bench {

struct Batch {
  uint32_t first=0; int runMin=0, runMax=0, thrHigh=0, thrLow=0;
  uint32_t missedTotal=0, pulses=0, latch=0, contrastLoss=0;
  uint16_t w[96]{};
};
struct Move {
  uint32_t seq=0; uint64_t us=0, rises=0, completed=0, aborts=0, gaps=0, sat=0, unreliable=0;
  int reason=0, span=0;
};
struct Capture {
  std::vector<Batch> batches;   // sorted by first sample
  std::vector<Move> moves;      // sorted by sequence
  std::map<uint32_t,size_t> batchAt;  // first sample -> index
};

inline Capture load(const char* path) {
  Capture c; FILE* f=std::fopen(path,"r");
  if(!f){std::fprintf(stderr,"cannot open %s\n",path);std::exit(2);}
  char tag;
  while(std::fscanf(f," %c",&tag)==1){
    if(tag=='B'){
      Batch b; unsigned w;
      if(std::fscanf(f,"%u %*u %d %d %d %d %u %u %u %u",&b.first,&b.runMin,&b.runMax,
                     &b.thrHigh,&b.thrLow,&b.missedTotal,&b.pulses,&b.latch,&b.contrastLoss)!=9)std::exit(3);
      for(int i=0;i<96;++i){if(std::fscanf(f,"%u",&w)!=1)std::exit(3);b.w[i]=(uint16_t)w;}
      c.batchAt[b.first]=c.batches.size(); c.batches.push_back(b);
    } else if(tag=='M'){
      Move m; unsigned long long v[8];
      if(std::fscanf(f,"%u %llu %llu %llu %llu %llu %llu %llu %d %d",&m.seq,&v[0],&v[1],&v[2],&v[3],
                     &v[4],&v[5],&v[6],&m.reason,&m.span)!=10)std::exit(3);
      m.us=v[0];m.rises=v[1];m.completed=v[2];m.aborts=v[3];m.gaps=v[4];m.sat=v[5];m.unreliable=v[6];
      c.moves.push_back(m);
    } else { std::exit(3); }
  }
  std::fclose(f);
  return c;
}

// A contiguous received range: batches [b0,b1), samples [first,end).
struct Range { size_t b0, b1; uint32_t first, end; };
inline std::vector<Range> ranges(const Capture& c) {
  std::vector<Range> r;
  for(size_t i=0;i<c.batches.size();){
    size_t j=i+1;
    while(j<c.batches.size() && c.batches[j].first==c.batches[j-1].first+96)++j;
    r.push_back({i,j,c.batches[i].first,c.batches[j-1].first+96});
    i=j;
  }
  return r;
}

enum : uint16_t { F_INPULSE=0x1000, F_RISE=0x2000, F_FALL=0x4000, F_CONTRAST=0x8000, F_MASK=0xf000 };

template<class D> uint16_t flagsOf(const D& d) {
  const int span=(int)d.high-(int)d.low; uint16_t f=0;
  if(span>=120)f|=F_CONTRAST; if(d.rise)f|=F_RISE; if(d.fall)f|=F_FALL; if(d.inPulse())f|=F_INPULSE;
  return f;
}

// Per-sample replay record.
struct Rec {
  uint8_t reason=0; uint16_t flagsObs=0, flagsRep=0, raw=0;
  uint32_t low=0, high=0;
  uint64_t completed=0, rises=0, aborts=0;
};

// Per-sample hook: called with (k, raw, us, before, after).
template<class D> struct NoHook { void operator()(uint32_t,uint16_t,uint64_t,const D&,const D&){} };

template<class D> struct Segment {
  Range range; uint32_t start=0;  // first replayed sample
  std::vector<Rec> rec;           // indexed by k-start
  uint32_t slips=0, scheduleViolations=0; unsigned fitCost=0;
  uint32_t syncedFrom=0;          // first sample after which comparisons are reported
  int64_t completedOffset=0;      // absolute count = replay count + offset (from first synced snapshot)
  const Rec* at(uint32_t k) const { return k>=start && k-start<rec.size() ? &rec[k-start] : nullptr; }
};

// Envelope of a window, as IrMovementDetector.h computes it on an update
// sample (5th/95th percentile of raw>>4 buckets). Used ONLY to choose the
// update schedule below; every replayed value then comes from the real
// detector, and the replay asserts the detector updated exactly on schedule.
struct EnvelopeWindow {
  uint16_t w[512]{}; uint16_t hist[256]{}; unsigned fill=0, index=0;
  void push(uint16_t raw){ if(fill==512)--hist[w[index]]; else ++fill; w[index]=raw>>4; ++hist[raw>>4]; index=(index+1)%512; }
  std::pair<unsigned,unsigned> env() const {
    unsigned sum=0; int lo=-1, hi=-1;
    for(int b=0;b<256;++b){ sum+=hist[b]; if(lo<0&&sum>fill*5/100)lo=b; if(hi<0&&sum>fill*95/100)hi=b; }
    return {unsigned(lo*16),unsigned(hi*16+15)};
  }
};

// Replays one contiguous range through a fresh detector from `make`.
// The ESP32 recomputes its envelope on the first sample at least 50,000 us
// after the previous update; with late=0 that is always 50 or 51 samples
// later, depending on microsecond jitter that was not transmitted. The
// schedule is chosen by dynamic programming to minimise disagreement with
// the transmitted CONTRAST bits, batch-start runMin/runMax and snapshot
// spans (the first 512
// samples, whose ESP32 window predates the received range, are ignored).
// RISE/FALL/INPULSE bits, rises, aborts and snapshot reasons are NOT used for the
// fit; they are the independent check.
template<class D, class Make, class Hook=NoHook<D>>
Segment<D> replay(const Capture& c, const Range& r, Make make, Hook hook=Hook{}, long snapshotOffsetMs=-1) {
  auto rawAt=[&](uint32_t k)->uint16_t{ const Batch& b=c.batches[r.b0+(k-r.first)/96]; return b.w[(k-r.first)%96]; };
  auto hdrAt=[&](uint32_t k)->const Batch*{ return (k-r.first)%96==0 ? &c.batches[r.b0+(k-r.first)/96] : nullptr; };
  const uint32_t n=r.end-r.first;
  std::vector<std::pair<unsigned,unsigned>> env(n);
  { EnvelopeWindow w; for(uint32_t i=0;i<n;++i){ w.push(rawAt(r.first+i)&0xfff); env[i]=w.env(); } }
  // Snapshot spans (high-low) also pin the envelope, at 10 Hz.
  std::map<uint32_t,int> snapSpan;
  if(snapshotOffsetMs>=0) for(const Move& m:c.moves){
    const long k=(long)(m.us/1000)-snapshotOffsetMs; if(k>=(long)r.first && k<(long)r.end) snapSpan[(uint32_t)k]=m.span; }
  // Cost of holding env(a) on samples [a,b).
  auto hold=[&](uint32_t a,uint32_t b)->unsigned{
    unsigned m=0; const auto e=env[a]; const bool con=(int)e.second-(int)e.first>=120;
    for(uint32_t i=std::max<uint32_t>(a,512);i<b;++i){
      const uint32_t k=r.first+i;
      m+= ((rawAt(k)&F_CONTRAST)!=0)!=con;
      if(const Batch* h=hdrAt(k)) m+= (h->runMin!=(int)e.first || h->runMax!=(int)e.second);
      if(!snapSpan.empty()){ auto it=snapSpan.find(k); if(it!=snapSpan.end()) m+= it->second!=(int)e.second-(int)e.first; }
    }
    return m;
  };
  const unsigned INF=~0u/2; std::vector<unsigned> cost(n,INF); std::vector<uint8_t> from(n,0);
  for(uint32_t u=0;u<50&&u<n;++u) cost[u]=0;
  for(uint32_t u=50;u<n;++u) for(uint32_t g=50;g<=51;++g) if(u>=g && cost[u-g]<INF){
    const unsigned v=cost[u-g]+hold(u-g,u); if(v<cost[u]){cost[u]=v;from[u]=(uint8_t)g;}
  }
  uint32_t last=n-1; unsigned bestEnd=INF;
  for(uint32_t u=n>50?n-50:0;u<n;++u){ const unsigned v=cost[u]<INF?cost[u]+hold(u,n):INF; if(v<bestEnd){bestEnd=v;last=u;} }
  std::vector<uint32_t> sched; for(uint32_t u=last;;){ sched.push_back(u); if(!from[u]) break; u-=from[u]; }
  std::reverse(sched.begin(),sched.end());
  Segment<D> seg; seg.range=r; seg.start=r.first+sched[0]; seg.fitCost=bestEnd;
  D d=make(); uint64_t slipUs=0; size_t next=1; const uint64_t base=1000000000ULL;
  for(uint32_t k=seg.start;k<r.end;++k){
    const uint32_t i=k-r.first;
    if(next<sched.size() && sched[next]-sched[next-1]==51 && i==sched[next-1]+50){ ++slipUs; ++seg.slips; }
    const uint64_t us=base+1000ULL*k-slipUs; const uint16_t raw=rawAt(k)&0xfff;
    D before=d; d.sample(us,raw);
    const bool updated=d.lastEnvelope_==us, due=next<sched.size() && i==sched[next];
    if(i!=sched[0] && updated!=due){ ++seg.scheduleViolations; if(std::getenv("IR_REPLAY_VERBOSE")) std::printf("  schedule k=%u i=%u updated=%d due=%d next=%u gap=%u\n",k,i,updated,due,next<sched.size()?sched[next]:0,next<sched.size()?sched[next]-sched[next-1]:0); }
    if(due) ++next;
    hook(k,raw,us,before,d);
    Rec x; x.reason=d.reason; x.flagsObs=rawAt(k)&F_MASK; x.flagsRep=flagsOf(d); x.raw=raw;
    x.low=d.low; x.high=d.high; x.completed=d.completed; x.rises=d.rises; x.aborts=d.aborts;
    seg.rec.push_back(x);
  }
  // Converged once replayed and transmitted flags agree for 2000 consecutive samples.
  uint32_t streak=0; seg.syncedFrom=r.end;
  for(uint32_t k=seg.start;k<r.end;++k){
    const Rec& x=seg.rec[k-seg.start];
    streak = x.flagsObs==x.flagsRep ? streak+1 : 0;
    if(streak==2000){ seg.syncedFrom=k-1999; break; }
  }
  return seg;
}

// Maps a type-5 snapshot to its ESP32 sample index: capturedUs is the
// timestamp of the sample that produced it, and sample k was taken at
// t0 + ~1000*k. Fits t0 (whole ms) so snapshot rises equal the rises
// reconstructed from raw headers and RISE bits.
inline long fitSnapshotOffsetMs(const Capture& c) {
  std::map<uint32_t,uint64_t> risesAt;  // sample -> cumulative rises after it
  for(const Batch& b:c.batches){
    uint64_t v=b.pulses;
    for(int i=95;i>=0;--i){ risesAt[b.first+i]=v; if(b.w[i]&F_RISE)--v; }
  }
  long best=0; size_t bestScore=0;
  for(long t=0;t<3000;++t){
    size_t score=0;
    for(const Move& m:c.moves){
      const long k=(long)(m.us/1000)-t; auto it=risesAt.find((uint32_t)k);
      if(k>=0 && it!=risesAt.end() && it->second==m.rises)++score;
    }
    if(score>bestScore){bestScore=score;best=t;}
  }
  return best;
}

inline const char* reasonName(int r){
  static const char* n[]={"PRIMING","INADEQUATE_CONTRAST","SATURATION","SAMPLE_GAP","SIGNAL_STALE","REACQUIRING","TRACKING"};
  return r>=0&&r<=6?n[r]:"?";
}
} // namespace ir_bench
