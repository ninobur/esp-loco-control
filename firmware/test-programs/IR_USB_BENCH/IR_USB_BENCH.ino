/* IR_USB_BENCH 1.1 - raw IR instrument, not navigation firmware.
 * GPIO34, 12-bit ADC, 1 kHz, USB UART 115200. No radio or pulse detector.
 * BIR2 batches retain actual timestamps and per-sample missed-slot counts.
 * Bounded queue separates sampling from serial; no invented catch-up samples.
 */
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>

struct __attribute__((packed)) Reading {
  uint16_t offsetUs;
  uint16_t raw;
  uint16_t missedDelta;
};
struct __attribute__((packed)) Batch {
  uint32_t magic;
  uint32_t boot;
  uint32_t seq;
  uint64_t firstUs;
  uint32_t missed;
  uint32_t dropped;
  uint16_t count;
  Reading samples[16];
  uint16_t crc;
};
static_assert(sizeof(Batch) == 128, "BIR2 wire size");
static QueueHandle_t batches;
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

static void enqueue(Batch& batch, uint32_t& dropped) {
  if (batch.count && xQueueSend(batches, &batch, 0) != pdTRUE)
    dropped += batch.count;
  batch = {};
}

static void sampleTask(void*) {
  uint32_t seq = 0, missed = 0, dropped = 0;
  uint64_t next = esp_timer_get_time();
  TickType_t tick = xTaskGetTickCount();
  Batch batch = {};
  for (;;) {
    const uint64_t now = esp_timer_get_time();
    if (now >= next) {
      const uint32_t skipped = (now - next) / 1000;
      missed += skipped;
      next += uint64_t(skipped + 1) * 1000;
      if (batch.count && (now - batch.firstUs > 65535 || missed - batch.missed > 65535))
        enqueue(batch, dropped);
      if (!batch.count) {
        batch.magic = 0x32524942;
        batch.boot = bootId;
        batch.seq = seq;
        batch.firstUs = now;
        batch.missed = missed;
        batch.dropped = dropped;
      }
      batch.samples[batch.count++] = {uint16_t(now - batch.firstUs),
                                      uint16_t(analogRead(34)), uint16_t(missed - batch.missed)};
      ++seq;
      if (batch.count == 16) enqueue(batch, dropped);
    }
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(34, ADC_11db);
  pinMode(34, INPUT);
  bootId = esp_random();
  batches = xQueueCreate(32, sizeof(Batch));
  if (!batches || xTaskCreatePinnedToCore(sampleTask, "ir_sample", 4096,
      nullptr, 2, nullptr, 0) != pdPASS) {
    while (true) delay(1000);
  }
}

void loop() {
  Batch batch;
  if (xQueueReceive(batches, &batch, portMAX_DELAY) == pdTRUE) {
    batch.crc = crc16(reinterpret_cast<const uint8_t*>(&batch), sizeof(batch) - 2);
    Serial.write(reinterpret_cast<const uint8_t*>(&batch), sizeof(batch));
  }
}
