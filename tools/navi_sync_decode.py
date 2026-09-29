#!/usr/bin/env python3
"""Decode NSR1 captures without interpreting them as navigation decisions."""

import argparse
import csv
import json
import os
import sys

import navi_sync_format as F

WIRE_FMT = "<HBBI" + "Q" * 10 + "IIQBBHH"
assert __import__("struct").calcsize(WIRE_FMT) == 110


def wire_fields(raw):
    f = __import__("struct").unpack(WIRE_FMT, raw)
    keys = ("magic", "version", "type", "sequence", "boot_id", "captured_us",
            "observed_rises", "completed_pulses", "inferred_added",
            "inferred_removed", "unreliable_samples", "saturated_samples",
            "sample_gaps", "open_aborts", "calibration_id", "pitch_um",
            "nominal_um", "optical_reason", "distance_validated", "span", "crc")
    return dict(zip(keys, f))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("capture")
    ap.add_argument("--hall-csv")
    ap.add_argument("--native-hall-csv")
    ap.add_argument("--ir-jsonl")
    ap.add_argument("--navi-jsonl")
    ap.add_argument("--status-jsonl")
    args = ap.parse_args()

    hall_f = open(args.hall_csv, "w", newline="") if args.hall_csv else None
    native_f = open(args.native_hall_csv, "w", newline="") if args.native_hall_csv else None
    ir_f = open(args.ir_jsonl, "w") if args.ir_jsonl else None
    navi_f = open(args.navi_jsonl, "w") if args.navi_jsonl else None
    status_f = open(args.status_jsonl, "w") if args.status_jsonl else None
    hall_writer = None
    native_writer = None
    counts = {"datagrams": 0, "hall_samples": 0, "native_hall_samples": 0,
              "ir_snapshots": 0, "navi": 0, "status": 0,
              "bad": 0, "sessions": set()}
    try:
        for recv_us, data in F.iter_capture(args.capture):
            counts["datagrams"] += 1
            try:
                h, payload = F.parse_record(data)
            except F.BadRecord as exc:
                counts["bad"] += 1
                print("BAD datagram kept at receiver time %d: %s" % (recv_us, exc),
                      file=sys.stderr)
                continue
            counts["sessions"].add((h.loco_id, h.session_id))
            base = {"loco_id": h.loco_id, "session_id": h.session_id,
                    "batch_seq": h.batch_seq, "t0_ms": h.t0_ms,
                    "t0_us": h.t0_us, "loco_boot_id": h.loco_boot_id,
                    "ring_drops": h.ring_drops, "nav_mm": h.nav_mm,
                    "nav_dir": h.nav_dir, "station_phase": h.station_phase,
                    "ctx_flags": h.ctx_flags, "recv_us": recv_us}
            if h.rec_type == F.REC_HALL:
                t_us = h.t0_us
                if hall_writer is None and hall_f:
                    hall_writer = csv.writer(hall_f)
                    hall_writer.writerow(["session_id", "batch_seq", "sample_seq", "t_us",
                                          "dt_us", "raw0", "raw1", "raw2", "raw3", "raw4",
                                          "median", "pwm_actual", "pwm_commanded", "flags",
                                          "nav_mm", "nav_dir", "station_phase", "ctx_flags"])
                for i, s in enumerate(F.iter_hall(h, payload)):
                    if i:
                        t_us += s["dt_us"]
                    counts["hall_samples"] += 1
                    if hall_writer:
                        hall_writer.writerow([h.session_id, h.batch_seq, s["sample_seq"], t_us,
                                              s["dt_us"], *s["raw"], s["median"],
                                              s["pwm_actual"], s["pwm_commanded"], s["flags"],
                                              h.nav_mm, h.nav_dir, h.station_phase, h.ctx_flags])
            elif h.rec_type == F.REC_IR:
                counts["ir_snapshots"] += 1
                item = F.parse_ir(payload)
                item["wire"] = wire_fields(item["wire"])
                item.update(base)
                if ir_f:
                    ir_f.write(json.dumps(item, sort_keys=True) + "\n")
            elif h.rec_type == F.REC_HALL_NATIVE:
                if native_writer is None and native_f:
                    native_writer = csv.writer(native_f)
                    native_writer.writerow(["session_id", "batch_seq", "serial", "t_us",
                                            "raw", "pwm", "direction", "nav_mm",
                                            "nav_dir", "station_phase", "ctx_flags"])
                for item in F.iter_native_hall(h, payload):
                    counts["native_hall_samples"] += 1
                    if native_writer:
                        native_writer.writerow([h.session_id, h.batch_seq, item["serial"],
                                                item["t_us"], item["raw"], item["pwm"],
                                                item["direction"], h.nav_mm, h.nav_dir,
                                                h.station_phase, h.ctx_flags])
            elif h.rec_type == F.REC_NAVI:
                counts["navi"] += 1
                item = F.parse_navi(payload); item.update(base)
                if navi_f:
                    navi_f.write(json.dumps(item, sort_keys=True) + "\n")
            elif h.rec_type == F.REC_STATUS:
                counts["status"] += 1
                item = F.parse_status(payload); item.update(base)
                if status_f:
                    status_f.write(json.dumps(item, sort_keys=True) + "\n")
    finally:
        for fh in (hall_f, native_f, ir_f, navi_f, status_f):
            if fh:
                fh.close()
    counts["sessions"] = len(counts["sessions"])
    print(json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
