#!/usr/bin/env python3
"""Session- and lap-level Hall baseline extraction from NGR run logs.

WHY THIS EXISTS
---------------
A proposed NAVI_ONE baseline controller wants to move the Hall reference by a
bounded amount once per completed lap, using route-wide evidence rather than a
one-second rolling median. Deciding how much authority to give it needs the
thing it would actually see: the resting Hall level summarised over whole laps,
anchored the way the firmware anchors position.

WHAT IT IS NOT
--------------
This reads logs. It changes no firmware, decides no policy, and treats the
rolling/shadow median strictly as measurement data — in X17 that value has no
navigation authority at all, and nothing here grants it any.

LAP ANCHORING
-------------
The lap origin is the location supplied by SET LOCATION, and it holds for the
rest of the boot session. A lap is 171 firmware-accepted advances that return
the locomotive to that fixed origin marker.

  SET LOCATION      establishes a new origin and resets everything.
  direction change  does NOT move the origin. The lap in progress is discarded;
                    counting resumes toward the same origin in the new
                    direction, beginning at the next arrival at the origin.
  reboot            resets everything (a session is one boot).
  rejections, refusals, withdrawals  republish `adv` unchanged and do not
                    advance the lap.

The firmware's own `adv` counter also resets at a direction change, which
re-anchors it onto wherever the locomotive happened to be at the reversal. That
is NOT the session origin. `adv` is used here only to detect that an advance was
accepted -- never as the lap anchor. build_advepoch() keeps the old anchoring
for comparison and nothing else.

Usage:
  extract_baseline_laps.py --logs DIR --out DIR [--loco 9950011] [--date 20260914]
"""

import argparse
import csv
import glob
import json
import math
import os
import re
from datetime import datetime, timedelta

ROUTE_N = 171                 # RouteMap.h: the route is 171 markers round.

# Friendly names. The firmware carries ids, not names; these are the operator's.
LOCO_NAMES = {"9950011": "Otto", "9950012": "Toby"}

# Defaults matched to X17. Each session overrides them from its own state/bootid
# when that message was captured, so a differently-built session is not scored
# against constants it never ran.
DEFAULT_ADAPT_PWM = 24        # NAVI_BASELINE_ADAPT_PWM
DEFAULT_MEDIAN_N = 41         # shadow median length, at 25 ms per sample
BASELINE_SAMPLE_MS = 25       # HallCapture baselineMs

EXCURSION_COUNTS = 5          # |obs - lap centre| at or above this is "conspicuous"
MAX_EXCURSIONS_LISTED = 8
TELEMETRY_GAP_MS = 3000       # STATUS is 1 Hz; a gap this long is missing data
PRIME_SETTLE_MS = 3000        # primeMs is 2000; read the fixed baseline after it
STALL_MS = 3000               # no accepted marker for this long: the shadow
                              # median may be sitting on a magnet, not on the
                              # resting level. See classify_observations.


# ---------------------------------------------------------------- parsing ----

TS_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+$")


def parse_ts(s):
    return datetime.strptime(s, "%Y-%m-%dT%H:%M:%S.%f")


def iter_records(path, loco):
    """Yield (ts, suffix, payload_raw) for one run log.

    Lines are `ISO8601<TAB>topic<TAB>payload`. Anything that does not parse is
    counted by the caller rather than silently dropped.
    """
    prefix = "ngr/loco/%s/" % loco
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 3 or not TS_RE.match(parts[0]):
                yield None
                continue
            if not parts[1].startswith(prefix):
                continue
            yield (parse_ts(parts[0]), parts[1][len(prefix):], parts[2])


def jload(raw):
    try:
        v = json.loads(raw)
        return v if isinstance(v, dict) else None
    except Exception:
        return None


# ------------------------------------------------------------- statistics ----

def median(xs):
    """Linear-interpolated median (the numpy convention), so an even-length lap
    reports the midpoint rather than one of the two middle samples."""
    if not xs:
        return None
    s = sorted(xs)
    n = len(s)
    m = n // 2
    return float(s[m]) if n % 2 else (s[m - 1] + s[m]) / 2.0


def mean(xs):
    return sum(xs) / float(len(xs)) if xs else None


def sd(xs):
    if len(xs) < 2:
        return None
    m = mean(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def per_mm(sel):
    """Collapse a lap's observations to one value per route position.

    STATUS arrives at 1 Hz in time, not in distance, so a slow stretch of track
    contributes more samples than a fast one and a straight time-average is
    weighted by dwell rather than by geography. Taking the median within each MM
    first, then summarising across positions, gives every marker one vote --
    which is also the evidence a whole-lap controller would actually be using.
    """
    by = {}
    for o in sel:
        if o["mm"] is not None:
            by.setdefault(o["mm"], []).append(o["shadow_baseline"])
    return [median(v) for v in by.values()], len(by)


def r2(v):
    return "" if v is None else round(v, 2)


# ------------------------------------------------------------ the machine ----

class Session(object):
    def __init__(self, sid, boot_ts, boot_source):
        self.sid = sid
        self.boot_ts = boot_ts
        self.boot_source = boot_source
        self.files = []
        self.bootid = None
        self.bootid_ts = None
        self.status = []          # kept observations, in arrival order
        self.seen_uptime = set()
        self.max_uptime = -1
        self.dup_uptime = 0
        self.out_of_order = 0
        self.markers = []         # (ts, dict)
        self.navs = []            # (ts, dict) non-AGREE nav events
        self.warnings = []        # (ts, text)
        self.acq = []             # (ts, dict) diag/acquisition
        self.end_ts = boot_ts

    def note_file(self, f):
        if f not in self.files:
            self.files.append(f)


def sessionise(paths, loco):
    """Split every record into sessions. A session is one boot.

    Boot is detected two ways, in this order of trust:
      1. a state/bootid message — the firmware announcing itself;
      2. alert.uptime_ms restarting near zero below the running maximum.
    The second catches boots whose bootid was published while the recorder was
    between files. Small backwards steps in uptime_ms are out-of-order MQTT
    delivery, not reboots, and are flagged rather than treated as a boot.
    """
    sessions = []
    cur = None
    malformed = 0
    pending_bootid = None     # (ts, dict) seen before this session's first STATUS

    for path in paths:
        base = os.path.basename(path)
        for rec in iter_records(path, loco):
            if rec is None:
                malformed += 1
                continue
            ts, suffix, raw = rec

            if suffix == "state/bootid":
                d = jload(raw) or {}
                cur = Session(None, ts, "bootid")
                cur.bootid = d
                cur.bootid_ts = ts
                cur.note_file(base)
                sessions.append(cur)
                pending_bootid = None
                continue

            if suffix == "alert":
                d = jload(raw)
                if not d or d.get("reason") != "STATUS":
                    continue
                u = d.get("uptime_ms")
                if u is None:
                    continue
                # A boot the bootid message did not announce to us.
                if cur is None or (u < cur.max_uptime and u < 30000):
                    cur = Session(None, ts - timedelta(milliseconds=u), "uptime_reset")
                    sessions.append(cur)
                cur.note_file(base)
                if cur.boot_ts is None:
                    cur.boot_ts = ts - timedelta(milliseconds=u)
                if u in cur.seen_uptime:
                    cur.dup_uptime += 1
                    continue                      # retained/duplicated publish
                cur.seen_uptime.add(u)
                ooo = u < cur.max_uptime
                if ooo:
                    cur.out_of_order += 1
                cur.max_uptime = max(cur.max_uptime, u)
                cur.status.append((ts, u, d, ooo, base))
                cur.end_ts = max(cur.end_ts, ts)
                continue

            if cur is None:
                continue
            # Only a record we actually attribute may extend the session. A
            # topic we ignore -- an LWT `online`, say -- can sit in the file
            # ahead of the next boot's bootid, and must not stretch the
            # previous session across the reboot.
            if suffix in ("mm/marker", "state/nav", "state/warning", "diag/acquisition"):
                cur.note_file(base)
                cur.end_ts = max(cur.end_ts, ts)

            if suffix == "mm/marker":
                d = jload(raw)
                if d:
                    cur.markers.append((ts, d, base))
            elif suffix == "state/nav":
                d = jload(raw)
                if d and d.get("event") != "AGREE":
                    cur.navs.append((ts, d))
            elif suffix == "state/warning":
                if raw.strip():
                    cur.warnings.append((ts, raw.strip()))
            elif suffix == "diag/acquisition":
                d = jload(raw)
                if d:
                    cur.acq.append((ts, d))

    for s in sessions:
        if s.boot_ts is None:
            s.boot_ts = s.bootid_ts
        s.sid = "%s-B%s" % (loco, s.boot_ts.strftime("%Y%m%dT%H%M%S"))
    return sessions, malformed


def epochs_of(session):
    """Cut a session's markers into anchor epochs.

    An anchor is what establishes the lap origin: the SET LOCATION (state/nav
    event DECLARED) or a session-direction change (event DIRECTION). Both reset
    the firmware's accepted-advance counter to zero, and the task forbids
    combining observations across either. Markers belong to the latest anchor at
    or before their timestamp; markers before any anchor sit in a synthetic BOOT
    epoch, which can hold rejections but never a lap.

    A reset of `adv` inside an epoch with no anchor to explain it is recorded as
    adv_reset_unexplained rather than quietly starting a new epoch.
    """
    anchors = [(ts, d.get("event"), d.get("mm"), d.get("dir"))
               for ts, d in session.navs
               if d.get("event") in ("DECLARED", "DIRECTION")]
    anchors.sort(key=lambda a: a[0])
    eps = [{"anchor_ts": None, "anchor_event": "BOOT", "anchor_mm": None,
            "anchor_dir": None, "markers": [], "adv_reset_unexplained": 0}]
    for ts, ev, mm, dr in anchors:
        eps.append({"anchor_ts": ts, "anchor_event": ev, "anchor_mm": mm,
                    "anchor_dir": dr, "markers": [], "adv_reset_unexplained": 0})

    def pick(ts):
        chosen = eps[0]
        for e in eps[1:]:
            if e["anchor_ts"] <= ts:
                chosen = e
            else:
                break
        return chosen

    for ts, d, base in session.markers:
        if d.get("adv") is None:
            continue
        pick(ts)["markers"].append((ts, d))

    for e in eps:
        prev = None
        for ts, d in e["markers"]:
            a = d["adv"]
            if prev is not None and a < prev:
                e["adv_reset_unexplained"] += 1
            prev = a
    return [e for e in eps if e["markers"]]


def window_events(session, t0, t1):
    """Everything notable that happened inside one lap's time window."""
    out = {
        "stops": [], "declarations": [], "directions": [],
        "withdrawals": [], "floor_rejections": 0, "other_nav": [],
    }
    for ts, d in session.navs:
        if not (t0 <= ts <= t1):
            continue
        ev = d.get("event")
        if ev == "DECLARED":
            out["declarations"].append(ts)
        elif ev == "DIRECTION":
            out["directions"].append(ts)
        elif ev in ("NO_POSITION", "DISAGREE"):
            out["withdrawals"].append(ts)
        else:
            out["other_nav"].append((ts, ev))
    for ts, txt in session.warnings:
        if t0 <= ts <= t1:
            out["stops"].append((ts, txt))
    for ts, d in session.acq:
        if t0 <= ts <= t1 and d.get("reason") == "DURATION_FLOOR":
            out["floor_rejections"] += 1
    return out


def classify_observations(session, adapt_pwm, median_n):
    """Decide which STATUS samples are fresh measurements of the resting level.

    The shadow median only advances while the capture task's mayAdapt gate is
    open — PWM above the tractive floor — so a value published below it is the
    last value from before, republished. It is a frozen reading at rest, not a
    fresh observation, and including it would count one measurement many times
    and drag every stationary period into the lap statistics.

    After PWM rises back above the floor the median window still holds stale
    samples until it has refilled: median_n samples at BASELINE_SAMPLE_MS.
    Samples inside that refill are excluded too.
    """
    warm_ms = median_n * BASELINE_SAMPLE_MS
    qualifying_since = None
    mm_since = None
    last_mm = None
    rows = []
    for ts, u, d, ooo, base in session.status:
        pwm = d.get("pwm")
        moving = d.get("moving")
        shadow = d.get("shadow_baseline")
        fixed = d.get("baseline")
        reason = ""
        qualifies = (pwm is not None and pwm > adapt_pwm and moving == 1)
        if qualifies:
            if qualifying_since is None:
                qualifying_since = u
        else:
            qualifying_since = None

        # Position-advance gate. The 41-sample median spans ~1 s, which a
        # 130 ms passage cannot move -- but a locomotive that has stopped
        # crawling and is sitting ON a magnet holds it under the sensor for
        # seconds, and then the median tracks the magnet instead of the resting
        # level. PWM does not see this: MM094 at 10:36:08 reads +185 counts at
        # PWM 44. Not passing a marker for STALL_MS is the symptom that does.
        mm_now = d.get("mm")
        if mm_now != last_mm:
            last_mm, mm_since = mm_now, u
        stalled = (mm_since is not None and (u - mm_since) >= STALL_MS)

        if ooo:
            reason = "out_of_order"
        elif shadow is None:
            reason = "no_shadow_field"
        elif not shadow:
            reason = "shadow_zero"
        elif moving != 1:
            reason = "not_moving"
        elif pwm is None or pwm <= adapt_pwm:
            reason = "pwm_at_or_below_adapt_floor"
        elif (u - qualifying_since) < warm_ms:
            reason = "shadow_window_refilling"
        elif d.get("nav_state") in ("UNSET", "STRUCK"):
            # No route position: the sample cannot be assigned to an MM, so it
            # cannot feed a per-MM lap estimate, and `mm` is stale rather than
            # wrong-but-moving. Excluded, and counted separately from a stall.
            reason = "position_unknown"
        elif stalled:
            reason = "position_not_advancing"
        rows.append({
            "ts": ts, "uptime_ms": u, "file": base,
            "mm": d.get("mm"), "dir": d.get("dir"), "pwm": pwm,
            "moving": moving, "powered": d.get("powered"),
            "est_mm_s": d.get("est_mm_s"),
            "fixed_baseline": fixed, "shadow_baseline": shadow,
            "shadow_delta": d.get("shadow_delta"),
            "nav_state": d.get("nav_state"), "trust": d.get("trust"),
            "floor_rej": d.get("floor_rej"),
            "ms_since_marker": (u - mm_since) if mm_since is not None else "",
            "usable": 1 if reason == "" else 0,
            "exclude_reason": reason,
        })
    return rows


def build_advepoch(session, obs_rows):
    """COMPARISON ONLY: laps cut on the firmware's own `adv` epochs.

    `adv` resets at every direction change, so this silently re-anchors the lap
    origin onto wherever the locomotive happened to be when the operator
    reversed. That is not the session origin and must not be used as one. Kept
    so the two segmentations can be compared, and for no other purpose.
    """
    laps = []
    eps = epochs_of(session)
    for ei, ep in enumerate(eps, start=1):
        ms = ep["markers"]
        if not ms:
            continue
        adv_last = ms[-1][1].get("adv", 0)
        n_laps = (adv_last + ROUTE_N - 1) // ROUTE_N
        by_adv = {}
        for ts, d in ms:
            by_adv.setdefault(d.get("adv"), (ts, d))
        prev_centre = None
        prev_end = ep["anchor_ts"] or ms[0][0]
        for k in range(1, n_laps + 1):
            lo, hi = (k - 1) * ROUTE_N + 1, k * ROUTE_N
            in_lap = [(ts, d) for ts, d in ms if lo <= d.get("adv", -1) <= hi]
            if not in_lap:
                continue
            complete = hi in by_adv
            t0 = prev_end
            t1 = by_adv[hi][0] if complete else in_lap[-1][0]
            advances = len(set(d.get("adv") for _, d in in_lap))
            end_mm = (by_adv[hi][1].get("mm") if complete else in_lap[-1][1].get("mm"))
            origin_mm = ep["anchor_mm"]
            returned = (complete and origin_mm is not None
                        and end_mm == origin_mm % ROUTE_N)

            sel = [o for o in obs_rows if o["usable"] and t0 < o["ts"] <= t1]
            vals = [o["shadow_baseline"] for o in sel]
            centre = median(vals)
            mu, s, mn, mx = mean(vals), sd(vals), (min(vals) if vals else None), (max(vals) if vals else None)

            pm, n_mm = per_mm(sel)
            pm_centre, pm_mean = median(pm), mean(pm)

            exc, big_pos, big_neg = [], None, None
            if centre is not None:
                for o in sel:
                    dv = o["shadow_baseline"] - centre
                    if abs(dv) >= EXCURSION_COUNTS:
                        exc.append((dv, o))
                    if big_pos is None or dv > big_pos[0]:
                        big_pos = (dv, o)
                    if big_neg is None or dv < big_neg[0]:
                        big_neg = (dv, o)
            exc.sort(key=lambda e: -abs(e[0]))
            exc_txt = ";".join(
                "MM%03d@%s%+d" % (e[1]["mm"], e[1]["ts"].strftime("%H:%M:%S"), round(e[0]))
                for e in exc[:MAX_EXCURSIONS_LISTED])

            # missing telemetry: a hole in the 1 Hz STATUS stream inside the lap
            win = [o for o in obs_rows if t0 < o["ts"] <= t1]
            gap = 0
            for a, b in zip(win, win[1:]):
                gap = max(gap, (b["ts"] - a["ts"]).total_seconds() * 1000.0)

            ev = window_events(session, t0, t1)
            fixed_seen = sorted(set(o["fixed_baseline"] for o in win
                                    if o["fixed_baseline"] is not None))
            startup = session_startup_baseline(session)

            laps.append({
                "session_id": session.sid,
                "epoch": ei,
                "lap": k,
                "lap_start_ts": t0.isoformat(timespec="milliseconds"),
                "lap_end_ts": t1.isoformat(timespec="milliseconds"),
                "duration_s": round((t1 - t0).total_seconds(), 3),
                "origin_mm": origin_mm,
                "direction": ep["anchor_dir"],
                "anchor_event": ep["anchor_event"],
                "anchor_ts": ep["anchor_ts"].isoformat(timespec="milliseconds") if ep["anchor_ts"] else "",
                "adv_reset_unexplained": ep["adv_reset_unexplained"],
                "accepted_advances": advances,
                "adv_first": in_lap[0][1].get("adv"),
                "adv_last": in_lap[-1][1].get("adv"),
                "complete": 1 if complete else 0,
                "end_mm": end_mm,
                "returned_to_origin": 1 if returned else (0 if complete else ""),
                "n_obs_usable": len(sel),
                "n_obs_in_window": len(win),
                "center_permm_median": r2(pm_centre),
                "center_permm_mean": r2(pm_mean),
                "n_mm_covered": n_mm,
                "center_median": r2(centre),
                "mean": r2(mu),
                "sd": r2(s),
                "min": mn if mn is not None else "",
                "max": mx if mx is not None else "",
                "span": (mx - mn) if (mn is not None) else "",
                "startup_fixed_baseline": startup if startup is not None else "",
                "center_minus_startup": r2(pm_centre - startup) if (pm_centre is not None and startup is not None) else "",
                "center_minus_prev_lap": r2(pm_centre - prev_centre) if (pm_centre is not None and prev_centre is not None) else "",
                "median_minus_startup": r2(centre - startup) if (centre is not None and startup is not None) else "",
                "fixed_baseline_in_lap": "|".join(str(v) for v in fixed_seen),
                "max_excursion_pos": r2(big_pos[0]) if big_pos else "",
                "max_excursion_pos_mm": big_pos[1]["mm"] if big_pos else "",
                "max_excursion_pos_ts": big_pos[1]["ts"].strftime("%H:%M:%S") if big_pos else "",
                "max_excursion_neg": r2(big_neg[0]) if big_neg else "",
                "max_excursion_neg_mm": big_neg[1]["mm"] if big_neg else "",
                "max_excursion_neg_ts": big_neg[1]["ts"].strftime("%H:%M:%S") if big_neg else "",
                "excursions": exc_txt,
                "has_stop": 1 if ev["stops"] else 0,
                "has_declaration": 1 if ev["declarations"] else 0,
                "has_direction_change": 1 if ev["directions"] else 0,
                "has_withdrawal": 1 if ev["withdrawals"] else 0,
                "floor_rejections": ev["floor_rejections"],
                "max_status_gap_ms": int(gap),
                "missing_telemetry": 1 if gap > TELEMETRY_GAP_MS else 0,
                "notes": "; ".join(t for _, t in ev["stops"][:3]),
            })
            if pm_centre is not None:
                prev_centre = pm_centre
            prev_end = t1
    return laps


# ------------------------------------------------- SET LOCATION lap rule ----

ADV_RULINGS = ("ADVANCED",)


def origin_runs(session):
    """Walk one session under the SET LOCATION lap rule.

    Returns (origin_runs, laps). A lap carries its markers; the caller adds the
    baseline statistics. Nothing here consults the firmware's `adv` epoch for
    anchoring -- only for deciding that an advance was accepted, which is what
    ruling == ADVANCED already says.
    """
    timeline = []
    for ts, d in session.navs:
        ev = d.get("event")
        if ev in ("DECLARED", "DIRECTION"):
            timeline.append((ts, ev, d))
    for ts, d, base in session.markers:
        timeline.append((ts, "MARKER", d))
    timeline.sort(key=lambda r: (r[0], 0 if r[1] != "MARKER" else 1))

    runs, laps = [], []
    run = None
    counting = awaiting = False
    count = 0
    lap_seq = 0
    lap_start_ts = None
    lap_markers = []
    approach = []

    def close_lap(reason, end_ts, end_mm):
        """Emit whatever lap is open. complete is decided by the caller of this
        helper -- only the 171st accepted advance closes one as complete."""
        nonlocal count, lap_markers, lap_start_ts
        if run is None or lap_start_ts is None:
            return
        if count == 0 and reason != "COMPLETE":
            lap_markers = []
            return
        laps.append({
            "run": run, "lap_seq": lap_seq,
            "start_ts": lap_start_ts, "end_ts": end_ts,
            "advances": count, "markers": list(lap_markers),
            "complete": reason == "COMPLETE",
            "end_reason": reason, "end_mm": end_mm,
        })
        if reason != "COMPLETE":
            run["discarded_advances"] += count
            run["laps_discarded"] += 1
        else:
            run["laps_complete"] += 1
        count = 0
        lap_markers = []

    def start_run(ts, ev, d):
        nonlocal run, counting, awaiting, count, lap_seq, lap_start_ts, lap_markers, approach
        run = {
            "index": len(runs) + 1, "origin_mm": d.get("mm"),
            "origin_event": ev, "origin_ts": ts,
            "origin_dir": d.get("dir"), "direction_changes": 0,
            "laps_complete": 0, "laps_discarded": 0,
            "discarded_advances": 0, "approach_advances": 0,
            "end_ts": ts, "end_reason": "",
        }
        runs.append(run)
        counting, awaiting = True, False
        count = 0
        lap_seq += 1
        lap_start_ts = ts
        lap_markers = []
        approach = []

    last_ts = session.boot_ts
    last_mm = None
    for ts, kind, d in timeline:
        last_ts = ts
        if kind == "DECLARED":
            close_lap("NEW_SET_LOCATION", ts, last_mm)
            if run is not None:
                run["end_ts"] = ts
                run["end_reason"] = "NEW_SET_LOCATION"
            start_run(ts, "DECLARED", d)
            continue

        if kind == "DIRECTION":
            if run is None:
                continue                      # no origin yet: nothing to anchor
            run["direction_changes"] += 1
            close_lap("DIRECTION_CHANGE", ts, last_mm)
            run["approach_advances"] += len(approach)
            approach = []
            if d.get("mm") == run["origin_mm"]:
                counting, awaiting = True, False
                lap_seq += 1
                lap_start_ts = ts
            else:
                counting, awaiting = False, True
                lap_start_ts = None
            continue

        # MARKER
        mm = d.get("mm")
        if d.get("ruling") not in ADV_RULINGS:
            continue                          # rejected / refused: no advance
        last_mm = mm
        if awaiting:
            approach.append((ts, d))
            if mm == run["origin_mm"]:
                run["approach_advances"] += len(approach)
                approach = []
                counting, awaiting = True, False
                count = 0
                lap_seq += 1
                lap_start_ts = ts
                lap_markers = []
            continue
        if counting:
            count += 1
            lap_markers.append((ts, d))
            if count == ROUTE_N:
                close_lap("COMPLETE", ts, mm)
                lap_seq += 1
                lap_start_ts = ts

    if run is not None:
        close_lap("END_OF_SESSION", last_ts, last_mm)
        run["approach_advances"] += len(approach)
        run["end_ts"] = last_ts
        if not run["end_reason"]:
            run["end_reason"] = "END_OF_SESSION"
    return runs, laps


def build_setloc_laps(session, obs_rows):
    """Lap rows under the SET LOCATION rule, with baseline statistics."""
    runs, raw = origin_runs(session)
    startup = session_startup_baseline(session)
    out = []
    prev_centre = {}
    for lp in raw:
        run = lp["run"]
        t0, t1 = lp["start_ts"], lp["end_ts"]
        sel = [o for o in obs_rows if o["usable"] and t0 < o["ts"] <= t1]
        vals = [o["shadow_baseline"] for o in sel]
        centre = median(vals)
        pm, n_mm = per_mm(sel)
        pm_centre, pm_mean = median(pm), mean(pm)
        mu, s = mean(vals), sd(vals)
        mn = min(vals) if vals else None
        mx = max(vals) if vals else None

        exc, big_pos, big_neg = [], None, None
        if pm_centre is not None:
            for o in sel:
                dv = o["shadow_baseline"] - pm_centre
                if abs(dv) >= EXCURSION_COUNTS:
                    exc.append((dv, o))
                if big_pos is None or dv > big_pos[0]:
                    big_pos = (dv, o)
                if big_neg is None or dv < big_neg[0]:
                    big_neg = (dv, o)
        exc.sort(key=lambda e: -abs(e[0]))
        exc_txt = ";".join("MM%03d@%s%+d" % (e[1]["mm"], e[1]["ts"].strftime("%H:%M:%S"), round(e[0]))
                           for e in exc[:MAX_EXCURSIONS_LISTED])

        win = [o for o in obs_rows if t0 < o["ts"] <= t1]
        gap = 0
        for a, b in zip(win, win[1:]):
            gap = max(gap, (b["ts"] - a["ts"]).total_seconds() * 1000.0)
        ev = window_events(session, t0, t1)
        dirs = sorted(set(d.get("dir") for _, d in lp["markers"] if d.get("dir")))

        key = run["index"]
        prev = prev_centre.get(key)
        out.append({
            "session_id": session.sid,
            "origin_index": run["index"],
            "origin_mm": run["origin_mm"],
            "origin_event": run["origin_event"],
            "origin_ts": run["origin_ts"].isoformat(timespec="milliseconds"),
            "lap_seq": lp["lap_seq"],
            "lap_start_ts": t0.isoformat(timespec="milliseconds"),
            "lap_end_ts": t1.isoformat(timespec="milliseconds"),
            "duration_s": round((t1 - t0).total_seconds(), 3),
            "direction": "|".join(dirs),
            "accepted_advances": lp["advances"],
            "complete": 1 if lp["complete"] else 0,
            "end_reason": lp["end_reason"],
            "end_mm": lp["end_mm"] if lp["end_mm"] is not None else "",
            "returned_to_origin": (1 if (lp["complete"] and lp["end_mm"] == run["origin_mm"]) else
                                   (0 if lp["complete"] else "")),
            "n_obs_usable": len(sel),
            "n_obs_in_window": len(win),
            "n_mm_covered": n_mm,
            "center_permm_median": r2(pm_centre),
            "center_permm_mean": r2(pm_mean),
            "center_obs_median": r2(centre),
            "mean": r2(mu), "sd": r2(s),
            "min": mn if mn is not None else "",
            "max": mx if mx is not None else "",
            "span": (mx - mn) if mn is not None else "",
            "startup_fixed_baseline": startup if startup is not None else "",
            "center_minus_startup": r2(pm_centre - startup) if (pm_centre is not None and startup is not None) else "",
            "center_minus_prev_lap": r2(pm_centre - prev) if (pm_centre is not None and prev is not None) else "",
            "max_excursion_pos": r2(big_pos[0]) if big_pos else "",
            "max_excursion_pos_mm": big_pos[1]["mm"] if big_pos else "",
            "max_excursion_pos_ts": big_pos[1]["ts"].strftime("%H:%M:%S") if big_pos else "",
            "max_excursion_neg": r2(big_neg[0]) if big_neg else "",
            "max_excursion_neg_mm": big_neg[1]["mm"] if big_neg else "",
            "max_excursion_neg_ts": big_neg[1]["ts"].strftime("%H:%M:%S") if big_neg else "",
            "excursions": exc_txt,
            "has_stop": 1 if ev["stops"] else 0,
            "has_declaration": 1 if ev["declarations"] else 0,
            "has_direction_change": 1 if ev["directions"] else 0,
            "has_withdrawal": 1 if ev["withdrawals"] else 0,
            "floor_rejections": ev["floor_rejections"],
            "max_status_gap_ms": int(gap),
            "missing_telemetry": 1 if gap > TELEMETRY_GAP_MS else 0,
            "notes": "; ".join(t for _, t in ev["stops"][:3]),
        })
        if lp["complete"] and pm_centre is not None:
            prev_centre[key] = pm_centre

    runrows = [{
        "session_id": session.sid,
        "origin_index": r["index"],
        "origin_mm": r["origin_mm"],
        "origin_event": r["origin_event"],
        "origin_ts": r["origin_ts"].isoformat(timespec="milliseconds"),
        "origin_dir": r["origin_dir"],
        "direction_changes": r["direction_changes"],
        "laps_complete": r["laps_complete"],
        "laps_discarded": r["laps_discarded"],
        "discarded_advances": r["discarded_advances"],
        "approach_advances": r["approach_advances"],
        "end_ts": r["end_ts"].isoformat(timespec="milliseconds"),
        "end_reason": r["end_reason"],
        "startup_fixed_baseline": startup if startup is not None else "",
    } for r in runs]
    return out, runrows


def marker_rows(session, obs_rows):
    """Every published marker, with the local resting level beside it.

    The safety replay needs the passage amplitude measured against the RESTING
    LEVEL, not against whatever reference happened to be in force. `peak` is
    published relative to entryBaseline_, which under X17 is the frozen startup
    baseline, so the amplitude has to be recovered:

        N (raw rises):  peak = A + E   ->  A = peak - E
        S (raw falls):  peak = A - E   ->  A = peak + E

    with E = resting level - operative baseline at that moment.
    """
    usable = [o for o in obs_rows if o["usable"]]

    # Floor rejections never reach mm/marker -- they are published on
    # diag/acquisition. A passage the 82 ms floor threw away is exactly the kind
    # a shifted reference clips short, so it belongs in the safety population.
    events = [(ts, d, "mm/marker") for ts, d, b in session.markers]
    for ts, d in session.acq:
        events.append((ts, {
            "mm": None, "dir": "", "ruling": d.get("reason"), "event": "REJECTED",
            "obs": ("S" if (d.get("signed_sum") or 0) < 0 else "N"),
            "expected": "", "peak": d.get("raw_peak"), "gain": None, "ratio": None,
            "dur_ms": d.get("dur_ms"), "gap_ms": None, "pwm_close": d.get("pwm_close"),
            "base_open": d.get("entry_baseline"), "raw_close": d.get("raw_close"),
            "adv": None,
        }, "diag/acquisition"))
    events.sort(key=lambda r: r[0])

    rows = []
    ui = 0
    for ts, d, src in events:
        while ui + 1 < len(usable) and usable[ui + 1]["ts"] <= ts:
            ui += 1
        near = None
        if usable:
            cand = usable[ui]
            if abs((cand["ts"] - ts).total_seconds()) <= 5.0:
                near = cand
        peak = d.get("peak")
        obs = d.get("obs") or ""
        bfield = d.get("base_open")
        rest = near["shadow_baseline"] if near else None
        e = (rest - bfield) if (rest is not None and bfield) else None
        if peak is None or e is None or obs not in ("N", "S"):
            amp = None
        else:
            amp = peak - e if obs == "N" else peak + e
        rows.append({
            "ts": ts.isoformat(timespec="milliseconds"),
            "session_id": session.sid,
            "mm": d.get("mm"), "dir": d.get("dir"),
            "ruling": d.get("ruling"), "event": d.get("event"),
            "obs": obs, "expected": d.get("expected") or "",
            "peak": peak, "gain": d.get("gain"), "ratio": d.get("ratio"),
            "dur_ms": d.get("dur_ms"), "gap_ms": d.get("gap_ms"),
            "pwm_close": d.get("pwm_close"),
            "base_open": bfield, "raw_close": d.get("raw_close"),
            "resting_level": rest if rest is not None else "",
            "field_error": e if e is not None else "",
            "amplitude": amp if amp is not None else "",
            "adv": d.get("adv"),
            "source": src,
        })
    return rows


def build_mm000(session, obs_rows):
    """A DELIBERATELY DIFFERENT segmentation, kept only for reconciliation.

    Earlier hand analysis of this run cut laps at the MM170->MM000 wrap. That is
    not what the firmware anchors to, and the brief forbids using it, but the
    numbers already reported to the operator were computed that way. Emitting
    both is the only honest way to say whether a disagreement is a measurement
    difference or a definition difference.
    """
    laps = []
    for ei, ep in enumerate(epochs_of(session), start=1):
        ms = [(ts, d) for ts, d in ep["markers"] if d.get("ruling") == "ADVANCED"]
        wraps = [i for i, (ts, d) in enumerate(ms) if d.get("mm") == 0]
        if len(wraps) < 2:
            continue
        prev_centre = None
        startup = session_startup_baseline(session)
        for k, (i0, i1) in enumerate(zip(wraps, wraps[1:]), start=1):
            t0, t1 = ms[i0][0], ms[i1][0]
            advances = i1 - i0
            sel = [o for o in obs_rows if o["usable"] and t0 < o["ts"] <= t1]
            vals = [o["shadow_baseline"] for o in sel]
            centre = median(vals)
            pm, n_mm = per_mm(sel)
            pm_centre = median(pm)
            laps.append({
                "session_id": session.sid, "epoch": ei, "wrap_lap": k,
                "center_permm_median": r2(pm_centre),
                "center_permm_mean": r2(mean(pm)),
                "n_mm_covered": n_mm,
                "lap_start_ts": t0.isoformat(timespec="milliseconds"),
                "lap_end_ts": t1.isoformat(timespec="milliseconds"),
                "accepted_advances": advances,
                "complete": 1 if advances == ROUTE_N else 0,
                "n_obs_usable": len(sel),
                "center_median": r2(centre), "mean": r2(mean(vals)), "sd": r2(sd(vals)),
                "min": min(vals) if vals else "", "max": max(vals) if vals else "",
                "span": (max(vals) - min(vals)) if vals else "",
                "startup_fixed_baseline": startup if startup is not None else "",
                "center_minus_startup": r2(pm_centre - startup) if (pm_centre is not None and startup is not None) else "",
                "center_minus_prev_lap": r2(pm_centre - prev_centre) if (pm_centre is not None and prev_centre is not None) else "",
            })
            if pm_centre is not None:
                prev_centre = pm_centre
    return laps


def session_startup_baseline(session):
    """The fixed baseline the session primed to.

    primeMs is 2000, so the first second of STATUS can still show a baseline
    mid-prime. Read the first sample after PRIME_SETTLE_MS.
    """
    for ts, u, d, ooo, base in session.status:
        if u >= PRIME_SETTLE_MS and d.get("baseline"):
            return d.get("baseline")
    for ts, u, d, ooo, base in session.status:
        if d.get("baseline"):
            return d.get("baseline")
    return None


# ------------------------------------------------------------------ main ----

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--logs", default=os.path.expanduser("~/ngr-telemetry/pi/NGR/telemetry/runs"),
                    help="directory of raw run logs")
    ap.add_argument("--out", required=True, help="directory for the CSVs")
    ap.add_argument("--loco", default="9950011")
    ap.add_argument("--date", default="20260914", help="YYYYMMDD in the filename, or 'all'")
    ap.add_argument("--observations", action="store_true", default=True)
    ap.add_argument("--compare", action="store_true",
                    help="also emit laps_mm000_compare.csv and "
                         "laps_advepoch_compare.csv -- alternative lap cuts, "
                         "for reconciliation only, never as the deliverable")
    ap.add_argument("--no-observations", dest="observations", action="store_false")
    args = ap.parse_args()

    pat = "%s_%s_*.log" % (args.loco, "" if args.date == "all" else args.date)
    pat = pat.replace("__", "_*_") if args.date == "all" else pat
    paths = sorted(glob.glob(os.path.join(args.logs, pat)))
    if not paths:
        raise SystemExit("no logs matched %s in %s" % (pat, args.logs))

    sessions, malformed = sessionise(paths, args.loco)
    os.makedirs(args.out, exist_ok=True)

    all_laps, all_obs, srows, mm000, advep, origins, marks = [], [], [], [], [], [], []
    for s in sessions:
        bid = s.bootid or {}
        adapt = bid.get("baseline_adapt_pwm", DEFAULT_ADAPT_PWM)
        med_n = bid.get("shadow_median_n", DEFAULT_MEDIAN_N)
        obs = classify_observations(s, adapt, med_n)
        laps, runs = build_setloc_laps(s, obs)
        all_laps.extend(laps)
        origins.extend(runs)
        marks.extend(marker_rows(s, obs))
        if args.compare:
            mm000.extend(build_mm000(s, obs))
            advep.extend(build_advepoch(s, obs))

        decls = [(ts, d) for ts, d in s.navs if d.get("event") == "DECLARED"]
        dirs = [(ts, d) for ts, d in s.navs if d.get("event") == "DIRECTION"]
        wdraw = [(ts, d) for ts, d in s.navs if d.get("event") in ("NO_POSITION", "DISAGREE")]
        # Constancy is judged after the prime window: during primeMs the
        # reference is still the running median and has not been frozen yet.
        bases = sorted(set(o["fixed_baseline"] for o in obs
                           if o["fixed_baseline"] and o["uptime_ms"] >= PRIME_SETTLE_MS))
        advs = [d.get("adv") for _, d, _ in s.markers if d.get("adv")]

        srows.append({
            "session_id": s.sid,
            "loco_id": args.loco,
            "loco_name": LOCO_NAMES.get(args.loco, ""),
            "sketch": bid.get("sketch", ""),
            "subtitle": bid.get("subtitle", ""),
            "build_class": bid.get("build_class", ""),
            "field_accepted": bid.get("field_accepted", ""),
            "baseline_mode": bid.get("baseline_mode", ""),
            "shadow_median_n": med_n,
            "baseline_adapt_pwm": adapt,
            "entry_margin": bid.get("entry", ""),
            "exit_margin": bid.get("exit", ""),
            "floor_ms": bid.get("floor_ms", ""),
            "guard_ms": bid.get("guard_ms", ""),
            "seq_n": bid.get("seq_n", ""),
            "profile_loco": bid.get("loco", ""),
            "identity_matches_profile": ("" if not bid else
                                         (1 if str(bid.get("loco")) == args.loco else 0)),
            "boot_ts": s.boot_ts.isoformat(timespec="milliseconds"),
            "boot_source": s.boot_source,
            "bootid_ts": s.bootid_ts.isoformat(timespec="milliseconds") if s.bootid_ts else "",
            "session_end_ts": s.end_ts.isoformat(timespec="milliseconds"),
            "duration_s": round((s.end_ts - s.boot_ts).total_seconds(), 1),
            "source_files": "|".join(s.files),
            "startup_fixed_baseline": session_startup_baseline(s) or "",
            "fixed_baseline_values": "|".join(str(b) for b in bases),
            "fixed_baseline_constant": 1 if len(bases) <= 1 else 0,
            "set_location_ts": decls[0][0].isoformat(timespec="milliseconds") if decls else "",
            "set_location_mm": decls[0][1].get("mm") if decls else "",
            "set_location_dir": decls[0][1].get("dir") if decls else "",
            "n_declarations": len(decls),
            "later_declarations": "|".join(t.strftime("%H:%M:%S") for t, _ in decls[1:]),
            "n_direction_changes": len(dirs),
            "direction_changes": "|".join("%s->%s" % (t.strftime("%H:%M:%S"), d.get("dir")) for t, d in dirs),
            "n_withdrawals": len(wdraw),
            "first_withdrawal_ts": wdraw[0][0].isoformat(timespec="milliseconds") if wdraw else "",
            "n_stops_warnings": len(s.warnings),
            "n_markers": len(s.markers),
            "max_adv": max(advs) if advs else 0,
            "n_origins": len(runs),
            "origins": "|".join("MM%03d@%s" % (r["origin_mm"], r["origin_ts"][11:19]) for r in runs),
            "n_laps_complete": sum(1 for l in laps if l["complete"]),
            "n_laps_incomplete": sum(1 for l in laps if not l["complete"]),
            "discarded_advances": sum(r["discarded_advances"] for r in runs),
            "approach_advances": sum(r["approach_advances"] for r in runs),
            "n_status": len(s.status),
            "n_status_usable": sum(1 for o in obs if o["usable"]),
            "dup_uptime_dropped": s.dup_uptime,
            "out_of_order": s.out_of_order,
            "floor_rejections": sum(1 for _, d in s.acq if d.get("reason") == "DURATION_FLOOR"),
        })

        if args.observations:
            lapidx = [(l["origin_index"], l["lap_seq"], l["complete"],
                       parse_ts(l["lap_start_ts"]), parse_ts(l["lap_end_ts"]))
                      for l in laps]
            for o in obs:
                ep = lp = cp = ""
                for e, k, c, t0, t1 in lapidx:
                    if t0 < o["ts"] <= t1:
                        ep, lp, cp = e, k, c
                        break
                all_obs.append({
                    "ts": o["ts"].isoformat(timespec="milliseconds"),
                    "session_id": s.sid, "origin_index": ep, "lap_seq": lp,
                    "in_complete_lap": cp,
                    "uptime_ms": o["uptime_ms"], "mm": o["mm"], "dir": o["dir"],
                    "pwm": o["pwm"], "moving": o["moving"], "est_mm_s": o["est_mm_s"],
                    "fixed_baseline": o["fixed_baseline"],
                    "shadow_baseline": o["shadow_baseline"],
                    "shadow_delta": o["shadow_delta"],
                    "nav_state": o["nav_state"], "trust": o["trust"],
                    "ms_since_marker": o["ms_since_marker"],
                    "usable": o["usable"], "exclude_reason": o["exclude_reason"],
                    "source_file": o["file"],
                })

    def write(name, rows):
        p = os.path.join(args.out, name)
        if not rows:
            open(p, "w").close()
            return p
        with open(p, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        return p

    print("sessions: %s (%d rows)" % (write("sessions.csv", srows), len(srows)))
    print("origins:  %s (%d rows)" % (write("origins.csv", origins), len(origins)))
    print("laps:     %s (%d rows)" % (write("laps.csv", all_laps), len(all_laps)))
    print("markers:  %s (%d rows)" % (write("markers.csv", marks), len(marks)))
    if args.observations:
        print("obs:      %s (%d rows)" % (write("observations.csv", all_obs), len(all_obs)))
    if args.compare:
        print("mm000:    %s (%d rows)" % (write("laps_mm000_compare.csv", mm000), len(mm000)))
        print("advepoch: %s (%d rows)" % (write("laps_advepoch_compare.csv", advep), len(advep)))
    if malformed:
        print("malformed lines skipped: %d" % malformed)


if __name__ == "__main__":
    main()
