// Real-lap gate. Replays the 2026-08-29 undeclared circuit -- 172 genuine Hall
// detections, recorded by NAVI_2 with position UNSET so nothing judged them --
// through the real Navigator.
//
// This exercises IDENTITY only: the field record carries polarity, peak and
// duration but not waveforms, so the recognizer's shape test cannot run here.
// Passages are presented as already-recognised. What it proves is that the map,
// the declaration, the one-target rule and the advance are correct against a
// real circuit, out of sample.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../Navigator.h"
using namespace navi_one;

static std::string field(const std::string& s,const char* key){
  std::string k=std::string("\"")+key+"\":"; size_t p=s.find(k);
  if(p==std::string::npos) return "";
  p+=k.size(); if(s[p]=='"'){ size_t e=s.find('"',p+1); return s.substr(p+1,e-p-1); }
  size_t e=p; while(e<s.size() && s[e]!=',' && s[e]!='}') ++e; return s.substr(p,e-p);
}
int main(int argc,char**argv){
  if(argc<2){ fprintf(stderr,"usage: replay_lap <markers.tsv>\n"); return 2; }
  FILE* f=fopen(argv[1],"r"); if(!f){ perror("open"); return 2; }
  // THE RECORD IS NOT ALL ONE RUN, AND THE GATE MUST SAY SO.
  //
  // 0.1's gate replayed `i < 172` and stopped. The file holds 177 records: the
  // last five are a later, separately-declared segment at 18:09 appended to
  // the same TSV. The cap was right for the lap and silent about the rest --
  // a gate quietly ignoring records inside its own evidence file is the sort
  // of thing this project's records exist to prevent. The two runs are
  // distinguishable in the data itself, so select on the data: the circuit was
  // driven with position UNSET, so every one of its records carries mm 0 and
  // timing_gate NO_POSITION. Nothing is skipped by position in the file.
  std::vector<uint8_t> pol; std::vector<int> pk;
  size_t lapRecords=0, otherRecords=0;
  char* line=nullptr; size_t cap=0;
  while(getline(&line,&cap,f)>0){
    std::string s(line); size_t b=s.find('{'); if(b==std::string::npos) continue;
    s=s.substr(b);
    std::string o=field(s,"obs"); if(o.empty()) continue;
    const bool undeclared = field(s,"timing_gate")=="NO_POSITION" && field(s,"mm")=="0";
    if(!undeclared){ ++otherRecords; continue; }
    ++lapRecords;
    pol.push_back(o=="N"); pk.push_back(atoi(field(s,"peak").c_str()));
  }
  fclose(f);
  printf("records in the file: %zu undeclared-circuit + %zu from the later "
         "18:09 declared segment\n", lapRecords, otherRecords);
  if(lapRecords!=172 || otherRecords!=5){
    printf("\nFAIL: expected 172 circuit records and 5 later ones\n");
    return 1;
  }

  RecognizerConfig cfg; MagnetRecognizer rec(cfg); Navigator nav;
  nav.declare(40,+1); if(nav.takeResetRequest()) rec.reset();   // operator started 040-041 CW
  printf("declared MM040 CW; first target is MM%03u\n", nav.status().target);

  size_t advanced=0, refused=0; int firstRefusalAt=-1; int closedAt=-1;
  for(size_t i=0;i<pol.size();i++){
    Passage p; p.polarity=pol[i]; p.peakCounts=(uint16_t)pk[i];
    Verdict v; v.isMagnet=true;             // recognizer stands in: see header
    Ruling r=nav.judge(p,v);
    if(r==Ruling::Advanced){ advanced++;
      // advance 171 must return to the declared marker: that IS the circuit
      if(advanced==ROUTE_N) closedAt=(int)nav.status().navMm;
    } else { refused++; if(firstRefusalAt<0) firstRefusalAt=(int)i; }
  }
  const NavStatus& s=nav.status();
  printf("\nadvanced %zu   refused %zu\n", advanced, refused);
  printf("final navMm %u   trust %s   sequence says %u\n",
         s.navMm, trustName(s.trust), s.seqAt);
  if(firstRefusalAt>=0) printf("first refusal at detection #%d\n", firstRefusalAt);

  printf("circuit closed at advance %u: MM%03d (declared MM040)\n", ROUTE_N, closedAt);
  // 171 advances returns to the declared marker; the 172nd detection is the
  // start magnet met a second time, so the run ends one past it.
  bool ok = (advanced==172) && (refused==0) && (closedAt==40) &&
            (s.navMm==41) && (s.trust==Trust::Proven);
  printf("\n%s: 172 advances, zero refusals, circuit closed at MM040, PROVEN\n",
         ok?"PASS":"FAIL");
  return ok?0:1;
}
