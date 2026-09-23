#!/usr/bin/env python3
"""
Otto, NAVI_ONE_1_0X15_DEPARTURE_DIAG, 2026-09-12/13 (field-records/logs/
20260913_otto_x16_floor82_session.log.gz). SECONDARY / cross-check dataset.

This is a DIFFERENT, OLDER, CLOSURE-ANCHORED event definition from Toby's
NAVI_COHERENCE data and is NOT pooled with it:
  - guard_ms=500 (per state/bootid), measured close-to-open (decision 0081's
    definition), NOT open-to-open.
  - `gap_ms` here IS a live field (unlike NAVI_COHERENCE's dead priorGapMs):
    it is the time from the PREVIOUS accepted event's CLOSE to THIS event's
    OPEN.
  - `pwm_close` is PWM sampled at THIS event's own CLOSE (~dur_ms after its
    own open), not at open. This is a genuinely different reference point
    than Toby's pwm_open; treated as an approximation of "current PWM," with
    the approximation only breaking down where PWM changes materially within
    one dur_ms window (i.e. exactly the ramp zones under discussion).
  - `dur_ms` = this event's own open-to-close duration.
  - Reconstruction: open-to-open(this, prev) ~= gap_ms(this) + dur_ms(prev).
    Computed ONLY for the 10 TOO_SOON events below, where prev's dur_ms is
    available from the immediately preceding accepted record in the same
    boot. This is an approximation with its own added noise (dur_ms varies);
    reported as such, not as a native measurement.
  - No event_serial-style boot-scoped id is present; boot segments are
    tracked via state/bootid line order, matching how mm/marker and
    state/nav duplicate every event (deduplicated here by keeping state/nav
    only).
"""
import json, os, gzip
from collections import defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(REPO, "field-records/logs/20260913_otto_x16_floor82_session.log.gz")

def iter_lines(path):
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "rt", errors="replace") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            parts = line.split("|", 2)
            if len(parts) < 3:
                continue
            ts, topic, payload = parts
            topic = topic.split("/", 3)[-1] if topic.startswith("ngr/") else topic
            yield n, ts, topic, payload

def main():
    boot_seg = -1
    events = []  # state/nav only (dedup vs mm/marker, identical payload)
    for n, ts, topic, payload in iter_lines(PATH):
        if topic == "state/bootid":
            boot_seg += 1
        elif topic == "state/nav":
            try:
                d = json.loads(payload)
            except Exception:
                continue
            d["_line"] = n; d["_boot_seg"] = boot_seg; d["_ts"] = ts
            events.append(d)

    print(f"Total state/nav events: {len(events)}, boot segments: {boot_seg+1}")

    genuine = [d for d in events if d.get("ruling") == "ADVANCED"]
    too_soon = [d for d in events if d.get("why") == "TOO_SOON"]
    wrong_mag = [d for d in events if d.get("ruling") == "WRONG_MAGNET"]
    print(f"ADVANCED (genuine): {len(genuine)}  TOO_SOON (false, time-gated): {len(too_soon)}  "
          f"WRONG_MAGNET (false, polarity -- different mechanism): {len(wrong_mag)}")

    def pwm_bin(pwm):
        if pwm >= 85: return ">=90"
        if pwm >= 75: return "80"
        if pwm >= 65: return "70"
        if pwm >= 55: return "60"
        if pwm >= 45: return "50"
        if pwm >= 35: return "40"
        return "<=30"
    BIN_ORDER = [">=90", "80", "70", "60", "50", "40", "<=30"]

    def pct(v, f):
        import math
        if not v: return float("nan")
        v = sorted(v); k = f*(len(v)-1); lo, hi = math.floor(k), math.ceil(k)
        return v[int(k)] if lo == hi else v[lo] + (v[hi]-v[lo])*(k-lo)

    print("\n=== Otto ADVANCED gap_ms (PREVIOUS CLOSE -> THIS OPEN; native units, NOT open-to-open) "
          "by pwm_close bin ===")
    by = defaultdict(list)
    for g in genuine:
        by[pwm_bin(g["pwm_close"])].append(g["gap_ms"])
    for b in BIN_ORDER:
        vals = by.get(b, [])
        if not vals:
            print(f"  {b:6} n=0"); continue
        print(f"  {b:6} n={len(vals):4d} min={min(vals):6d} p1={pct(vals,.01):6.0f} p5={pct(vals,.05):6.0f} "
              f"median={pct(vals,.5):7.0f}")

    print("\n=== Otto dur_ms (own open->close duration) distribution, for reconstruction error context ===")
    durs = [g["dur_ms"] for g in genuine]
    print(f"  n={len(durs)} min={min(durs)} p5={pct(durs,.05):.0f} median={pct(durs,.5):.0f} "
          f"p95={pct(durs,.95):.0f} max={max(durs)}")

    print("\n=== Otto TOO_SOON (false, time-gated) events -- full ground truth ===")
    # Reconstruct open-to-open using the immediately preceding record (any
    # ruling) in file order within the same boot segment, using ITS dur_ms.
    for h in too_soon:
        prev = None
        for cand in reversed(events):
            if cand["_line"] < h["_line"] and cand["_boot_seg"] == h["_boot_seg"] and "dur_ms" in cand:
                prev = cand; break
        recon = (h["gap_ms"] + prev["dur_ms"]) if prev else None
        print(f"  line {h['_line']} boot_seg {h['_boot_seg']} mm {h['mm']}->tgt {h['tgt']} dir {h['dir']} "
              f"pwm_close={h['pwm_close']} gap_ms(close->open)={h['gap_ms']} own_dur_ms={h['dur_ms']} "
              f"prev_ruling={prev['ruling'] if prev else '?'} prev_dur_ms={prev['dur_ms'] if prev else '?'} "
              f"RECONSTRUCTED_open_to_open={recon}")

    print("\n=== Otto WRONG_MAGNET (polarity mismatch -- different mechanism, NOT time-gated; "
          "excluded from the guard-relevant false population) ===")
    for h in wrong_mag:
        print(f"  line {h['_line']} boot_seg {h['_boot_seg']} mm {h['mm']}->tgt {h['tgt']} dir {h['dir']} "
              f"pwm_close={h['pwm_close']} gap_ms={h['gap_ms']} obs={h['obs']} expected={h['expected']}")

    print("\n=== Otto: closest genuine ADVANCED gap_ms values (bottom 15), for separation check ===")
    for g in sorted(genuine, key=lambda x: x["gap_ms"])[:15]:
        print(f"  line {g['_line']} pwm_close={g['pwm_close']} gap_ms={g['gap_ms']} dur_ms={g['dur_ms']} "
              f"mm {g.get('mm')}")

if __name__ == "__main__":
    main()
