#!/usr/bin/env python3
"""Score the Epiphany bench test.

    python3 tools/curves/score_bench.py <telemetry log> <phaseA start> <phaseA end> <phaseB start> <phaseB end>

Times as HH:MM (local, as in the log). Every published Hall record for Toby
inside each window is decoded whole and scored: readings more than 20 counts
from a five-wide median of the record are counted, dropouts (below) and spikes
(above) separately, per thousand samples. Each record is also labelled by its
shape -- a still hold has a long flat top, a slide has a Gaussian-ish arc -- so
the A/B (holds) and C (crossings) parts of the protocol separate on their own.
"""
import base64, json, struct, sys, collections

HDR = "<8B6H2f3I"
LOCO = "9950012"

def med5(v):
    m = list(v)
    for i in range(len(v)):
        w = sorted(v[max(0, i-2):i+3]); m[i] = w[len(w)//2]
    return m

def records(path, t0, t1):
    slots = collections.OrderedDict()
    for line in open(path, errors="replace"):
        if "diag/waveform" not in line or LOCO not in line: continue
        t = line[11:16]
        if not (t0 <= t <= t1): continue
        try: j = json.loads(line.split("\t")[-1]); b = base64.b64decode(j["payload_b64"])
        except Exception: continue
        h = struct.unpack(HDR, b[:40]); si, st = h[0], h[1]; scount, ccount, coff, dec = h[8], h[9], h[10], h[11]
        peak, gain = h[12], h[13]; opened, closed = h[17], h[18]
        # one passage can arrive twice -- as its refusal dump and again as
        # slot 0 of the withdraw window -- so key on when it opened, only
        d = slots.setdefault(opened, dict(t=line[11:19], n=scount, dec=dec, peak=peak, wall=closed-opened, s={}))
        for i, x in enumerate(struct.unpack("<%dh" % ccount, b[40:40+ccount*2])): d["s"][coff+i] = x
    out = []
    for d in slots.values():
        if len(d["s"]) < d["n"]: continue
        v = [d["s"][i] for i in sorted(d["s"])]
        m = med5(v)
        pk = max(m) if m else 0
        if pk < 40: continue
        # shape: how much of the record sits within 10% of its top
        top = sum(1 for x in m if x >= 0.9*pk) / len(m)
        kind = "hold" if top > 0.35 else "slide"
        if kind == "hold":
            # A held record is scored on its flat top only. At decimation 32
            # the slide in and out is a few stored samples of 30-count steps,
            # and a five-wide median flags every one of them; those are the
            # ruler moving, not the sensor. 2026-09-03 00:17:05: four "bad"
            # samples, all four on the ramps, a flat top of 427 with none.
            idx = [i for i, x in enumerate(m) if x >= 0.8 * pk]
            a, b2 = idx[0] + 3, idx[-1] - 3
            if b2 - a >= 32: v, m = v[a:b2 + 1], m[a:b2 + 1]
        drop = sum(1 for a, b2 in zip(v, m) if a - b2 < -20)
        spike = sum(1 for a, b2 in zip(v, m) if a - b2 > 20)
        worst = max((abs(a - b2) for a, b2 in zip(v, m)), default=0)
        out.append(dict(t=d["t"], kind=kind, n=len(v), dec=d["dec"], wall=d["wall"], peak=pk, drop=drop, spike=spike, worst=worst))
    return out

def summarise(name, rs):
    print("\n%s: %d records" % (name, len(rs)))
    print("  %-8s %-6s %5s %4s %7s %5s | %5s %5s %6s %s" % ("time", "kind", "n", "dec", "wall", "peak", "drop", "spike", "worst", "per 1000"))
    for r in rs:
        print("  %-8s %-6s %5d %4d %6dms %5d | %5d %5d %6d  %.1f" % (r["t"], r["kind"], r["n"], r["dec"], r["wall"], r["peak"], r["drop"], r["spike"], r["worst"], 1000.0*(r["drop"]+r["spike"])/max(1, r["n"])))
    for kind in ("hold", "slide"):
        k = [r for r in rs if r["kind"] == kind]
        if not k: continue
        n = sum(r["n"] for r in k); bad = sum(r["drop"]+r["spike"] for r in k)
        print("  %-6s total: %d records, %d samples, %d bad (%.1f per 1000), %d with none" % (kind, len(k), n, bad, 1000.0*bad/max(1, n), sum(1 for r in k if r["drop"]+r["spike"] == 0)))

def main():
    if len(sys.argv) < 6: print(__doc__); sys.exit(2)
    log, a0, a1, b0, b1 = sys.argv[1:6]
    A = records(log, a0, a1); B = records(log, b0, b1)
    summarise("PHASE A (one read per sample)", A)
    summarise("PHASE B (X11, median of five reads)", B)

if __name__ == "__main__": main()
