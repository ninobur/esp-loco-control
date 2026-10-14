from pathlib import Path
import re,subprocess
r=Path(__file__).resolve().parents[1];p=r/'firmware/programs/NAVI_EWO_0_4'
s=(p/'NAVI_EWO_0_4.ino').read_text()
def fn(name):
 m=re.search(r'^static [^\n]*\b'+name+r'\(',s,re.M);start=s.index('{',m.start());i=start+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[m.start():i]
head=r'''
#define main dart_scenario_main
#include "../firmware/programs/NAVI_EWO_0_4/tests/test_dart.cpp"
#undef main
#include <deque>
#include <atomic>
static Fixture a,b;
static NaviIntegratedCore& navi=b.n;
static std::deque<DartRecord> q;
static void* dartQ=&q;
static std::atomic<uint32_t> dartPublishDrops{0},dartPublishBytes{0},dartPublishMessages{0};
static uint32_t dartQueueDrops=0;static uint64_t dartMaxProcessingUs=0,dartMaxHallLatencyUs=0,bootId=1;
static const char* LOCO_NAME="OFFLINE_TEST";
static bool failQueue=false,online=true,success=true;
static constexpr int pdTRUE=1;
static int xQueueSend(void*,const DartRecord* record,int){if(failQueue||q.size()==8)return 0;q.push_back(*record);return 1;}
static int xQueueReceive(void*,DartRecord* record,int){if(q.empty())return 0;*record=q.front();q.pop_front();return 1;}
static unsigned millis(){return 0;}
static uint64_t esp_timer_get_time(){return 0;}
static unsigned uxTaskGetStackHighWaterMark(void*){return 0;}
static bool pub(const char*,const char*){return false;}
static struct Esp{unsigned getFreeHeap(){return 0;}unsigned getMinFreeHeap(){return 0;}}ESP;
static struct Mqtt{bool connected(){return online;}bool publish(const char*,const char*,bool){return success;}}mqtt;
'''
body=r'''
int main(){for(unsigned mode=0;mode<5;++mode){a=Fixture{};b=Fixture{};q.clear();failQueue=mode==1;online=mode!=2;success=mode!=3;dartQ=mode==4?nullptr:&q;a.start();b.start();
 for(unsigned i=0;i<20;++i){a.ir(25+i);b.ir(25+i);a.field(-1);b.field(-1);a.complete();b.complete();publishDart();serviceDartTelemetry();
 CHECK(a.n.mm()==b.n.mm());CHECK(a.n.openingIrUm()==b.n.openingIrUm());CHECK(a.n.candidateBoundaryUm()==b.n.candidateBoundaryUm());CHECK(a.n.confirmedCount()==b.n.confirmedCount());CHECK(a.n.latestIr().sequence==b.n.latestIr().sequence);}}
 CHECK(dartQueueDrops>0);CHECK(dartPublishDrops.load()>0);std::printf("PASS %u actual transport assertions; queue_drop=%u mqtt_drop=%u\n",checks,dartQueueDrops,dartPublishDrops.load());return 0;}
'''
out=r/'dart-validation/transport_generated.cpp';out.write_text(head+fn('publishDart')+'\n'+fn('serviceDartTelemetry')+'\n'+body)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out),'-o','/tmp/dart-transport-test'],check=True)
res=subprocess.run(['/tmp/dart-transport-test'],capture_output=True,text=True,check=True)
(r/'dart-validation/transport-test.log').write_text(res.stdout+res.stderr);print(res.stdout)
