#include <iostream>
#include "Navigator.h"

int main(){
  using namespace navi_one;
  for(auto state:{NavState::Declared,NavState::Unresolved,
                  NavState::Declared,NavState::LimitStopped,NavState::Unset}){
    std::cout<<"{\"state\":\""<<consoleNavName(state)<<"\",\"nav\":\""
             <<consoleNavName(state)<<"\",\"nav_state\":\""<<navStateName(state)
             <<"\",\"mm\":40,\"nav_ready\":"<<consoleHasReference(state)<<"}\n";
  }
}
