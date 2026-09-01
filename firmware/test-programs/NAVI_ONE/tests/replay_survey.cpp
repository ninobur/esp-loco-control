// Survey replay gate: runs the REAL MagnetRecognizer header over the
// 2026-08-28 circuit survey, in time order, and checks the labels.
//   rej=0  a primary magnet passage      -> must be accepted
//   rej=1  short electrical excursion    -> must be refused
//   rej=2  sub-threshold magnet rebound  -> must be refused
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "../MagnetRecognizer.h"
using namespace navi_one;

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
int main(int argc,char**argv){
  if(argc<2){ fprintf(stderr,"usage: replay_survey <survey.log>\n"); return 2; }
  FILE* f=fopen(argv[1],"r"); if(!f){ perror("open"); return 2; }
  RecognizerConfig cfg; MagnetRecognizer rec(cfg);
  std::map<int,std::map<std::string,int>> tally;   // rej -> outcome -> n
  int total=0; std::vector<std::string> wrong;
  char* line=nullptr; size_t cap=0;
  while(getline(&line,&cap,f)>0){
    std::string s(line); size_t b=s.find('{'); if(b==std::string::npos) continue;
    s=s.substr(b);
    std::string rj=field(s,"rej"); if(rj.empty()) continue;
    int rej=atoi(rj.c_str());
    Passage p;
    p.openedAtMs=(uint32_t)strtoul(field(s,"t").c_str(),nullptr,10);
    p.peakCounts=(uint16_t)atoi(field(s,"pk").c_str());
    uint16_t dur=(uint16_t)atoi(field(s,"dur").c_str());
    p.closedAtMs=p.openedAtMs+dur;
    p.polarity = field(s,"pol")=="N";
    p.preSamples=(uint16_t)atoi(field(s,"pre").c_str());
    p.truncated=atoi(field(s,"tr").c_str())!=0;
    p.clipped=atoi(field(s,"clip").c_str())!=0;
    int sc=atoi(field(s,"sc").c_str()); if(!sc) sc=1;
    std::vector<int16_t> samples=decode(field(s,"d"),sc);
    // Stored samples are SIGNED baseline-relative counts. Orienting the
    // passage's own pole positive is the caller's job -- the recognizer must
    // never see N or S. On the locomotive HallCapture does this; here the
    // replay must do the same or it is testing a different function.
    if(!p.polarity) for(auto& x: samples) x = (int16_t)(-x);
    p.oriented=samples.data(); p.sampleCount=(uint16_t)samples.size();
    Verdict v=rec.examine(p);
    tally[rej][outcomeName(v.outcome)]++;
    total++;
    bool shouldAccept = (rej==0);
    if(v.isMagnet!=shouldAccept){
      char buf[220];
      snprintf(buf,sizeof(buf),"rej=%d t=%u pk=%u dur=%u ratio=%.3f resid=%.4f gap=%u -> %s",
               rej,p.openedAtMs,p.peakCounts,dur,v.amplitudeRatio,v.residual,v.gapMs,
               outcomeName(v.outcome));
      wrong.push_back(buf);
    }
  }
  fclose(f);
  printf("records replayed: %d\n\n", total);
  const char* label[3]={"rej=0 PRIMARY (must accept)","rej=1 SHORT (must refuse)","rej=2 REBOUND (must refuse)"};
  for(auto& kv: tally){
    int n=0; for(auto& o: kv.second) n+=o.second;
    printf("  %-30s n=%d\n", (kv.first>=0&&kv.first<3)?label[kv.first]:"rej=?", n);
    for(auto& o: kv.second) printf("      %-12s %d\n", o.first.c_str(), o.second);
  }
  printf("\nmisclassified: %zu\n", wrong.size());
  for(size_t i=0;i<wrong.size() && i<12;i++) printf("   %s\n", wrong[i].c_str());
  if(wrong.empty()){ printf("\nPASS: every primary accepted, no non-primary counted\n"); return 0; }
  printf("\nFAIL\n"); return 1;
}
