// Compile-only fixture. No motor, Hall, radio, MQTT, or navigation control.
#include <Arduino.h>
#include "../../MmDistanceReference.h"

ngr_nav::IrOdometryEpoch odometry;
ngr_nav::MmDistanceReference reference;

void setup() {
  Serial.begin(115200);
  ir_movement::WireSnapshot w;
  w.bootId=1;w.calibrationId=1;w.capturedUs=1000;
  w.opticalReason=ir_movement::TRACKING;
  auto update=odometry.ingest(w);
  reference.synchronize(40,1,odometry.point(),odometry);
  const auto health=ngr_nav::classifyIrInstrument(w);
  Serial.printf("%s %s %s reference=%u distance_available=%u\n",
                ngr_nav::irHealthName(health.fault),ngr_nav::irReadinessName(health.readiness),
                ngr_nav::irEpochBreakName(update.reason),reference.validFor(odometry)?1u:0u,
                reference.distanceFromMm(odometry).available?1u:0u);
}
void loop() {}
