/* IR_SCOPE_ESPNOW_RX_1_2 — checked radio startup; unchanged packets and ACKs.
 * 1.1: channel-11 ESP-NOW to USB recorder with fusion ACKs.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_err.h>

static const uint8_t CHANNEL = 11;
static const uint16_t MAGIC = 0x4952; // "IR"
static const uint8_t VERSION = 1;
static const size_t MAX_PAYLOAD = 250;
static const uint8_t FUSION_TYPE = 3, ACK_TYPE = 4;

struct __attribute__((packed)) FusionPrefix {
  uint16_t magic; uint8_t version,type; uint32_t sid,reportSequence;
};

struct __attribute__((packed)) FusionAckPacket {
  uint16_t magic; uint8_t version,type; uint32_t sid,reportSequence; uint16_t crc;
};
static_assert(sizeof(FusionAckPacket)==14,"fusion ACK wire size changed");

struct RxFrame {
  uint8_t mac[6];
  int8_t rssi;
  uint16_t len;
  uint8_t data[MAX_PAYLOAD];
};

static QueueHandle_t rxQueue;
static QueueHandle_t ackQueue;
static volatile uint32_t rxFrames = 0, queueDrops = 0, badLength = 0;
static uint32_t ackSent=0,ackErrors=0,ackQueueDrops=0;

struct AckRequest { uint8_t mac[6]; uint32_t sid,reportSequence; };

static uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) { c ^= (uint16_t)*p++ << 8; for (int i=0;i<8;i++) c = (c & 0x8000) ? (c<<1)^0x1021 : c<<1; }
  return c;
}

static void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len <= 0 || len > (int)MAX_PAYLOAD) { badLength++; return; }
  RxFrame f{};
  memcpy(f.mac, info->src_addr, 6);
  f.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  f.len = (uint16_t)len;
  memcpy(f.data, data, len);
  if (xQueueSend(rxQueue, &f, 0) != pdTRUE) queueDrops++; else rxFrames++;
  if(len==86){
    FusionPrefix prefix{};memcpy(&prefix,data,sizeof(prefix));
    if(prefix.magic==MAGIC&&prefix.version==VERSION&&prefix.type==FUSION_TYPE){
      AckRequest request{};memcpy(request.mac,info->src_addr,6);request.sid=prefix.sid;request.reportSequence=prefix.reportSequence;
      if(xQueueSend(ackQueue,&request,0)!=pdTRUE)ackQueueDrops++;
    }
  }
}

static void printHex(const uint8_t *p, size_t n) {
  static const char h[]="0123456789abcdef";
  while (n--) { uint8_t v=*p++; Serial.write(h[v>>4]); Serial.write(h[v&15]); }
}

static void startupFatal(const char *step) {
  Serial.printf("FATAL radio_startup step=%s\n", step);
  Serial.flush();
  while (true) delay(1000);
}

static void checkRadio(const char *step, esp_err_t result) {
  Serial.printf("RADIO step=%s result=%s code=%d\n", step, esp_err_to_name(result), (int)result);
  if (result != ESP_OK) startupFatal(step);
}

void setup() {
  Serial.begin(921600);
  delay(300);
  Serial.println("BOOT IR_SCOPE_ESPNOW_RX_1_2 baud=921600");
  rxQueue = xQueueCreate(48, sizeof(RxFrame));ackQueue=xQueueCreate(32,sizeof(AckRequest));
  if (!rxQueue||!ackQueue) { Serial.println("FATAL queue"); while(true) delay(1000); }
  const bool stationStarted = WiFi.mode(WIFI_STA);
  Serial.printf("RADIO step=WiFi.mode ok=%u\n", (unsigned)stationStarted);
  if (!stationStarted) startupFatal("WiFi.mode");
  // An already-disconnected station may return false; verify radio state below.
  Serial.printf("RADIO step=WiFi.disconnect ok=%u\n", (unsigned)WiFi.disconnect(false, true));
  wifi_mode_t mode;
  checkRadio("get_mode", esp_wifi_get_mode(&mode));
  Serial.printf("RADIO actual_mode=%u\n", (unsigned)mode);
  if (mode != WIFI_MODE_STA) startupFatal("station_mode_mismatch");
  uint8_t mac[6] = {};
  checkRadio("get_mac", esp_wifi_get_mac(WIFI_IF_STA, mac));
  Serial.printf("RADIO mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  bool allZero = true;
  for (uint8_t b : mac) if (b != 0) allZero = false;
  if (allZero || (mac[0] & 1)) startupFatal("invalid_mac");
  checkRadio("set_channel", esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));
  uint8_t actualChannel = 0;
  wifi_second_chan_t secondary;
  checkRadio("get_channel", esp_wifi_get_channel(&actualChannel, &secondary));
  Serial.printf("RADIO actual_channel=%u secondary=%u\n", actualChannel, (unsigned)secondary);
  if (actualChannel != CHANNEL || secondary != WIFI_SECOND_CHAN_NONE) startupFatal("channel_mismatch");
  checkRadio("esp_now_init", esp_now_init());
  checkRadio("register_recv_cb", esp_now_register_recv_cb(onReceive));
  Serial.printf("READY IR_SCOPE_ESPNOW_RX_1_2 channel=%u baud=921600 fusion_ack=1 mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
    actualChannel, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("FORMAT RX <millis> <rssi> <len> <crc16> <hex>");
}

void loop() {
  AckRequest request;
  while(xQueueReceive(ackQueue,&request,0)==pdTRUE){
    if(!esp_now_is_peer_exist(request.mac)){esp_now_peer_info_t peer{};memcpy(peer.peer_addr,request.mac,6);peer.channel=CHANNEL;peer.encrypt=false;if(esp_now_add_peer(&peer)!=ESP_OK){ackErrors++;continue;}}
    FusionAckPacket ack{};ack.magic=MAGIC;ack.version=VERSION;ack.type=ACK_TYPE;ack.sid=request.sid;ack.reportSequence=request.reportSequence;ack.crc=0;ack.crc=crc16((uint8_t*)&ack,sizeof(ack)-2);
    if(esp_now_send(request.mac,(uint8_t*)&ack,sizeof(ack))==ESP_OK)ackSent++;else ackErrors++;
  }
  RxFrame f;
  if (xQueueReceive(rxQueue, &f, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.printf("RX %lu %d %u %04x ", (unsigned long)millis(), (int)f.rssi,
                  (unsigned)f.len, (unsigned)crc16(f.data, f.len));
    printHex(f.data, f.len);
    Serial.write('\n');
  }
  static uint32_t then=0;
  if (millis()-then >= 5000) {
    then=millis();
    Serial.printf("STAT rx=%lu qdrop=%lu badlen=%lu ack=%lu ackerr=%lu ackqdrop=%lu q=%u heap=%lu\n",
      (unsigned long)rxFrames,(unsigned long)queueDrops,(unsigned long)badLength,
      (unsigned long)ackSent,(unsigned long)ackErrors,(unsigned long)ackQueueDrops,
      (unsigned)uxQueueMessagesWaiting(rxQueue),(unsigned long)ESP.getFreeHeap());
  }
}
