#!/usr/bin/env python3
"""Measure the ORDINARY-TRACK Hall level in each magnet interval, and the
change from one interval to the next.

    tools/hall_interval_levels.py RUN.log [--csv OUTDIR]

The level is computed ONLY from raw Hall counts. The diag/waveform candidate
records carry int16 samples relative to the record's own localRef L, oriented
so the declared pole is positive, so the raw counts come back exactly:

    raw[i] = localRef + (polarity ? s[i] : -s[i])

rest_ref, local_ref, the fixed baseline and the shadow baseline are printed for
cross-reference and are never the measured value.

WHAT COUNTS AS ORDINARY TRACK
  Ordinary track is FLAT. A magnet is steep, and a station ramp or the climb
  out of a displaced field is a steady drift. One test rejects all three: a
  sample is usable only if it sits at the centre of a QWIN-ms window whose
  MOV-ms moving mean spans no more than QSPAN counts, and only if the GUARD ms
  between it and the detection instant are also quiet. Grillers' departure ramp
  drifts ~220 counts/s = 22 counts per 100 ms and fails the test; 1 kHz Hall
  noise on straight track spans 4-8 counts and passes.

  Flatness alone is NOT sufficient -- Grillers parks Otto on a magnet and the
  resulting 2141-count shelf is perfectly flat. Intervals in the Grillers zone
  are therefore reported as their own class, never folded into "ordinary".
"""
import sys, os, json, base64, struct, collections, statistics as st, datetime as dt

# --------------------------------------------------------------- wire format
HDR = "<" + "B"*9 + "H"*10 + "h"*6 + "f" + "I"*3
HS = struct.calcsize(HDR)
FIELDS = ("version slotIndex slotTotal chunkIndex chunkTotal polarity outcome "
          "isMagnet widthRejected sampleCount chunkSampleCount chunkOffset "
          "preSamples peakCounts gain excursionFirst excursionLast "
          "widthCaliperMs widthFracMs peakSigned rawAtDetect localRef "
          "reference shadowRef departAtDetect amplitudeRatio gapMs "
          "detectedAtMs windowEndMs").split()

# ------------------------------------------------------------------ tunables
MOV, QWIN, QSPAN, GUARD = 21, 101, 10, 40
MIN_PRE, MIN_TAIL = 120, 40          # ms of usable samples required
GRILLERS = set(range(57, 68))        # platform MM63, rest MM60/61, ramp MM62


def rows(path):
    for n, ln in enumerate(open(path, errors="replace"), 1):
        p = ln.rstrip("\n").split("\t")
        if len(p) < 3: continue
        yield n, dt.datetime.fromisoformat(p[0]), p[1].split("/", 3)[-1], p[2]


def jrows(path):
    for n, ts, topic, pay in rows(path):
        if pay[:1] in "{[":
            try: yield n, ts, topic, json.loads(pay)
            except Exception: pass
        else:
            yield n, ts, topic, pay


def waveforms(path):
    chunks = []
    for n, ts, topic, pay in rows(path):
        if topic != "diag/waveform": continue
        try: b = base64.b64decode(json.loads(pay)["payload_b64"])
        except Exception: continue
        if not b or b[0] != 2: continue          # X18 (version-less) records
        h = dict(zip(FIELDS, struct.unpack(HDR, b[:HS])))
        h["s"] = list(struct.unpack("<%dh" % h["chunkSampleCount"], b[HS:HS+h["chunkSampleCount"]*2]))
        h["line"] = n
        chunks.append(h)
    by = collections.defaultdict(list)
    for c in chunks:
        by[(c["detectedAtMs"], c["sampleCount"])].append(c)
    out = []
    for _, cs in sorted(by.items()):
        cs.sort(key=lambda c: c["chunkOffset"])
        seen, keep = set(), []
        for c in cs:                              # a record can be dumped twice
            if c["chunkOffset"] in seen: continue
            seen.add(c["chunkOffset"]); keep.append(c)
        r = dict(keep[0])
        r["s"] = [v for c in keep for v in c["s"]]
        r["complete"] = len(r["s"]) == r["sampleCount"]
        sign = 1 if r["polarity"] else -1
        r["raw"] = [r["localRef"] + sign*v for v in r["s"]]
        r["lines"] = [c["line"] for c in keep]
        out.append(r)
    out.sort(key=lambda r: r["detectedAtMs"])
    return out


# ------------------------------------------------------------- the flat test
def movmean(v, w):
    from itertools import accumulate
    cs = [0] + list(accumulate(v)); h = w//2; n = len(v)
    return [(cs[min(n, i+h+1)] - cs[max(0, i-h)]) / (min(n, i+h+1) - max(0, i-h))
            for i in range(n)]


def quiet(raw, qspan=QSPAN, qwin=QWIN):
    n = len(raw); mm = movmean(raw, MOV); ok = [False]*n; h = qwin//2
    for i in range(h, n-h):
        seg = mm[i-h:i+h+1]
        ok[i] = (max(seg) - min(seg)) <= qspan
    return ok


def measure(r, qspan=QSPAN, guard=GUARD):
    """Usable pre-roll and tail sample indices. Erosion is ONE-SIDED, toward
    the magnet: a pre-roll sample needs `guard` ms of quiet after it, a tail
    sample `guard` ms of quiet before it."""
    raw, pre, n = r["raw"], r["preSamples"], len(r["raw"])
    ok = quiet(raw, qspan); ok[pre] = False
    pre_i  = [i for i in range(pre)      if ok[i] and all(ok[i:min(pre, i+guard+1)])]
    tail_i = [i for i in range(pre+1, n) if ok[i] and all(ok[max(pre+1, i-guard):i+1])]
    return pre_i, tail_i


def level(v):
    m = st.median(v)
    return dict(n=len(v), med=m, mad=st.median(sorted(abs(x-m) for x in v)),
                lo=min(v), hi=max(v))


# ----------------------------------------------------------------- the model
class Run:
    def __init__(self, path):
        self.path = path
        J = list(jrows(path))
        boots = [n for n, _, t, _ in J if t == "state/bootid"]
        breaks = sorted(n for n, _, t, p in J if
                        t in ("state/bootid", "state/start_interval", "state/session_direction")
                        or (t == "state/auto" and str(p) == "0")
                        or (t == "state/warning" and "WRONG MAGNET" in str(p)))
        bootof  = lambda ln: sum(1 for b in boots if ln >= b)
        epochof = lambda ln: sum(1 for b in breaks if ln >= b)

        exc = [(n, ts, p) for n, ts, t, p in J if t == "diag/excursion"]
        mkr = [(n, ts, p) for n, ts, t, p in J if t == "mm/marker"]
        self.DET = []
        for (en, ets, ep), (mn, _, mp) in zip(exc, mkr):
            self.DET.append(dict(line=en, mkline=mn, ts=ets, t=ep["detected_ms"],
                                 mm=ep["mm"], pwm=ep["pwm"], station=ep["station"],
                                 dir=ep["dir"], ruling=mp["ruling"], e=ep, m=mp,
                                 boot=bootof(en), epoch=epochof(en)))
        self.DET.sort(key=lambda d: (d["boot"], d["t"]))
        for i, d in enumerate(self.DET): d["idx"] = i
        bydet = {d["t"]: d for d in self.DET}

        self.W = waveforms(path)
        for r in self.W:
            r["det"] = bydet.get(r["detectedAtMs"])
            if not r["det"]: continue
            pi, ti = measure(r)
            r["pre_i"], r["tail_i"] = pi, ti
            r["pre_ok"], r["tail_ok"] = len(pi) >= MIN_PRE, len(ti) >= MIN_TAIL
            r["pre_lvl"]  = level([r["raw"][i] for i in pi]) if pi else None
            r["tail_lvl"] = level([r["raw"][i] for i in ti]) if ti else None
            r["pre_last"]   = (max(pi) - r["preSamples"]) if pi else None
            r["tail_first"] = (min(ti) - r["preSamples"]) if ti else None
        self.BY = {r["det"]["idx"]: r for r in self.W if r["det"]}

    # I_k = detection k -> k+1, or None when it spans a break
    def interval(self, k):
        if k < 0 or k+1 >= len(self.DET): return None
        a, b = self.DET[k], self.DET[k+1]
        if (a["boot"], a["epoch"], a["dir"]) != (b["boot"], b["epoch"], b["dir"]):
            return None
        return (a, b)

    def regime(self, k):
        a, b = self.interval(k)
        if a["mm"] in GRILLERS or b["mm"] in GRILLERS: return "GRILLERS"
        if a["station"] != "IDLE" or b["station"] != "IDLE":
            s = {a["station"], b["station"]} - {"IDLE"}
            return "DEPART" if "DEPART" in s else ("ZERO_RAMP" if "ZERO_RAMP" in s else "APPROACH")
        if min(a["pwm"], b["pwm"]) < 45: return "SLOW"
        if b["pwm"] < a["pwm"] - 8:      return "DECEL"
        return "ORDINARY"

    def pairs(self):
        """Adjacent interval pairs (I_{k}, I_{k+1}).
        A = one record straddles both (its pre-roll ends I_k, its tail opens I_{k+1})
        B = two records, pre-roll to pre-roll, a full interval apart."""
        out = []
        for k in range(len(self.DET)):
            if not (self.interval(k) and self.interval(k+1)): continue
            rA = self.BY.get(k+1)
            if rA and rA["pre_ok"] and rA["tail_ok"]:
                out.append(dict(kind="A", k=k, a=rA["pre_lvl"], b=rA["tail_lvl"],
                                rec=[rA], sep=rA["tail_first"]-rA["pre_last"]))
            r0, r1 = self.BY.get(k+1), self.BY.get(k+2)
            if r0 and r1 and r0["pre_ok"] and r1["pre_ok"]:
                out.append(dict(kind="B", k=k, a=r0["pre_lvl"], b=r1["pre_lvl"], rec=[r0, r1],
                                sep=(self.DET[k+2]["t"]+r1["pre_last"]) - (self.DET[k+1]["t"]+r0["pre_last"])))
        for p in out:
            p["reg0"], p["reg1"] = self.regime(p["k"]), self.regime(p["k"]+1)
            p["reg"] = p["reg0"] if p["reg0"] == p["reg1"] else p["reg0"]+"/"+p["reg1"]
            p["d"] = p["b"]["med"] - p["a"]["med"]
        return out

    def spans(self):
        """Consecutive pre-roll levels: mean |change| per interval over N
        intervals. Bounds the AVERAGE, never a single step inside the span."""
        ws = sorted((r for r in self.W if r.get("pre_ok")), key=lambda r: r["detectedAtMs"])
        out = []
        for a, b in zip(ws, ws[1:]):
            da, db = a["det"], b["det"]
            if (da["boot"], da["epoch"]) != (db["boot"], db["epoch"]): continue
            n = db["idx"] - da["idx"]
            if n < 1: continue
            seg = self.DET[da["idx"]:db["idx"]+1]
            out.append(dict(k0=da["idx"], k1=db["idx"], n=n, mm0=da["mm"], mm1=db["mm"],
                            d=b["pre_lvl"]["med"]-a["pre_lvl"]["med"],
                            secs=(db["t"]-da["t"])/1000.0,
                            interrupted=any(s["pwm"] < 20 or s["station"] != "IDLE" for s in seg)))
        return out


def q(v, f):
    v = sorted(v)
    return v[min(len(v)-1, int(round(f*(len(v)-1))))] if v else float("nan")


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    run = Run(sys.argv[1])
    D, W = run.DET, run.W
    iv = [k for k in range(len(D)-1) if run.interval(k)]
    ends   = {k-1 for k, r in run.BY.items() if r.get("pre_ok")  and run.interval(k-1)}
    starts = {k   for k, r in run.BY.items() if r.get("tail_ok") and run.interval(k)}
    P = run.pairs(); S = run.spans()

    print("detections %d   intervals (no break) %d   waveform records %d (complete %d)"
          % (len(D), len(iv), len(W), sum(r["complete"] for r in W)))
    print("continuous raw Hall: %.1f s of %.0f s run (%.2f%%)"
          % (len(W)*0.913, (D[-1]["t"]-D[0]["t"])/1000, 100*len(W)*0.913/((D[-1]["t"]-D[0]["t"])/1000)))
    print("intervals with an END level %d, a START level %d, either %d (%.1f%% of %d)"
          % (len(ends), len(starts), len(ends|starts), 100*len(ends|starts)/len(iv), len(iv)))
    print("adjacent pairs %d (A=%d B=%d) over %d distinct adjacencies (%.1f%%)"
          % (len(P), sum(p["kind"]=="A" for p in P), sum(p["kind"]=="B" for p in P),
             len({p["k"] for p in P}), 100*len({p["k"] for p in P})/len(iv)))
    print()
    by = collections.defaultdict(list)
    for p in P: by[p["reg"]].append(p)
    print("%-22s %4s %6s %6s %6s %6s %8s" % ("regime", "n", "med", "p90", "p99", "max", "signed"))
    for reg in sorted(by, key=lambda x: -len(by[x])):
        a = [abs(p["d"]) for p in by[reg]]
        print("%-22s %4d %6.1f %6.1f %6.1f %6.1f  %+4.0f..%+4.0f"
              % (reg, len(a), q(a,.5), q(a,.9), q(a,.99), max(a),
                 min(p["d"] for p in by[reg]), max(p["d"] for p in by[reg])))
    print()
    clean = [s for s in S if not s["interrupted"]]
    for lbl, ss in (("uninterrupted", clean), ("all", S)):
        r = [abs(s["d"])/s["n"] for s in ss]
        print("per-interval |change| averaged over a span (%s): n=%d med %.2f p90 %.2f max %.2f"
              % (lbl, len(r), q(r,.5), q(r,.9), max(r)))

    if "--csv" in sys.argv:
        out = sys.argv[sys.argv.index("--csv")+1]
        os.makedirs(out, exist_ok=True)
        with open(os.path.join(out, "levels.csv"), "w") as f:
            f.write("clock,det_ms,det_idx,mm,pwm,station,side,level,mad,n,first_ms,last_ms,"
                    "local_ref,reference,shadow_ref,log_lines\n")
            for r in sorted(W, key=lambda r: r["detectedAtMs"]):
                d = r["det"]
                for side, lv, off in (("pre", r.get("pre_lvl"), r.get("pre_last")),
                                      ("tail", r.get("tail_lvl"), r.get("tail_first"))):
                    if not lv or not (r["pre_ok"] if side == "pre" else r["tail_ok"]): continue
                    idxs = r["pre_i"] if side == "pre" else r["tail_i"]
                    f.write("%s,%d,%d,%d,%d,%s,%s,%g,%g,%d,%d,%d,%d,%d,%d,%s\n" % (
                        d["ts"].strftime("%H:%M:%S.%f")[:-3], r["detectedAtMs"], d["idx"],
                        d["mm"], d["pwm"], d["station"], side, lv["med"], lv["mad"], lv["n"],
                        min(idxs)-r["preSamples"], max(idxs)-r["preSamples"],
                        r["localRef"], r["reference"], r["shadowRef"],
                        " ".join(map(str, r["lines"]))))
        with open(os.path.join(out, "pairs.csv"), "w") as f:
            f.write("kind,k,regime,mm_a0,mm_a1,mm_b0,mm_b1,pwm_a0,pwm_a1,pwm_b0,pwm_b1,"
                    "level_a,level_b,signed,abs,sep_ms,det_ms,log_lines\n")
            for p in sorted(P, key=lambda p: p["k"]):
                a0, a1 = run.interval(p["k"]); b0, b1 = run.interval(p["k"]+1)
                f.write("%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%g,%g,%+g,%g,%d,%s,%s\n" % (
                    p["kind"], p["k"], p["reg"], a0["mm"], a1["mm"], b0["mm"], b1["mm"],
                    a0["pwm"], a1["pwm"], b0["pwm"], b1["pwm"],
                    p["a"]["med"], p["b"]["med"], p["d"], abs(p["d"]), p["sep"],
                    " ".join(str(r["detectedAtMs"]) for r in p["rec"]),
                    " ".join(str(l) for r in p["rec"] for l in r["lines"])))
        with open(os.path.join(out, "spans.csv"), "w") as f:
            f.write("k0,k1,n_intervals,mm0,mm1,d_level,per_interval,secs,interrupted\n")
            for s in S:
                f.write("%d,%d,%d,%d,%d,%+g,%.2f,%.1f,%d\n" % (
                    s["k0"], s["k1"], s["n"], s["mm0"], s["mm1"], s["d"],
                    abs(s["d"])/s["n"], s["secs"], s["interrupted"]))
        print("\nwrote levels.csv, pairs.csv, spans.csv to", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
