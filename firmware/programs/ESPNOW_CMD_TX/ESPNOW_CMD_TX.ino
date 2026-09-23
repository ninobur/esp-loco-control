/*
 * ============================================================================
 * ESPNOW_CMD_TX — dispatcher command backup, TX bridge (Phase A)
 * ============================================================================
 * ROLE (decision 0018): diagnostic/instrument. Runs on a Pi-attached ESP32.
 * Serial line-protocol in from the Flask console, encrypted ESP-NOW unicast
 * frames out to locomotives, per NGR_ESPNOW_COMMAND_BACKUP_SPEC.md Rev 4
 * (CODEX-approved at 9ac350c).
 *
 * Wire contract (FROZEN, spec E1): 22-byte packed little-endian frame,
 *   magic u32 = 0x4E475243, version u8 = 1,
 *   cmd u8 (1=ESTOP_ENGAGE, 2=PAUSE — closed enum), reserved u16 = 0,
 *   nonce u32 (random per bridge boot), seq u32 (per command press),
 *   target u32 (receiver's LOCO_ID; NO broadcast value — "all" is fan-out),
 *   crc u16 = CRC-16/CCITT-FALSE over bytes 0..19.
 * Each press: x5 frames per addressed peer, ~50 ms apart, same (nonce,seq).
 *
 * Serial protocol (115200):
 *   in : "ESTOP ALL" | "ESTOP <loco_id>" | "PAUSE <loco_id>"
 *   out: "[HB] ok=<0|1> ch=<n> peers=<n> nonce=<hex> seq=<n> last=<txt>"  (2 s)
 *        "[TX] ..." per frame set;  "[ERR] ..." on refusal
 *
 * FAIL-CLOSED (spec R3-1, local half): placeholder/invalid keys or a peer
 * add failure disable transmission entirely; the heartbeat reports ok=0 and
 * the console must show BACKUP OFFLINE. The bridge can only ever vouch for
 * its own half: "SENT", never "delivered".
 * ============================================================================
 */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "espnow_keys.h"

#define ESPNOW_CHANNEL 11   // MUST match the EAP's fixed channel (runbook)

struct PeerCfg { const char* name; uint32_t id; uint8_t mac[6]; uint8_t lmk[16]; };
static PeerCfg PEERS[] = NGR_ESPNOW_PEERS;
static const uint8_t NPEERS = sizeof(PEERS)/sizeof(PEERS[0]);
static const uint8_t PMK[16] = NGR_ESPNOW_PMK;

#pragma pack(push,1)
struct CmdFrame {
  uint32_t magic; uint8_t version; uint8_t cmd; uint16_t reserved;
  uint32_t nonce; uint32_t seq; uint32_t target; uint16_t crc;
};
#pragma pack(pop)
static_assert(sizeof(CmdFrame)==22, "wire contract is 22 bytes");

static bool     cfgOk=false;
static uint32_t bootNonce=0, cmdSeq=0;
static char     lastSend[48]="none";

static uint16_t crc16_ccitt_false(const uint8_t* d, size_t n){
  uint16_t c=0xFFFF;
  for(size_t i=0;i<n;i++){ c^=(uint16_t)d[i]<<8;
    for(uint8_t b=0;b<8;b++) c=(c&0x8000)?(c<<1)^0x1021:(c<<1); }
  return c;
}
static bool zeroed(const uint8_t* p, size_t n){ for(size_t i=0;i<n;i++) if(p[i]) return false; return true; }

static void sendTo(const PeerCfg& p, uint8_t cmd){
  CmdFrame f{}; f.magic=0x4E475243UL; f.version=1; f.cmd=cmd; f.reserved=0;
  f.nonce=bootNonce; f.seq=cmdSeq; f.target=p.id;
  f.crc=crc16_ccitt_false((const uint8_t*)&f, 20);
  for(uint8_t i=0;i<5;i++){
    esp_err_t e=esp_now_send(p.mac,(const uint8_t*)&f,sizeof(f));
    Serial.printf("[TX] %s cmd=%u seq=%lu rep=%u send=%s\n",
                  p.name,cmd,(unsigned long)cmdSeq,i+1,(e==ESP_OK)?"ok":"err");
    delay(50);
  }
}
static void handle(uint8_t cmd, bool all, uint32_t id){
  if(!cfgOk){ Serial.println("[ERR] refused: keys/peers not configured (fail closed)"); return; }
  cmdSeq++;                                  // one press = one seq, spec E2/R3-2
  bool sent=false;
  for(uint8_t i=0;i<NPEERS;i++)
    if(all || PEERS[i].id==id){ sendTo(PEERS[i],cmd); sent=true; }
  if(!sent) Serial.printf("[ERR] no such peer id %lu\n",(unsigned long)id);
  else snprintf(lastSend,sizeof(lastSend),"cmd=%u %s seq=%lu",cmd,all?"ALL":"one",(unsigned long)cmdSeq);
}

void setup(){
  Serial.begin(115200); delay(200);
  bootNonce=esp_random();
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  bool bad = zeroed(PMK,16);
  for(uint8_t i=0;i<NPEERS;i++) if(zeroed(PEERS[i].mac,6)||zeroed(PEERS[i].lmk,16)) bad=true;
  if(bad){ Serial.println("[ERR] placeholder keys/MACs — TX DISABLED (fail closed)"); cfgOk=false; return; }
  if(esp_now_init()!=ESP_OK){ Serial.println("[ERR] esp_now_init failed"); cfgOk=false; return; }
  esp_now_set_pmk(PMK);
  cfgOk=true;
  for(uint8_t i=0;i<NPEERS;i++){
    esp_now_peer_info_t pi{}; memcpy(pi.peer_addr,PEERS[i].mac,6);
    pi.channel=ESPNOW_CHANNEL; pi.encrypt=true; memcpy(pi.lmk,PEERS[i].lmk,16);
    if(esp_now_add_peer(&pi)!=ESP_OK){
      Serial.printf("[ERR] add_peer %s failed — TX DISABLED\n",PEERS[i].name);
      cfgOk=false;                            // one bad peer = whole path down, visibly
    }
  }
  Serial.printf("[BOOT] ESPNOW_CMD_TX ch=%d peers=%u ok=%d nonce=%08lX\n",
                ESPNOW_CHANNEL,NPEERS,cfgOk?1:0,(unsigned long)bootNonce);
}

void loop(){
  static char buf[48]; static uint8_t len=0;
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'||c=='\r'){
      buf[len]=0;
      if(len){
        uint32_t id=0; char arg[16]={0};
        if(sscanf(buf,"ESTOP %15s",arg)==1){
          if(!strcmp(arg,"ALL")) handle(1,true,0);
          else { id=strtoul(arg,nullptr,10); handle(1,false,id); }
        } else if(sscanf(buf,"PAUSE %15s",arg)==1){
          id=strtoul(arg,nullptr,10); handle(2,false,id);
        } else Serial.printf("[ERR] bad line: %s\n",buf);
      }
      len=0;
    } else if(len<sizeof(buf)-1) buf[len++]=c;
  }
  static unsigned long hb=0;
  if(millis()-hb>=2000UL){ hb=millis();
    Serial.printf("[HB] ok=%d ch=%d peers=%u nonce=%08lX seq=%lu last=%s\n",
                  cfgOk?1:0,ESPNOW_CHANNEL,NPEERS,(unsigned long)bootNonce,
                  (unsigned long)cmdSeq,lastSend);
  }
}
