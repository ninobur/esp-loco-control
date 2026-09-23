#!/usr/bin/env python3
"""
MM detection-to-detection timing guard analysis (Toby, NAVI_COHERENCE 0.4/0.5,
2026-09-22). Primary dataset for the guard-timing evidence task.

See header comments in the original draft (preserved in report doc) for the
full source-verified event-definition writeup. Key points:

  - opened_ms / pwm_open are locomotive-clock (millis()) and actualPwm AT THE
    HALL OPENING SAMPLE (verified: NAVI_COHERENCE_0_4.ino hallTask(), lines
    ~490-498: pending.openedAtMs=opening.detectedAtMs; pending.pwm=actualPwm;)
  - `gap_ms` in state/nav is DEAD (Judged.priorGapMs declared, never assigned,
    in both 0.4 and 0.5 source). Intervals are computed here independently
    from consecutive opened_ms.
  - The only timing guard in force is MIN_MARKER_MS=500 (Navigator.h),
    applied ONLY on the Hall-only path (IR interval not usable / no anchor
    yet), anchored at the previous ACCEPTED opening. When IR is usable there
    is NO time floor -- a distance window governs instead. Both a Hall-only
    reject and an IR-distance reject publish ruling=NON_LANDMARK_HALL
    (indistinguishable without joining nav/discrepancy's distance_assessable
    field, which this script does, by (boot segment, event_serial)).
  - `firmware/programs/NAVI_ONE/variants/NAVI_ONE_X22/ExcursionDetector.h` shows the
    shared acquisition library's DEFAULT refractoryMs is 645 (the number
    decision 0087 fixed the old detection-to-closure guard at). NAVI_COHERENCE's
    hallConfig() (HallObserver.h) explicitly overrides this to refractoryMs=0.
    windowMs stays at its default, 400 (matches state/bootid's "window_ms":400
    in every boot this session). With zero refractory, the detector can
    re-arm the instant a 400 ms identification window closes. This is
    material to the false-event population found below.
  - `20260922_navi_coherence_0_4_lap.log` is DROPPED from this analysis: its
    entire productive content (182 genuine advances, identical opened_ms/mm/
    pwm signatures) is a byte-for-byte duplicate of one boot segment of
    `runs/9950012_20260922_133918.log` (verified directly, not assumed).
    Pooling both would double-count one session. 133918.log is kept as the
    canonical copy (recorder-native, has a matching meta.json).
"""
import json, re, sys, math, os
from collections import defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT  = os.path.join(REPO, "field-records/analysis")

ROUTE_N = 171
ROUTE_SPACING_MM = [
  330,340,330,315,325,330,315,300,300,295,
  300,290,300,315,315,325,310,300,300,320,
  315,315,305,300,295,300,300,300,300,315,
  330,320,315,310,300,300,300,300,300,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,300,300,300,295,320,300,
  315,320,315,325,315,305,300,305,300,300,
  295,295,300,300,300,300,300,300,300,305,
  300,300,305,300,300,330,300,300,305,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,295,300,300,300,300,300,305,300,
  300,320,320,300,300,300,300,300,300,300,
  300,300,300,300,300,300,280,300,300,290,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,305,300,305,300,295,300,
  300,300,305,300,300,305,320,290,320,300,
  300,305,330,330,320,325,315,355,330,330,
  330,
]
assert len(ROUTE_SPACING_MM) == ROUTE_N

ROUTE_POLARITY = [
  1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,
  0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
  0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,
  0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
  1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,
  1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
  1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,
  0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
  1,0,1,0,0,0,0,1,1,1,0,
]
assert len(ROUTE_POLARITY) == ROUTE_N
def polarity_at(mm):
    return "N" if ROUTE_POLARITY[mm % ROUTE_N] else "S"

def span_mm(mm, direction):
    mm %= ROUTE_N
    return ROUTE_SPACING_MM[mm] if direction > 0 else ROUTE_SPACING_MM[(mm - 1) % ROUTE_N]

def mapped_distance(mm_from, mm_to, direction):
    if direction > 0:
        steps = (mm_to - mm_from) % ROUTE_N
    else:
        steps = (mm_from - mm_to) % ROUTE_N
    dist, cur = 0, mm_from
    for _ in range(steps):
        dist += span_mm(cur, direction)
        cur = (cur + direction) % ROUTE_N
    return dist, steps

STATIONS = {
    15:  dict(name="Patio",    pwmCW=60, pwmCCW=60, stopCW=1,  stopCCW=0),
    63:  dict(name="Grillers", pwmCW=60, pwmCCW=72, stopCW=-1, stopCCW=-1),
    108: dict(name="Arches",   pwmCW=60, pwmCCW=60, stopCW=0,  stopCCW=0),
    157: dict(name="Bamboo",   pwmCW=60, pwmCCW=60, stopCW=1,  stopCCW=-1),
}
APPROACH_START, ZONE_START, OVERSHOOT_ABANDON = -10, -5, 5
GRILLERS_GRADE_CW = set(range(65, 86))
PATIO_CURVE_CCW   = set(range(26, 34))

def offset_to_centre(mm, direction, centre):
    d = (mm - centre) % ROUTE_N if direction > 0 else (centre - mm) % ROUTE_N
    if d > ROUTE_N // 2:
        d -= ROUTE_N
    return d

def regime_tags(mm_from, mm_to, direction):
    tags = set()
    for mm in (mm_from, mm_to):
        for centre, s in STATIONS.items():
            off = offset_to_centre(mm, direction, centre)
            if APPROACH_START <= off <= OVERSHOOT_ABANDON + 3:
                tags.add("STATION:" + s["name"])
    if direction > 0 and (mm_from in GRILLERS_GRADE_CW or mm_to in GRILLERS_GRADE_CW):
        tags.add("GRILLERS_GRADE_CW")
    if direction < 0 and (mm_from in PATIO_CURVE_CCW or mm_to in PATIO_CURVE_CCW):
        tags.add("PATIO_CURVE_CCW")
    return tags if tags else {"ORDINARY"}

TOPIC_RE = re.compile(r'^ngr/loco/\d+/(.+)$')

def iter_lines(path):
    with open(path, errors="replace") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            ts, topic, payload = parts[0], parts[1], "\t".join(parts[2:])
            m = TOPIC_RE.match(topic)
            if m:
                topic = m.group(1)
            yield n, ts, topic, payload

def parse_file(path):
    nav_events, disc_by_pos, bootid_meta = [], [], []
    boot_seg = -1
    for n, ts, topic, payload in iter_lines(path):
        if topic == "state/bootid":
            boot_seg += 1
            try:
                bootid_meta.append((boot_seg, json.loads(payload)))
            except Exception:
                pass
        elif topic == "state/nav":
            try:
                d = json.loads(payload)
            except Exception:
                continue
            d["_reset"] = "event_serial" not in d
            d["_line"] = n
            d["_boot_seg"] = boot_seg
            d["_ts"] = ts
            nav_events.append(d)
        elif topic == "nav/discrepancy":
            try:
                d = json.loads(payload)
            except Exception:
                continue
            disc_by_pos.append((n, boot_seg, d))

    disc_index = {}
    for n, bseg, d in disc_by_pos:
        disc_index[(bseg, d.get("event_serial"))] = d
    for d in nav_events:
        if not d["_reset"]:
            d["_disc"] = disc_index.get((d["_boot_seg"], d.get("event_serial")))
    return dict(path=path, nav_events=nav_events, bootid_meta=bootid_meta)

GENUINE_RULINGS = {"ADVANCED", "ADVANCED_WITH_DISCREPANCY", "MISSED_AND_ADVANCED"}
REJECT_RULINGS  = {"NON_LANDMARK_HALL"}
IGNORE_RULINGS  = {"NO_POSITION", "LOCATION_UNRESOLVED"}


def real_guard(pwm):
    return 500.0

def fixed_700(pwm):
    return 700.0

def proposed_ramp(pwm):
    return min(1000.0, max(700.0, 1150.0 - 5.0 * pwm))


def simulate(nav_events, guard_fn):
    """Replay one file's ordered nav_events under `guard_fn`, applied ONLY on
    the Hall-only path (distance_assessable==0). IR-path judgments (0.. wait,
    ==1) and Uncertain/NoPosition are treated as exogenous: real outcome
    kept, anchors updated from the REAL result. Returns per-event verdicts."""
    anchor_ms, anchor_mm, anchor_dir = {}, {}, {}
    just_reset = set()
    out = []
    for d in nav_events:
        bseg = d["_boot_seg"]
        if d["_reset"]:
            just_reset.add(bseg)
            anchor_ms.pop(bseg, None)
            continue
        ruling = d.get("ruling")
        if ruling in IGNORE_RULINGS:
            continue
        pwm = d.get("pwm_open")
        opened = d.get("opened_ms")
        mm_after = d.get("mm")
        direction = 1 if d.get("dir") == "CW" else (-1 if d.get("dir") == "CCW" else 0)
        disc = d.get("_disc")
        dist_assessable = disc.get("distance_assessable") if disc else None
        prev_ms, prev_mm, prev_dir = anchor_ms.get(bseg), anchor_mm.get(bseg), anchor_dir.get(bseg)
        came_from_reset = bseg in just_reset
        has_anchor = (prev_ms is not None) and not came_from_reset

        rec = dict(line=d["_line"], boot_seg=bseg, event_serial=d.get("event_serial"),
                   real_ruling=ruling, pwm_open=pwm, opened_ms=opened,
                   dist_assessable=dist_assessable, dir=d.get("dir"))

        if dist_assessable == 0 and has_anchor:
            # Hall-only path: candidate guard applies. Real world used 500.
            elapsed = opened - prev_ms
            would_accept = elapsed >= guard_fn(pwm)
            real_accept = ruling in GENUINE_RULINGS
            rec.update(elapsed_since_last_accept=elapsed, counterfactual_accept=would_accept,
                       real_accept=real_accept, mm_before=prev_mm)
            if would_accept:
                dist_mm, steps = (mapped_distance(prev_mm, mm_after, direction)
                                   if direction in (1, -1) else (None, 1))
                rec.update(mapped_mm=dist_mm, steps=steps, mm_after=mm_after)
                anchor_ms[bseg] = opened; anchor_mm[bseg] = mm_after; anchor_dir[bseg] = direction
            # else: anchors unchanged, candidate stays pending (matches firmware: reject() untouched state)
            just_reset.discard(bseg)
        else:
            # Exogenous: IR-distance path (dist_assessable==1), or no anchor yet
            # (first-after-reset, dist_assessable==0 but has_anchor False). No
            # candidate guard governs this judgment either way -- keep the real
            # outcome. Still record interval_ms/mapped_mm when a valid anchor
            # exists, for the PHYSICAL timing-vs-PWM distribution (this is a
            # genuine consecutive-MM interval; it is simply not one a Hall-only
            # guard change could have affected).
            rec.update(exogenous=True, real_accept=ruling in GENUINE_RULINGS)
            if ruling in GENUINE_RULINGS:
                if has_anchor:
                    dist_mm, steps = (mapped_distance(prev_mm, mm_after, direction)
                                       if direction in (1, -1) else (None, 1))
                    rec.update(elapsed_since_last_accept=opened - prev_ms,
                               mapped_mm=dist_mm, steps=steps, mm_before=prev_mm, mm_after=mm_after)
                anchor_ms[bseg] = opened; anchor_mm[bseg] = mm_after; anchor_dir[bseg] = direction
                just_reset.discard(bseg)
        out.append(rec)
    return out


def pct(v, f):
    if not v:
        return float("nan")
    v = sorted(v)
    k = f * (len(v) - 1)
    lo, hi = math.floor(k), math.ceil(k)
    return v[int(k)] if lo == hi else v[lo] + (v[hi] - v[lo]) * (k - lo)


def pwm_bin(pwm):
    if pwm >= 85:
        return ">=90"
    if pwm >= 75:
        return "80"
    if pwm >= 65:
        return "70"
    if pwm >= 55:
        return "60"
    if pwm >= 45:
        return "50"
    if pwm >= 35:
        return "40"
    return "<=30"

BIN_ORDER = [">=90", "80", "70", "60", "50", "40", "<=30"]


def main():
    files = [
        "field-records/logs/20260922_navi_coherence_0_4_runs/9950012_20260922_132859.log",
        "field-records/logs/20260922_navi_coherence_0_4_runs/9950012_20260922_133918.log",
        "field-records/logs/20260922_navi_coherence_0_4_runs/9950012_20260922_134700.log",
        # "20260922_navi_coherence_0_4_lap.log" DELIBERATELY EXCLUDED: verified
        # byte-for-byte duplicate (same opened_ms/mm/pwm_open triples) of the
        # 133918 session above. See module docstring.
        "field-records/logs/20260922_navi_coherence_0_4_run2.log",
        "field-records/logs/20260922_navi_coherence_0_5_auto_run.log",
    ]

    parsed_all = [parse_file(os.path.join(REPO, f)) for f in files]

    # Cross-file duplicate-session safety check (opened_ms,mm,pwm signature overlap).
    sigsets = {}
    for p in parsed_all:
        sig = set()
        for d in p["nav_events"]:
            if not d["_reset"]:
                sig.add((d.get("opened_ms"), d.get("mm"), d.get("pwm_open")))
        sigsets[p["path"]] = sig
    paths = list(sigsets)
    for i in range(len(paths)):
        for j in range(i + 1, len(paths)):
            ov = sigsets[paths[i]] & sigsets[paths[j]]
            if ov:
                print(f"!! DUPLICATE-SESSION WARNING: {paths[i]} and {paths[j]} share "
                      f"{len(ov)} identical (opened_ms,mm,pwm) events", file=sys.stderr)

    # ---- Reality-check replay (guard=500) must match the logs' own rulings ----
    mismatches = 0
    for p in parsed_all:
        sim = simulate(p["nav_events"], real_guard)
        for r in sim:
            if r.get("exogenous"):
                continue
            if r["counterfactual_accept"] != r["real_accept"]:
                mismatches += 1
                print("MISMATCH", p["path"], r)
    print(f"Reality-check (guard_fn=500 flat) mismatches vs logged rulings: {mismatches} "
          f"(expect 0 -- confirms the replay model matches firmware behaviour)\n")

    # ---- Collect the genuine ("clean consecutive-MM") population once, from
    # the real-guard replay, for the primary physical-timing distributions ----
    genuine_all, hall_only_genuine, hall_only_false = [], [], []
    for p in parsed_all:
        sim = simulate(p["nav_events"], real_guard)
        for r in sim:
            if not r.get("real_accept"):
                if not r.get("exogenous") and r.get("dist_assessable") == 0:
                    hall_only_false.append(dict(r, src=p["path"]))
                continue
            if "elapsed_since_last_accept" not in r:
                continue  # first-after-reset: not a clean consecutive-MM interval
            rec = dict(r, src=p["path"], interval_ms=r["elapsed_since_last_accept"])
            genuine_all.append(rec)
            if r["dist_assessable"] == 0:
                hall_only_genuine.append(rec)

    print(f"Clean consecutive-MM genuine advances (ALL evidentiary paths, primary physical population): {len(genuine_all)}")
    print(f"  of which Hall-only path (distance_assessable=0, guard-relevant subset): {len(hall_only_genuine)}")
    print(f"  of which IR-distance path (distance_assessable=1, no time floor governs these): {len(genuine_all)-len(hall_only_genuine)}")
    print(f"Hall-only-path false events (NON_LANDMARK_HALL, dist_assessable=0): {len(hall_only_false)}")
    genuine = genuine_all

    def summarize(label, pop, key="interval_ms"):
        print(f"\n=== {label} (n={len(pop)}) by PWM bin ===")
        print(f"{'bin':6} {'n':4} {'min':6} {'p1':6} {'p5':6} {'p25':6} {'median':7} {'mapped_mm range'}")
        by = defaultdict(list)
        for g in pop:
            by[pwm_bin(g["pwm_open"])].append(g)
        for b in BIN_ORDER:
            grp = by.get(b, [])
            if not grp:
                print(f"{b:6} {0:4}")
                continue
            vals = [g[key] for g in grp]
            mm_r = [g["mapped_mm"] for g in grp if g.get("mapped_mm") is not None]
            mm_str = f"{min(mm_r)}-{max(mm_r)}" if mm_r else "n/a"
            print(f"{b:6} {len(grp):4} {min(vals):6.0f} {pct(vals,.01):6.0f} {pct(vals,.05):6.0f} "
                  f"{pct(vals,.25):6.0f} {pct(vals,.50):7.0f}  {mm_str}")

    # Station dwell (StationMachine STATION_DWELL_MS=5000, Stations.h) and manual
    # operator pauses (MANUAL-mode sessions have no automatic dwell logic at all --
    # the operator's own hold IS the pause) both produce intervals that include a
    # stop, not a continuous transit. These can only ever make an interval LONGER,
    # never shorter, so they can never cause guard suppression -- but they DO
    # badly distort percentile/median statistics if pooled with moving transits.
    # Threshold: interval_ms > 6000 (STATION_DWELL_MS=5000 + ramp overhead; the
    # observed distribution itself jumps from a 4271-5129ms cluster straight to a
    # 7611ms+ cluster, confirming this is a real gap, not an arbitrary cut).
    DWELL_CUTOFF_MS = 6000
    moving = [g for g in genuine if g["interval_ms"] <= DWELL_CUTOFF_MS]
    dwelling = [g for g in genuine if g["interval_ms"] > DWELL_CUTOFF_MS]
    moving_ho = [g for g in hall_only_genuine if g["interval_ms"] <= DWELL_CUTOFF_MS]
    dwelling_ho = [g for g in hall_only_genuine if g["interval_ms"] > DWELL_CUTOFF_MS]
    print(f"\nDwell/pause-spanning genuine intervals excluded from moving-transit tables: "
          f"{len(dwelling)} of {len(genuine)} total ({len(dwelling_ho)} of them Hall-only-path)")
    print("(A stop can only lengthen an interval, never shorten it, so excluding these "
          "cannot hide a suppression risk -- it only removes median/percentile distortion.)")

    summarize("MOVING-TRANSIT genuine consecutive-MM advances (dwell/pause-spanning excluded)", moving)
    summarize("MOVING-TRANSIT Hall-only-path genuine advances ONLY (guard-relevant subset)", moving_ho)

    print(f"\n=== Dwell/pause-spanning intervals, by PWM bin (excluded above; shown for completeness) ===")
    by = defaultdict(list)
    for g in dwelling:
        by[pwm_bin(g["pwm_open"])].append(g)
    for b in BIN_ORDER:
        grp = by.get(b, [])
        if grp:
            vals = [g["interval_ms"] for g in grp]
            print(f"  {b:6} n={len(grp):3d}  interval range {min(vals):.0f}-{max(vals):.0f} ms")

    print("\n=== exact-PWM spikes (n>=15) among ALL genuine, for context ===")
    exact = defaultdict(list)
    for g in genuine:
        exact[g["pwm_open"]].append(g["interval_ms"])
    for pwm in sorted(exact):
        if len(exact[pwm]) >= 15:
            vals = exact[pwm]
            print(f"  pwm={pwm:4d} n={len(vals):4d} min={min(vals):.0f} p1={pct(vals,.01):.0f} "
                  f"p5={pct(vals,.05):.0f} median={pct(vals,.5):.0f}")

    print("\n=== Hall-only false events (guard-relevant false population), full detail ===")
    for h in sorted(hall_only_false, key=lambda x: x["opened_ms"]):
        prev_pol = polarity_at(h["mm_before"]) if h.get("mm_before") is not None else "?"
        print(f"  {os.path.basename(h['src'])} line {h['line']} boot_seg {h['boot_seg']} "
              f"pwm_open {h['pwm_open']} elapsed_since_last_accept {h['elapsed_since_last_accept']:.0f}ms "
              f"dir {h['dir']} prev_magnet_polarity={prev_pol}")

    print("\n=== Bottom 20 Hall-only genuine intervals (closest calls to any candidate guard) ===")
    for g in sorted(hall_only_genuine, key=lambda x: x["interval_ms"])[:20]:
        print(f"  {os.path.basename(g['src'])} line {g['line']} ruling {g['real_ruling']} "
              f"pwm_open {g['pwm_open']} interval_ms {g['interval_ms']:.0f} mapped_mm {g.get('mapped_mm')} "
              f"steps {g.get('steps')} mm {g.get('mm_before')}->{g.get('mm_after')} dir {g['dir']}")

    print("\n=== Regime tagging (station/grade proximity) among MOVING-TRANSIT Hall-only genuine "
          "advances (dwell/pause-spanning excluded -- see dwell table above) ===")
    regime_ct = defaultdict(list)
    for g in moving_ho:
        direction = 1 if g["dir"] == "CW" else (-1 if g["dir"] == "CCW" else 0)
        tags = regime_tags(g["mm_before"], g["mm_after"], direction) if direction else {"UNKNOWN"}
        g["regimes"] = sorted(tags)
        for t in tags:
            regime_ct[t].append(g)
    for t in sorted(regime_ct, key=lambda x: -len(regime_ct[x])):
        vals = [g["interval_ms"] for g in regime_ct[t]]
        pwms = [g["pwm_open"] for g in regime_ct[t]]
        print(f"  {t:20} n={len(vals):3d} pwm_range={min(pwms)}-{max(pwms)} "
              f"interval_min={min(vals):.0f} interval_median={pct(vals,.5):.0f}")

    print("\n=== STATION/GRADE/CURVE-tagged MOVING-TRANSIT Hall-only genuine events (station "
          "deceleration & departure check: does the guard bite here even though pwm_open is low "
          "at journey's end?) ===")
    for g in moving_ho:
        tags = g.get("regimes", [])
        if not any(t.startswith("STATION") or "GRADE" in t or "CURVE" in t for t in tags):
            continue
        print(f"  {os.path.basename(g['src'])} line {g['line']} regimes={tags} "
              f"pwm_open(now)={g['pwm_open']} interval_ms={g['interval_ms']:.0f} "
              f"mapped_mm={g.get('mapped_mm')} mm {g.get('mm_before')}->{g.get('mm_after')}")

    # ---- Guard comparison: 500 (real) vs fixed 700 vs proposed ramp ----
    print("\n=== Guard comparison across the SAME Hall-only-path event stream ===")
    for name, fn in (("current (500 flat)", real_guard), ("fixed 700", fixed_700),
                      ("proposed ramp 700-1000 (1150-5*pwm)", proposed_ramp)):
        genuine_suppressed, genuine_admitted, false_admitted, false_rejected = [], [], [], []
        for p in parsed_all:
            sim = simulate(p["nav_events"], fn)
            for r in sim:
                if r.get("exogenous") or "elapsed_since_last_accept" not in r:
                    continue
                real_g = r["real_accept"]  # ground truth: was this a genuine magnet?
                if real_g:
                    if r["counterfactual_accept"]:
                        genuine_admitted.append(r)
                    else:
                        genuine_suppressed.append(dict(r, src=p["path"]))
                else:
                    if r["counterfactual_accept"]:
                        false_admitted.append(dict(r, src=p["path"]))
                    else:
                        false_rejected.append(r)
        print(f"\n-- {name} --")
        print(f"  genuine admitted:   {len(genuine_admitted)}")
        print(f"  genuine SUPPRESSED: {len(genuine_suppressed)}")
        print(f"  false admitted (slips through): {len(false_admitted)}")
        print(f"  false rejected:     {len(false_rejected)}")
        for s in genuine_suppressed:
            print(f"    SUPPRESSED: {os.path.basename(s['src'])} line {s['line']} pwm_open {s['pwm_open']} "
                  f"elapsed {s['elapsed_since_last_accept']:.0f}ms real_ruling {s['real_ruling']}")
        for a in false_admitted:
            print(f"    FALSE-ADMITTED: {os.path.basename(a['src'])} line {a['line']} pwm_open {a['pwm_open']} "
                  f"elapsed {a['elapsed_since_last_accept']:.0f}ms")

    # dump for report reproducibility
    os.makedirs(OUT, exist_ok=True)
    g_path = os.path.join(OUT, "20260922_mm_guard_timing_toby_genuine.json")
    f_path = os.path.join(OUT, "20260922_mm_guard_timing_toby_hall_only_false.json")
    with open(g_path, "w") as f:
        json.dump(genuine, f, indent=1, default=str)
    with open(f_path, "w") as f:
        json.dump(hall_only_false, f, indent=1, default=str)
    print(f"\nwrote {g_path}, {f_path}")


if __name__ == "__main__":
    main()
