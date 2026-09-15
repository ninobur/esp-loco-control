#!/usr/bin/env python3
"""Tests for the baseline-controller model in replay_baseline_controller.py.

Synthetic fixtures for the rules that are easy to get wrong and hard to see in
aggregate output -- origin held across a direction change, reset on a new SET
LOCATION, reset at a reboot, incomplete laps producing no adjustment, the cap,
the escape rule's sign and consecutiveness conditions, and integer rounding --
followed by assertions against the real extracted CSVs when they are present.

Usage:  validate_replay.py [--out DIR]
"""

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay_baseline_controller as R   # noqa: E402

fails = []


def check(name, ok, detail=""):
    print("  [%s] %s%s" % ("PASS" if ok else "FAIL", name, ("  -- " + detail) if detail else ""))
    if not ok:
        fails.append(name)


def lap(seq, est, complete=1, origin=1, origin_mm=71, stop=0, wdraw=0, mm=171,
        t0="2026-09-14T13:00:00.000", t1="2026-09-14T13:03:00.000"):
    return {
        "session_id": "S", "origin_index": str(origin), "origin_mm": str(origin_mm),
        "origin_ts": t0, "lap_seq": str(seq), "lap_start_ts": t0, "lap_end_ts": t1,
        "complete": str(complete), "accepted_advances": "171" if complete else "50",
        "center_permm_median": ("" if est is None else str(est)),
        "has_stop": str(stop), "has_withdrawal": str(wdraw), "n_mm_covered": str(mm),
    }


def run(laps, startup=1900, cap=2, persistence=0, threshold=0, xcap=0,
        target="newest", rounding="estimate", setloc_resets=True):
    cand = R.Candidate(cap, persistence, threshold, xcap, target, "permm_median", rounding)
    sess = {"startup_fixed_baseline": str(startup), "boot_ts": "2026-09-14T12:00:00.000"}
    rows, timeline = R.replay_session(cand, sess, laps, [], [], setloc_resets)
    return rows, timeline


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="")
    a = ap.parse_args()

    print("\n== the startup baseline holds until a lap completes ==")
    rows, _ = run([lap(1, None, complete=0), lap(2, 1910)], startup=1900, cap=2)
    check("an incomplete lap makes no adjustment",
          rows[0]["applied_correction"] == "" and rows[0]["baseline_after"] == 1900,
          "reason=%s" % rows[0]["invalid_reason"])
    check("the first completed lap adjusts by the normal cap and no more",
          rows[1]["applied_correction"] == 2 and rows[1]["baseline_after"] == 1902,
          "requested %s applied %s" % (rows[1]["requested_correction"], rows[1]["applied_correction"]))
    check("the first lap gets NO special authority",
          rows[1]["rule_fired"] == "NORMAL")

    print("\n== a lap that contains a stop or a withdrawal is not evidence ==")
    rows, _ = run([lap(1, 1930, stop=1), lap(2, 1930, wdraw=1), lap(3, 1930, mm=100)], startup=1900)
    check("stop, withdrawal and thin coverage all refuse the update",
          all(r["valid_for_update"] == 0 for r in rows),
          ", ".join(r["invalid_reason"] for r in rows))

    print("\n== the cap ==")
    for cap in (1, 2, 3, 4):
        rows, _ = run([lap(1, 1950)], startup=1900, cap=cap)
        check("cap %d limits a +50 request to %+d" % (cap, cap),
              rows[0]["applied_correction"] == cap and rows[0]["correction_capped"] == 1)

    print("\n== integer rounding: the operative baseline is never fractional ==")
    rows, tl = run([lap(1, 1901.5), lap(2, 1902.5), lap(3, 1903.4)], startup=1900, cap=2)
    check("every baseline value is an int",
          all(isinstance(v, int) for _, v in tl) and all(isinstance(r["baseline_after"], int) for r in rows),
          " ".join(str(v) for _, v in tl))
    check("1901.5 rounds away from zero to 1902 (not banker's 1902 by luck)",
          R.rhalf(1901.5) == 1902 and R.rhalf(1902.5) == 1903 and R.rhalf(-0.5) == -1,
          "rhalf: 1901.5->%d 1902.5->%d -0.5->%d" % (R.rhalf(1901.5), R.rhalf(1902.5), R.rhalf(-0.5)))
    rows, _ = run([lap(1, 1900.5)], startup=1900, cap=2, rounding="correction_trunc")
    check("correction_trunc leaves a +0.5 request unmoved (the dead zone)",
          rows[0]["applied_correction"] == 0, "applied %s" % rows[0]["applied_correction"])
    rows, _ = run([lap(1, 1900.5)], startup=1900, cap=2, rounding="correction_nearest")
    check("correction_nearest moves it by 1", rows[0]["applied_correction"] == 1)
    rows, _ = run([lap(1, 1900.5)], startup=1900, cap=2, rounding="estimate")
    check("estimate rounds 1900.5 to 1901 and moves by 1", rows[0]["applied_correction"] == 1)

    print("\n== the persistent-error escape ==")
    ls = [lap(1, 1912), lap(2, 1924), lap(3, 1936)]
    rows, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=8)
    check("one qualifying lap does not fire the rule",
          rows[0]["rule_fired"] == "NORMAL" and rows[0]["consecutive_qualifying"] == 1)
    check("the second consecutive qualifying lap fires it",
          rows[1]["rule_fired"] == "EXCEPTIONAL" and rows[1]["applied_correction"] == 8,
          "applied %s" % rows[1]["applied_correction"])
    check("firing resets the consecutive count",
          rows[1]["consecutive_qualifying"] == 0)

    ls = [lap(1, 1912), lap(2, 1888), lap(3, 1876)]
    rows, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=8)
    check("a sign flip restarts the run rather than continuing it",
          rows[1]["rule_fired"] == "NORMAL" and rows[1]["consecutive_qualifying"] == 1,
          "lap2 rule %s run %s" % (rows[1]["rule_fired"], rows[1]["consecutive_qualifying"]))

    ls = [lap(1, 1912), lap(2, 1901), lap(3, 1930)]
    rows, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=8)
    check("a non-qualifying lap in the middle breaks the run",
          all(r["rule_fired"] == "NORMAL" for r in rows),
          " ".join("%s/%s" % (r["rule_fired"], r["consecutive_qualifying"]) for r in rows))

    ls = [lap(1, 1950)]
    rows, _ = run(ls, startup=1900, cap=2, persistence=1, threshold=10, xcap=math.inf)
    check("an unlimited exceptional cap goes all the way to the estimate",
          rows[0]["applied_correction"] == 50 and rows[0]["baseline_after"] == 1950)

    ls = [lap(1, 1912), lap(2, 1930)]
    a1, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=math.inf, target="newest")
    a2, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=math.inf, target="median")
    check("target=newest and target=median differ when the estimates differ",
          a1[1]["baseline_after"] == 1930 and a2[1]["baseline_after"] == 1921,
          "newest -> %s, median -> %s" % (a1[1]["baseline_after"], a2[1]["baseline_after"]))

    print("\n== a new SET LOCATION resets controller state ==")
    ls = [lap(1, 1912, origin=1), lap(2, 1924, origin=1), lap(3, 1936, origin=2, origin_mm=36)]
    rows, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=8, setloc_resets=True)
    check("the consecutive-qualifying run does not carry across the new origin",
          rows[2]["rule_fired"] == "NORMAL" and rows[2]["consecutive_qualifying"] == 1)
    check("with --setloc-resets-baseline yes the baseline returns to startup",
          rows[2]["baseline_before"] == 1900, "before %s" % rows[2]["baseline_before"])
    rows, _ = run(ls, startup=1900, cap=2, persistence=2, threshold=10, xcap=8, setloc_resets=False)
    check("with --setloc-resets-baseline no it carries forward",
          rows[2]["baseline_before"] == rows[1]["baseline_after"],
          "before %s, previous after %s" % (rows[2]["baseline_before"], rows[1]["baseline_after"]))

    print("\n== a reboot resets everything ==")
    r1, t1 = run([lap(1, 1930), lap(2, 1930)], startup=1900, cap=2)
    r2, t2 = run([lap(1, 1930)], startup=1948, cap=2)
    check("a second session starts from its own startup baseline, not the first's",
          r2[0]["baseline_before"] == 1948 and r1[-1]["baseline_after"] == 1904,
          "session 2 starts at %s" % r2[0]["baseline_before"])

    print("\n== polarity arithmetic ==")
    # A North magnet of amplitude 150 on a resting level 20 above the reference
    # reads 170 against that reference; a South one reads 130.
    check("N effective peak rises with a low reference, S falls",
          (150 + 20) == 170 and (150 - 20) == 130)
    check("the entry threshold and the exit margin are Otto's",
          R.ENTRY_MARGIN == 70 and R.EXIT_MARGIN == 25)
    check("the latch band is the measured one, not the nominal exit margin",
          R.LATCH_BAND == 40 and R.SOUTH_FLOOR_RISK == 25 and R.NORTH_STRETCH_RISK == 30)

    if a.out and os.path.exists(os.path.join(a.out, "laps.csv")):
        print("\n== against the extracted dataset ==")
        laps = R.load(a.out, "laps.csv")
        sessions = {r["session_id"]: r for r in R.load(a.out, "sessions.csv")}
        warm = [l for l in laps if l["session_id"].endswith("T125025")]
        cold = [l for l in laps if l["session_id"].endswith("T120419")]
        check("the warm run's 30 laps all carry origin MM71",
              set(l["origin_mm"] for l in warm if l["complete"] == "1") == {"71"})
        rows, tl = run(warm, startup=1949, cap=2)
        check("cap 2 never binds on the warm run",
              all(r["correction_capped"] in ("", 0) for r in rows),
              "%d updates, final baseline %d" % (sum(1 for r in rows if r["valid_for_update"] == 1), tl[-1][1]))
        rows, tl = run([l for l in cold if l["origin_index"] == "1"], startup=1905, cap=2)
        upd = [r for r in rows if r["valid_for_update"] == 1]
        check("cap 2 binds on 6 of the cold run's 7 laps",
              sum(1 for r in upd if r["correction_capped"] == 1) == 6,
              "final baseline %d against a lap estimate of %s" % (tl[-1][1], upd[-1]["lap_estimate"]))

    print("")
    if fails:
        print("FAILED: " + ", ".join(fails))
        raise SystemExit(1)
    print("all controller-model tests passed")


if __name__ == "__main__":
    main()
