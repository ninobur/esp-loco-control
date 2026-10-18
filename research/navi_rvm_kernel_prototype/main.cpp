#include "rvm_kernel.hpp"
#include <iostream>
int main(){
 rvm::Model m; m.mm_peak={5,105,205,305,405};
 for(int x=0;x<450;x++){
   float signal=2000;
   for(int p:m.mm_peak)signal+=90*std::exp(-0.5f*std::pow((x-p)/11.f,2));
   m.landscape.push_back({signal,10.f});
 }
 rvm::Config cfg;cfg.motion_step=1;cfg.max_phase_step=3;
 rvm::Kernel k(m,cfg,0);
 int tokens=0;
 for(int x=5;x<445;x++){
   auto d=k.observe(m.landscape[x].mean,80);
   if(d.advance){std::cout<<"tile "<<d.tile<<" at sample "<<x<<"\n";tokens++;}
 }
 std::cout<<"tokens="<<tokens<<"\n";
 return 0;
}
