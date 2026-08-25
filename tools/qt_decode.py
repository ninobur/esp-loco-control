#!/usr/bin/env python3
"""qt_decode.py — decode a QUORUM TRACE capture to CSV, with an integrity
report.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

    python3 tools/qt_decode.py qt_20260824_193000.qtcap -o run.csv

Four record classes, kept apart on purpose (same discipline as
tools/hwt_decode.py's phys_/ann_/ctl_/op_ split, extended for QUORUM's
richer evidence -- see CAPTURE_FORMAT-equivalent notes in
firmware/QUORUM/README_TRACE.md):
  phys_*  physical measurement       raw ADC-derived Hall value, baseline
  det_*   detector interpretation    event-open state, opening pole, peaks
  dec_*   navigation decision        one record per decision; see dec_kind
  ctl_*   motor context              PWM, direction, E-stop
  op_*    operator anchor            ngr/loco/<id>/cmd/trace_anchor; see README_TRACE.md

Unlike HALL_WAVEFORM_TEST's format, SAMPLE and DECISION are two independent
streams with their own batchSeq spaces (QuorumTrace.h does not share one
counter across record types -- see that header's QtHeader.batchSeq comment
for why). Rows in the output CSV are merged and time-sorted per session
(by t_ms) so a plot can draw one coherent timeline; each stream's own
transport-gap detection stays per-stream and correct regardless.

Column classes:
  row_type  SAMPLE | DECISION | STATUS | ANCHOR | GAP | SESSION | BAD
  phys_*    measured        det_*   detector      dec_*  decision
  ctl_*     motor context   op_*    operator      info   free text
"""

import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qt_format as F   # noqa: E402

COLUMNS = [
    "row_type", "session", "t_ms", "batch_seq",
    "phys_sample_seq", "phys_dt_ms", "phys_raw", "phys_baseline",
    "det_event_active", "det_event_pole", "det_peak_n", "det_peak_s",
    "ctl_pwm_actual", "ctl_pwm_commanded", "ctl_dir", "ctl_estop", "ctl_late",
    "dec_kind", "dec_quorum_event", "dec_timing_gate",
    "dec_nav_mm_before", "dec_nav_mm_after",
    "dec_nav_state_before", "dec_nav_state_after",
    "dec_observed_polarity", "dec_expected_polarity",
    "dec_miss_streak", "dec_eval_count",
    "dec_leader_offset", "dec_runner_up_offset", "dec_quorum_margin",
    "dec_scores", "dec_dt", "dec_dt_expected_ms", "dec_dt_conserve_ratio",
    "dec_event_peak", "dec_event_duration_ms", "dec_ring_inserted",
    "op_anchor_id", "op_sample_seq", "op_text", "info",
]


def blank():
    return {c: "" for c in COLUMNS}


def decode(path):
    """Return (rows, report). Rows are ordered by session (first-seen order),
    then time within a session.

    Each of SAMPLE/DECISION/STATUS/ANCHOR has its OWN batchSeq space per
    QuorumTrace.h; a gap is detected independently per (session, rec_type)
    stream, matching qt_receiver.py's own accounting.
    """
    sessions_order = []
    per_session = {}   # sid -> {rec_type: {batch_seq: (hdr, payload)}}
    bad = []
    dup = 0

    for _recv_us, data in F.read_capture(path):
        try:
            hdr, payload = F.parse_record(data)
        except F.BadRecord as e:
            bad.append(str(e))
            continue
        if hdr.session_id not in per_session:
            per_session[hdr.session_id] = {F.REC_SAMPLE: {}, F.REC_DECISION: {},
                                           F.REC_STATUS: {}, F.REC_ANCHOR: {}}
            sessions_order.append(hdr.session_id)
        bucket = per_session[hdr.session_id][hdr.rec_type]
        if hdr.batch_seq in bucket:
            dup += 1
            continue
        bucket[hdr.batch_seq] = (hdr, payload)

    rows = []
    report = {
        "file": path, "sessions": len(sessions_order), "bad_records": len(bad),
        "duplicate_records": dup, "samples": 0, "decisions": 0, "status": 0,
        "anchors": 0, "transport_gaps": 0, "transport_lost_records": 0,
        "late_samples": 0, "bad_detail": bad[:10],
    }

    for si, sid in enumerate(sessions_order):
        if si:
            r = blank()
            r.update(row_type="SESSION", session="%08X" % sid,
                     info="new boot session — do not join across this line")
            rows.append(r)

        streams = per_session[sid]
        session_rows = []

        for rec_type, label in ((F.REC_SAMPLE, "SAMPLE"), (F.REC_DECISION, "DECISION"),
                                (F.REC_STATUS, "STATUS"), (F.REC_ANCHOR, "ANCHOR")):
            bucket = streams[rec_type]
            prev_seq = None
            for bseq in sorted(bucket):
                if prev_seq is not None and bseq != prev_seq + 1:
                    lost = bseq - prev_seq - 1
                    report["transport_gaps"] += 1
                    report["transport_lost_records"] += lost
                    hdr_next, _ = bucket[bseq]
                    r = blank()
                    r.update(row_type="GAP", session="%08X" % sid,
                             t_ms=hdr_next.t0_ms, batch_seq=bseq,
                             info="%d %s record(s) lost in transport, batchSeq %d..%d"
                                  % (lost, label, prev_seq + 1, bseq - 1))
                    session_rows.append(r)
                prev_seq = bseq

                hdr, payload = bucket[bseq]
                if rec_type == F.REC_SAMPLE:
                    for s in F.iter_samples(hdr, payload):
                        report["samples"] += 1
                        report["late_samples"] += s["late"]
                        r = blank()
                        r.update(row_type="SAMPLE", session="%08X" % sid,
                                 t_ms=s["t_ms"], batch_seq=bseq,
                                 phys_sample_seq=s["sample_seq"], phys_dt_ms=s["dt_ms"],
                                 phys_raw=s["raw"], phys_baseline=s["baseline"],
                                 det_event_active=int(s["event_active"]),
                                 det_event_pole=s["event_pole"] if s["event_active"] else "",
                                 det_peak_n=s["peak_n"], det_peak_s=s["peak_s"],
                                 ctl_pwm_actual=s["pwm_actual"], ctl_pwm_commanded=s["pwm_commanded"],
                                 ctl_dir=s["dir"], ctl_estop=s["estop"], ctl_late=s["late"])
                        session_rows.append(r)
                elif rec_type == F.REC_DECISION:
                    report["decisions"] += 1
                    d = F.parse_decision(payload)
                    r = blank()
                    r.update(row_type="DECISION", session="%08X" % sid,
                             t_ms=d["t_ms"], batch_seq=bseq,
                             dec_kind=d["kind"], dec_quorum_event=d["quorum_event"],
                             dec_timing_gate=d["timing_gate"],
                             dec_nav_mm_before=d["nav_mm_before"], dec_nav_mm_after=d["nav_mm_after"],
                             dec_nav_state_before=d["nav_state_before"], dec_nav_state_after=d["nav_state_after"],
                             dec_observed_polarity=("" if d["observed_polarity"] is None
                                                     else ("N" if d["observed_polarity"] else "S")),
                             dec_expected_polarity=("" if d["expected_polarity"] is None
                                                     else ("N" if d["expected_polarity"] else "S")),
                             dec_miss_streak=d["miss_streak"], dec_eval_count=d["eval_count"],
                             dec_leader_offset=d["leader_offset"], dec_runner_up_offset=d["runner_up_offset"],
                             dec_quorum_margin=d["quorum_margin"],
                             dec_scores=";".join(str(x) for x in d["scores"]),
                             dec_dt=d["dt"], dec_dt_expected_ms=d["dt_expected_ms"],
                             dec_dt_conserve_ratio="%.3f" % d["dt_conserve_ratio"],
                             dec_event_peak=d["event_peak"], dec_event_duration_ms=d["event_duration_ms"],
                             dec_ring_inserted=int(d["ring_inserted"]),
                             ctl_pwm_actual=d["pwm_actual"], ctl_pwm_commanded=d["pwm_commanded"])
                    session_rows.append(r)
                elif rec_type == F.REC_STATUS:
                    report["status"] += 1
                    s = F.parse_status(payload)
                    r = blank()
                    r.update(row_type="STATUS", session="%08X" % sid, t_ms=s["uptime_ms"], batch_seq=bseq,
                             info=("samples=%d decisions=%d sample_ring_drops=%d decision_ring_drops=%d "
                                   "anchor_ring_drops=%d hall_queue_drops=%d floor_rejects=%d "
                                   "free_heap=%d udp_fail=%d "
                                   "deadband=%d entry_margin=%d quorum(trigger=%d margin=%d max=%d cand=%d)"
                                   % (s["sample_seq"], s["decision_seq"], s["cum_sample_ring_drops"],
                                      s["cum_decision_ring_drops"], s["cum_anchor_ring_drops"],
                                      s["cum_hall_queue_drops"],
                                      s["cum_floor_rejects"], s["free_heap"], s["udp_send_failures"],
                                      s["hall_deadband_counts"], s["hall_entry_margin_counts"],
                                      s["quorum_trigger"], s["quorum_margin"], s["quorum_max"],
                                      s["quorum_candidates"])))
                    session_rows.append(r)
                elif rec_type == F.REC_ANCHOR:
                    report["anchors"] += 1
                    a = F.parse_anchor(payload)
                    r = blank()
                    r.update(row_type="ANCHOR", session="%08X" % sid, t_ms=a["t_ms"], batch_seq=bseq,
                             op_anchor_id=a["anchor_id"], op_sample_seq=a["sample_seq"],
                             op_text=a["text"],
                             ctl_dir=a["dir"], ctl_pwm_actual=a["pwm_actual"],
                             ctl_pwm_commanded=a["pwm_commanded"], info="operator anchor")
                    session_rows.append(r)

        # One coherent timeline per session: SAMPLE and DECISION are
        # independent streams on the wire: this is the merge point.
        session_rows.sort(key=lambda r: (r["t_ms"] if r["t_ms"] != "" else -1))
        rows.extend(session_rows)

    for b in bad:
        r = blank()
        r.update(row_type="BAD", info=b)
        rows.append(r)

    return rows, report


def print_report(rep):
    print("QUORUM TRACE capture report (investigatory)")
    print("  file                     %s" % rep["file"])
    print("  boot sessions            %d" % rep["sessions"])
    print("  samples decoded          %d" % rep["samples"])
    print("  decisions decoded        %d" % rep["decisions"])
    print("  status records           %d" % rep["status"])
    print("  operator anchors         %d" % rep["anchors"])
    print("  late samples             %d" % rep["late_samples"])
    print("  transport gaps           %d, covering %d record(s) — per-stream (SAMPLE/DECISION/STATUS/ANCHOR "
          "each have their own batchSeq space; see qt_format.py)"
          % (rep["transport_gaps"], rep["transport_lost_records"]))
    print("  duplicate records        %d (kept once)" % rep["duplicate_records"])
    print("  unreadable datagrams     %d" % rep["bad_records"])
    for d in rep["bad_detail"]:
        print("      %s" % d)
    if rep["sessions"] > 1:
        print("  NOTE: more than one boot session in this file — traces from "
              "different sessions must not be joined.")
    print("  NOTE: this report describes what the trace RECORDED, not QUORUM's "
          "detection accuracy — no different from HWT1's own report in this respect.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", help=".qtcap file from qt_receiver.py")
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
