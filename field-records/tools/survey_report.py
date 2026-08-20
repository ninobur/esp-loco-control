#!/usr/bin/env python3
"""Summarise ESPNOW_REPEATER survey records from a capture.sh log.

Why this exists: the first long survey was captured by reading the repeater's
USB serial line, and 2,145 of its 2,322 lines came back as one repeated
fragment — the capture path corrupted the evidence while looking healthy, which
is the exact failure capture.sh was written to prevent. The repeater publishes
the same records over MQTT with wall-clock stamps, capture.sh already
subscribes to ngr/#, so surveys are captured that way and read with this.

Usage:
    field-records/tools/survey_report.py <capture.log> [--by-mm] [--site NAME]

Reports, per locomotive and per site: delivery from sequence numbers (not from
timing), RSSI distribution, loss-run lengths, and the worst receive gaps. With
--by-mm it bins by the sender's own reported marker position, which is what
turns a survey into a coverage map.
"""
import sys, json, statistics as st
from collections import defaultdict

NAMES = {9950011: "Otto", 9950012: "Toby"}

def load(path, site_filter=None):
    frames, events, sites = [], [], set()
    for line in open(path, errors="replace"):
        parts = line.split(" ", 2)
        if len(parts) < 3 or "ngr/survey/" not in parts[1]:
            continue
        try:
            d = json.loads(parts[2])
        except (ValueError, TypeError):
            continue
        if not isinstance(d, dict):
            continue          # scalar payloads: online "1", etc.
        d["_host"] = float(parts[0])
        topic = parts[1]
        if topic.endswith("/event"):
            events.append(d)
        elif topic.endswith("/rx") and d.get("type") == "cto":
            sites.add(d.get("site", "?"))
            if site_filter and d.get("site") != site_filter:
                continue
            frames.append(d)
    return frames, events, sites

def summarise(name, rows, by_mm=False):
    rows = sorted(rows, key=lambda r: r["seq"])
    # A sender restart resets the sequence; split rather than report a bogus span.
    segs, cur = [], [rows[0]]
    for a, b in zip(rows, rows[1:]):
        if b["seq"] >= a["seq"]:
            cur.append(b)
        else:                      # sender rebooted: start a new segment
            segs.append(cur)
            cur = [b]
    segs.append(cur)
    recv = sum(len(s) for s in segs)
    span = sum(s[-1]["seq"] - s[0]["seq"] + 1 for s in segs)
    rssi = [r["rssi"] for r in rows]
    runs = [b["seq"] - a["seq"] - 1 for s in segs for a, b in zip(s, s[1:]) if b["seq"] - a["seq"] > 1]
    gaps = sorted((b["_host"] - a["_host"] for s in segs for a, b in zip(s, s[1:])), reverse=True)
    mins, secs = divmod(int(rows[-1]["_host"] - rows[0]["_host"]), 60)
    print(f"\n{name}: {recv}/{span} frames = {100*recv/span:.1f}%  over {mins}m{secs:02d}s")
    print(f"   RSSI  mean {st.mean(rssi):.1f}  median {st.median(rssi):.0f}  "
          f"p10 {sorted(rssi)[len(rssi)//10]}  min {min(rssi)}  max {max(rssi)}  sd {st.pstdev(rssi):.1f}")
    print(f"   loss  {span-recv} frames in {len(runs)} runs; longest run {max(runs) if runs else 0}")
    print(f"   worst receive gaps: {', '.join(f'{g:.1f}s' for g in gaps[:5]) or 'none'}")
    if by_mm:
        bins = defaultdict(list)
        for r in rows:
            if r.get("mm") is not None:
                bins[(r["mm"] // 10) * 10].append(r["rssi"])
        print("   by marker (10-mm bins):")
        for lo in sorted(bins):
            v = bins[lo]
            print(f"     mm {lo:3d}-{lo+9:3d}  n={len(v):4d}  rssi mean {st.mean(v):6.1f}  min {min(v):4d}")

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__); return 1
    site = None
    if "--site" in sys.argv:
        site = sys.argv[sys.argv.index("--site") + 1]
    frames, events, sites = load(args[0], site)
    if not frames:
        print("No survey frames in that capture."
              + (f" Sites present: {', '.join(sorted(sites))}" if sites else ""))
        return 1
    print(f"sites in capture: {', '.join(sorted(sites))}"
          + (f"   (reporting {site} only)" if site else ""))
    for ev in events:
        if ev.get("event") in ("SITE_CHANGED", "CHANNEL_MISMATCH"):
            print(f"  ! {ev}")
    by_src = defaultdict(list)
    for f in frames:
        by_src[f["src"]].append(f)
    for src, rows in sorted(by_src.items()):
        summarise(NAMES.get(src, str(src)), rows, "--by-mm" in sys.argv)
    if len({f.get("site") for f in frames}) > 1:
        print("\nNOTE: this capture spans more than one site. Pass --site NAME;"
              "\n      samples from different positions must not be pooled.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
