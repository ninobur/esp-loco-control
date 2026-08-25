#!/usr/bin/env python3
"""test_qt_plot.py — host tests for tools/qt_plot.py's data plumbing.

INVESTIGATORY / UNAPPROVED.

Covers spec requirement 14 ("plotter renders anchors at the correct trace
time") on the data side: qt_plot.load() must parse an ANCHOR row out of a
decoded CSV with its timestamp, id, sample_seq and text intact, and place
it in the same list main() draws from. This does NOT verify the rendered
pixels -- that half was checked once this session by an actual end-to-end
render (synthetic capture -> qt_decode.py -> qt_plot.py -> PNG), by eye,
and is not automated here, the same honest limitation this repo's other
plotting tools have (matplotlib output is not asserted against in any
test file in this codebase).
"""

import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))
import qt_plot as P   # noqa: E402

failures = 0
checks = 0


def ck(cond, what):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  %s" % what)


def ckEq(got, want, what):
    ck(got == want, "%s (got %r, want %r)" % (what, got, want))


def write_csv(rows):
    path = os.path.join(HERE, "_test_qt_plot_tmp.csv")
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=[
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
        ])
        w.writeheader()
        w.writerows(rows)
    return path


def blank():
    return {"row_type": "", "session": "", "t_ms": "", "batch_seq": "",
            "phys_sample_seq": "", "phys_dt_ms": "", "phys_raw": "", "phys_baseline": "",
            "det_event_active": "0", "det_event_pole": "", "det_peak_n": "0", "det_peak_s": "0",
            "ctl_pwm_actual": "", "ctl_pwm_commanded": "", "ctl_dir": "", "ctl_estop": "", "ctl_late": "",
            "dec_kind": "", "dec_quorum_event": "", "dec_timing_gate": "",
            "dec_nav_mm_before": "", "dec_nav_mm_after": "",
            "dec_nav_state_before": "", "dec_nav_state_after": "",
            "dec_observed_polarity": "", "dec_expected_polarity": "",
            "dec_miss_streak": "", "dec_eval_count": "",
            "dec_leader_offset": "", "dec_runner_up_offset": "", "dec_quorum_margin": "",
            "dec_scores": "", "dec_dt": "", "dec_dt_expected_ms": "", "dec_dt_conserve_ratio": "",
            "dec_event_peak": "", "dec_event_duration_ms": "", "dec_ring_inserted": "",
            "op_anchor_id": "", "op_sample_seq": "", "op_text": "", "info": ""}


def test_load_parses_anchor_rows():
    print("qt_plot.load() parses ANCHOR rows with timestamp/id/sample_seq/text intact")
    rows = []
    s = blank(); s.update(row_type="SAMPLE", t_ms="1000", phys_sample_seq="0",
                          phys_raw="1000", phys_baseline="1000", ctl_pwm_actual="90")
    rows.append(s)
    a = blank(); a.update(row_type="ANCHOR", t_ms="1500", op_anchor_id="7",
                          op_sample_seq="41", op_text="Grillers platform",
                          ctl_dir="CW", ctl_pwm_actual="88", ctl_pwm_commanded="90")
    rows.append(a)
    path = write_csv(rows)
    samples, decisions, anchors, breaks, thresholds = P.load(path)
    os.unlink(path)
    ckEq(len(anchors), 1, "exactly one anchor parsed")
    t, aid, sseq, text = anchors[0]
    ckEq(t, 1.5, "anchor timestamp parsed (seconds, from t_ms)")
    ckEq(aid, "7", "anchor id parsed")
    ckEq(sseq, "41", "anchor sample_seq parsed")
    ckEq(text, "Grillers platform", "anchor text parsed")


def test_load_multiple_anchors_preserve_order():
    print("qt_plot.load() keeps multiple anchors in file order")
    rows = []
    s = blank(); s.update(row_type="SAMPLE", t_ms="1000", phys_sample_seq="0",
                          phys_raw="1000", phys_baseline="1000", ctl_pwm_actual="90")
    rows.append(s)
    for i, (t_ms, text) in enumerate([("2000", "first"), ("5000", "second"), ("9000", "third")]):
        a = blank(); a.update(row_type="ANCHOR", t_ms=t_ms, op_anchor_id=str(i + 1),
                              op_sample_seq=str(i * 100), op_text=text)
        rows.append(a)
    path = write_csv(rows)
    _, _, anchors, _, _ = P.load(path)
    os.unlink(path)
    ckEq(len(anchors), 3, "all three anchors parsed")
    ckEq([a[3] for a in anchors], ["first", "second", "third"], "anchors preserve file order")
    ckEq([a[0] for a in anchors], [2.0, 5.0, 9.0], "anchor timestamps parsed correctly, each")


def main():
    print("qt_plot.py — anchor data-plumbing host tests (investigatory)\n")
    test_load_parses_anchor_rows()
    test_load_multiple_anchors_preserve_order()
    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
