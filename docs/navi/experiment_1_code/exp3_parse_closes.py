#!/usr/bin/env python3
"""Parse CLOSE_AND_ADVANCE events (mm, target, wall-clock) from an
NGR telemetry log."""
import sys, json, datetime, zoneinfo

TZ = zoneinfo.ZoneInfo("America/Los_Angeles")

def parse(path):
    out = []
    seen = set()
    with open(path) as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 3:
                continue
            ts, topic, payload = parts[0], parts[1], parts[2]
            if "state/nav" not in topic or topic.endswith("nav_ready"):
                continue
            try:
                d = json.loads(payload)
            except Exception:
                continue
            if d.get("event") != "CLOSE_AND_ADVANCE":
                continue
            key = (d.get("mm"), d.get("target"), d.get("recognized"))
            if key in seen:
                continue
            seen.add(key)
            # ts like 2026-10-22T15:32:04.557
            dt = datetime.datetime.fromisoformat(ts).replace(tzinfo=TZ)
            out.append({"wall_us": int(dt.timestamp() * 1e6), "ts": ts,
                        "mm": d.get("mm"), "target": d.get("target"),
                        "dir": d.get("dir"), "departure": d.get("departure")})
    return out

if __name__ == "__main__":
    r = parse(sys.argv[1])
    print(f"n={len(r)}")
    for e in r:
        print(e)
