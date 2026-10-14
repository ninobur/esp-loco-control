
#define main dart_scenario_main
#include "../firmware/programs/NAVI_EWO_0_5/tests/test_dart.cpp"
#undef main
#include <deque>
#include <atomic>
static Fixture a,b;
#include "../firmware/programs/NAVI_EWO_0_5/NaviTileTiming.h"
static TileTimingCommand lastTileTiming;
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
static void publishDart() {
  DartRecord record;
  for(unsigned i=0;i<2 && navi.takeDartRecord(record);++i) {
    if(!dartQ || xQueueSend(dartQ,&record,0)!=pdTRUE) ++dartQueueDrops;
  }
  static uint32_t last=0;
  if(uint32_t(millis()-last)<1000) return;
  last=millis(); char payload[650];
  const auto view=stopView(navi,stopMarkerUm(45)+150000,esp_timer_get_time());
  snprintf(payload,sizeof(payload),
    "{\"diag_drop\":%lu,\"queue_drop\":%lu,\"publish_drop\":%lu,\"messages\":%lu,\"bytes\":%lu,"
    "\"anchor_mismatch\":%lu,\"pending_hwm\":%u,\"pending_overflow\":%lu,"
    "\"incomplete_batch\":%lu,\"comparison_incomplete\":%u,\"candidate\":%u,"
    "\"processing_max_us\":%llu,\"hall_latency_max_us\":%llu,\"core_bytes\":%u,"
    "\"heap_free\":%u,\"heap_min\":%u,\"stack_hwm\":%u,"
    "\"identity_available\":%u,\"mm\":%u,\"tile\":%u,\"fine_available\":%u}",
    (unsigned long)navi.diagnosticDrops(),(unsigned long)dartQueueDrops,(unsigned long)dartPublishDrops.load(),
    (unsigned long)dartPublishMessages.load(),(unsigned long)dartPublishBytes.load(),
    (unsigned long)navi.anchorMismatchCount(),unsigned(navi.pendingHighWater()),
    (unsigned long)navi.pendingOverflows(),(unsigned long)navi.incompleteBatches(),
    unsigned(navi.comparisonIncomplete()),unsigned(navi.candidateAvailable()),
    (unsigned long long)dartMaxProcessingUs,(unsigned long long)dartMaxHallLatencyUs,
    unsigned(sizeof(navi)),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),
    unsigned(uxTaskGetStackHighWaterMark(nullptr)),unsigned(view.identityAvailable),
    unsigned(view.identifiedMm),unsigned(view.tile),unsigned(view.fine));
  pub("diag/navi_status",payload);
  char operating[600];
  snprintf(operating,sizeof(operating),
    "{\"method\":%u,\"ir_authority\":%u,\"ir_class\":%u,\"stop\":%u,\"stop_reason\":\"%s\","
    "\"completed_laps\":%lu,\"selected_id\":%lu,\"tile_mode\":%u,\"tile_duration_us\":%llu,"
    "\"tile_elapsed_us\":%llu,\"entry_pwm\":%u,\"manual_timing_paused\":%u}",
    unsigned(navi.comparisonMetric()),unsigned(navi.irAuthority()),unsigned(navi.irCredibility()),
    unsigned(navi.stopReason()),navi.stopReasonText(),(unsigned long)navi.completedTimingLaps(),
    (unsigned long)navi.selectedOpeningId(),unsigned(lastTileTiming.mode),
    (unsigned long long)lastTileTiming.durationUs,(unsigned long long)lastTileTiming.elapsedUs,
    unsigned(lastTileTiming.entryPwm),unsigned(navi.manualTimingPaused()));
  pub("diag/navi_operating",operating);
}
static void serviceDartTelemetry() {
  DartRecord record;
  for(unsigned i=0;i<2 && dartQ && xQueueReceive(dartQ,&record,0)==pdTRUE;++i) {
    char payload[2048],topic[72]; const int size=formatDart(payload,sizeof(payload),record,bootId);
    snprintf(topic,sizeof(topic),"ngr/loco/%s/diag/navi_decision",LOCO_NAME);
    if(size<0 || size>=int(sizeof(payload)) || !mqtt.connected() || !mqtt.publish(topic,payload,false))
      ++dartPublishDrops;
    else { ++dartPublishMessages; dartPublishBytes+=unsigned(size); }
  }
}

int main(){for(unsigned mode=0;mode<5;++mode){a=Fixture{};b=Fixture{};q.clear();failQueue=mode==1;online=mode!=2;success=mode!=3;dartQ=mode==4?nullptr:&q;a.start();b.start();
 for(unsigned i=0;i<20;++i){a.ir(25+i);b.ir(25+i);a.field(-1);b.field(-1);a.complete();b.complete();publishDart();serviceDartTelemetry();
 CHECK(a.n.mm()==b.n.mm());CHECK(a.n.openingIrUm()==b.n.openingIrUm());CHECK(a.n.candidateBoundaryUm()==b.n.candidateBoundaryUm());CHECK(a.n.confirmedCount()==b.n.confirmedCount());CHECK(a.n.latestIr().sequence==b.n.latestIr().sequence);CHECK(a.n.irAuthority()==b.n.irAuthority());CHECK(a.n.stopReason()==b.n.stopReason());}}
 CHECK(dartQueueDrops>0);CHECK(dartPublishDrops.load()>0);std::printf("PASS %u actual transport assertions; queue_drop=%u mqtt_drop=%u\n",checks,dartQueueDrops,dartPublishDrops.load());return 0;}
