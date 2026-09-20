/* IR_USB_BENCH 1.0 - raw IR bench instrument, not navigation firmware.
 * ESP32 data car, GPIO34, 12-bit ADC, 1 kHz, USB UART at 460800 baud.
 * No radio, network, motor control, filtering, or pulse detector.
 * A bounded queue separates sampling from serial output. Sequence numbers,
 * actual timestamps, missed slots and queue drops expose missing evidence.
 * See README.md for the 32-byte little-endian CRC-protected record.
 */
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>

struct __attribute__((packed)) Record {
  uint32_t magic;
  uint32_t boot;
  uint32_t seq;
  uint64_t us;
  uint16_t raw;
  uint32_t missed;
  uint32_t dropped;
  uint16_t crc;
};
static_assert(sizeof(Record) == 32, "Wire format must be 32 bytes");
static QueueHandle_t records;
static uint32_t bootId;

static uint16_t crc16(const uint8_t* data, size_t size) {
  uint16_t crc = 0xffff;
  while (size--) {
    crc ^= uint16_t(*data++) << 8;
    for (int b = 0; b < 8; ++b)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

static void sampleTask(void*) {
  uint32_t seq = 0, missed = 0, dropped = 0;
  uint64_t next = esp_timer_get_time();
  TickType_t tick = xTaskGetTickCount();
  for (;;) {
    const uint64_t now = esp_timer_get_time();
    if (now >= next) {
      // Do not fabricate catch-up samples after a scheduler stall.
      const uint32_t skipped = (now - next) / 1000;
      missed += skipped;
      next += uint64_t(skipped + 1) * 1000;
      Record r = {0x31524942, bootId, seq++, now,
                  uint16_t(analogRead(34)), missed, dropped, 0};
      if (xQueueSend(records, &r, 0) != pdTRUE) ++dropped;
    }
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(460800);
  analogReadResolution(12);
  analogSetPinAttenuation(34, ADC_11db);
  pinMode(34, INPUT);
  bootId = esp_random();
  records = xQueueCreate(512, sizeof(Record));
  if (!records || xTaskCreatePinnedToCore(sampleTask, "ir_sample", 4096,
      nullptr, 2, nullptr, 0) != pdPASS) {
    while (true) delay(1000);
  }
}

void loop() {
  Record r;
  if (xQueueReceive(records, &r, portMAX_DELAY) == pdTRUE) {
    r.crc = crc16(reinterpret_cast<const uint8_t*>(&r), sizeof(r) - 2);
    Serial.write(reinterpret_cast<const uint8_t*>(&r), sizeof(r));
  }
}
