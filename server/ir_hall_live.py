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
DASHBOARD_PKPH_PER_MM_S = 3.6 * 45.0 / 1000.0
DISPLAY_PERIOD_SECONDS = 0.1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument(
        "--pkph-per-mm-s",
        type=float,
        default=DASHBOARD_PKPH_PER_MM_S,
        help="display conversion; default 0.162 matches the current dashboard",
    )
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
    print(f"pKPH factor {args.pkph_per_mm_s:.5f} per mm/s "
          "(current dashboard convention).")
    try:
        while True:
            now = time.monotonic()
            hall_fresh = now - state["hall_at"] <= FRESH_SECONDS
            ir_fresh = now - state["ir_at"] <= FRESH_SECONDS

            if hall_fresh and state["hall"] is not None:
                hall_value = 0.0 if state["hall_moving"] is False else state["hall"]
                hall_pkph = hall_value * args.pkph_per_mm_s
                hall_text = f"{hall_pkph:6.1f} pKPH"
                motion = "MOVING" if state["hall_moving"] else "STOPPED"
            elif state["hall"] is not None:
                hall_value = None
                hall_pkph = (0.0 if state["hall_moving"] is False
                             else state["hall"] * args.pkph_per_mm_s)
                hall_age = now - state["hall_at"]
                hall_text = f"{hall_pkph:6.1f} pKPH"
                motion = f"STALE {hall_age:4.1f}s"
            else:
                hall_value = None
                hall_pkph = None
                hall_text = "    -- pKPH"
                motion = "NO DATA"

            if ir_fresh and state["ir_valid"] and state["ir"] is not None:
                ir_value = state["ir"]
                ir_pkph = ir_value * args.pkph_per_mm_s
                ir_text = f"{ir_pkph:6.1f} pKPH VALID"
                ir_condition = ""
            elif ir_fresh:
                ir_value = None
                ir_pkph = None
                ir_text = f"    -- pKPH {state['ir_state']}"
                ir_condition = ""
            elif state["ir"] is not None:
                ir_value = None
                ir_pkph = state["ir"] * args.pkph_per_mm_s
                ir_age = now - state["ir_at"]
                ir_text = f"{ir_pkph:6.1f} pKPH"
                ir_condition = f" STALE {ir_age:4.1f}s"
            else:
                ir_value = None
                ir_pkph = None
                ir_text = "    -- pKPH NO DATA"
                ir_condition = ""

            if (hall_value is not None and ir_value is not None and
                    state["hall_moving"]):
                delta_mmps = ir_value - hall_value
                delta_pkph = delta_mmps * args.pkph_per_mm_s
                percent = 100.0 * delta_mmps / hall_value if hall_value else 0.0
                delta_text = f"delta {delta_pkph:+5.1f} pKPH ({percent:+5.1f}%)"
            else:
                delta_text = "delta    -- pKPH"

            span_text = (str(state["ir_span"])
                         if ir_fresh and state["ir_span"] is not None else "--")
            line = (f"HALL {hall_text} {motion:11s}  |  "
                    f"IR {ir_text:20s}{ir_condition:12s}  |  "
                    f"{delta_text}  |  span {span_text}")
            print("\r\033[2K" + line, end="", flush=True)
            time.sleep(DISPLAY_PERIOD_SECONDS)
    except KeyboardInterrupt:
        print()
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
