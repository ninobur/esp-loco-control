#!/usr/bin/env python3
"""
align_markers.py — score a SENSORTEST run against the known marker sequence.

The locomotive does not know the map, so it cannot score itself. This does the
comparison in post, and separates three failures that look identical from
inside the locomotive and want completely different fixes:

    SUBSTITUTION   read S where the magnet is N   -> thresholds, classification
    DELETION       no event where a magnet exists -> sensitivity, speed, gap
    INSERTION      event where no magnet exists   -> noise, debounce, geometry

Every earlier measurement compared each reading against an `expected` value
derived from the odometer. One MISSED marker desynchronised the odometer and
every subsequent reading then scored as wrong -- one error became a hundred.
That is why 27% was only ever an upper bound. Alignment does not have that
failure mode: a deletion costs one deletion and the rest of the run stays in
register.

USAGE
    mosquitto_sub -h <broker> -t 'ngr/marker/+/event' -v > run.log
    python3 align_markers.py run.log
    python3 align_markers.py run.log --dir CW --verbose

Accepts the raw `mosquitto_sub -v` format, plain JSON lines, or serial capture
with a [MARK] prefix.
"""

import argparse, json, re, sys
from collections import Counter

# ---------------------------------------------------------------------------
# The map. 1 = N, 0 = S. Must match NGR_DNA1[] in the locomotive firmware.
# ---------------------------------------------------------------------------
DNA = [
    1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,
    0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
    0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,
    0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
    1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,
    1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
    1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,
    0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
    1,0,1,0,0,0,0,1,1,1,0
]
N = len(DNA)
assert N == 171, f"map is {N} entries, expected 171"

LANDMARKS = {0:"Southpoint", 15:"Patio", 63:"Grillers", 72:"Westpoint",
             98:"Northpoint", 107:"Arches", 140:"Eastpoint", 157:"Bamboo"}


# ---------------------------------------------------------------------------
def parse(path):
    """Return (events, marks). Events are dicts with at least seq and pol."""
    events, marks = [], []
    for raw in open(path, errors="replace"):
        line = raw.strip()
        if not line:
            continue
        i = line.find("{")
        if i < 0:
            continue
        try:
            d = json.loads(line[i:])
        except json.JSONDecodeError:
            continue
        if "mark" in d:
            marks.append(d)
        elif "pol" in d:
            events.append(d)
    return events, marks


def check_sequence(events):
    """Gaps in seq mean MQTT dropped a message, not that the sensor missed a
    marker. Scoring those as sensor error is the mistake this guards against."""
    gaps = []
    for a, b in zip(events, events[1:]):
        if "seq" in a and "seq" in b and b["seq"] != a["seq"] + 1:
            gaps.append((a["seq"], b["seq"], b["seq"] - a["seq"] - 1))
    return gaps


# ---------------------------------------------------------------------------
def align(obs, ref):
    """Semi-global alignment: every observation must be consumed, but the
    reference may start and end anywhere (free end gaps). Unit costs.
    Returns (cost, ops) where ops are ('=', o, r) / ('X', o, r) /
    ('D', None, r) / ('I', o, None)."""
    n, m = len(obs), len(ref)
    INF = float("inf")
    # dp[i][j] = best cost aligning obs[:i] against ref[:j]
    dp = [[INF] * (m + 1) for _ in range(n + 1)]
    bt = [[None] * (m + 1) for _ in range(n + 1)]
    for j in range(m + 1):
        dp[0][j] = 0          # free leading gap in reference
    for i in range(1, n + 1):
        dp[i][0] = i          # leading observations with no reference: insertions
        bt[i][0] = "I"
    for i in range(1, n + 1):
        oi = obs[i - 1]
        row, prev = dp[i], dp[i - 1]
        for j in range(1, m + 1):
            sub = prev[j - 1] + (0 if oi == ref[j - 1] else 1)
            dele = row[j - 1] + 1          # reference consumed, no observation
            ins = prev[j] + 1              # observation consumed, no reference
            best = sub; op = "=" if oi == ref[j - 1] else "X"
            if dele < best: best, op = dele, "D"
            if ins < best:  best, op = ins, "I"
            row[j] = best; bt[i][j] = op
    # free trailing reference gap: pick the best j at i = n
    jend = min(range(m + 1), key=lambda j: dp[n][j])
    ops, i, j = [], n, jend
    while i > 0:
        op = bt[i][j]
        if op in ("=", "X"):
            ops.append((op, i - 1, j - 1)); i -= 1; j -= 1
        elif op == "D":
            ops.append(("D", None, j - 1)); j -= 1
        else:
            ops.append(("I", i - 1, None)); i -= 1
    ops.reverse()
    return dp[n][jend], ops, jend


def best_alignment(obs, direction=None):
    """Try every start offset and both directions; keep the cheapest.
    The reference is repeated to cover multi-lap runs."""
    laps = len(obs) // N + 2
    best = None
    dirs = [1, -1] if direction is None else [direction]
    for d in dirs:
        for start in range(N):
            ref = [DNA[(start + d * k) % N] for k in range(N * laps)]
            cost, ops, used = align(obs, ref)
            if best is None or cost < best[0]:
                best = (cost, ops, d, start, ref)
    return best


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile")
    ap.add_argument("--dir", choices=["CW", "CCW"], default=None,
                    help="constrain direction; omit to let the aligner decide")
    ap.add_argument("--verbose", action="store_true",
                    help="list every error with its map position")
    args = ap.parse_args()

    events, marks = parse(args.logfile)
    if not events:
        sys.exit("no marker events found — check the log format")

    obs = [1 if e["pol"] == "N" else 0 for e in events]
    d = None if args.dir is None else (1 if args.dir == "CW" else -1)

    print(f"observations : {len(obs)}")
    gaps = check_sequence(events)
    if gaps:
        lost = sum(g[2] for g in gaps)
        print(f"\n*** MQTT dropped {lost} message(s) in {len(gaps)} gap(s). ***")
        print("    These are TRANSPORT losses, not sensor misses. They will")
        print("    score as deletions below and inflate the miss rate.")
        for a, b, k in gaps[:10]:
            print(f"      seq {a} -> {b}  ({k} missing)")
        if len(gaps) > 10:
            print(f"      ... and {len(gaps)-10} more")

    cost, ops, d_best, start, ref = best_alignment(obs, d)

    counts = Counter(op for op, _, _ in ops)
    good = counts["="]
    sub, dele, ins = counts["X"], counts["D"], counts["I"]
    aligned = good + sub + dele          # reference positions covered

    # The free leading gap makes many start offsets equivalent, so report the
    # reference position the alignment actually begins at, not the search seed.
    first_r = next((r for _, _, r in ops if r is not None), 0)
    first_mm = (start + d_best * first_r) % N
    print(f"\nbest fit     : direction {'CW' if d_best==1 else 'CCW'}, "
          f"first marker MM{first_mm:03d} ({LANDMARKS.get(first_mm,'—')})")
    print(f"edit cost    : {cost}\n")

    print(f"{'correct':<24}{good:>6}   {good/len(obs):>7.1%} of observations")
    print(f"{'wrong polarity':<24}{sub:>6}   {sub/len(obs):>7.1%}")
    print(f"{'missed markers':<24}{dele:>6}   {dele/aligned:>7.1%} of map positions")
    print(f"{'spurious events':<24}{ins:>6}   {ins/len(obs):>7.1%} of observations")

    err = (sub + dele + ins) / max(aligned, 1)
    print(f"\ncombined per-marker error rate: {err:.1%}")
    print("\n  <5%   architecture works untouched")
    print("  5-10% workable; LOST occasional, reacquires in ~20-30 markers")
    print("  10-20% marginal; LOST common")
    print("  >20%  not viable — fix the sensor before anything else")

    # where do the errors sit?
    if sub or dele:
        pos = [((start + d_best * r) % N) for op, _, r in ops
               if op in ("X", "D") and r is not None]
        hot = Counter(p for p in pos)
        worst = [p for p, c in hot.most_common(8)]
        if worst:
            print("\nmost frequent error positions:")
            for p in worst:
                print(f"   MM{p:03d} {LANDMARKS.get(p,''):<11} {hot[p]}x")

    if args.verbose:
        print("\nseq   obs  map   position")
        for op, o, r in ops:
            if op == "=":
                continue
            mm = (start + d_best * r) % N if r is not None else None
            e = events[o] if o is not None else None
            tag = {"X": "WRONG", "D": "MISSED", "I": "SPURIOUS"}[op]
            print(f"{e['seq'] if e else '   -':>5}  "
                  f"{('N' if e and e['pol']=='N' else 'S') if e else '-':>3}  "
                  f"{('N' if mm is not None and DNA[mm] else 'S') if mm is not None else '-':>3}   "
                  f"{('MM%03d'%mm) if mm is not None else '  -  '}  {tag}"
                  + (f"  peak={e['peak']} ms={e['ms']}" if e else ""))

    if marks:
        print(f"\noperator marks: {len(marks)}")
        for m in marks:
            print(f"   seq {m.get('seq','?')}  t={m.get('t','?')}  {m['mark']}")


if __name__ == "__main__":
    main()
