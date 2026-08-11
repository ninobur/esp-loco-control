#!/usr/bin/env python3
"""Live, read-only Hall/IR speed comparator for IR commissioning."""

import argparse
import json
import sys
import time

import paho.mqtt.client as mqtt

HALL_TOPIC = "ngr/loco/9950011/alert"
IR_TOPIC = "ngr/spoke/IR_SPEED_SENSOR/telem/speed"
FRESH_SECONDS = 5.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    args = parser.parse_args()

    state = {
        "hall": None,
        "hall_moving": None,
        "hall_at": 0.0,
        "ir": None,
        "ir_valid": False,
        "ir_state": "NO DATA",
        "ir_span": None,
        "ir_at": 0.0,
    }

    def on_connect(client, _userdata, _flags, rc, _properties=None):
        if rc != 0:
            print(f"MQTT connection failed: {rc}", file=sys.stderr)
            return
        client.subscribe([(HALL_TOPIC, 0), (IR_TOPIC, 0)])

    def on_message(_client, _userdata, message):
        try:
            payload = json.loads(message.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        now = time.monotonic()
        if message.topic == HALL_TOPIC:
            if "est_mm_s" in payload:
                state["hall"] = float(payload["est_mm_s"])
                state["hall_moving"] = bool(payload.get("moving"))
                state["hall_at"] = now
        elif message.topic == IR_TOPIC:
            state["ir_valid"] = bool(payload.get("speed_valid"))
            state["ir"] = (float(payload["speed_mmps"])
                           if state["ir_valid"] else None)
            state["ir_state"] = ("VALID" if state["ir_valid"]
                                 else str(payload.get("state", "INVALID")))
            state["ir_span"] = payload.get("span")
            state["ir_at"] = now

    client = mqtt.Client(client_id="ngr-ir-hall-live")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.broker, args.port, keepalive=30)
    client.loop_start()

    print("Live speed comparison; Ctrl-C exits. This display sends no commands.")
    try:
        while True:
            now = time.monotonic()
            hall_fresh = now - state["hall_at"] <= FRESH_SECONDS
            ir_fresh = now - state["ir_at"] <= FRESH_SECONDS

            if hall_fresh and state["hall"] is not None:
                hall_value = 0.0 if state["hall_moving"] is False else state["hall"]
                hall_text = f"{hall_value:7.1f} mm/s"
                motion = "MOVING" if state["hall_moving"] else "STOPPED"
            else:
                hall_value = None
                hall_text = "     -- mm/s"
                motion = "NO DATA"

            if ir_fresh and state["ir_valid"] and state["ir"] is not None:
                ir_value = state["ir"]
                ir_text = f"{ir_value:7.1f} mm/s VALID"
            elif ir_fresh:
                ir_value = None
                ir_text = f"     -- mm/s {state['ir_state']}"
            else:
                ir_value = None
                ir_text = "     -- mm/s NO DATA"

            if (hall_value is not None and ir_value is not None and
                    state["hall_moving"]):
                delta = ir_value - hall_value
                percent = 100.0 * delta / hall_value if hall_value else 0.0
                delta_text = f"delta {delta:+6.1f} ({percent:+5.1f}%)"
            else:
                delta_text = "delta      --"

            span_text = (str(state["ir_span"])
                         if ir_fresh and state["ir_span"] is not None else "--")
            line = (f"HALL {hall_text} {motion:7s}  |  IR {ir_text:24s}  |  "
                    f"{delta_text}  |  span {span_text}")
            print("\r\033[2K" + line, end="", flush=True)
            time.sleep(0.2)
    except KeyboardInterrupt:
        print()
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
