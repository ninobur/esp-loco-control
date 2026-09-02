#!/usr/bin/env python3
"""Pull every real, ACCEPTED magnet passage out of the field records into one
plain text file, one passage per line, for tools/two_sided/calibrate.cpp.

    python3 tools/two_sided/extract_passages.py > passages.txt

Line format:   <tag>  <n>  <pre>  v0 v1 v2 ...
The values are ORIENTED, baseline-relative counts exactly as the firmware
recorded them -- nothing here is smoothed, resampled or reconstructed.

Sources:
  * the 2026-08-28 circuit survey -- rej=0 primary magnets, untruncated
  * the daily telemetry mirrors -- every window-dump slot the recognizer
    ruled MAGNET, from ~/ngr-telemetry/pi/NGR/telemetry/all_*.log
"""
import base64, glob, gzip, json, os, struct, sys, collections

HDR = "<8B6H2f3I"
assert struct.calcsize(HDR) == 40

def survey(path):
    out = []
    with gzip.open(path, "rt", errors="replace") as f:
        for line in f:
            b = line.find("{")
            if b < 0:
                continue
            try:
                j = json.loads(line[b:])
            except Exception:
                continue
            if j.get("rej") != 0 or j.get("tr"):
                continue
            sc = j.get("sc") or 1
            raw = base64.b64decode(j["d"])
            v = [(x - 128) * sc for x in raw]
            if j.get("pol") == "S":
                v = [-x for x in v]           # orient the pole positive
            out.append(("survey_mm%d_t%d" % (j["mm"], j["t"]), j.get("pre", 12), v))
    return out

def telemetry(path):
    slots = collections.OrderedDict()
    day = os.path.basename(path).replace("all_", "").replace(".log", "")
    with open(path, errors="replace") as f:
        for n, line in enumerate(f, 1):
            if "diag/waveform" not in line:
                continue
            try:
                j = json.loads(line.split("\t")[-1])
            except Exception:
                continue
            b = base64.b64decode(j["payload_b64"])
            if len(b) < 40:
                continue
            h = struct.unpack(HDR, b[:40])
            si, st, ci, ct, pol, outc, ism, shp = h[0:8]
            scount, ccount, coff, dec, peak, gain = h[8:14]
            gap, opened, closed = h[16:19]
            key = (st, si, opened)
            d = slots.setdefault(key, dict(line=n, ism=ism, outc=outc, n=scount,
                                           dec=dec, opened=opened, s={}))
            vals = struct.unpack("<%dh" % ccount, b[40:40 + ccount * 2])
            for i, x in enumerate(vals):
                d["s"][coff + i] = x
    out = []
    for (st, si, opened), d in slots.items():
        if not d["ism"] or d["outc"] != 0:
            continue
        if len(d["s"]) != d["n"]:
            continue                            # a chunk went missing
        v = [d["s"][i] for i in sorted(d["s"])]
        # the published record has no pre-sample count in the header; the
        # firmware's pre-roll is PRE=12 stored points at whatever decimation
        pre = 12 if d["dec"] == 1 else max(1, 12 // d["dec"])
        out.append(("telem_%s_L%d" % (day, d["line"]), pre, v))
    return out

def main():
    rows = []
    rows += survey("field-records/logs/20260828_survey/toby_1_13X_survey_waveforms.log.gz")
    for p in sorted(glob.glob(os.path.expanduser("~/ngr-telemetry/pi/NGR/telemetry/all_*.log"))):
        rows += telemetry(p)
    for tag, pre, v in rows:
        print("%s %d %d %s" % (tag, len(v), pre, " ".join(str(x) for x in v)))
    print("# %d passages" % len(rows), file=sys.stderr)

main()
