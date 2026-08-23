#!/usr/bin/env python3
"""hwt_decode.py — decode a HALL_WAVEFORM_TEST capture to CSV, with an
integrity report.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

    python3 tools/hwt_decode.py hwt_20260823_193000.hwt -o run.csv

What it guarantees:
  - every sample that arrived appears exactly once, in sample-sequence order,
    even if its datagram arrived late or twice;
  - every discontinuity is a row of its own, named for what caused it:
      MISSED   the locomotive declared slots it never acquired
      GAP      batches lost between the locomotive and the recorder
      DROP     batches the locomotive's own ring dropped during an outage
      SESSION  a reboot; nothing may be joined across it
      BAD      an unreadable or CRC-failed datagram
  - nothing is interpolated, smoothed or reconstructed. A hole stays a hole.

Column classes are kept apart on purpose:
  phys_*   measured        raw ADC, timestamps, cadence
  ann_*    annotation      what the OLD Module C threshold WOULD have said
  ctl_*    motor context   PWM and direction at that sample
  op_*     operator        anchors — the only ground truth in the file
"""

import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_format as F   # noqa: E402

COLUMNS = [
    "row_type", "session", "batch_seq", "phys_sample_seq", "phys_t_us",
    "phys_t_s", "phys_ch0_raw", "phys_ch1_raw", "phys_dt_us", "phys_late",
    "ch0_present", "ch1_present", "ann_ch0", "ann_ch1",
    "ctl_pwm_actual", "ctl_pwm_commanded", "ctl_dir", "ctl_estop",
    "ctl_fixed_mode", "ctl_seq_running", "op_anchor_id", "op_text", "info",
]


def blank():
    return {c: "" for c in COLUMNS}


def decode(path):
    """Return (rows, report). Rows are ordered by (session, sample_seq)."""
    sessions = {}          # session_id -> {batch_seq: (hdr, payload)}
    order = []             # session ids in first-seen order
    anchors, statuses = [], []
    bad = []
    dup = 0

    for _recv_us, data in F.read_capture(path):
        try:
            hdr, payload = F.parse_record(data)
        except F.BadRecord as e:
            bad.append(str(e))
            continue
        if hdr.session_id not in sessions:
            sessions[hdr.session_id] = {}
            order.append(hdr.session_id)
        if hdr.rec_type == F.REC_SAMPLES:
            if hdr.batch_seq in sessions[hdr.session_id]:
                dup += 1                      # duplicated datagram: keep one
                continue
            sessions[hdr.session_id][hdr.batch_seq] = (hdr, payload)
        elif hdr.rec_type == F.REC_ANCHOR:
            anchors.append((hdr, F.parse_anchor(payload)))
        elif hdr.rec_type == F.REC_STATUS:
            statuses.append((hdr, F.parse_status(payload)))

    rows = []
    report = {
        "file": path, "sessions": len(sessions), "bad_records": len(bad),
        "duplicate_batches": dup, "samples": 0, "anchors": len(anchors),
        "transport_gaps": 0, "transport_lost_batches": 0,
        "declared_missed_slots": 0, "loco_queue_drops": 0,
        "late_samples": 0, "max_gap_us": 0, "bad_detail": bad[:10],
    }

    for si, sid in enumerate(order):
        batches = sessions[sid]
        if si:
            r = blank()
            r.update(row_type="SESSION", session="%08X" % sid,
                     info="new boot session — do not join across this line")
            rows.append(r)

        anchors_here = sorted([a for a in anchors if a[0].session_id == sid],
                              key=lambda x: x[1]["sample_seq"])
        ai = 0
        prev_seq = None
        prev_end = None

        for bseq in sorted(batches):
            hdr, payload = batches[bseq]

            if prev_seq is not None and bseq != prev_seq + 1:
                lost = bseq - prev_seq - 1
                report["transport_gaps"] += 1
                report["transport_lost_batches"] += lost
                r = blank()
                r.update(row_type="GAP", session="%08X" % sid, batch_seq=bseq,
                         info="%d batch(es) lost in transport, batchSeq %d..%d; "
                              "samples %s..%s absent"
                              % (lost, prev_seq + 1, bseq - 1,
                                 prev_end if prev_end is not None else "?",
                                 hdr.first_sample_seq - 1))
                rows.append(r)
            prev_seq = bseq

            if hdr.missed_before:
                report["declared_missed_slots"] += hdr.missed_before
                r = blank()
                r.update(row_type="MISSED", session="%08X" % sid, batch_seq=bseq,
                         phys_sample_seq=hdr.first_sample_seq,
                         info="locomotive declared %d slot(s) never acquired — "
                              "no samples exist for them" % hdr.missed_before)
                rows.append(r)

            report["max_gap_us"] = max(report["max_gap_us"], hdr.max_gap_us)
            report["loco_queue_drops"] = max(report["loco_queue_drops"],
                                             hdr.cum_queue_drops)

            for s in F.iter_samples(hdr, payload):
                # Anchors are placed at the sample they name, in order.
                while ai < len(anchors_here) and \
                        anchors_here[ai][1]["sample_seq"] <= s["sample_seq"]:
                    a = anchors_here[ai][1]
                    r = blank()
                    r.update(row_type="ANCHOR", session="%08X" % sid,
                             phys_sample_seq=a["sample_seq"], phys_t_us=a["t_us"],
                             phys_t_s="%.6f" % (a["t_us"] / 1e6),
                             ctl_pwm_actual=a["pwm_actual"],
                             ctl_pwm_commanded=a["pwm_commanded"], ctl_dir=a["dir"],
                             op_anchor_id=a["anchor_id"], op_text=a["text"],
                             info="operator anchor")
                    rows.append(r)
                    ai += 1

                report["samples"] += 1
                report["late_samples"] += s["late"]
                r = blank()
                r.update(row_type="SAMPLE", session="%08X" % sid, batch_seq=bseq,
                         phys_sample_seq=s["sample_seq"], phys_t_us=s["t_us"],
                         phys_t_s="%.6f" % (s["t_us"] / 1e6),
                         phys_ch0_raw=s["ch0_raw"],
                         phys_ch1_raw=s["ch1_raw"] if s["ch1_present"] else "",
                         phys_dt_us=s["dt_us"], phys_late=s["late"],
                         ch0_present=int(s["ch0_present"]),
                         ch1_present=int(s["ch1_present"]),
                         ann_ch0=s["ch0_ann"], ann_ch1=s["ch1_ann"],
                         ctl_pwm_actual=s["pwm_actual"],
                         ctl_pwm_commanded=s["pwm_commanded"], ctl_dir=s["dir"],
                         ctl_estop=s["estop"], ctl_fixed_mode=s["fixed_mode"],
                         ctl_seq_running=s["seq_running"])
                rows.append(r)
            prev_end = hdr.first_sample_seq + hdr.n_samples

        # Anchors after the last sample still belong in the record.
        while ai < len(anchors_here):
            a = anchors_here[ai][1]
            r = blank()
            r.update(row_type="ANCHOR", session="%08X" % sid,
                     phys_sample_seq=a["sample_seq"], phys_t_us=a["t_us"],
                     phys_t_s="%.6f" % (a["t_us"] / 1e6),
                     op_anchor_id=a["anchor_id"], op_text=a["text"],
                     ctl_dir=a["dir"], info="operator anchor (after last sample)")
            rows.append(r)
            ai += 1

    for hdr, s in statuses:
        r = blank()
        r.update(row_type="STATUS", session="%08X" % hdr.session_id,
                 phys_sample_seq=s["sample_seq"],
                 info=("measured %.1f Hz, channels %d, missed %d, loco queue drops %d, "
                       "queue high water %d, max gap %d us, max slot %d us, "
                       "udp failures %d, heap %d, baseline A %d B %d"
                       % (s["measured_hz"], s["channels"], s["cum_missed"],
                          s["cum_queue_drops"], s["queue_high_water"],
                          s["max_gap_us"], s["max_slot_us"], s["udp_send_failures"],
                          s["free_heap"], s["baseline_a"], s["baseline_b"])))
        rows.append(r)

    for b in bad:
        r = blank()
        r.update(row_type="BAD", info=b)
        rows.append(r)

    if report["loco_queue_drops"]:
        r = blank()
        r.update(row_type="DROP",
                 info="locomotive ring dropped %d batch(es) during a transport "
                      "outage — oldest first, counted, never silent"
                      % report["loco_queue_drops"])
        rows.append(r)

    return rows, report


def print_report(rep):
    print("HALL_WAVEFORM_TEST capture report (investigatory)")
    print("  file                     %s" % rep["file"])
    print("  boot sessions            %d" % rep["sessions"])
    print("  samples decoded          %d" % rep["samples"])
    print("  operator anchors         %d" % rep["anchors"])
    print("  late samples             %d" % rep["late_samples"])
    print("  worst acquisition gap    %d us" % rep["max_gap_us"])
    print("  slots never acquired     %d  (declared by the locomotive)"
          % rep["declared_missed_slots"])
    print("  loco ring drops          %d batch(es)" % rep["loco_queue_drops"])
    print("  transport gaps           %d, covering %d batch(es)"
          % (rep["transport_gaps"], rep["transport_lost_batches"]))
    print("  duplicate datagrams      %d (kept once)" % rep["duplicate_batches"])
    print("  unreadable datagrams     %d" % rep["bad_records"])
    for d in rep["bad_detail"]:
        print("      %s" % d)
    lost = (rep["declared_missed_slots"]
            + rep["transport_lost_batches"] * 125
            + rep["loco_queue_drops"] * 125)
    total = rep["samples"] + lost
    if total:
        print("  completeness             %.4f%% of expected samples present "
              "(%d absent and accounted for)"
              % (100.0 * rep["samples"] / total, lost))
    if rep["sessions"] > 1:
        print("  NOTE: more than one boot session in this file — traces from "
              "different sessions must not be joined.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", help=".hwt file from hwt_receiver.py")
    ap.add_argument("-o", "--out", help="CSV output (default: alongside the capture)")
    ap.add_argument("--report-only", action="store_true")
    args = ap.parse_args()

    rows, rep = decode(args.capture)
    print_report(rep)

    if args.report_only:
        return 0

    out = args.out or os.path.splitext(args.capture)[0] + ".csv"
    with open(out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=COLUMNS)
        w.writeheader()
        w.writerows(rows)
    print("\nwrote %s (%d rows)" % (out, len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
