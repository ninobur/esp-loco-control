#!/usr/bin/env python3
"""Decode X19 diag/waveform records (format version 2) from a telemetry log.

X19 publishes a CANDIDATE RECORD: 512 ms of raw pre-roll plus the 400 ms
acquisition window, at 1 kHz, never decimated, expressed in counts relative to
that candidate's own local reference L and oriented so its pole is positive.

Usage:
    tools/x19_waveform_decode.py RUN.log [--csv OUTDIR] [--plot]

Each reassembled record prints the detector's own framing numbers, so a field
run can be replayed without inferring what the firmware thought happened.
X18 records (no version byte, 40-byte header) are counted and skipped.
"""
import sys, json, base64, struct, collections, os

HDR = "<" + "B"*9 + "H"*10 + "h"*6 + "f" + "I"*3
HS = struct.calcsize(HDR)          # 57
FIELDS = ("version slotIndex slotTotal chunkIndex chunkTotal polarity outcome "
          "isMagnet widthRejected sampleCount chunkSampleCount chunkOffset "
          "preSamples peakCounts gain excursionFirst excursionLast "
          "widthCaliperMs widthFracMs peakSigned rawAtDetect localRef "
          "reference shadowRef departAtDetect amplitudeRatio gapMs "
          "detectedAtMs windowEndMs").split()
OUTCOME = ["MAGNET","TOO_SOON","TOO_WEAK","WRONG_SHAPE","INSUFFICIENT","NO_CURVE"]


def decode(path):
    chunks, legacy = [], 0
    for ln in open(path, errors="replace"):
        parts = ln.rstrip("\n").split("\t")
        if len(parts) < 3 or "diag/waveform" not in parts[1]:
            continue
        try:
            raw = base64.b64decode(json.loads(parts[2])["payload_b64"])
        except Exception:
            continue
        if not raw or raw[0] != 2:
            legacy += 1
            continue
        h = dict(zip(FIELDS, struct.unpack(HDR, raw[:HS])))
        n = h["chunkSampleCount"]
        h["samples"] = list(struct.unpack("<%dh" % n, raw[HS:HS + n*2]))
        h["ts"] = parts[0]
        chunks.append(h)
    if legacy:
        print("skipped %d X18-format records (no version byte)" % legacy)

    by = collections.defaultdict(list)
    for c in chunks:
        by[(c["detectedAtMs"], c["sampleCount"])].append(c)
    out = []
    for key, cs in sorted(by.items()):
        cs.sort(key=lambda c: c["chunkOffset"])
        rec = dict(cs[0]); rec["samples"] = [s for c in cs for s in c["samples"]]
        rec["complete"] = len(rec["samples"]) == rec["sampleCount"]
        out.append(rec)
    return out


def summarise(r):
    pre, n = r["preSamples"], r["sampleCount"]
    s = r["samples"]
    quiet = s[:pre] if pre else []
    return (
        "{ts} det={det:<10} pk={pk:<4}{pol} out={out:<12} ratio={ratio:.2f} "
        "gain={gain:<4} L={L:<5} ref={ref:<5} ref-L={d:<5} depart={dep:<5} "
        "w_cal={wc:<4} w_frac={wf:<4} exc=[{e0}..{e1}] n={n} pre={pre} "
        "preroll(min/med/max)={lo}/{md}/{hi} {ok}"
    ).format(
        ts=r["ts"][11:23], det=r["detectedAtMs"], pk=r["peakCounts"],
        pol="N" if r["polarity"] else "S", out=OUTCOME[r["outcome"]] if r["outcome"] < 6 else "?",
        ratio=r["amplitudeRatio"], gain=r["gain"], L=r["localRef"],
        ref=r["reference"], d=r["reference"] - r["localRef"],
        dep=r["departAtDetect"], wc=r["widthCaliperMs"], wf=r["widthFracMs"],
        e0=r["excursionFirst"], e1=r["excursionLast"], n=n, pre=pre,
        lo=min(quiet) if quiet else "-",
        md=sorted(quiet)[len(quiet)//2] if quiet else "-",
        hi=max(quiet) if quiet else "-",
        ok="" if r["complete"] else "**INCOMPLETE**")


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    recs = decode(sys.argv[1])
    print("records: %d  (complete: %d)" % (len(recs), sum(r["complete"] for r in recs)))
    for r in recs:
        print(" ", summarise(r))
    if "--csv" in sys.argv:
        d = sys.argv[sys.argv.index("--csv") + 1]
        os.makedirs(d, exist_ok=True)
        for r in recs:
            f = os.path.join(d, "x19_%d.csv" % r["detectedAtMs"])
            with open(f, "w") as fh:
                fh.write("t_ms_rel_detect,counts_rel_L\n")
                for i, v in enumerate(r["samples"]):
                    fh.write("%d,%d\n" % (i - r["preSamples"], v))
        print("wrote %d CSVs to %s" % (len(recs), d))
    return 0


if __name__ == "__main__":
    sys.exit(main())
