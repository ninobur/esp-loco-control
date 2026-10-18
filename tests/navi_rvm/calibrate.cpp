#include "RvmRecognizer.h"
#include "RvmReference.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
int main(int argc,char** argv){
 if(argc!=3)return 2;
 std::vector<rvm::Block> blocks;std::ifstream in(argv[1]);std::string line;std::getline(in,line);
 while(std::getline(in,line)){
  for(char& c:line)if(c==',')c=' ';rvm::Block b;int pwm,dir,gap,ir;std::istringstream s(line);
  s>>b.firstSerial>>b.lastSerial>>b.firstUs>>b.lastUs>>b.hall>>pwm>>dir>>gap>>ir>>b.irSpeed;
  b.pwm=pwm;b.direction=dir==1?1:-1;b.gap=gap;b.irValid=ir;blocks.push_back(b);
 }
 std::ifstream labels(argv[2]);int mm;uint64_t previous,opened;
 std::cout<<"mm,quiet_max,appearance_max,appearance_quality\n";
 while(labels>>mm>>previous>>opened){
  rvm::Recognizer e(rvm_reference::cw);e.setAppearanceEvidence(1e9f);
  // Leave the preceding recorded ensemble; compare the middle of the interval
  // with the next leading region. These are calibration annotations, not a gate.
  const uint64_t start=previous+(opened-previous)/3;
  const uint64_t quietEnd=previous+2*(opened-previous)/3;
  e.declare((mm+170)%171,1,start-1);float quiet=0,appeared=0,q=0;
  for(const auto& b:blocks){if(b.firstUs<start)continue;if(b.lastUs>opened+250000)break;
   rvm::Token t;e.observe(b,t);
   if(b.lastUs<=quietEnd)quiet=std::max(quiet,e.evidence());
   if(b.lastUs>=opened && e.evidence()>appeared){appeared=e.evidence();q=e.quality();}
  }
  std::cout<<mm<<','<<quiet<<','<<appeared<<','<<q<<'\n';
 }
}
