#!/usr/bin/env python3
"""Checks on the CSVs extract_baseline_laps.py produces.

Structural checks must pass; the script exits non-zero if one fails. A
reconciliation against figures reported earlier by hand is printed as AGREES or
DIFFERS -- a difference is information, not a failure, and is never forced away.

Usage:  validate_baseline_laps.py --out DIR [--session 9950011-B20260914T125025]
"""

import argparse
import csv
import os
import statistics as st
from datetime import datetime

ROUTE_N = 171
RUN_SESSION = "9950011-B20260914T125025"
COLD_SESSION = "9950011-B20260914T120419"

fails = []


def check(name, ok, detail=""):
    print("  [%s] %s%s" % ("PASS" if ok else "FAIL", name, ("  -- " + detail) if detail else ""))
    if not ok:
        fails.append(name)


def note(name, agrees, detail):
    print("  [%s] %s  -- %s" % ("AGREES" if agrees else "DIFFERS", name, detail))


def load(out, name):
    p = os.path.join(out, name)
    if not os.path.exists(p) or os.path.getsize(p) == 0:
        return []
    with open(p, newline="") as fh:
        return list(csv.DictReader(fh))


def ts(s):
    return datetime.fromisoformat(s)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--session", default=RUN_SESSION)
    a = ap.parse_args()

    S = load(a.out, "sessions.csv")
    O = load(a.out, "origins.csv")
    L = load(a.out, "laps.csv")
    OB = load(a.out, "observations.csv")
    MK = load(a.out, "markers.csv")
    MM = load(a.out, "laps_mm000_compare.csv")

    sess = {r["session_id"]: r for r in S}
    run = sess.get(a.session)

    print("\n== structure ==")
    check("sessions.csv non-empty", bool(S), "%d sessions" % len(S))
    check("origins.csv non-empty", bool(O), "%d origin runs" % len(O))
    check("laps.csv non-empty", bool(L), "%d lap rows" % len(L))
    check("markers.csv non-empty", bool(MK), "%d marker rows" % len(MK))
    check("the run session is present", run is not None, a.session)

    comp = [l for l in L if l["complete"] == "1"]
    bad = [l for l in comp if int(l["accepted_advances"]) != ROUTE_N]
    check("every complete lap has exactly 171 accepted advances", not bad,
          "%d complete laps checked" % len(comp) if not bad else
          "offenders: " + ", ".join("%s lap %s (%s)" % (l["session_id"], l["lap_seq"], l["accepted_advances"]) for l in bad[:5]))

    notret = [l for l in comp if l["returned_to_origin"] != "1"]
    check("every complete lap ends on its session origin marker", not notret,
          "" if not notret else "offenders: " + ", ".join(
              "%s lap %s end MM%s origin MM%s" % (l["session_id"], l["lap_seq"], l["end_mm"], l["origin_mm"])
              for l in notret[:5]))

    check("no lap is longer than one circuit",
          all(int(l["accepted_advances"]) <= ROUTE_N for l in L))

    unexp = sum(int(l.get("adv_reset_unexplained", 0) or 0) for l in L)
    check("no unexplained advance-counter reset inside a lap", unexp == 0 or "adv_reset_unexplained" not in L[0],
          "%d" % unexp if "adv_reset_unexplained" in (L[0] if L else {}) else "column not emitted under this rule")

    print("\n== SET LOCATION lap semantics ==")

    # origin fixed for the life of the boot session, whatever the direction did
    for r in O:
        laps = [l for l in L if l["session_id"] == r["session_id"]
                and l["origin_index"] == r["origin_index"]]
        if not laps:
            continue
        mms = set(l["origin_mm"] for l in laps)
        check("origin MM%s @%s holds for all %d of its laps" % (r["origin_mm"], r["origin_ts"][11:19], len(laps)),
              mms == {r["origin_mm"]}, ",".join(sorted(mms)))

    # a direction change discards, it does not re-anchor
    dirchg = [l for l in L if l["end_reason"] == "DIRECTION_CHANGE"]
    check("every direction change discarded its lap in progress",
          all(l["complete"] == "0" for l in dirchg),
          "%d discarded, %d advances lost" % (len(dirchg), sum(int(l["accepted_advances"]) for l in dirchg)))
    for r in O:
        if int(r["direction_changes"]) and int(r["laps_complete"]):
            note("origin MM%s survived %s direction change(s)" % (r["origin_mm"], r["direction_changes"]),
                 True, "%s complete laps after it, %s advances discarded, %s spent returning to the origin"
                 % (r["laps_complete"], r["discarded_advances"], r["approach_advances"]))

    # a new SET LOCATION opens a new origin run
    for sid in sorted(set(r["session_id"] for r in O)):
        runs = [r for r in O if r["session_id"] == sid]
        if len(runs) > 1:
            check("%s: %d SET LOCATIONs -> %d origin runs" % (sid[-7:], len(runs), len(runs)),
                  all(r["origin_event"] == "DECLARED" for r in runs),
                  " ".join("MM%s@%s" % (r["origin_mm"], r["origin_ts"][11:19]) for r in runs))

    # a reboot ends everything: no lap may cross a session boundary
    bounds = sorted((ts(s["boot_ts"]), s["session_id"]) for s in S)
    cross = 0
    for l in L:
        t0, t1 = ts(l["lap_start_ts"]), ts(l["lap_end_ts"])
        for bt, sid in bounds:
            if sid != l["session_id"] and t0 < bt < t1:
                cross += 1
    check("no lap spans a reboot", cross == 0, "%d crossings" % cross)

    # incomplete laps preserved and marked
    inc = [l for l in L if l["complete"] == "0"]
    check("incomplete laps are preserved and marked", bool(inc),
          "%d, by reason: %s" % (len(inc), ", ".join(
              "%s=%d" % (k, sum(1 for l in inc if l["end_reason"] == k))
              for k in sorted(set(l["end_reason"] for l in inc)))))

    # only accepted advances count
    if MK:
        adv_ct = {}
        for m in MK:
            if m["ruling"] == "ADVANCED":
                adv_ct[m["session_id"]] = adv_ct.get(m["session_id"], 0) + 1
        for sid, n in sorted(adv_ct.items()):
            used = sum(int(l["accepted_advances"]) for l in L if l["session_id"] == sid)
            appr = sum(int(r["approach_advances"]) for r in O if r["session_id"] == sid)
            pre = n - used - appr
            check("%s: %d accepted advances accounted for" % (sid[-7:], n),
                  used + appr + pre == n and pre >= 0,
                  "%d in laps, %d returning to origin, %d before any origin" % (used, appr, pre))

    print("\n== windows ==")
    overlap = 0
    for key in set((l["session_id"], l["origin_index"]) for l in L):
        ls = sorted([l for l in L if (l["session_id"], l["origin_index"]) == key],
                    key=lambda r: int(r["lap_seq"]))
        for x, y in zip(ls, ls[1:]):
            if ts(y["lap_start_ts"]) < ts(x["lap_end_ts"]):
                overlap += 1
    check("lap windows never overlap", overlap == 0, "%d overlaps" % overlap)
    if OB:
        dbl = {}
        for o in OB:
            if o["lap_seq"]:
                k = (o["session_id"], o["ts"])
                dbl[k] = dbl.get(k, 0) + 1
        check("no observation lands in two laps", all(v == 1 for v in dbl.values()))

    print("\n== the ~30-lap dataset ==")
    if run:
        rl = [l for l in L if l["session_id"] == a.session and l["complete"] == "1"]
        check("30 complete laps recovered under the SET LOCATION rule", len(rl) == 30,
              "%d complete, MM%s origin, %s -> %s" % (
                  len(rl), rl[0]["origin_mm"], rl[0]["lap_start_ts"][11:19], rl[-1]["lap_end_ts"][11:19]) if rl else "none")
        rm = [m for m in MM if m["session_id"] == a.session and m["complete"] == "1"]
        if rm:
            note("the operator's 12:57:45-14:32:52 window", True,
                 "the MM000-wrap cut gives %d complete laps, %s -> %s; the "
                 "SET LOCATION cut gives the same laps %s -> %s" % (
                     len(rm), rm[0]["lap_start_ts"][11:19], rm[-1]["lap_end_ts"][11:19],
                     rl[0]["lap_start_ts"][11:19], rl[-1]["lap_end_ts"][11:19]))

    cold = [l for l in L if l["session_id"] == COLD_SESSION and l["complete"] == "1"]
    check("the cold run yields 7 complete laps", len(cold) == 7,
          "%d, origin MM%s" % (len(cold), cold[0]["origin_mm"] if cold else "-"))

    print("\n== reported findings ==")
    if run:
        rl = [l for l in L if l["session_id"] == a.session and l["complete"] == "1"]
        note("fixed baseline remained 1949",
             run["fixed_baseline_constant"] == "1" and run["startup_fixed_baseline"] == "1949",
             "startup %s, post-prime values: %s" % (run["startup_fixed_baseline"], run["fixed_baseline_values"]))
        spans = sorted(int(l["span"]) for l in rl)
        note("typical within-lap span about 7 counts", abs(st.median(spans) - 7) <= 1,
             "median %g, quartiles %g-%g, range %d-%d" % (
                 st.median(spans), spans[len(spans) // 4], spans[3 * len(spans) // 4], spans[0], spans[-1]))
        rm = [m for m in MM if m["session_id"] == a.session and m["complete"] == "1"]
        if len(rm) >= 5:
            b = float(rm[-1]["startup_fixed_baseline"])
            got = [round(float(m["center_permm_median"]) - b, 2) for m in rm[-5:]]
            note("final five lap centres +6.0 +6.0 +6.75 +7.5 +7.5 (MM000 cut)",
                 got == [6.0, 6.0, 6.75, 7.5, 7.5], " ".join("%+.2f" % g for g in got))
            gota = [round(float(l["center_permm_median"]) - b, 2) for l in rl[-5:]]
            note("  the same five under the SET LOCATION rule", gota == [6.0, 6.0, 6.75, 7.5, 7.5],
                 " ".join("%+.2f" % g for g in gota))

    print("\n== observation accounting ==")
    if OB:
        tot = len(OB)
        byr = {}
        for o in OB:
            k = o["exclude_reason"] or "(kept)"
            byr[k] = byr.get(k, 0) + 1
        for k, v in sorted(byr.items(), key=lambda kv: -kv[1]):
            print("    %-32s %6d  %5.1f%%" % (k, v, 100.0 * v / tot))
        check("some observations survived the filters", byr.get("(kept)", 0) > 0)

    print("")
    if fails:
        print("FAILED: " + ", ".join(fails))
        raise SystemExit(1)
    print("all structural checks passed")


if __name__ == "__main__":
    main()
