#!/usr/bin/env python3
"""
dualcap_report.py — reproduce the 2026-08-10 daylight findings (or judge any
future dual-stream capture) from a broker-local ngr/# log.

The log is produced by ~/NGR/tools/ngr_capture.sh on the Pi:
    <unix_ts> <topic> <payload>
one line per MQTT message, Otto and the IR car interleaved on one clock.

Applies the standing rule from IR_SENSOR_NOTES.md: every gap in the IR pulse
train is cross-checked against (a) an independent motion witness — Otto's
pwm — before being called a stop, and (b) the firmware's own pulse COUNTER
before being called sensor blindness. A counter that advances across a
silence proves the sensor kept seeing and the TELEMETRY failed.

USAGE
    python3 dualcap_report.py field-records/logs/20260810_ir-daylight_otto-tow.log
"""
import json
import re
import sys


def load(path):
    t0 = None
    P, IDLE, PWM, MM, NOTE, ONL = [], [], [], [], [], []
    for line in open(path, errors="ignore"):
        p = line.split(" ", 2)
        if len(p) < 3:
            continue
        try:
            t = float(p[0])
        except ValueError:
            continue
        topic, payload = p[1], p[2]
        if t0 is None:
            t0 = t
        tt = t - t0
        if "IR_SPEED_SENSOR/diag" in topic:
            d = dict(re.findall(r"(\w+)=\s*([+-]?\d+)", payload))
            if payload.startswith("PULSE"):
                s = re.match(r"PULSE #\s*(\d+)", payload)
                if s:
                    P.append((tt, int(s.group(1)), int(d.get("raw", 0)),
                              int(d.get("span", 0)), int(d.get("rh", 0)),
                              int(d.get("fh", 0))))
            elif payload.startswith("IDLE"):
                IDLE.append((tt, int(d.get("raw", 0)), int(d.get("span", 0))))
        elif "IR_SPEED_SENSOR/status/online" in topic:
            ONL.append((tt, payload.strip()))
        elif topic.endswith("/alert") or topic.endswith("/state/nav"):
            try:
                d = json.loads(payload)
            except Exception:
                continue
            if d.get("pwm") is not None:
                PWM.append((tt, d["pwm"]))
        elif "mm/marker" in topic:
            try:
                d = json.loads(payload)
            except Exception:
                continue
            if "mm" in d:
                MM.append((tt, d["mm"]))
        elif "spoke/note" in topic:
            NOTE.append((tt, payload.strip()))
    return P, IDLE, PWM, MM, NOTE, ONL


def med(v):
    v = sorted(v)
    return v[len(v) // 2] if v else 0


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    P, IDLE, PWM, MM, NOTE, ONL = load(sys.argv[1])
    if not P:
        print("no IR PULSE lines in this capture")
        sys.exit(1)

    print("=== capture ===")
    print("span %.0f s | IR pulses published %d | idle beats %d | "
          "Otto pwm samples %d | markers %d | notes %d"
          % (P[-1][0], len(P), len(IDLE), len(PWM), len(MM), len(NOTE)))
    print("LWT online transitions: %d" % len(ONL))

    print("\n=== 1. saturation / ambient ===")
    raws = [x[2] for x in P] + [x[1] for x in IDLE]
    print("max raw %d of 4095 (>=4000 = blinded)  |  raw range %d..%d"
          % (max(raws), min(raws), max(raws)))
    print("SENSOR BLINDED BY LIGHT: %s" % ("YES" if max(raws) >= 4000 else "no"))

    print("\n=== 2. optical margins by minute (published pulses) ===")
    print(" min     n     raw    span      rh      fh   fh<0")
    for b in range(0, int(P[-1][0]) + 60, 60):
        d = [x for x in P if b <= x[0] < b + 60]
        if not d:
            continue
        print("%4d %5d %7d %7d %7d %7d %6d"
              % (b // 60, len(d), med([x[2] for x in d]), med([x[3] for x in d]),
                 med([x[4] for x in d]), med([x[5] for x in d]),
                 sum(1 for x in d if x[5] < 0)))

    print("\n=== 3. standing rule: gaps vs motion witness AND pulse counter ===")

    def moving_at(tt):
        v = 0
        for (t2, p2) in PWM:
            if t2 <= tt:
                v = p2
            else:
                break
        return v > 0

    total_powered = sum(t2 - t1 for (t1, p1), (t2, _) in zip(PWM, PWM[1:]) if p1 > 0)
    silent = 0
    print("   from     to   silence  counter_advance   verdict")
    for a, b in zip(P, P[1:]):
        if b[0] - a[0] < 3.0 or not moving_at(a[0] + 0.5):
            continue
        adv = b[1] - a[1]
        silent += b[0] - a[0]
        verdict = ("TELEMETRY FAILED (sensor kept counting)" if adv > 3
                   else "genuinely blind or stopped")
        print(" %6.0f %6.0f  %6.1f s %16d   %s"
              % (a[0], b[0], b[0] - a[0], adv, verdict))
    print("\nOtto under power %.0f s | IR train silent %.0f s (%.0f%%)"
          % (total_powered, silent, 100 * silent / total_powered if total_powered else 0))
    print("sensor detected %d pulses; %d published (%.0f%% delivered)"
          % (P[-1][1], len(P), 100.0 * len(P) / P[-1][1] if P[-1][1] else 0))


if __name__ == "__main__":
    main()
