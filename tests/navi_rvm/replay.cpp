#include "RvmRecognizer.h"
#include "RvmReference.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
int main(int argc,char** argv){
 if(argc<4){std::cerr<<"replay fixture.csv declared_mm direction [trace]\n";return 2;}
 const int direction=std::stoi(argv[3]);
 rvm::Recognizer engine(direction>0?rvm_reference::cw:rvm_reference::ccw);
 if(argc>4 && std::string(argv[4])=="--calibration")engine.setAtypicalQuality(1e9f);
 engine.setBoot(1); std::ifstream in(argv[1]); if(!in)return 2;
 std::string line;std::getline(in,line);int tokens=0;uint64_t us=0;double maximum=0,total=0;int blocks=0;
 std::cout<<"expected,committed,ordinal,serial_first,serial_last,time_first_us,time_last_us,commit_us,evidence,quality,phase,trusted,atypical,mismatches,pwm,ir_valid,model,final_quality,stop_decision\n";
 while(std::getline(in,line)){
  for(char& c:line)if(c==',')c=' ';
  rvm::Block b;int pwm,dir,gap,ir;std::istringstream s(line);
  s>>b.firstSerial>>b.lastSerial>>b.firstUs>>b.lastUs>>b.hall>>pwm>>dir>>gap>>ir>>b.irSpeed;
  b.pwm=pwm;b.direction=dir==1?1:-1;b.gap=gap;b.irValid=ir;
  if(!us)engine.declare(std::stoi(argv[2]),direction,b.firstUs-1);
  us=b.lastUs;
  rvm::Token t;const auto start=std::chrono::steady_clock::now();bool emitted=engine.observe(b,t);
  const double cost=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();maximum=std::max(maximum,cost);total+=cost;++blocks;
  if(emitted){if(!t.finalQuality && !t.stopDecision)++tokens;std::cout<<t.expected<<','<<t.committed<<','<<t.ordinal<<','<<t.firstSerial<<','<<t.lastSerial<<','<<t.firstUs<<','<<t.lastUs<<','<<t.commitUs<<','<<t.evidence<<','<<t.quality<<','<<t.phase<<','<<t.trusted<<','<<t.atypical<<','<<int(t.mismatches)<<','<<int(t.pwm)<<','<<t.irValid<<','<<t.model<<','<<t.finalQuality<<','<<t.stopDecision<<'\n';}
  if(argc>4 && std::string(argv[4])=="--trace" && blocks%10==0)std::cerr<<b.lastUs<<','<<b.hall<<','<<engine.phase()<<','<<engine.evidence()<<','<<engine.quality()<<'\n';
 }
 std::cerr<<"blocks="<<blocks<<" tokens="<<tokens<<" last_mm="<<engine.committed()<<" unreachable="<<engine.unreachableCount()<<" pattern_stop="<<engine.lossOfPattern()<<" stall="<<engine.stalled()<<" recognizer_bytes="<<sizeof(engine)<<" host_mean_us="<<total/std::max(1,blocks)<<" host_max_us="<<maximum<<'\n';
}
