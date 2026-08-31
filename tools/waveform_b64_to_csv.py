#!/usr/bin/env python3
"""
waveform_b64_to_csv.py -- decode NAVI_ONE `diag/waveform` dumps out of a Pi
runlog into per-passage CSVs.

WHY THIS EXISTS
---------------
NAVI_ONE 0.3 publishes its trailing six-passage window as raw binary on
ngr/loco/<id>/diag/waveform when navigation withdraws AUTO (decision 0063).
On 2026-08-31 the first live dump was logged through ngr_runlog.py's
utf-8 "replace" decode, which turned every non-UTF-8 byte into U+FFFD and
destroyed it irreversibly. The logger now wraps waveform payloads as

    {"encoding":"base64","byte_length":N,"payload_b64":"..."}

This reads that back. It does NOT accept the pre-fix corrupted form: a
U+FFFD-riddled payload is not recoverable and pretending otherwise would
produce a plausible-looking waveform that is not the one the loco saw.

WIRE FORMAT
-----------
Mirrors WavHeader in firmware/test-programs/NAVI_ONE/WaveformDump.h, which is
#pragma pack(1) on a little-endian target: 8 uint8, 6 uint16, 2 float,
3 uint32 = 40 bytes, followed by chunkSampleCount int16 samples.

A slot's samples may be split across several messages; chunks carry
chunkOffset so they reassemble regardless of arrival order.

Usage:
    tools/waveform_b64_to_csv.py RUNLOG [-o OUTDIR]
    tools/waveform_b64_to_csv.py --self-test
"""

import argparse
import base64
import csv
import json
import os
import struct
import sys

TOPIC_SUFFIX = "/diag/waveform"

# '<' little-endian, no padding -- matches #pragma pack(push, 1).
HDR = struct.Struct("<8B6H2f3I")
HDR_FIELDS = [
    "slotIndex", "slotTotal", "chunkIndex", "chunkTotal",
    "polarity", "outcome", "isMagnet", "shapeTested",
    "sampleCount", "chunkSampleCount", "chunkOffset",
    "decimation", "peakCounts", "gain",
    "amplitudeRatio", "residual",
    "gapMs", "openedAtMs", "closedAtMs",
]

# MagnetRecognizer.h, enum class Outcome.
OUTCOME_NAMES = {0: "MAGNET", 1: "TOO_SOON", 2: "TOO_WEAK",
                 3: "WRONG_SHAPE", 4: "NO_CURVE"}


def parse_header(blob):
    if len(blob) < HDR.size:
        raise ValueError("short payload: %d bytes, header needs %d"
                         % (len(blob), HDR.size))
    return dict(zip(HDR_FIELDS, HDR.unpack_from(blob, 0)))


def parse_chunk(blob):
    """Returns (header dict, list of int16 samples carried by THIS chunk)."""
    h = parse_header(blob)
    n = h["chunkSampleCount"]
    need = HDR.size + n * 2
    if len(blob) != need:
        raise ValueError("payload is %d bytes, header declares %d "
                         "(%d samples)" % (len(blob), need, n))
    samples = list(struct.unpack_from("<%dh" % n, blob, HDR.size))
    return h, samples


def iter_waveform_payloads(path):
    """Yields (lineno, timestamp, topic, payload bytes) for each dump message.

    Reads binary and decodes only the timestamp/topic columns, so an
    unrelated corrupted line elsewhere in the log cannot abort the run.
    """
    with open(path, "rb") as f:
        for lineno, raw in enumerate(f, 1):
            if TOPIC_SUFFIX.encode() not in raw:
                continue
            parts = raw.rstrip(b"\r\n").split(b"\t")
            if len(parts) < 3:
                continue
            ts = parts[0].decode("ascii", "replace")
            topic = parts[1].decode("ascii", "replace")
            if not topic.endswith(TOPIC_SUFFIX):
                continue
            body = b"\t".join(parts[2:])
            try:
                rec = json.loads(body)
            except Exception:
                sys.stderr.write(
                    "line %d: not JSON -- this is the pre-2026-08-31 "
                    "corrupted form and cannot be recovered; skipping\n"
                    % lineno)
                continue
            if rec.get("encoding") != "base64":
                sys.stderr.write("line %d: unexpected encoding %r; skipping\n"
                                 % (lineno, rec.get("encoding")))
                continue
            blob = base64.b64decode(rec["payload_b64"], validate=True)
            declared = rec.get("byte_length")
            if declared is not None and declared != len(blob):
                raise ValueError("line %d: byte_length %d != decoded %d"
                                 % (lineno, declared, len(blob)))
            yield lineno, ts, topic, blob


def group_dumps(messages):
    """Splits a flat message stream into dumps.

    A dump is emitted slot 0..slotTotal-1, each slot chunk 0..chunkTotal-1,
    so slotIndex==0 and chunkIndex==0 starts a new one.
    """
    dumps, cur = [], None
    for lineno, ts, topic, blob in messages:
        h, samples = parse_chunk(blob)
        if h["slotIndex"] == 0 and h["chunkIndex"] == 0:
            cur = {"ts": ts, "topic": topic, "slots": {}}
            dumps.append(cur)
        if cur is None:
            sys.stderr.write("line %d: chunk before any dump start; "
                             "skipping\n" % lineno)
            continue
        slot = cur["slots"].setdefault(
            h["slotIndex"], {"hdr": h, "samples": {}, "seen": set()})
        slot["seen"].add(h["chunkIndex"])
        for i, v in enumerate(samples):
            slot["samples"][h["chunkOffset"] + i] = v
    return dumps


def slot_samples(slot):
    """Ordered sample list, or raises if the slot is incomplete."""
    h = slot["hdr"]
    total = h["sampleCount"]
    missing = total - len(slot["samples"])
    if missing:
        raise ValueError("slot %d incomplete: %d of %d samples "
                         "(chunks seen: %s of %d)"
                         % (h["slotIndex"], len(slot["samples"]), total,
                            sorted(slot["seen"]), h["chunkTotal"]))
    return [slot["samples"][i] for i in range(total)]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("runlog", nargs="?", help="Pi runlog (all_*.log or per-run)")
    ap.add_argument("-o", "--outdir", default=".", help="where to write CSVs")
    ap.add_argument("--self-test", action="store_true",
                    help="verify the struct layout against WaveformDump.h")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if not args.runlog:
        ap.error("a runlog path is required (or --self-test)")

    dumps = group_dumps(iter_waveform_payloads(args.runlog))
    if not dumps:
        print("no decodable waveform dumps found in %s" % args.runlog)
        return 1

    os.makedirs(args.outdir, exist_ok=True)
    for d in dumps:
        stamp = d["ts"].replace(":", "").replace("-", "").replace(".", "_")
        print("\ndump at %s  (%d slots)" % (d["ts"], len(d["slots"])))
        print("  %-4s %-9s %-4s %-7s %-8s %-9s %-8s %s"
              % ("slot", "outcome", "pol", "peak", "ampRatio", "residual",
                 "samples", "openedAtMs"))
        for idx in sorted(d["slots"]):
            slot = d["slots"][idx]
            h = slot["hdr"]
            try:
                samples = slot_samples(slot)
            except ValueError as e:
                sys.stderr.write("  %s\n" % e)
                continue
            print("  %-4d %-9s %-4d %-7d %-8.4f %-9.4f %-8d %d"
                  % (idx, OUTCOME_NAMES.get(h["outcome"], "?%d" % h["outcome"]),
                     h["polarity"], h["peakCounts"], h["amplitudeRatio"],
                     h["residual"], h["sampleCount"], h["openedAtMs"]))
            path = os.path.join(args.outdir,
                                "waveform_%s_slot%d.csv" % (stamp, idx))
            with open(path, "w", newline="") as fh:
                w = csv.writer(fh)
                for k in HDR_FIELDS:
                    if k not in ("chunkIndex", "chunkTotal",
                                 "chunkSampleCount", "chunkOffset"):
                        w.writerow(["#" + k, h[k]])
                w.writerow(["#outcomeName",
                            OUTCOME_NAMES.get(h["outcome"], h["outcome"])])
                w.writerow(["sample_index", "counts"])
                for i, v in enumerate(samples):
                    w.writerow([i, v])
    return 0


def self_test():
    """Round-trips a synthetic chunk and checks the layout against the C++."""
    ok = True

    if HDR.size != 40:
        print("FAIL header size %d, WaveformDump.h packs 40" % HDR.size)
        ok = False
    else:
        print("PASS header size 40 bytes")

    samples = [0, 1234, -1234, 32767, -32768, 7]
    blob = HDR.pack(3, 6, 0, 1, 1, 0, 1, 1,
                    len(samples), len(samples), 0, 1, 900, 4,
                    0.87, 0.0421,
                    1500, 123456, 123999)
    blob += struct.pack("<%dh" % len(samples), *samples)

    h, got = parse_chunk(blob)
    checks = [
        ("slotIndex", h["slotIndex"], 3),
        ("slotTotal", h["slotTotal"], 6),
        ("polarity", h["polarity"], 1),
        ("outcome", h["outcome"], 0),
        ("isMagnet", h["isMagnet"], 1),
        ("sampleCount", h["sampleCount"], len(samples)),
        ("peakCounts", h["peakCounts"], 900),
        ("gapMs", h["gapMs"], 1500),
        ("closedAtMs", h["closedAtMs"], 123999),
        ("samples", got, samples),
    ]
    for name, got_v, want in checks:
        if got_v != want:
            print("FAIL %s: got %r want %r" % (name, got_v, want))
            ok = False
    if abs(h["amplitudeRatio"] - 0.87) > 1e-6:
        print("FAIL amplitudeRatio %r" % h["amplitudeRatio"]); ok = False
    if abs(h["residual"] - 0.0421) > 1e-6:
        print("FAIL residual %r" % h["residual"]); ok = False
    if ok:
        print("PASS round-trip of all %d header fields + int16 samples"
              % len(HDR_FIELDS))

    # A truncated chunk must be refused, not silently half-decoded.
    try:
        parse_chunk(blob[:-2])
        print("FAIL truncated chunk accepted"); ok = False
    except ValueError:
        print("PASS truncated chunk refused")

    print("\n%s" % ("ALL CHECKS PASSED" if ok else "CHECKS FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
