#!/usr/bin/env python3
"""Publish one Hall/IR speed comparison when Otto passes a Hall marker.

Diagnostic only. This bridge subscribes to telemetry and publishes a compact
comparison record; it never publishes a command and cannot affect a train.
"""

import json
import time

import paho.mqtt.client as mqtt

BROKER = "127.0.0.1"
PORT = 1883
LOCO = "9950011"
ALERT_TOPIC = f"ngr/loco/{LOCO}/alert"
MARKER_TOPIC = f"ngr/loco/{LOCO}/mm/marker"
IR_TOPIC = "ngr/spoke/IR_SPEED_SENSOR/telem/speed"
OUT_TOPIC = f"ngr/compare/{LOCO}/hall_ir_mmps"
IR_FRESH_SECONDS = 5.0


def main():
    latest_hall = None
    latest_ir = None
    latest_ir_at = 0.0
    marker_pending = False

    def publish_comparison(client):
        nonlocal marker_pending
        ir = latest_ir if time.monotonic() - latest_ir_at <= IR_FRESH_SECONDS else None
        payload = json.dumps(
            {"hall_mmps": latest_hall, "ir_mmps": ir},
            separators=(",", ":"),
        )
        result = client.publish(OUT_TOPIC, payload, qos=0, retain=False)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(payload, flush=True)
        else:
            print(f"publish failed rc={result.rc}", flush=True)
        marker_pending = False

    def on_connect(client, _userdata, _flags, rc, _properties=None):
        if rc != 0:
            print(f"MQTT connection failed: {rc}", flush=True)
            return
        client.subscribe([
            (ALERT_TOPIC, 0),
            (MARKER_TOPIC, 0),
            (IR_TOPIC, 0),
        ])

    def on_message(client, _userdata, message):
        nonlocal latest_hall, latest_ir, latest_ir_at, marker_pending
        try:
            payload = json.loads(message.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return

        if message.topic == IR_TOPIC:
            latest_ir = (float(payload["speed_mmps"])
                         if payload.get("speed_valid") else None)
            latest_ir_at = time.monotonic()
        elif message.topic == MARKER_TOPIC:
            # The alert publishes the Hall speed derived from the segment that
            # has just closed. Waiting for that next alert binds one record to
            # the new Hall value rather than periodically sampling it.
            marker_pending = True
        elif message.topic == ALERT_TOPIC and "est_mm_s" in payload:
            latest_hall = (0.0 if str(payload.get("moving")) == "0"
                           else float(payload["est_mm_s"]))
            if marker_pending:
                publish_comparison(client)

    client = mqtt.Client(client_id="ngr-hall-ir-marker-bridge")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=30)
    print(f"publishing marker comparisons to {OUT_TOPIC}", flush=True)
    client.loop_forever()


if __name__ == "__main__":
    main()
