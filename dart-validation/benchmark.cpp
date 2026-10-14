#include <chrono>
#include <cstdio>
#ifdef BASELINE
#include "../firmware/programs/NAVI_EWO_0_1_MM045_STOP/NaviIntegratedCore.h"
#else
#include "../firmware/programs/NAVI_EWO_0_4/NaviIntegratedCore.h"
#endif
int main(){navi_eyes::NaviIntegratedCore n;n.declare(16,1,1);uint64_t total=0,maximum=0;
 for(unsigned batch=0;batch<10000;++batch){auto start=std::chrono::steady_clock::now();for(unsigned j=0;j<5;++j){navi_eyes::HallSample h{};h.sampleSerial=batch*5+j+1;h.timestampUs=batch*1000+j+10;h.pwm=40;h.raw=2000;h.direction=1;n.observeHall(h);}
 const auto dt=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();total+=dt;if(uint64_t(dt)>maximum)maximum=dt;}
 std::printf("{\"host_core_bytes\":%zu,\"batches\":10000,\"native_samples\":50000,\"mean_batch_ns\":%.1f,\"max_batch_ns\":%llu}\n",sizeof(n),double(total)/10000,(unsigned long long)maximum);}
