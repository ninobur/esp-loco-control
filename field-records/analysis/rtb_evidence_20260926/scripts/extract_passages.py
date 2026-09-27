#!/usr/bin/env python3
"""Extract wide, traceable Hall passages for independent waveform review.

This is evidence packaging only. It does not identify Return to Baseline,
classify a passage, interpolate samples, smooth values, or apply navigation
thresholds. XHR decoding uses the repository's tools/xhr_format.py. The QT
wire layout is the existing QUORUM TRACE layout from the
claude/quorum-hall-waveform-diagnostic-plutez tooling lineage.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve()
ROOT = HERE.parents[4]
sys.path.insert(0, str(ROOT / "tools"))
import xhr_format as XHR  # noqa: E402


# Existing QUORUM TRACE format, kept here only so this package is reproducible
# on the current branch, where the historical qt_format.py is not present.
QT_FILE_HDR = struct.Struct("<8sQ")
QT_FRAME = struct.Struct("<QH")
QT_HDR = struct.Struct("<4sBBHIIIIII")
QT_SAMPLE = struct.Struct("<HhhhhBBBB")
QT_DECISION = struct.Struct("<I" + "B" * 11 + "b" * 3 + "b" * 6 + "HH" + "f" + "HH" + "B" * 4)
QT_MAGIC = b"QTRACE01"
QT_RECORD_MAGIC = b"QTR1"
QT_SAMPLE_REC = 1
QT_DECISION_REC = 2
QT_SAMPLE_ACTIVE = 0x01
QT_SAMPLE_POLE = 0x02
QT_SAMPLE_DIR_SHIFT = 2
QT_SAMPLE_ESTOP = 0x10
QT_SAMPLE_LATE = 0x20
QT_DIR = {0: "UNSET", 1: "CW", 2: "CCW", 3: "?"}
QT_KIND = {1: "EVENT_OPENED", 2: "EVENT_FLOOR_REJECT", 3: "EVENT_CLOSED",
           4: "NAV_ON_MARKER_ENTRY", 5: "TIMING_GATE_RESULT",
           6: "ACCEPT_EVENT", 7: "AGREE", 8: "DISAGREE", 9: "QUORUM_EVENT"}
QT_QUORUM = {0: "OTHER", 1: "QUORUM_OPEN", 2: "QUORUM_TIED",
             3: "QUORUM_ADOPTED", 4: "QUORUM_REOPENED", 5: "QUORUM_CLOSED",
             6: "NO_QUORUM", 7: "PHANTOM_REJECTED", 8: "FIXTURE_REJECTED",
             9: "FORCED_OFFSET"}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def unwrap_qt_decision(payload: bytes) -> dict:
    f = QT_DECISION.unpack(payload)
    (t_ms, kind, quorum_event, timing_gate, mm_before, mm_after,
     state_before, state_after, obs_pol, exp_pol, miss_streak, eval_count,
     leader_off, runner_off, margin, s0, s1, s2, s3, s4, s5,
     dt, dt_expected, ratio, peak, duration_ms,
     pwm_actual, pwm_commanded, ring_inserted, _pad) = f
    return {
        "t_ms": t_ms, "kind": QT_KIND.get(kind, f"?{kind}"),
        "quorum_event": QT_QUORUM.get(quorum_event, f"?{quorum_event}"),
        "observed_polarity": None if obs_pol == 0xFF else ("N" if obs_pol else "S"),
        "expected_polarity": None if exp_pol == 0xFF else ("N" if exp_pol else "S"),
        "nav_mm_before": mm_before, "nav_mm_after": mm_after,
        "pwm_actual": pwm_actual, "pwm_commanded": pwm_commanded,
        "event_peak": peak, "event_duration_ms": duration_ms,
        "dt_ms": dt, "dt_expected_ms": dt_expected, "ratio": ratio,
        "ring_inserted": bool(ring_inserted), "leader_offset": leader_off,
        "runner_up_offset": runner_off, "quorum_margin": margin,
        "scores": [s0, s1, s2, s3, s4, s5],
    }


def iter_qt(path: Path):
    with path.open("rb") as fh:
        head = fh.read(QT_FILE_HDR.size)
        if len(head) != QT_FILE_HDR.size or QT_FILE_HDR.unpack(head)[0] != QT_MAGIC:
            raise ValueError(f"not a QTRACE01 capture: {path}")
        while True:
            frame = fh.read(QT_FRAME.size)
            if len(frame) != QT_FRAME.size:
                return
            recv_us, length = QT_FRAME.unpack(frame)
            data = fh.read(length)
            if len(data) != length:
                return
            if len(data) < QT_HDR.size:
                continue
            fields = QT_HDR.unpack_from(data)
            if fields[0] != QT_RECORD_MAGIC or fields[1] != 1:
                continue
            rec_type, n_items = fields[2], fields[3]
            payload = data[QT_HDR.size:]
            yield recv_us, fields, rec_type, n_items, payload


def qt_sample(fields, payload, index):
    dt_ms, raw, baseline, peak_n, peak_s, pwm_actual, pwm_commanded, flags, _pad = \
        QT_SAMPLE.unpack_from(payload, index * QT_SAMPLE.size)
    return {
        "sample_seq": (fields[7] + index) & 0xFFFFFFFF,
        "t_ms": fields[8] if index == 0 else None,
        "dt_ms": dt_ms, "raw": raw, "baseline": baseline,
        "peak_n": peak_n, "peak_s": peak_s,
        "pwm_actual": pwm_actual, "pwm_commanded": pwm_commanded,
        "event_active": int(bool(flags & QT_SAMPLE_ACTIVE)),
        "event_pole": "N" if flags & QT_SAMPLE_POLE else "S",
        "dir": QT_DIR.get((flags >> QT_SAMPLE_DIR_SHIFT) & 0x03, "?"),
        "estop": int(bool(flags & QT_SAMPLE_ESTOP)),
        "late": int(bool(flags & QT_SAMPLE_LATE)),
    }


def choose_separated(candidates, count, key_fn, gap):
    chosen = []
    for candidate in sorted(candidates, key=key_fn):
        if all(abs(key_fn(candidate) - key_fn(old)) >= gap for old in chosen):
            chosen.append(candidate)
            if len(chosen) >= count:
                break
    return chosen


def choose_xhr(rulings, pre, post):
    # The source ruling is only an extraction anchor. It is never called RTB.
    usable = [r for r in rulings if r["duration_ms"] != 65535 and
              r["duration_ms"] <= 1800 and
              r["sample_seq"] >= pre and
              r["sample_seq"] + post < r["session_samples"]]
    selected = []
    for polarity in ("N", "S"):
        for lo, hi, label in ((0, 60, "low_pwm"), (61, 90, "mid_pwm"), (91, 255, "high_pwm")):
            pool = [r for r in usable if r["polarity"] == polarity and lo <= r["pwm_actual"] <= hi]
            pick = choose_separated(pool, 1, lambda x: x["sample_seq"], pre + post + 1)
            if pick:
                pick[0]["selection"] = f"{label}_{polarity}"
                selected.extend(pick)
    difficult = [r for r in usable if r["ruling"] not in ("ADVANCED", "NONE")]
    for r in choose_separated(difficult, 3, lambda x: x["sample_seq"], pre + post + 1):
        r["selection"] = "difficult_" + r["ruling"].lower()
        selected.append(r)
    return sorted(selected, key=lambda x: x["sample_seq"])


def choose_qt(decisions, session_samples, pre, post):
    usable = [d for d in decisions if d["kind"] in ("ACCEPT_EVENT", "AGREE", "DISAGREE", "QUORUM_EVENT") and
              d["event_duration_ms"] < 1800 and d["t_ms"] >= pre and
              d["t_ms"] + post < session_samples]
    selected = []
    for polarity in ("N", "S"):
        for lo, hi, label in ((0, 60, "low_pwm"), (61, 90, "mid_pwm"), (91, 255, "high_pwm")):
            pool = [d for d in usable if d["observed_polarity"] == polarity and lo <= d["pwm_actual"] <= hi]
            pick = choose_separated(pool, 1, lambda x: x["t_ms"], pre + post + 1)
            if pick:
                pick[0]["selection"] = f"{label}_{polarity}"
                selected.extend(pick)
    difficult = [d for d in usable if d["kind"] in ("DISAGREE", "QUORUM_EVENT")]
    for d in choose_separated(difficult, 3, lambda x: x["t_ms"], pre + post + 1):
        d["selection"] = "difficult_" + d["kind"].lower()
        selected.append(d)
    return sorted(selected, key=lambda x: x["t_ms"])


def write_csv(path: Path, rows, fields):
    with path.open("w", newline="") as fh:
        out = csv.DictWriter(fh, fieldnames=fields)
        out.writeheader()
        out.writerows(rows)


def extract_xhr(path, outdir, session_id, pre, post, source_label):
    rulings = []
    session_samples = 0
    for _recv, data in XHR.iter_capture(str(path)):
        try:
            hdr, payload = XHR.parse_record(data)
        except XHR.BadRecord:
            continue
        if hdr.session_id != session_id:
            continue
        if hdr.rec_type == XHR.REC_SAMPLES:
            session_samples = max(session_samples, hdr.first_sample_seq + hdr.n_items)
        elif hdr.rec_type == XHR.REC_RULING:
            r = XHR.parse_ruling(payload)
            r.update(session=f"{session_id:08X}", session_samples=0)
            rulings.append(r)
    for r in rulings:
        r["session_samples"] = session_samples
    selected = choose_xhr(rulings, pre, post)
    manifest = []
    selected_ranges = [(r["sample_seq"] - pre, r["sample_seq"] + post, r) for r in selected]
    fields = ["source_session", "sample_seq", "t_ms", "t_us", "dt_us", "raw",
              "baseline", "pwm_actual", "pwm_commanded", "direction", "flags",
              "nav_mm", "nav_direction", "station_phase", "gap_before"]
    open_files = {}
    for start, end, r in selected_ranges:
        name = f"otto_{r['selection']}_seq{r['sample_seq']}.csv"
        fh = (outdir / name).open("w", newline="")
        writer = csv.DictWriter(fh, fieldnames=fields)
        writer.writeheader()
        open_files[id(r)] = (fh, writer, start, end, 0)
        manifest.append({"file": f"passages/{name}", "source": source_label,
                         "session": f"{session_id:08X}", "anchor_sample_seq": r["sample_seq"],
                         "window_start_sample": start, "window_end_sample": end,
                         "selection": r["selection"], "source_ruling": r["ruling"],
                         "source_polarity": r["polarity"], "source_pwm_actual": r["pwm_actual"],
                         "source_pwm_commanded": r.get("pwm_commanded", ""),
                         "source_duration_ms": r["duration_ms"], "source_peak": r["peak"]})
    for _recv, data in XHR.iter_capture(str(path)):
        try:
            hdr, payload = XHR.parse_record(data)
        except XHR.BadRecord:
            continue
        if hdr.session_id != session_id or hdr.rec_type != XHR.REC_SAMPLES:
            continue
        for s in XHR.iter_samples(hdr, payload):
            for start, end, r in selected_ranges:
                if start <= s["sample_seq"] <= end:
                    fh, writer, _start, _end, previous = open_files[id(r)]
                    gap = 0 if previous == 0 else max(0, s["sample_seq"] - previous)
                    writer.writerow({"source_session": f"{session_id:08X}",
                                     "sample_seq": s["sample_seq"], "t_ms": "%.3f" % (s["t_us"] / 1000.0),
                                     "t_us": s["t_us"], "dt_us": s["dt_us"], "raw": s["raw"],
                                     "baseline": hdr.baseline, "pwm_actual": s["pwm_actual"],
                                     "pwm_commanded": s["pwm_commanded"], "direction": s["dir"],
                                     "flags": s["flags"], "nav_mm": "" if hdr.nav_mm == XHR.MM_NA else hdr.nav_mm,
                                     "nav_direction": hdr.nav_dir, "station_phase": hdr.phase_name,
                                     "gap_before": gap})
                    open_files[id(r)] = (fh, writer, _start, _end, s["sample_seq"] + 1)
    for fh, *_ in open_files.values():
        fh.close()
    return manifest


def extract_qt(path, outdir, session_id, pre, post, source_label):
    decisions = []
    session_samples = 0
    for _recv, fields, rec_type, n_items, payload in iter_qt(path):
        if fields[5] != session_id:
            continue
        if rec_type == QT_SAMPLE_REC:
            session_samples = max(session_samples, fields[7] + n_items)
        elif rec_type == QT_DECISION_REC and len(payload) >= QT_DECISION.size:
            decisions.append(unwrap_qt_decision(payload[:QT_DECISION.size]))
    selected = choose_qt(decisions, session_samples, pre, post)
    manifest = []
    ranges = [(d["t_ms"] - pre, d["t_ms"] + post, d) for d in selected]
    fields_out = ["source_session", "sample_seq", "t_ms", "dt_ms", "raw", "baseline",
                  "peak_n", "peak_s", "pwm_actual", "pwm_commanded", "direction",
                  "event_active", "event_polarity", "estop", "late", "gap_before"]
    writers = {}
    for start, end, d in ranges:
        name = f"toby_{d['selection']}_t{d['t_ms']}.csv"
        fh = (outdir / name).open("w", newline="")
        writer = csv.DictWriter(fh, fieldnames=fields_out)
        writer.writeheader()
        writers[id(d)] = (fh, writer, start, end, None, 0)
        manifest.append({"file": f"passages/{name}", "source": source_label,
                         "session": f"{session_id:08X}", "anchor_time_ms": d["t_ms"],
                         "window_start_ms": start, "window_end_ms": end,
                         "selection": d["selection"], "source_kind": d["kind"],
                         "source_quorum_event": d["quorum_event"],
                         "source_polarity": d["observed_polarity"],
                         "source_pwm_actual": d["pwm_actual"],
                         "source_pwm_commanded": d["pwm_commanded"],
                         "source_duration_ms": d["event_duration_ms"],
                         "source_peak": d["event_peak"]})
    current_t = {}
    for _recv, rec_fields, rec_type, n_items, payload in iter_qt(path):
        if rec_fields[5] != session_id or rec_type != QT_SAMPLE_REC:
            continue
        t_ms = rec_fields[8]
        for i in range(n_items):
            s = qt_sample(rec_fields, payload, i)
            if i:
                t_ms += s["dt_ms"]
            s["t_ms"] = t_ms
            for start, end, d in ranges:
                if start <= t_ms <= end:
                    fh, writer, _start, _end, previous_t, count = writers[id(d)]
                    gap = 0 if previous_t is None else max(0, t_ms - previous_t - 1)
                    writer.writerow({
                                         "source_session": f"{session_id:08X}",
                                         "sample_seq": s["sample_seq"], "t_ms": s["t_ms"],
                                         "dt_ms": s["dt_ms"], "raw": s["raw"],
                                         "baseline": s["baseline"], "peak_n": s["peak_n"],
                                         "peak_s": s["peak_s"], "pwm_actual": s["pwm_actual"],
                                         "pwm_commanded": s["pwm_commanded"], "direction": s["dir"],
                                         "event_active": s["event_active"],
                                         "event_polarity": s["event_pole"], "estop": s["estop"],
                                         "late": s["late"], "gap_before": gap})
                    writers[id(d)] = (fh, writer, _start, _end, t_ms, count + 1)
    for fh, *_ in writers.values():
        fh.close()
    return manifest


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xhr", type=Path, required=True)
    ap.add_argument("--qt", type=Path, required=True)
    ap.add_argument("--outdir", type=Path, required=True)
    ap.add_argument("--xhr-session", default="C3B93D0B")
    ap.add_argument("--qt-session", default="D7651658")
    ap.add_argument("--xhr-source-label", default=None)
    ap.add_argument("--qt-source-label", default=None)
    ap.add_argument("--pre", type=int, default=2000,
                    help="samples/ms of context before the recorded event anchor")
    ap.add_argument("--post", type=int, default=2000,
                    help="samples/ms of context after the recorded event anchor")
    args = ap.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    passages = args.outdir / "passages"
    passages.mkdir(exist_ok=True)
    manifest = {
        "tool": "extract_passages.py",
        "window_context": {"pre": args.pre, "post": args.post,
                            "meaning": "packaging context only; no RTB point is assigned"},
        "sources": [{"path": args.xhr_source_label or str(args.xhr), "sha256": sha256(args.xhr),
                     "format": "XHR1", "session": args.xhr_session},
                    {"path": args.qt_source_label or str(args.qt), "sha256": sha256(args.qt),
                     "format": "QTRACE01", "session": args.qt_session}],
        "passages": []
    }
    manifest["passages"].extend(extract_xhr(
        args.xhr, passages, int(args.xhr_session, 16), args.pre, args.post,
        args.xhr_source_label or str(args.xhr)))
    manifest["passages"].extend(extract_qt(
        args.qt, passages, int(args.qt_session, 16), args.pre, args.post,
        args.qt_source_label or str(args.qt)))
    (args.outdir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"passages": len(manifest["passages"]), "manifest": str(args.outdir / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
