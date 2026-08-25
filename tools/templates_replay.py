#!/usr/bin/env python3
"""Offline, read-only provisional TEMPLATES replay for QUORUM TRACE captures.

This is design evidence, not firmware.  It streams .qtcap files, extracts the
firmware's closed-event records, applies a conservative passage gate, then
replays map admission and bounded omission recovery.  QUORUM decision labels
are used only for retrospective measurement; they never affect admission.
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qt_format as F
from quorum_map import QuorumMap


def decision_records(path):
    """Yield (session, decision) without materialising the 1 kHz samples."""
    for _recv_us, wire in F.read_capture(path):
        try:
            hdr, payload = F.parse_record(wire)
        except F.BadRecord:
            continue
        if hdr.rec_type == F.REC_DECISION:
            yield hdr.session_id, F.parse_decision(payload)


def extract_events(path):
    """Join EVENT_CLOSED to its later QUORUM decisions by session/time."""
    sessions = collections.OrderedDict()
    for sid, d in decision_records(path):
        state = sessions.setdefault(sid, {"events": [], "pending": collections.deque(),
                                          "current": None})
        if d["kind"] == "EVENT_CLOSED":
            ev = {
                "session": "%08X" % sid, "t_ms": d["t_ms"],
                "polarity": ("N" if d["observed_polarity"] else "S"),
                "peak": d["event_peak"],
                "duration_ms": d["event_duration_ms"], "pwm": d["pwm_actual"],
                "label": "RECEIVED", "nav_before": None, "nav_after": None,
                "nav_state": None,
            }
            state["events"].append(ev)
            state["pending"].append(ev)
        elif d["kind"] == "NAV_ON_MARKER_ENTRY" and state["pending"]:
            ev = state["pending"].popleft()
            ev["nav_before"] = d["nav_mm_before"]
            ev["nav_state"] = d["nav_state_before"]
            state["current"] = ev
        elif d["kind"] in ("AGREE", "DISAGREE"):
            # The comparison follows the corresponding nav entry
            # synchronously, but its trace timestamp is loop time whereas the
            # event carries opening time.  Join by control-flow order, not an
            # invalid timestamp equality assumption.
            ev = state["current"]
            if ev is not None:
                ev["label"] = d["kind"]
                ev["nav_after"] = d["nav_mm_after"]
        elif d["kind"] == "TIMING_GATE_RESULT" and d["timing_gate"] == "ACTIVE_PHANTOM":
            ev = state["current"]
            if ev is not None and ev["label"] == "RECEIVED":
                ev["label"] = "TIMING_PHANTOM"
    return sessions


class Replay:
    def __init__(self, qmap, direction, start_mm, *, min_duration=80,
                 min_peak=60, merge_ms=350, min_arrival_ms=280,
                 max_omissions=4, recovery_observations=6, recovery_margin=1):
        self.m = qmap
        self.direction = direction
        self.mm = start_mm
        self.min_duration = min_duration
        self.min_peak = min_peak
        self.merge_ms = merge_ms
        self.min_arrival_ms = min_arrival_ms
        self.max_omissions = max_omissions
        self.recovery_observations = recovery_observations
        self.recovery_margin = recovery_margin
        self.last_credible_ms = None
        self.recovery = None
        self.safe_stopped = False
        self.metrics = collections.Counter()
        self.rows = []

    def _record(self, ev, action, **extra):
        row = dict(ev)
        row.update(action=action, templates_mm=self.mm, **extra)
        self.rows.append(row)

    def _start_recovery(self, ev):
        candidates = []
        # k is the number of conservatively omitted mapped markers before
        # this credible observation.  k=0 preserves a no-omission hypothesis.
        for k in range(self.max_omissions + 1):
            pos = (self.mm + self.direction * (k + 1)) % self.m.n
            candidates.append({"omissions": k, "pos": pos,
                               "score": int(self.m.dna_at(pos) == (ev["polarity"] == "N"))})
        self.recovery = {"candidates": candidates, "count": 1,
                         "start_ms": ev["t_ms"], "observations": [ev["t_ms"]]}

    def _continue_recovery(self, ev):
        r = self.recovery
        if r["count"]:
            for c in r["candidates"]:
                c["pos"] = (c["pos"] + self.direction) % self.m.n
                c["score"] += int(self.m.dna_at(c["pos"]) == (ev["polarity"] == "N"))
        r["count"] += 1
        r["observations"].append(ev["t_ms"])
        ranked = sorted(r["candidates"], key=lambda c: c["score"], reverse=True)
        margin = ranked[0]["score"] - ranked[1]["score"]
        # Adoption requires one candidate to explain every credible
        # observation and every rival to fail at least once.  This is a
        # stronger condition than merely leading a score table.
        if (r["count"] >= self.recovery_observations and
                ranked[0]["score"] == r["count"] and
                ranked[1]["score"] < r["count"] and
                margin >= self.recovery_margin):
            winner = ranked[0]
            self.mm = winner["pos"]
            self.metrics["recoveries"] += 1
            self.metrics["recovery_omissions"] += winner["omissions"]
            self.metrics["recovery_observations"] += r["count"]
            self.metrics["recovery_latency_ms"] += ev["t_ms"] - r["start_ms"]
            self.recovery = None
            return "RECOVERY_ADOPT", winner["omissions"], margin
        if r["count"] >= 12:
            self.safe_stopped = True
            self.metrics["safe_stops"] += 1
            return "SAFE_STOP", None, margin
        return "RECOVERY_OBSERVE", None, margin

    def feed(self, ev):
        if self.safe_stopped:
            self._record(ev, "STOPPED_IGNORE")
            return
        if ev["duration_ms"] < self.min_duration or ev["peak"] < self.min_peak:
            self.metrics["artifacts"] += 1
            self._record(ev, "ARTIFACT")
            return
        if self.last_credible_ms is not None:
            dt = ev["t_ms"] - self.last_credible_ms
            if dt < self.merge_ms:
                self.metrics["companions_merged"] += 1
                self._record(ev, "COMPANION_MERGED", dt_ms=dt)
                return
            if dt < self.min_arrival_ms:
                self.metrics["arrival_rejects"] += 1
                self._record(ev, "IMPOSSIBLE_ARRIVAL", dt_ms=dt)
                return
        self.last_credible_ms = ev["t_ms"]
        self.metrics["credible"] += 1
        if self.recovery:
            action, omitted, margin = self._continue_recovery(ev)
            self._record(ev, action, adopted_omissions=omitted, margin=margin)
            return
        expected = (self.mm + self.direction) % self.m.n
        if self.m.dna_at(expected) == (ev["polarity"] == "N"):
            self.mm = expected
            self.metrics["primary_advances"] += 1
            self._record(ev, "EXPECTED_ADVANCE")
        else:
            self.metrics["credible_contradictions"] += 1
            self._start_recovery(ev)
            self._record(ev, "RECOVERY_OBSERVE", margin=0)


def infer_direction(events):
    pairs = [(e["nav_before"], e["nav_after"]) for e in events
             if e["nav_before"] is not None and e["nav_after"] is not None]
    cw = sum(1 for a, b in pairs if (b - a) % 171 == 1)
    ccw = sum(1 for a, b in pairs if (a - b) % 171 == 1)
    return 1 if cw > ccw else -1


def reference_metrics(events, replay):
    """Transparent retrospective proxy labels; not ground truth.

    Broad QUORUM AGREE events protect obvious genuine passages.  Short events
    and QUORUM timing phantoms measure known contamination.  Ambiguous broad
    disagreements remain explicitly unlabelled.
    """
    by_key = {(r["session"], r["t_ms"]): r for r in replay.rows}
    out = collections.Counter()
    for ev in events:
        row = by_key[(ev["session"], ev["t_ms"])]
        advance = row["action"] in ("EXPECTED_ADVANCE", "RECOVERY_ADOPT")
        genuine_proxy = (ev["label"] == "AGREE" and ev["duration_ms"] >= 100 and ev["peak"] >= 80)
        phantom_proxy = (ev["label"] == "TIMING_PHANTOM" or ev["duration_ms"] < 60)
        if genuine_proxy:
            out["genuine_proxy_total"] += 1
            out["genuine_proxy_survived"] += int(row["action"] not in
                                                  ("ARTIFACT", "IMPOSSIBLE_ARRIVAL"))
            out["genuine_proxy_advanced_or_observed"] += int(row["action"] != "ARTIFACT")
            out["deliberate_omissions"] += int(row["action"] in
                                                ("ARTIFACT", "IMPOSSIBLE_ARRIVAL"))
        if phantom_proxy:
            out["phantom_proxy_total"] += 1
            out["false_coordinate_insertions"] += int(advance)
            out["observation_recovery_contamination"] += int(
                row["action"] in ("RECOVERY_OBSERVE", "RECOVERY_ADOPT"))
        if ev["label"] == "DISAGREE" and ev["duration_ms"] >= 80 and ev["peak"] >= 60:
            out["unresolved_broad_disagreements"] += 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("captures", nargs="+")
    ap.add_argument("--quorum-ino", default="firmware/QUORUM/QUORUM.ino")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()
    qmap = QuorumMap(args.quorum_ino)
    reports = []
    for path in args.captures:
        sessions = extract_events(path)
        for sid, state in sessions.items():
            events = state["events"]
            positioned = [e for e in events if e["nav_before"] is not None and
                          e["nav_state"] != "UNSET"]
            if not positioned:
                continue
            direction = infer_direction(events)
            rp = Replay(qmap, direction, positioned[0]["nav_before"])
            start_index = events.index(positioned[0])
            replay_events = events[start_index:]
            for ev in replay_events:
                rp.feed(ev)
            report = {
                "capture": path, "session": "%08X" % sid,
                "direction": "CW" if direction > 0 else "CCW",
                "start_mm": positioned[0]["nav_before"], "end_mm": rp.mm,
                "events": len(replay_events), "pipeline": dict(rp.metrics),
                "measurement": dict(reference_metrics(replay_events, rp)),
                "assumptions": {
                    "min_duration_ms": rp.min_duration, "min_peak": rp.min_peak,
                    "merge_ms": rp.merge_ms, "min_arrival_ms": rp.min_arrival_ms,
                    "max_omissions": rp.max_omissions,
                    "proxy_labels_are_ground_truth": False,
                    "position_drift": "not measurable without independent physical anchors",
                    "incorrect_recovery_adoption": "not measurable without independent physical anchors",
                },
            }
            reports.append(report)
    if args.json:
        print(json.dumps(reports, indent=2, sort_keys=True))
    else:
        for r in reports:
            print("%s session=%s %s events=%d start=%03d end=%03d" %
                  (r["capture"], r["session"], r["direction"], r["events"],
                   r["start_mm"], r["end_mm"]))
            print("  pipeline    " + json.dumps(r["pipeline"], sort_keys=True))
            print("  measurement " + json.dumps(r["measurement"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
