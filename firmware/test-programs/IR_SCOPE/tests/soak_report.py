#!/usr/bin/env python3
"""
soak_report.py — judge an IR_SCOPE bench soak / recovery drill from a
plotter CSV. Companion to the soak protocol in ../README.md.

Reports, per session:
  - firmware-time coverage (samples logged vs expected from the span)
  - transport GAP rows (count, extent, largest) and sampler MISSED rows
  - online 0/1 transitions from retained status, with wall-clock recovery
    time for each offline episode
  - first->last deltas of every network diagnostic counter in the status
    beat (bdrop, wifi_disc, wifi_kicks, wifi_restarts, mqtt_att, mqtt_ok,
    pub_fail, pub_slow, fdrops, miss_n) plus q_hw / pub_max_ms maxima
  - flags: any GAP not attributable to a forced drop (fdrops), and any
    offline episode longer than RECOVERY_LIMIT_S

USAGE
    python3 soak_report.py ~/NGR/irscope_logs/irscope_YYYYMMDD_HHMMSS.csv
"""

import csv
import json
import sys
from datetime import datetime

RECOVERY_LIMIT_S = 30.0

COUNTERS = ["bdrop", "wifi_disc", "wifi_kicks", "wifi_restarts", "mqtt_att",
            "mqtt_ok", "pub_fail", "pub_slow", "fdrops", "miss_n"]
MAXIMA = ["q_hw", "pub_max_ms", "q"]


def wall(ts):
    try:
        return datetime.fromisoformat(ts)
    except ValueError:
        return None


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    path = sys.argv[1]

    sessions = {}          # sid -> stats
    online = None          # last seen online value
    offline_since = None   # wall time the LWT fired
    episodes = []          # (offline_wall, recovery_s or None)
    first = {}
    last = {}
    maxima = {k: 0 for k in MAXIMA}
    gaps = []              # (session, wall, info)
    missed = []
    n_status = 0

    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            rt = row.get("row_type", "")
            sid = row.get("session", "")
            if rt == "SAMPLE":
                s = sessions.setdefault(sid, {"lo": None, "hi": None, "n": 0})
                sm = int(row["sample"])
                s["lo"] = sm if s["lo"] is None else min(s["lo"], sm)
                s["hi"] = sm if s["hi"] is None else max(s["hi"], sm)
                s["n"] += 1
            elif rt == "GAP":
                gaps.append((sid, row.get("wall_time", ""), row.get("info", "")))
            elif rt == "MISSED":
                missed.append((sid, row.get("info", "")))
            elif rt == "STATUS":
                try:
                    d = json.loads(row.get("info", "") or "{}")
                except json.JSONDecodeError:
                    continue
                n_status += 1
                w = wall(row.get("wall_time", ""))
                if "online" in d:
                    v = d["online"]
                    if online is not None and v != online:
                        if v == 0:
                            offline_since = w
                        elif offline_since is not None and w is not None:
                            episodes.append(
                                (offline_since,
                                 (w - offline_since).total_seconds()))
                            offline_since = None
                    online = v
                for k in COUNTERS:
                    if k in d:
                        first.setdefault(k, d[k])
                        last[k] = d[k]
                for k in MAXIMA:
                    if k in d and isinstance(d[k], (int, float)):
                        maxima[k] = max(maxima[k], d[k])

    print("soak report — %s" % path)
    for sid, s in sessions.items():
        if s["lo"] is None:
            continue
        span = s["hi"] - s["lo"] + 1
        cov = 100.0 * s["n"] / span if span else 0.0
        print("  session %s: %.1f min firmware time, %d/%d samples logged "
              "(%.2f%% coverage)"
              % (sid, span / 60000.0, s["n"], span, cov))

    print("  transport GAPs: %d" % len(gaps))
    for (sid, w, info) in gaps[:20]:
        print("    [%s] %s" % (w, info))
    if len(gaps) > 20:
        print("    ... %d more" % (len(gaps) - 20))
    print("  sampler MISSED rows: %d" % len(missed))

    print("  status beats parsed: %d" % n_status)
    if first:
        print("  counter deltas (first -> last):")
        for k in COUNTERS:
            if k in first:
                print("    %-13s %s -> %s  (+%s)"
                      % (k, first[k], last[k], last[k] - first[k]))
        print("  maxima: " + "  ".join("%s=%s" % (k, maxima[k])
                                       for k in MAXIMA))

    print("  offline episodes (LWT online 0 -> 1): %d" % len(episodes))
    slow = 0
    for (w, dur) in episodes:
        flag = "  <-- EXCEEDS %.0f s" % RECOVERY_LIMIT_S if dur > RECOVERY_LIMIT_S else ""
        if dur > RECOVERY_LIMIT_S:
            slow += 1
        print("    offline at %s, recovered in %.1f s%s" % (w, dur, flag))
    if offline_since is not None:
        print("    STILL OFFLINE at end of log (since %s) — FAIL" % offline_since)

    forced = (last.get("fdrops", 0) - first.get("fdrops", 0)) if first else 0
    print("\n  verdict inputs: %d gap(s), %d forced drop(s), %d slow "
          "recover(ies)%s"
          % (len(gaps), forced, slow,
             ", device still offline" if offline_since else ""))
    print("  pass requires: every gap attributable (forced drop or declared"
          " AP outage), all recoveries <= %.0f s, no still-offline end, and"
          " coverage gaps matching bdrop accounting." % RECOVERY_LIMIT_S)


if __name__ == "__main__":
    main()
