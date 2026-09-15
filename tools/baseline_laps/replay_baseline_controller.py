#!/usr/bin/env python3
"""Replay a whole-lap baseline controller over the mined dataset.

WHAT IT MODELS
--------------
  * the startup baseline is measured before the locomotive moves and stays
    authoritative until a completed lap is available;
  * every completed lap yields one route-wide resting-level estimate;
  * the operative baseline is an INTEGER and moves only at completed-lap
    boundaries;
  * normal adjustment is capped at +/- `--normal-cap` counts per completed lap;
  * a persistent-error escape may permit a larger correction when several
    consecutive completed laps independently show the same-signed error beyond
    a threshold;
  * a new SET LOCATION and a reboot reset controller state;
  * no rolling one-second estimator ever touches the operative baseline.

The first completed lap gets NO special authority. The cold run shows why: its
first completed lap is still at the startup value and the warming arrives
afterwards, so first-lap privilege buys nothing and would hand full authority to
whichever lap happened to be first.

WHAT IT DOES NOT DO
-------------------
It decides no firmware policy, and it does not model passage DURATION, so it
cannot say whether a passage that opens more weakly would then fall under the
82 ms floor. Where that matters it is said out loud rather than estimated.

POLARITY
--------
Otto's raw level rises through a North magnet and falls through a South one, and
`peak` is published against entryBaseline_ -- the frozen startup baseline -- not
against the resting level. With E = resting level - operative baseline:

    N:  peak = A + E      S:  peak = A - E

so the true amplitude A is recovered first, and the passage is then re-tested
against the candidate's baseline. A reference that sits BELOW the resting level
(E > 0) makes South magnets harder to open and North magnets easier; it is not
symmetric, and an absolute error alone cannot say whether a candidate is safe.

Usage:
  replay_baseline_controller.py --out DIR [--normal-cap 1,2,3,4] ...
"""

import argparse
import csv
import itertools
import math
import os
import statistics as st
from datetime import datetime

ENTRY_MARGIN = 70      # Otto: HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS
EXIT_MARGIN = 25       # Otto: HALL_DEADBAND_COUNTS
AMP_FLOOR = 0.34       # recognizer AMPLITUDE gate, peak/gain
ERR_BANDS = (8, 10, 15, 20, 25, 30, 40)

# Empirical bands, measured on this dataset rather than assumed. Duration of an
# accepted passage against the signed error E = resting level - operative
# baseline, over 6,807 accepted passages of 2026-09-14:
#
#   E band     SOUTH median / p10 / min      NORTH median / p90 / max
#   +0..+4       127 / 115 / 102 ms            131 / 146 / 1333 ms
#   +15..+19     116 / 104 /  96 ms            151 / 169 /  185 ms
#   +25..+29     108 /  95 /  84 ms            166 / 185 /  245 ms
#   +30..+34     102 /  94 /  85 ms            183 / 240 /  435 ms
#   +40..+49     152 /  93 /  93 ms           1415 / 1784 / 2542 ms
#
# The two failure modes are OPPOSITE polarities and they are not symmetric:
# a reference below the resting level shortens South passages towards the 82 ms
# floor -- both floor rejections with a known resting level are South, at
# E >= +20 -- and stretches North passages towards a latch, which is what the
# 1784 ms and 2542 ms passages are. The nominal exit margin of 25 is NOT where
# closing fails: raw noise carries the close well past it, and the measured
# latches appear around |E| = 40.
SOUTH_FLOOR_RISK = 25     # South passages at or beyond this reach 84 ms
NORTH_STRETCH_RISK = 30   # North passages at or beyond this reach 435 ms
LATCH_BAND = 40           # measured latches: 1784 ms and 2542 ms


def ts(s):
    return datetime.fromisoformat(s)


def rhalf(x):
    """Round half AWAY FROM ZERO. Python's round() is banker's rounding, which
    on a dataset this full of exact .5 estimates would silently alternate."""
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))


def clamp(x, lim):
    return max(-lim, min(lim, x))


def load(out, name):
    p = os.path.join(out, name)
    with open(p, newline="") as fh:
        return list(csv.DictReader(fh))


def fnum(v, default=None):
    if v is None or v == "":
        return default
    try:
        return float(v)
    except ValueError:
        return default


# --------------------------------------------------------------- candidate ---

class Candidate(object):
    def __init__(self, cap, persistence, threshold, exc_cap, target,
                 estimator, rounding):
        self.cap = cap
        self.persistence = persistence          # 0 = escape rule disabled
        self.threshold = threshold
        self.exc_cap = exc_cap                  # math.inf allowed
        self.target = target                    # newest | median
        self.estimator = estimator
        self.rounding = rounding

    @property
    def id(self):
        if not self.persistence:
            return "cap%g/noescape/%s/%s" % (self.cap, self.estimator, self.rounding)
        return "cap%g/P%d/T%g/X%s/%s/%s/%s" % (
            self.cap, self.persistence, self.threshold,
            ("inf" if self.exc_cap == math.inf else "%g" % self.exc_cap),
            self.target, self.estimator, self.rounding)

    def as_dict(self):
        return {
            "candidate": self.id, "normal_cap": self.cap,
            "persistence": self.persistence or "",
            "threshold": self.threshold if self.persistence else "",
            "exceptional_cap": ("inf" if self.exc_cap == math.inf else self.exc_cap) if self.persistence else "",
            "target": self.target if self.persistence else "",
            "estimator": self.estimator, "rounding": self.rounding,
        }


def apply_rounding(cand, want_float, cap, est_float, base):
    """Return the integer correction actually applied.

    `estimate`            round the lap estimate to an integer first, then take
                          an integer difference and clamp it. The operative
                          baseline is integral, so this is the arithmetic the
                          firmware could actually do.
    `correction_nearest`  clamp the fractional difference, then round it.
    `correction_trunc`    clamp the fractional difference, then truncate toward
                          zero -- which turns every |delta| < 1 into no movement
                          at all, a dead zone worth seeing.
    """
    if cand.rounding == "estimate":
        d = rhalf(est_float) - base
        return int(clamp(d, cap)) if cap != math.inf else int(d)
    v = want_float if cap == math.inf else clamp(want_float, cap)
    if cand.rounding == "correction_trunc":
        return int(v)                      # truncation toward zero
    return rhalf(v)


# ------------------------------------------------------------------ replay ---

def replay_session(cand, sess, laps, obs, marks, setloc_resets):
    """Walk one boot session under one candidate. Returns (lap_rows, timeline)."""
    startup = fnum(sess["startup_fixed_baseline"])
    if startup is None:
        return [], []
    base = int(startup)
    timeline = [(ts(sess["boot_ts"]), base)]

    run_count, run_sign, run_est = 0, 0, []
    cur_origin = None
    rows = []

    for lp in laps:
        oi = lp["origin_index"]
        if oi != cur_origin:
            if cur_origin is not None:
                # A new SET LOCATION resets lap-controller state. Whether the
                # operative baseline itself reverts to the startup value is an
                # ambiguity in the brief, so it is a switch, not an assumption.
                if setloc_resets:
                    base = int(startup)
                    timeline.append((ts(lp["origin_ts"]), base))
            run_count, run_sign, run_est = 0, 0, []
            cur_origin = oi

        complete = lp["complete"] == "1"
        est = fnum(lp["center_" + cand.estimator])
        valid = (complete and est is not None
                 and lp["has_stop"] != "1" and lp["has_withdrawal"] != "1"
                 and int(lp["n_mm_covered"] or 0) >= 150)
        why = ""
        if not complete:
            why = "incomplete"
        elif est is None:
            why = "no_estimate"
        elif lp["has_stop"] == "1" or lp["has_withdrawal"] == "1":
            why = "lap_contains_stop_or_withdrawal"
        elif int(lp["n_mm_covered"] or 0) < 150:
            why = "thin_route_coverage"

        before = base
        requested = applied = 0
        rule = "NONE"
        capped = 0
        if valid:
            delta = est - base
            sgn = 0 if abs(delta) < 1e-9 else (1 if delta > 0 else -1)
            qualifies = abs(delta) >= cand.threshold if cand.persistence else False
            if qualifies and (run_sign == 0 or sgn == run_sign):
                run_sign = sgn
                run_count += 1
                run_est.append(est)
            elif qualifies:
                run_sign, run_count, run_est = sgn, 1, [est]
            else:
                run_sign, run_count, run_est = 0, 0, []

            if cand.persistence and run_count >= cand.persistence:
                tgt = est if cand.target == "newest" else st.median(run_est[-cand.persistence:])
                requested = tgt - base
                applied = apply_rounding(cand, requested, cand.exc_cap, tgt, base)
                rule = "EXCEPTIONAL"
                capped = 1 if abs(requested) > cand.exc_cap + 1e-9 else 0
                run_sign, run_count, run_est = 0, 0, []
            else:
                requested = delta
                applied = apply_rounding(cand, requested, cand.cap, est, base)
                rule = "NORMAL"
                capped = 1 if abs(requested) > cand.cap + 1e-9 else 0
            base = before + applied
            timeline.append((ts(lp["lap_end_ts"]), base))

        rows.append({
            "candidate": cand.id,
            "session_id": lp["session_id"],
            "origin_index": oi, "origin_mm": lp["origin_mm"],
            "lap_seq": lp["lap_seq"],
            "lap_start_ts": lp["lap_start_ts"], "lap_end_ts": lp["lap_end_ts"],
            "complete": lp["complete"], "valid_for_update": 1 if valid else 0,
            "invalid_reason": why,
            "accepted_advances": lp["accepted_advances"],
            "lap_estimate": est if est is not None else "",
            "lap_estimate_rounded": rhalf(est) if est is not None else "",
            "startup_baseline": int(startup),
            "baseline_before": before, "baseline_after": base,
            "requested_correction": round(requested, 3) if valid else "",
            "applied_correction": applied if valid else "",
            "rule_fired": rule, "correction_capped": capped if valid else "",
            "consecutive_qualifying": run_count,
            "error_before": round(est - before, 3) if est is not None else "",
            "error_after": round(est - base, 3) if est is not None else "",
        })
    return rows, timeline


def _reversals(steps):
    """How often the correction changes sign. Unnecessary movement during stable
    running shows up here, where a net-movement figure hides it."""
    n, prev = 0, 0
    for v in steps:
        if not v:
            continue
        sgn = 1 if v > 0 else -1
        if prev and sgn != prev:
            n += 1
        prev = sgn
    return n


def baseline_at(timeline, t):
    b = timeline[0][1]
    for tt, v in timeline:
        if tt <= t:
            b = v
        else:
            break
    return b


def in_lap_marks(marks, windows):
    return [m for m in marks if any(t0 < ts(m["ts"]) <= t1 for t0, t1 in windows)]


def safety(sess, timeline, obs, marks, inlap_only):
    """Instantaneous error, band residency, and the polarity-aware passage test.

    `obs` and `marks` are pre-filtered by the caller to the same stretch of
    running, so their counts describe one population and can be compared.
    """
    out = {}
    o = [r for r in obs if r["usable"] == "1"]
    errs = []
    for r in o:
        t = ts(r["ts"])
        errs.append((t, float(r["shadow_baseline"]) - baseline_at(timeline, t), r))
    if not errs:
        return None

    e_vals = [e for _, e, _ in errs]
    emax, emin = max(e_vals), min(e_vals)
    eabs = max(abs(emax), abs(emin))
    out["n_obs"] = len(errs)
    out["err_max_pos"] = round(emax, 2)
    out["err_max_neg"] = round(emin, 2)
    out["err_max_abs"] = round(eabs, 2)
    out["err_median"] = round(st.median(e_vals), 2)
    out["err_p95_abs"] = round(sorted(abs(v) for v in e_vals)[int(0.95 * len(e_vals))], 2)

    # STATUS is 1 Hz, so a sample is a second.
    for b in ERR_BANDS:
        out["secs_abs_gt_%d" % b] = sum(1 for v in e_vals if abs(v) > b)

    # South is the polarity that a too-low reference starves; North the one a
    # too-high reference starves. Report each requirement on its own.
    out["south_opening_requirement"] = ENTRY_MARGIN + max(0.0, emax)
    out["north_opening_requirement"] = ENTRY_MARGIN + max(0.0, -emin)
    out["closing_margin_min"] = round(EXIT_MARGIN - eabs, 2)
    out["nominal_close_margin_negative"] = 1 if eabs >= EXIT_MARGIN else 0
    out["in_measured_latch_band"] = 1 if eabs >= LATCH_BAND else 0
    out["permanently_open"] = 1 if eabs >= ENTRY_MARGIN else 0

    # passages
    n_adv = swallow = amp_fail = 0
    south_floor = north_stretch = latch_band = 0
    worst = None
    floor_weaker = floor_stronger = 0
    mk_bands = dict((b, 0) for b in ERR_BANDS)
    for m in marks:
        amp = fnum(m["amplitude"])
        rest = fnum(m["resting_level"])
        if amp is None or rest is None or m["obs"] not in ("N", "S"):
            continue
        t = ts(m["ts"])
        e = rest - baseline_at(timeline, t)
        eff = amp + e if m["obs"] == "N" else amp - e
        if m["ruling"] == "ADVANCED":
            n_adv += 1
            for b in ERR_BANDS:
                if abs(e) > b:
                    mk_bands[b] += 1
            if eff < ENTRY_MARGIN:
                swallow += 1
            if m["obs"] == "S" and e >= SOUTH_FLOOR_RISK:
                south_floor += 1
            if m["obs"] == "N" and e >= NORTH_STRETCH_RISK:
                north_stretch += 1
            if abs(e) >= LATCH_BAND:
                latch_band += 1
            g = fnum(m["gain"])
            if g and eff / g < AMP_FLOOR:
                amp_fail += 1
            if worst is None or eff < worst[0]:
                worst = (eff, m, e)
        elif m["ruling"] == "DURATION_FLOOR":
            peak = fnum(m["peak"])
            if peak is not None:
                if eff > peak:
                    floor_stronger += 1
                else:
                    floor_weaker += 1

    out["n_advanced_passages"] = n_adv
    out["passages_would_not_open"] = swallow
    out["south_floor_risk_passages"] = south_floor
    out["north_stretch_risk_passages"] = north_stretch
    out["passages_in_latch_band"] = latch_band
    out["passages_below_amp_floor"] = amp_fail
    out["worst_effective_peak"] = round(worst[0], 1) if worst else ""
    out["worst_effective_peak_mm"] = (worst[1]["mm"] if worst else "")
    out["worst_effective_peak_obs"] = (worst[1]["obs"] if worst else "")
    out["worst_effective_peak_ts"] = (worst[1]["ts"][11:19] if worst else "")
    out["worst_effective_peak_margin"] = round(worst[0] - ENTRY_MARGIN, 1) if worst else ""
    out["floor_rejected_weaker"] = floor_weaker
    out["floor_rejected_stronger"] = floor_stronger
    for b in ERR_BANDS:
        out["markers_abs_gt_%d" % b] = mk_bands[b]
    return out


def parse_list(sval, conv=float):
    out = []
    for tok in sval.split(","):
        tok = tok.strip()
        if not tok:
            continue
        out.append(math.inf if tok.lower() in ("inf", "unlimited", "none") else conv(tok))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="directory holding the extracted CSVs")
    ap.add_argument("--normal-cap", default="1,2,3,4")
    ap.add_argument("--persistence", default="1,2,3",
                    help="consecutive qualifying laps; add 0 for a no-escape control")
    ap.add_argument("--threshold", default="8,10,12,15,20")
    ap.add_argument("--exceptional-cap", default="4,6,8,10,inf")
    ap.add_argument("--target", default="newest,median")
    ap.add_argument("--estimator", default="permm_median")
    ap.add_argument("--rounding", default="estimate")
    ap.add_argument("--setloc-resets-baseline", default="yes", choices=["yes", "no"])
    ap.add_argument("--inlap-only", action="store_true",
                    help="restrict the safety metrics to observations taken inside a COMPLETE lap -- i.e. while the controller was actually in charge and the locomotive was circulating")
    ap.add_argument("--detail", default="",
                    help="comma-separated substrings; lap rows and safety "
                         "events are written in full for candidates whose id "
                         "contains any of them")
    ap.add_argument("--prefix", default="")
    a = ap.parse_args()

    sessions = {r["session_id"]: r for r in load(a.out, "sessions.csv")}
    laps_all, obs_all, mk_all = {}, {}, {}
    for r in load(a.out, "laps.csv"):
        laps_all.setdefault(r["session_id"], []).append(r)
    for r in load(a.out, "observations.csv"):
        obs_all.setdefault(r["session_id"], []).append(r)
    for r in load(a.out, "markers.csv"):
        mk_all.setdefault(r["session_id"], []).append(r)

    caps = parse_list(a.normal_cap)
    pers = [int(x) for x in parse_list(a.persistence)]
    thrs = parse_list(a.threshold)
    xcaps = parse_list(a.exceptional_cap)
    tgts = [t.strip() for t in a.target.split(",") if t.strip()]
    ests = [t.strip() for t in a.estimator.split(",") if t.strip()]
    rnds = [t.strip() for t in a.rounding.split(",") if t.strip()]

    cands = []
    for cap, est, rnd in itertools.product(caps, ests, rnds):
        cands.append(Candidate(cap, 0, 0, 0, "newest", est, rnd))     # control
        for p, th, xc, tg in itertools.product([x for x in pers if x > 0], thrs, xcaps, tgts):
            cands.append(Candidate(cap, p, th, xc, tg, est, rnd))

    setloc_resets = (a.setloc_resets_baseline == "yes")
    detail_keys = [k.strip() for k in a.detail.split(",") if k.strip()]
    summary, detail_laps, detail_events = [], [], []
    nodata = {}

    for cand in cands:
        for sid, sess in sorted(sessions.items()):
            laps = laps_all.get(sid, [])
            obs = obs_all.get(sid, [])
            marks = mk_all.get(sid, [])
            rows, timeline = replay_session(cand, sess, laps, obs, marks, setloc_resets)
            if not timeline:
                continue
            windows = [(ts(l["lap_start_ts"]), ts(l["lap_end_ts"]))
                       for l in laps if l["complete"] == "1"]
            obs_used = ([r for r in obs if r["in_complete_lap"] == "1"]
                        if a.inlap_only else obs)
            marks_used = in_lap_marks(marks, windows) if a.inlap_only else marks
            saf = safety(sess, timeline, obs_used, marks_used, a.inlap_only)
            if saf is None:
                # A stationary bench boot has no sample taken above the tractive
                # floor, so it has no measured resting level to be in error
                # against. It is listed once, in its own file, rather than
                # repeated identically under every candidate: its startup
                # baseline is still evidence about where a fixed reference lands.
                if sid not in nodata:
                    nodata[sid] = {
                        "session_id": sid,
                        "sketch": sess.get("sketch", ""),
                        "boot_ts": sess["boot_ts"],
                        "duration_s": sess.get("duration_s", ""),
                        "startup_fixed_baseline": sess["startup_fixed_baseline"],
                        "n_markers": sess.get("n_markers", ""),
                        "n_laps_complete": sess.get("n_laps_complete", ""),
                        "reason": "no observation above the tractive floor with a known position",
                    }
                continue
            saf["no_moving_observations"] = 0
            upd = [r for r in rows if r["valid_for_update"] == 1]
            errs_after = [abs(float(r["error_after"])) for r in upd if r["error_after"] != ""]
            row = cand.as_dict()
            row.update({
                "session_id": sid,
                "startup_baseline": sess["startup_fixed_baseline"],
                "n_laps_complete": sum(1 for r in rows if r["complete"] == "1"),
                "n_updates": len(upd),
                "n_exceptional": sum(1 for r in upd if r["rule_fired"] == "EXCEPTIONAL"),
                "n_capped": sum(1 for r in upd if r["correction_capped"] == 1),
                "total_movement": sum(abs(r["applied_correction"]) for r in upd),
                "net_movement": sum(r["applied_correction"] for r in upd),
                "n_nonzero_updates": sum(1 for r in upd if r["applied_correction"]),
                "n_sign_reversals": _reversals([r["applied_correction"] for r in upd]),
                "final_baseline": timeline[-1][1],
                "baseline_after_last_update": (upd[-1]["baseline_after"] if upd else int(fnum(sess["startup_fixed_baseline"]) or 0)),
                "max_lap_error_after": round(max(errs_after), 2) if errs_after else "",
            })
            row.update(saf)
            summary.append(row)
            if detail_keys and any(k in cand.id for k in detail_keys):
                detail_laps.extend(rows)
                for m in marks_used:
                    amp = fnum(m["amplitude"])
                    rest = fnum(m["resting_level"])
                    if amp is None or rest is None or m["obs"] not in ("N", "S"):
                        continue
                    e = rest - baseline_at(timeline, ts(m["ts"]))
                    eff = amp + e if m["obs"] == "N" else amp - e
                    concern = []
                    if m["ruling"] == "ADVANCED" and eff < ENTRY_MARGIN:
                        concern.append("WOULD_NOT_OPEN")
                    if m["obs"] == "S" and e >= SOUTH_FLOOR_RISK:
                        concern.append("SOUTH_TOWARDS_82MS_FLOOR")
                    if m["obs"] == "N" and e >= NORTH_STRETCH_RISK:
                        concern.append("NORTH_STRETCHING")
                    if abs(e) >= LATCH_BAND:
                        concern.append("MEASURED_LATCH_BAND")
                    if abs(e) >= EXIT_MARGIN:
                        concern.append("NOMINAL_CLOSE_MARGIN_NEGATIVE")
                    if abs(e) >= ENTRY_MARGIN:
                        concern.append("PERMANENTLY_OPEN")
                    g = fnum(m["gain"])
                    if m["ruling"] == "ADVANCED" and g and eff / g < AMP_FLOOR:
                        concern.append("BELOW_AMP_FLOOR")
                    if eff - ENTRY_MARGIN < 20 and m["ruling"] == "ADVANCED":
                        concern.append("THIN_OPENING_MARGIN")
                    if not concern:
                        continue
                    detail_events.append({
                        "candidate": cand.id, "session_id": sid, "ts": m["ts"],
                        "mm": m["mm"], "obs": m["obs"], "ruling": m["ruling"],
                        "published_peak": m["peak"], "amplitude": round(amp, 1),
                        "resting_level": rest,
                        "operative_baseline": baseline_at(timeline, ts(m["ts"])),
                        "error": round(e, 2), "effective_peak": round(eff, 1),
                        "opening_margin": round(eff - ENTRY_MARGIN, 1),
                        "closing_margin": round(EXIT_MARGIN - abs(e), 2),
                        "gain": m["gain"], "dur_ms": m["dur_ms"],
                        "concerns": "|".join(concern),
                    })

    def write(name, rows):
        p = os.path.join(a.out, a.prefix + name)
        if not rows:
            open(p, "w").close()
            return p
        keys = []
        for r in rows:
            for k in r:
                if k not in keys:
                    keys.append(k)
        with open(p, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=keys)
            w.writeheader()
            for r in rows:
                w.writerow(r)
        return p

    print("candidates: %d, sessions replayed: %d, sessions without usable "
          "observations: %d" % (len(cands), len(set(r["session_id"] for r in summary)), len(nodata)))
    if nodata:
        print("no-data:  %s (%d rows)" % (write("replay_sessions_no_data.csv",
                                                sorted(nodata.values(), key=lambda r: r["boot_ts"])), len(nodata)))
    print("summary:  %s (%d rows)" % (write("replay_candidates.csv", summary), len(summary)))
    if a.detail:
        print("lap rows: %s (%d rows)" % (write("replay_laps.csv", detail_laps), len(detail_laps)))
        print("events:   %s (%d rows)" % (write("replay_safety_events.csv", detail_events), len(detail_events)))


if __name__ == "__main__":
    main()
