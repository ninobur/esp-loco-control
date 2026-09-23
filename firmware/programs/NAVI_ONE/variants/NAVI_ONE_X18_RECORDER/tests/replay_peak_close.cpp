// ---------------------------------------------------------------------------
// TEST ONLY. Replays the operator's proposed peak-relative close rule against
// every waveform corpus in the repository, beside the production rule.
//
//   production : close when |raw - baseline| < exitMargin for exitHoldMs
//   proposed   : close when the signal has spent N consecutive ms below its
//                OWN running peak, measured against the frozen entry reference
//
// The question is not "does it close the latch" -- section G of
// gate_cruise_ceiling already shows that. It is whether it damages the
// measurement of ordinary magnets: does polarity survive, does peak survive,
// and above all does it ever SPLIT one physical magnet into two passages,
// which would double-advance position.
//
// No production header is changed and this is not in run_tests.sh.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "../HallCapture.h"
#include "HallCaptureCeiling.h"

static const int16_t BASE = 1834;

static int b64v(char c){
  if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26;
  if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63;
  return -1;
}
static std::vector<int16_t> decode(const std::string& b64,int scale){
  std::vector<int16_t> out; int acc=0,bits=0;
  for(char c: b64){ int v=b64v(c); if(v<0) continue; acc=(acc<<6)|v; bits+=6;
    if(bits>=8){ bits-=8; int byte=(acc>>bits)&0xff; out.push_back((int16_t)((byte-128)*scale)); } }
  return out;
}
static std::string field(const std::string& s,const char* key){
  std::string k=std::string("\"")+key+"\":"; size_t p=s.find(k);
  if(p==std::string::npos) return "";
  p+=k.size(); if(s[p]=='"'){ size_t e=s.find('"',p+1); return s.substr(p+1,e-p-1); }
  size_t e=p; while(e<s.size() && s[e]!=',' && s[e]!='}') ++e; return s.substr(p,e-p);
}

struct Result { int closes=0; uint8_t pol=0; uint16_t peak=0; uint32_t dur=0; };

// production
static Result runProd(const std::vector<int16_t>& d,int16_t entry){
  navi_one::CaptureConfig cfg; cfg.entryMargin=entry; cfg.floorMs=0;
  navi_one::HallCapture<> cap(cfg);
  uint32_t t=0; Result r;
  for(; t<2600; ++t) cap.sample(t,BASE,true);
  if(!cap.ready()) return r;
  for(size_t i=0;i<d.size();++i,++t)
    if(cap.sample(t,(int16_t)(BASE+d[i]),true)){
      if(!r.closes){ r.pol=cap.passage().polarity; r.peak=cap.passage().peakCounts;
                     r.dur=cap.passage().closedAtMs-cap.passage().openedAtMs; }
      ++r.closes;
    }
  for(int i=0;i<80;++i,++t)
    if(cap.sample(t,BASE,true)){
      if(!r.closes){ r.pol=cap.passage().polarity; r.peak=cap.passage().peakCounts;
                     r.dur=cap.passage().closedAtMs-cap.passage().openedAtMs; }
      ++r.closes;
    }
  return r;
}
// proposed
static Result runPeak(const std::vector<int16_t>& d,int16_t entry,uint16_t N,int16_t H){
  navi_ceiling::CeilingConfig cfg; cfg.entryMargin=entry; cfg.floorMs=0;
  cfg.peakCloseN=N; cfg.peakCloseHyst=H; cfg.peakCloseLockout=true;
  navi_ceiling::HallCaptureCeiling<> cap(cfg);
  uint32_t t=0; Result r;
  for(; t<2600; ++t) cap.sample(t,BASE,true);
  if(!cap.ready()) return r;
  for(size_t i=0;i<d.size();++i,++t)
    if(cap.sample(t,(int16_t)(BASE+d[i]),true)){
      if(!r.closes){ r.pol=cap.passage().polarity; r.peak=cap.passage().peakCounts;
                     r.dur=cap.passage().closedAtMs-cap.passage().openedAtMs; }
      ++r.closes;
    }
  for(int i=0;i<80;++i,++t)
    if(cap.sample(t,BASE,true)){
      if(!r.closes){ r.pol=cap.passage().polarity; r.peak=cap.passage().peakCounts;
                     r.dur=cap.passage().closedAtMs-cap.passage().openedAtMs; }
      ++r.closes;
    }
  return r;
}

int main(int argc,char**argv){
  if(argc<3){ fprintf(stderr,"usage: replay_peak_close <entryMargin> <log> [log...]\n"); return 2; }
  const int16_t ENTRY=(int16_t)atoi(argv[1]);
  struct Cfg { uint16_t N; int16_t H; };
  const Cfg cfgs[] = {{10,0},{10,3},{20,3},{20,8},{40,8}};
  const int NC = (int)(sizeof(cfgs)/sizeof(cfgs[0]));

  struct Tally { int n=0, splits=0, noclose=0, polDiff=0, peakDiff=0;
                 long durProd=0, durPeak=0; int prodNoclose=0;
                 std::vector<uint32_t> dq, dp; long totalCloses=0; int maxCloses=0; };
  Tally T[NC];
  int records=0, primaries=0;

  for(int a=2;a<argc;++a){
    FILE* f=fopen(argv[a],"r"); if(!f){ fprintf(stderr,"cannot open %s\n",argv[a]); continue; }
    char* line=nullptr; size_t cap=0;
    while(getline(&line,&cap,f)>0){
      std::string s(line); size_t b=s.find('{'); if(b==std::string::npos) continue;
      s=s.substr(b);
      if(field(s,"d").empty()) continue;
      ++records;
      const std::string rj=field(s,"rej");
      if(rj.empty()||atoi(rj.c_str())!=0) continue;     // admitted primaries only
      if(atoi(field(s,"tr").c_str())!=0) continue;       // untruncated only
      if(atoi(field(s,"pwm").c_str())<70) continue;      // cruise only
      ++primaries;
      int sc=atoi(field(s,"sc").c_str()); if(!sc) sc=1;
      std::vector<int16_t> d=decode(field(s,"d"),sc);
      Result P=runProd(d,ENTRY);
      for(int c=0;c<NC;++c){
        Result Q=runPeak(d,ENTRY,cfgs[c].N,cfgs[c].H);
        Tally& t=T[c]; ++t.n;
        if(P.closes==0) ++t.prodNoclose;
        t.totalCloses+=Q.closes; if(Q.closes>t.maxCloses) t.maxCloses=Q.closes;
        if(Q.closes==0) ++t.noclose;
        else if(Q.closes>1) ++t.splits;
        if(P.closes&&Q.closes){
          if(P.pol!=Q.pol) ++t.polDiff;
          if(P.peak!=Q.peak) ++t.peakDiff;
          t.durProd+=P.dur; t.durPeak+=Q.dur; t.dq.push_back(Q.dur); t.dp.push_back(P.dur);
        }
      }
    }
    free(line); fclose(f);
  }
  printf("records read: %d;  admitted, untruncated, PWM>=70: %d\n",records,primaries);
  printf("entry margin used: %d\n\n",ENTRY);
  printf("(the duration floor is DISABLED in both, so \"no close\" means no close)\n\n");
  printf("%-12s %6s %7s %8s %8s %9s | %s\n",
         "rule","n","SPLITS","noclose","pol dif","peak dif","proposed duration: min  p5  median  p95   under-82");
  for(int c=0;c<NC;++c){
    Tally& t=T[c];
    std::sort(t.dq.begin(),t.dq.end()); std::sort(t.dp.begin(),t.dp.end());
    size_t m=t.dq.size();
    int u82=0; for(uint32_t x:t.dq) if(x<82) ++u82;
    printf("N=%-3u H=%-4d %6d %7d %8d %8d %9d | %4u %4u %6u %5u   %d (%.1f%%)\n",
           cfgs[c].N,cfgs[c].H,t.n,t.splits,t.noclose,t.polDiff,t.peakDiff,
           m?t.dq[0]:0, m?t.dq[m/20]:0, m?t.dq[m/2]:0, m?t.dq[m*19/20]:0,
           u82, m?100.0*u82/m:0.0);
  }
  printf("\nHOW MANY PASSAGES PER MAGNET (proposed rule, floor disabled):\n");
  for(int c=0;c<NC;++c){
    printf("  N=%-3u H=%-4d  mean %.2f   max %d\n",cfgs[c].N,cfgs[c].H,
           T[c].n?(double)T[c].totalCloses/T[c].n:0.0,T[c].maxCloses);
  }
  { Tally& t=T[0]; std::sort(t.dp.begin(),t.dp.end()); size_t m=t.dp.size();
    int u82=0; for(uint32_t x:t.dp) if(x<82) ++u82;
    printf("%-12s %6zu %7s %8s %8s %9s | %4u %4u %6u %5u   %d (%.1f%%)\n",
           "PRODUCTION",m,"-","-","-","-",
           m?t.dp[0]:0,m?t.dp[m/20]:0,m?t.dp[m/2]:0,m?t.dp[m*19/20]:0,
           u82, m?100.0*u82/m:0.0); }
  printf("\nSPLITS is the one that matters: one physical magnet becoming two\n");
  printf("passages would double-advance position.\n");
  return 0;
}
