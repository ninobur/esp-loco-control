#!/usr/bin/env python3
"""Replay noon association evidence, NOT the new Hall detector or ESP-NOW link.

Use the existing analysis's nearest IR counts (old Hall close timestamps), and
actual logged Hall polarity. Only association and state transitions are tested.
The old sketch's MM is a comparison label, not independent physical truth.
"""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
analysis = json.loads((ROOT / "field-records/analysis/20260920_ir_noon_analysis.json").read_text())
events = analysis["events"]
markers = []
for line in (ROOT / "field-records/logs/20260920_toby_noon_capture.log").read_text().splitlines():
    parts = line.split("\t", 2)
    if len(parts) != 3 or parts[1] != "ngr/loco/9950012/mm/marker":
        continue
    row = json.loads(parts[2])
    if row.get("ruling") == "ADVANCED" and row.get("why") == "MAGNET":
        markers.append(row)
assert len(markers) == len(events), "Do not silently omit or pair unmatched Hall records"
origin = dt.datetime.fromisoformat(events[0]["time"]).timestamp() - 1
lines = []
for event, marker in zip(events, markers):
    assert event["mm"] == marker["mm"] and marker["dir"] == "CW"
    assert marker["obs"] in ("N", "S")
    us = round((dt.datetime.fromisoformat(event["time"]).timestamp() - origin) * 1e6)
    lines.append(f'{us} {event["count"]} {int(marker["obs"] == "N")} '
                 f'{int(abs(event["offset_ms"]) <= 150)} {event["mm"]}')
result = subprocess.run([sys.argv[1]], input="\n".join(lines)+"\n", text=True,
                        check=True, capture_output=True)
print(result.stdout, end="")
print("Historical association replay only; not new acquisition, transport, or ground-truth validation.")
