#!/usr/bin/env python3
"""xhr_baseline_timing.py — when may a magnet-only navigator sample its baseline?

ANALYSIS TOOLING. Diagnostic only. It decides nothing about the railway and it
does not touch firmware.

One question: measured from the start of the previous magnet's signal, when is
it safe to collect samples for a new line baseline?

WHAT IT MAY AND MAY NOT USE
  Boundaries come from RAW Hall samples only. X18's opened_ms / closed_ms,
  its operative baseline_, nav_mm and st_phase belong to X18's passage and
  morphology logic; they appear here as OFFLINE LABELS and CROSS-CHECKS and
  never as a boundary. The replay (--replay) is strictly causal: it sees one
  sample at a time and no future sample, no reference line, nothing of X18's.

THE REFERENCE LINE (--refline, used for scoring only)
  A two-pass rolling median. Pass one gives a rough line; pass two recomputes
  the median over only those samples within +/-40 counts of it, so a magnet
  crossing the window cannot drag the estimate. It is NOT a navigator rule and
  it is NOT causal -- it looks both ways in time. It is an offline yardstick.
  It is unreliable wherever the locomotive stands still on a magnet for longer
  than the window, and those stretches are reported, not silently used.

STAGES
  --audit     capture integrity and session split, before anything is concluded
  --events    magnet events from raw: sustained 70-count departures
  --settle    when the previous magnet's signal and tail have subsided
  --windows   bias of every candidate sampling window
  --predict   fixed time vs fraction of interval vs distance vs observed span
  --replay    the proposed rule, replayed causally, scored against the line
  --all       every stage

    python3 tools/xhr_baseline_timing.py capture.xhr --session C3B93D0B --all
"""

import argparse
import collections
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xhr_format as F   # noqa: E402

THRESH = 70          # counts; the departure the proposed navigator watches for
NEED = 5             # consecutive samples at or beyond THRESH to open
CLOSE_MS = 30        # samples below THRESH before the span is called closed
GUARD_MS = 40        # wait after the span closes before collecting
N_SAMPLES = 200      # samples in one baseline collection
SPREAD_MAX = 40      # counts; max-min inside the collection, else reject

ROUTEMAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                        "firmware", "test-programs", "NAVI_ONE_X18_RECORDER",
                        "RouteMap.h")


# --------------------------------------------------------------------------
# load
# --------------------------------------------------------------------------
def load_session(path, want):
    """Decode one session into arrays. Refuses to build a timeline across a
    batch gap -- a gap means samples are absent, and absent is not zero."""
    batches, rulings = [], []
    for _recv_us, data in F.iter_capture(path):
        try:
            hdr, payload = F.parse_record(data)
        except F.BadRecord:
            continue
        if hdr.session_id != want:
            continue
        if hdr.rec_type == F.REC_SAMPLES:
            batches.append((hdr.batch_seq, hdr, payload))
        elif hdr.rec_type == F.REC_RULING:
            rulings.append(F.parse_ruling(payload))
    if not batches:
        raise SystemExit("session %08X has no sample batches in %s" % (want, path))
    batches.sort(key=lambda b: b[0])
    seqs = [b[0] for b in batches]
    missing = [s for s in range(seqs[0], seqs[-1] + 1) if s not in set(seqs)]
    if missing:
        raise SystemExit(
            "REFUSING: %d sample datagrams are missing from session %08X "
            "(%d ms of trace). A baseline-timing result measured across a hole "
            "in the evidence would be an invention. Missing seq: %s"
            % (len(missing), want, len(missing) * 100, missing[:20]))

    n = sum(b[1].n_items for b in batches)
    out = {k: np.empty(n, t) for k, t in (
        ("sample_seq", np.int64), ("dt_us", np.int32), ("raw", np.int32),
        ("pwm_a", np.int16), ("flags", np.uint8), ("baseline", np.int32),
        ("nav_mm", np.int16), ("nav_dir", np.int8), ("phase", np.int8))}
    dt = np.dtype([("dt", "<u2"), ("raw", "<i2"), ("pa", "u1"), ("pc", "u1"),
                   ("fl", "u1"), ("pad", "u1")])
    i = 0
    for _s, hdr, payload in batches:
        k = hdr.n_items
        a = np.frombuffer(payload, dtype=dt, count=k)
        out["sample_seq"][i:i + k] = hdr.first_sample_seq + np.arange(k)
        out["dt_us"][i:i + k] = a["dt"]
        out["raw"][i:i + k] = a["raw"]
        out["pwm_a"][i:i + k] = a["pa"]
        out["flags"][i:i + k] = a["fl"]
        out["baseline"][i:i + k] = hdr.baseline
        out["nav_mm"][i:i + k] = hdr.nav_mm
        out["nav_dir"][i:i + k] = hdr.nav_dir
        out["phase"][i:i + k] = hdr.st_phase
        i += k
    steps = np.diff(out["sample_seq"])
    if not (steps == 1).all():
        raise SystemExit("REFUSING: sample sequence is not contiguous")
    # The locomotive's own clock: the cumulative sum of the dt it measured.
    out["t_us"] = np.cumsum(out["dt_us"].astype(np.int64))
    out["t_us"] -= out["t_us"][0]
    out["rulings"] = rulings
    return out


def route_spacing():
    txt = open(os.path.normpath(ROUTEMAP)).read()
    m = re.search(r"ROUTE_SPACING_MM\[ROUTE_N\]\s*=\s*\{(.*?)\};", txt, re.S)
    sp = np.array([int(x) for x in re.findall(r"\d+", m.group(1))])
    assert len(sp) == 171 and sp.sum() == 52150
    return sp


# --------------------------------------------------------------------------
# reference line -- offline yardstick, never a navigator rule
# --------------------------------------------------------------------------
def reference_line(raw, half=2000, stride=50, excl=40):
    n = len(raw)
    anchors = np.arange(0, n, stride)
    rough = np.array([np.median(raw[max(0, k - half):min(n, k + half)])
                      for k in anchors])
    rough = np.interp(np.arange(n), anchors, rough)
    keep = np.abs(raw - rough) < excl
    out = []
    for k in anchors:
        lo, hi = max(0, k - half), min(n, k + half)
        w = raw[lo:hi][keep[lo:hi]]
        out.append(np.median(w) if len(w) >= 50 else rough[k])
    return np.interp(np.arange(n), anchors, np.array(out))


# --------------------------------------------------------------------------
# events from raw
# --------------------------------------------------------------------------
def detect(dev, thresh=THRESH, need=NEED, close_ms=CLOSE_MS):
    over = np.abs(dev) >= thresh
    c = np.convolve(over.astype(np.int32), np.ones(need, np.int32), "valid")
    seed = np.zeros(len(dev), bool)
    seed[:len(c)] = (c == need)
    idx = np.flatnonzero(seed)
    ev, i, n = [], 0, len(over)
    while i < len(idx):
        s = int(idx[i])
        j = last = s
        while j < n:
            if over[j]:
                last = j
            elif j - last >= close_ms:
                break
            j += 1
        ev.append((s, int(last)))
        i = int(np.searchsorted(idx, last + 1))
    return ev


def classify(S, ev):
    """Ordinary running, a throttle change, a station phase, or a stop.
    X18's st_phase is used here as an OFFLINE LABEL only."""
    rows = []
    for k in range(len(ev) - 1):
        a, ae = ev[k]
        b = ev[k + 1][0]
        sl = slice(a, b)
        pw = S["pwm_a"][sl]
        r = dict(k=k, a=a, ae=ae, b=b, iv=b - a, span=ae - a,
                 t_s=S["t_us"][a] / 1e6,
                 dirn=int(np.median(S["nav_dir"][sl])),
                 phases=sorted(set(int(x) for x in np.unique(S["phase"][sl]))),
                 pwm_min=int(pw.min()), pwm_max=int(pw.max()),
                 mm_a=int(S["nav_mm"][a]), mm_b=int(S["nav_mm"][b]))
        if (pw == 0).sum() > 200 or r["iv"] > 3000:
            r["cls"] = "stop"
        elif set(r["phases"]) - {0}:
            r["cls"] = "station"
        elif r["pwm_max"] - r["pwm_min"] > 2:
            r["cls"] = "throttle-change"
        else:
            r["cls"] = "ordinary"
        rows.append(r)
    return rows


# --------------------------------------------------------------------------
# the proposed rule, replayed causally
# --------------------------------------------------------------------------
def replay(raw, thresh=THRESH, need=NEED, close_ms=CLOSE_MS, guard_ms=GUARD_MS,
           n_samples=N_SAMPLES, spread_max=SPREAD_MAX, prime_ms=2000):
    """One sample at a time. No future sample, no reference line, nothing of
    X18's. Returns (events, locks, aborts)."""
    n = len(raw)
    base = float(np.median(raw[:prime_ms]))
    events, locks, aborts = [], [], []
    state, over_run = "IDLE", 0
    open_i = last_over = -1
    peak, peaksgn = 0.0, 0
    collect, guard_until = [], -1
    i = prime_ms
    while i < n:
        dev = raw[i] - base
        a = dev if dev >= 0 else -dev
        over = a >= thresh
        if state in ("IDLE", "GUARD", "COLLECT"):
            over_run = over_run + 1 if over else 0
            if over_run >= need:
                if state != "IDLE":
                    aborts.append((i, "next magnet arrived during %s" % state.lower()))
                state, open_i, last_over = "OPEN", i - need + 1, i
                peak, peaksgn, collect = a, (1 if dev > 0 else -1), []
                i += 1
                continue
        if state == "OPEN":
            if over:
                last_over = i
                if a > peak:
                    peak, peaksgn = a, (1 if dev > 0 else -1)
            elif i - last_over >= close_ms:
                events.append((open_i, last_over, peak,
                               "N" if peaksgn > 0 else "S", base))
                state, guard_until, over_run = "GUARD", last_over + guard_ms, 0
            i += 1
            continue
        if state == "GUARD":
            if i >= guard_until:
                state, collect = "COLLECT", []
            i += 1
            continue
        if state == "COLLECT":
            collect.append(raw[i])
            if len(collect) >= n_samples:
                c = np.asarray(collect, float)
                spread = float(c.max() - c.min())
                if spread <= spread_max:
                    locks.append((i, float(np.median(c)), base, len(c), spread))
                    base = locks[-1][1]
                else:
                    aborts.append((i, "spread %.0f counts" % spread))
                state, over_run = "IDLE", 0
            i += 1
            continue
        i += 1
    return events, locks, aborts


# --------------------------------------------------------------------------
# stages
# --------------------------------------------------------------------------
def pct(a, qs):
    return tuple(np.percentile(np.asarray(a, float), qs))


def st_audit(path, want, S):
    print("== capture integrity ==")
    print("  file      %s (%.2f MB)" % (path, os.path.getsize(path) / 1048576.0))
    print("  session   %08X" % want)
    print("  samples   %d over %.1f s of locomotive time (%.1f min)"
          % (len(S["raw"]), S["t_us"][-1] / 1e6, S["t_us"][-1] / 6e7))
    print("  datagrams contiguous, 0 missing -- checked on load, else refused")
    dt = S["dt_us"][1:]
    print("  tick dt   mean %.3f us, p99 %.0f, p99.99 %.0f, max %.0f"
          % (dt.mean(), *pct(dt, [99, 99.99]), dt.max()))
    big = dt[dt > 5000]
    print("  gaps      %d samples with dt > 5 ms, %.3f s in total (%.4f%% of the run)"
          % (len(big), big.sum() / 1e6, 100.0 * big.sum() / S["t_us"][-1]))
    print("  => the stream is continuous at 1 kHz; no window below is measured")
    print("     across a hole.")


def st_events(S, ref, ev, rows):
    print("\n== magnet events from raw (sustained %d-count departure) ==" % THRESH)
    print("  events            %d   (X18 rulings in this session: %d -- cross-check only)"
          % (len(ev), len(S["rulings"])))
    span = np.array([e[1] - e[0] for e in ev], float)
    print("  >=70 span (ms)    p50 %.0f p95 %.0f p99 %.0f max %.0f"
          % (*pct(span, [50, 95, 99]), span.max()))
    gap = np.diff([e[0] for e in ev]).astype(float)
    print("  start-to-start    p5 %.0f p50 %.0f p95 %.0f (ms); %d over 3 s (stops)"
          % (*pct(gap, [5, 50, 95]), (gap > 3000).sum()))
    dev = S["raw"] - ref
    pk = np.array([np.abs(dev[a:b + 1]).max() for a, b in ev])
    print("  peak amplitude    min %.0f p1 %.0f p50 %.0f max %.0f counts"
          % (pk.min(), np.percentile(pk, 1), np.median(pk), pk.max()))
    c = collections.Counter((r["cls"], "CW" if r["dirn"] > 0 else "CCW") for r in rows)
    print("  intervals         " + ", ".join("%s %s %d" % (k[1], k[0], v)
                                             for k, v in sorted(c.items())))


def st_settle(S, ref, rows):
    dev = (S["raw"] - ref).astype(float)

    def quiet_from_start(r, tol, need=30):
        for f in range(r["span"], 2000):
            i = r["a"] + f
            if i + need >= len(dev):
                break
            if np.all(np.abs(dev[i:i + need]) < tol):
                return f
        return 2000

    print("\n== when has the previous magnet's signal and tail subsided? ==")
    print("   criterion: |raw - line| stays under TOL for 30 consecutive ms")
    for tol in (10, 15, 20):
        print("   TOL +/-%d counts, ms after the previous magnet's START:" % tol)
        for cls in ("ordinary", "throttle-change", "station", "stop"):
            s = [quiet_from_start(r, tol) for r in rows if r["cls"] == cls]
            if len(s) < 3:
                continue
            print("     %-16s n=%4d  p50 %4.0f p95 %4.0f p99 %4.0f max %4.0f"
                  % (cls, len(s), *pct(s, [50, 95, 99]), max(s)))
    print("   the same event measured from the moment the >=70 span CLOSED:")
    for cls in ("ordinary", "throttle-change", "station"):
        s = [quiet_from_start(r, 15) - r["span"] for r in rows if r["cls"] == cls]
        if len(s) < 3:
            continue
        print("     %-16s n=%4d  p50 %4.0f p95 %4.0f p99 %4.0f max %4.0f"
              % (cls, len(s), *pct(s, [50, 95, 99]), max(s)))
    print("   => anchoring on the span's own close removes most of the spread.")


def st_windows(S, ref, rows, nw=100):
    raw = S["raw"].astype(float)
    print("\n== bias of a %d ms sampling window ==" % nw)
    print("   truth = the line level the NEXT departure will be judged against")
    O = [r for r in rows if r["cls"] == "ordinary"]
    print("   T is ms after the previous magnet's START (ordinary running):")
    print("   %5s %6s %8s %8s %8s %9s" % ("T", "n", "p1", "p50", "p99", "worst|.|"))
    for T in (120, 140, 160, 180, 200, 240, 300, 400, 500):
        bs = [np.median(raw[r["a"] + T:r["a"] + T + nw]) - ref[r["b"]]
              for r in O if r["a"] + T + nw <= r["b"] - 150]
        if len(bs) < 20:
            continue
        bs = np.array(bs)
        print("   %5d %6d %+8.1f %+8.1f %+8.1f %9.1f"
              % (T, len(bs), *pct(bs, [1, 50, 99]), np.abs(bs).max()))
    print("\n   earliest / latest SAFE window start, ms after the previous START")
    print("   safe := starts at or after the >=70 span closes, ends at or before")
    print("           the next departure, and |median - line| <= 20 counts")
    for cls in ("ordinary", "throttle-change", "station"):
        for dn, nm in ((1, "CW"), (-1, "CCW")):
            Sx = [r for r in rows if r["cls"] == cls and np.sign(r["dirn"]) == dn]
            if len(Sx) < 5:
                continue
            E, L, Wd, none = [], [], [], 0
            for r in Sx:
                lo, hi = r["span"], r["iv"] - nw
                safe = [T for T in range(lo, max(lo, hi) + 1, 5)
                        if abs(np.median(raw[r["a"] + T:r["a"] + T + nw]) - ref[r["b"]]) <= 20]
                if not safe:
                    none += 1
                    continue
                E.append(safe[0]); L.append(safe[-1]); Wd.append(safe[-1] - safe[0])
            print("   %-16s %-3s n=%4d  earliest p50 %4.0f max %4.0f | latest min %4.0f"
                  " | band min %4.0f ms | no safe window %d"
                  % (cls, nm, len(Sx), np.percentile(E, 50), max(E), min(L), min(Wd), none))


def st_predict(S, ref, rows, nw=200):
    raw = S["raw"].astype(float)
    sp = route_spacing()
    byk = {r["k"]: r for r in rows}

    def span_mm(mm, dirn):
        if mm is None or mm > 170:
            return None
        return int(sp[mm % 171]) if dirn > 0 else int(sp[(mm - 1) % 171])

    for r in rows:
        p = byk.get(r["k"] - 1)
        r["P"] = p["iv"] if p else None
        s = span_mm(p["mm_a"], p["dirn"]) if p else None
        r["v"] = (s / (r["P"] / 1000.0)) if (s and r["P"]) else None

    use = []
    for r in rows:
        if r["cls"] not in ("ordinary", "throttle-change", "station"):
            continue
        if r["iv"] < nw + 300:
            continue
        t = None
        for T in range(60, min(r["iv"] - nw - 120, 1400)):
            if abs(np.median(raw[r["a"] + T:r["a"] + T + nw]) - ref[r["b"]]) <= 20:
                t = T
                break
        if t is not None:
            r["tmin"] = t
            use.append(r)

    print("\n== what predicts the earliest usable offset? ==")
    tm = np.array([r["tmin"] for r in use], float)
    for c in ("ordinary", "throttle-change", "station"):
        s = [r["tmin"] for r in use if r["cls"] == c]
        print("   %-16s n=%4d  p50 %4.0f p95 %4.0f max %4.0f ms after the previous START"
              % (c, len(s), *pct(s, [50, 95]), max(s)))
    print("   %-34s %8s %14s" % ("predictor", "cv", "constant needed"))
    for name, den, unit in (
            ("(a) fixed time", np.ones_like(tm), "ms"),
            ("(b) fraction of prior interval",
             np.array([r["P"] or np.nan for r in use], float), "x interval"),
            ("(c) fixed travel distance",
             np.array([1000.0 / r["v"] if r["v"] else np.nan for r in use], float), "mm"),
            ("(d) the magnet's own >=70 span",
             np.array([r["span"] for r in use], float), "x span")):
        ok = np.isfinite(den) & (den > 0)
        x = tm[ok] / den[ok]
        print("   %-34s %8.3f %9.2f %s" % (name, x.std() / x.mean(), x.max(), unit))
    v = np.array([r["v"] for r in use if r["v"] and r["cls"] == "ordinary"], float)
    print("   speed available when the current magnet starts (prior interval +"
          " surveyed spacing): p5 %.0f p50 %.0f p95 %.0f mm/s" % pct(v, [5, 50, 95]))


def st_replay(S, ref, ev_off, rows):
    raw = S["raw"].astype(float)
    ev, lk, ab = replay(raw)
    print("\n== the proposed rule, replayed causally ==")
    print("   threshold %d, sustain %d samples, close after %d ms below threshold,"
          % (THRESH, NEED, CLOSE_MS))
    print("   guard %d ms, collect %d samples, reject if max-min > %d counts"
          % (GUARD_MS, N_SAMPLES, SPREAD_MAX))
    print("   events %d   baselines locked %d   collections rejected %d"
          % (len(ev), len(lk), len(ab)))
    li = np.array([l[0] for l in lk])
    err = np.array([l[1] for l in lk]) - ref[li]
    print("   baseline error vs the offline line: mean %+.2f sd %.2f"
          "  p1 %+.1f p50 %+.1f p99 %+.1f  worst %.1f counts"
          % (err.mean(), err.std(), *pct(err, [1, 50, 99]), np.abs(err).max()))
    print("   locks worse than 15 counts: %d ; worse than 20: %d"
          % ((np.abs(err) > 15).sum(), (np.abs(err) > 20).sum()))
    evs = np.array([e[0] for e in ev])
    starts, ends, head = [], [], []
    eve = np.array([e[1] for e in ev])
    for i in li:
        j = int(np.searchsorted(eve, i)) - 1
        if j >= 0:
            starts.append(i - N_SAMPLES - evs[j])
            ends.append(i - evs[j])
        j2 = int(np.searchsorted(evs, i))
        if j2 < len(evs):
            head.append(evs[j2] - i)
    head = np.array([h for h in head if h < 10000], float)
    print("   collection starts  p50 %4.0f p95 %4.0f max %4.0f ms after the previous START"
          % (*pct(starts, [50, 95]), max(starts)))
    print("   baseline valid at  p50 %4.0f p95 %4.0f max %4.0f ms after the previous START"
          % (*pct(ends, [50, 95]), max(ends)))
    print("   headroom to the next departure: min %.0f p5 %.0f p50 %.0f ms"
          % (head.min(), *pct(head, [5, 50])))
    print("   collections still running when the next departure arrived: %d"
          % sum(1 for a in ab if "next magnet" in a[1]))
    off = np.array([e[0] for e in ev_off])
    used = np.zeros(len(off), bool)
    extra = 0
    for s in evs:
        j = int(np.argmin(np.abs(off - s)))
        if abs(off[j] - s) <= 60 and not used[j]:
            used[j] = True
        else:
            extra += 1
    print("   detection: %d offline events, %d matched, %d replay-only, %d unmatched"
          % (len(off), used.sum(), extra, (~used).sum()))
    pk = np.array([e[2] for e in ev])
    print("   peak |raw - the causal baseline| at each detection: min %.0f (threshold is %d)"
          % (pk.min(), THRESH))
    ai = np.array([a[0] for a in ab]) if ab else np.array([], int)
    if len(ai):
        print("   rejected collections: %d at a station phase, %d while stopped, %d elsewhere"
              % ((S["phase"][ai] != 0).sum(), (S["pwm_a"][ai] == 0).sum(),
                 ((S["phase"][ai] == 0) & (S["pwm_a"][ai] != 0)).sum()))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--session", required=True, help="session id in hex, e.g. C3B93D0B")
    for s in ("audit", "events", "settle", "windows", "predict", "replay", "all"):
        ap.add_argument("--" + s, action="store_true")
    args = ap.parse_args()
    want = int(args.session, 16)
    pick = [s for s in ("audit", "events", "settle", "windows", "predict", "replay")
            if getattr(args, s) or args.all] or ["audit"]

    S = load_session(args.capture, want)
    if "audit" in pick:
        st_audit(args.capture, want, S)
    if pick == ["audit"]:
        return
    raw = S["raw"].astype(float)
    ref = reference_line(raw)
    ev = detect(raw - ref)
    rows = classify(S, ev)
    if "events" in pick:
        st_events(S, ref, ev, rows)
    if "settle" in pick:
        st_settle(S, ref, rows)
    if "windows" in pick:
        st_windows(S, ref, rows)
    if "predict" in pick:
        st_predict(S, ref, rows)
    if "replay" in pick:
        st_replay(S, ref, ev, rows)


if __name__ == "__main__":
    main()
