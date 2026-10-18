#include "ManualControl.h"
#include "WaveformObserver.h"
#include "FixedReference.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
using namespace manual_rvm;
int main(int argc,char** argv) {
  if(argc==4) {
    static WaveformObserver w(fixed_reference::cw,fixed_reference::ccw);
    std::ifstream f(argv[1]); std::string line; std::getline(f,line);
    bool first=true;
    while(std::getline(f,line)) {
      std::stringstream ss(line);std::vector<double> v;std::string field;
      while(std::getline(ss,field,',')) v.push_back(std::stod(field));
      if(first) { w.declare(std::stoi(argv[2]),std::stoi(argv[3]),uint64_t(v[2])-1); first=false; }
      w.observe({uint64_t(v[3]),float(v[4]),uint8_t(v[5]),bool(v[7])});
      const auto& r=w.report();
      if(r.changed) std::cout<<v[3]<<","<<r.mm<<","<<r.phase<<","<<r.quality<<","<<r.evidence<<","<<r.uncertainty<<"\n";
    }
    return 0;
  }
  ManualControl m;
  assert(m.setDirection(2)); assert(m.throttle(80));
  for(unsigned t=150;t<=12000;t+=150) m.tick(t);
  assert(m.pwm()==80); assert(!m.setDirection(0));
  m.stop(); for(unsigned t=12031;t<15000;t+=31)m.tick(t);
  assert(m.pwm()==0);assert(m.setDirection(0));
  m.throttle(60);m.tick(16000);assert(m.pwm()==1);
  m.emergency(true);assert(m.pwm()==0 && !m.throttle(20));
  m.emergency(false);assert(m.direction()==1);m.throttle(30);m.tick(17000);assert(m.pwm()==0);
  m.setDirection(2);m.throttle(40);
  for(int i=0;i<5;++i)m.voltage(13.f);
  assert(m.lowVoltage() && !m.throttle(30));m.voltage(14);assert(!m.lowVoltage() && m.target()==0);
  assert(fixed_reference::cw.size==10453 && fixed_reference::cw.magnets==171);
  static WaveformObserver w(fixed_reference::cw,fixed_reference::ccw);
  w.declare(60,1,0);
  for(uint64_t us=20000;us<60000000;us+=20000)w.observe({us,1900,90,false});
  assert(w.report().mm==60 && w.report().anchors==0);
  std::cout<<"PASS manual ramp, direction, STOP, E-STOP, neutral, battery, reference dimensions, PWM-only no geographic advancement\n";
}
