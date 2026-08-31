// Survey polarity replay: drives the 2026-08-28 circuit survey through the
// REAL HallCapture and compares the pole it derives from the completed passage
// against the pole the surveying firmware latched at the entry crossing.
//
// A disagreement is NOT automatically a regression. The survey was recorded by
// QUORUM 1.13X, whose latch has the defect of findings 05/06, so a disagreement
// may be this rule catching a mis-latch that has sat in the survey since August.
// Every one is printed with its evidence so it can be judged, not counted.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../HallCapture.h"
using namespace navi_one;

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
  std::string k=std::string("\"")+key+"\":"; size_t p=s.find(k); if(p==std::string::npos) return "";
  p+=k.size(); if(s[p]=='"'){ size_t e=s.find('"',p+1); return s.substr(p+1,e-p-1); }
  size_t e=p; while(e<s.size() && s[e]!=',' && s[e]!='}') ++e; return s.substr(p,e-p);
}
static bool run(const std::vector<int16_t>& d, Passage& out){
  CaptureConfig cfg; HallCapture<> cap(cfg);
  uint32_t t=0;
  for(; t<2600; ++t) cap.sample(t,BASE);
  if(!cap.ready()) return false;
  // Take the FIRST passage to close. A truncated survey record can contain a
  // trailing opposite-sign tail that opens a second passage of its own; an
  // earlier draft kept the last close and compared the tail against the
  // record's pole, which looked like a survey mis-latch and was not one.
  bool closed=false;
  for(size_t i=0;i<d.size() && !closed;++i,++t)
    if(cap.sample(t,(int16_t)(BASE+d[i]))){ out=cap.passage(); closed=true; }
  for(int i=0;i<40 && !closed;++i,++t)
    if(cap.sample(t,BASE)){ out=cap.passage(); closed=true; }
  return closed;
}

int main(int argc,char**argv){
  if(argc<2){ fprintf(stderr,"usage: replay_polarity_survey <survey.log>\n"); return 2; }
  FILE* f=fopen(argv[1],"r"); if(!f){ perror("open"); return 2; }
  int magnets=0, agree=0, noclose=0, disagree=0, trunc=0, truncDis=0;
  char* line=nullptr; size_t cap=0;
  while(getline(&line,&cap,f)>0){
    std::string s(line); size_t b=s.find('{'); if(b==std::string::npos) continue;
    s=s.substr(b);
    std::string rj=field(s,"rej"); if(rj.empty()) continue;
    if(atoi(rj.c_str())!=0) continue;                 // primary magnet passages only
    // A truncated record's stored buffer is not a faithful single passage --
    // it can begin mid-arc and end on a sustained rail -- so it cannot arbitrate
    // a polarity rule. Counted and reported, never scored.
    const bool truncated = atoi(field(s,"tr").c_str())!=0;
    if(truncated) ++trunc; else ++magnets;
    const int surveyPol = field(s,"pol")=="N";
    int sc=atoi(field(s,"sc").c_str()); if(!sc) sc=1;
    std::vector<int16_t> d=decode(field(s,"d"),sc);
    // The survey stores SIGNED baseline-relative counts -- orienting is the
    // caller's job (see replay_survey.cpp). So these are fed as recorded; an
    // earlier draft of this file negated them again and manufactured a 50%
    // disagreement rate out of its own bug.
    Passage p;
    if(!run(d,p)){ ++noclose; continue; }
    if(p.polarity==surveyPol) { if(!truncated) ++agree; continue; }
    if(truncated) { ++truncDis; continue; }
    ++disagree;
    if(disagree<=20)
      printf("  DISAGREE t=%-10s survey=%s  rule=%s  sum=%lld  peak=%u\n",
             field(s,"t").c_str(), surveyPol?"N":"S", p.polarity?"N":"S",
             (long long)p.signedSum, p.peakCounts);
  }
  free(line); fclose(f);
  printf("\nsurvey magnet passages, untruncated: %d\n", magnets);
  printf("  pole agrees with the surveying firmware: %d (%.3f%%)\n",
         agree, magnets? 100.0*agree/magnets : 0.0);
  printf("  disagreements needing inspection:        %d\n", disagree);
  printf("  did not close under replay:              %d\n", noclose);
  printf("  truncated records (excluded): %d, of which %d differ\n", trunc, truncDis);
  if(disagree==0 && noclose==0){ printf("SURVEY POLARITY REPLAY PASSED\n"); return 0; }
  printf("SURVEY POLARITY REPLAY: see above\n");
  return disagree? 1 : 0;
}
