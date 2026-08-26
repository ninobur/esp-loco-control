#!/usr/bin/env python3
"""Convert an IR_SCOPE_ESPNOW_RX capture into the IR_SCOPE CSV that
IR_SCOPE_Replay.py reads.

The ESP-NOW transmitter runs the same detector as IR_SCOPE (same envelope
percentiles, same thrHigh = runMin + 2*span/3 / thrLow = runMin + span/3,
same 15 ms debounce and 2500 ms latch), so a converted capture is valid
replay input. Two differences are recorded honestly rather than papered over:

  * The ESP-NOW packet carries ONE envelope per 96-sample batch, taken at
    batch start; IR_SCOPE's CSV carried mid-batch recompute offsets. The
    envelope is therefore quantised to 96 ms here. It is still the recorded
    envelope, never a recomputed one.
  * Batch-sequence discontinuities are transport loss and are emitted as GAP
    rows; the transmitter's own `missedBefore` (skipped sampler slots) is
    emitted as MISSED. The replay treats these differently and correctly.

Usage:
    ir_scope_espnow_to_csv.py capture.log out.csv [--start EPOCH] [--end EPOCH]
"""
import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ir_scope_espnow_analyze import (parse_file, RAW_MASK, FLAG_INPULSE,
                                     FLAG_RISE, FLAG_FALL, FLAG_CONTRAST)

FIELDS = ["row_type", "t_s", "sample", "raw", "run_min", "run_max",
          "contrast_valid", "in_pulse", "rise", "fall", "thr_high", "thr_low",
          "info"]


def convert(rows, out_path):
    rows = sorted(rows, key=lambda r: r["firstSample"])
    n_gap = n_missed = n_session = n_sample = 0
    with open(out_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        prev = None
        for r in rows:
            if prev is None or r["sid"] != prev["sid"]:
                w.writerow({"row_type": "SESSION", "info": "sid=%08x" % r["sid"]})
                n_session += 1
            elif r["batchSeq"] != prev["batchSeq"] + 1:
                # transport loss: the detector ran, the samples are missing
                w.writerow({"row_type": "GAP",
                            "info": "batchSeq %d->%d" % (prev["batchSeq"], r["batchSeq"])})
                n_gap += 1
            elif r["missedBefore"]:
                # sampler skipped slots; detector ran continuously
                w.writerow({"row_type": "MISSED",
                            "info": "missed=%d" % r["missedBefore"]})
                n_missed += 1
            for i, s in enumerate(r["samples"][:r["count"]]):
                w.writerow({
                    "row_type": "SAMPLE",
                    "t_s": "%.3f" % ((r["firstSample"] + i) / 1000.0),
                    "sample": r["firstSample"] + i,
                    "raw": s & RAW_MASK,
                    "run_min": r["runMin"],
                    "run_max": r["runMax"],
                    "contrast_valid": 1 if s & FLAG_CONTRAST else 0,
                    "in_pulse": 1 if s & FLAG_INPULSE else 0,
                    "rise": 1 if s & FLAG_RISE else 0,
                    "fall": 1 if s & FLAG_FALL else 0,
                    "thr_high": r["thrHigh"],
                    "thr_low": r["thrLow"],
                })
                n_sample += 1
            prev = r
    return n_sample, n_gap, n_missed, n_session


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capture")
    ap.add_argument("out_csv")
    ap.add_argument("--start", type=float, help="only samples at/after this Pi epoch")
    ap.add_argument("--end", type=float, help="only samples at/before this Pi epoch")
    a = ap.parse_args()

    rows = parse_file(a.capture)
    if a.start is not None:
        rows = [r for r in rows if r["pi_t"] >= a.start]
    if a.end is not None:
        rows = [r for r in rows if r["pi_t"] <= a.end]
    if not rows:
        sys.exit("no packets in range")
    n_sample, n_gap, n_missed, n_session = convert(rows, a.out_csv)
    print("%s: %d SAMPLE, %d GAP, %d MISSED, %d SESSION -> %s"
          % (os.path.basename(a.capture), n_sample, n_gap, n_missed,
             n_session, a.out_csv))


if __name__ == "__main__":
    main()
