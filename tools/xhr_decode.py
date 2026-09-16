#!/usr/bin/env python3
"""xhr_decode.py — decode and audit an X18 continuous Hall capture.

OBSERVATION TOOLING. Diagnostic only. This program reports what is in the
capture and what is missing from it. It does not fill gaps, interpolate,
resample onto a 1 kHz grid, smooth, or decide anything about baselines --
choosing measurement boundaries is the analysis's job, done afterwards, from
the raw trace this emits.

    python3 tools/xhr_decode.py capture.xhr --report
    python3 tools/xhr_decode.py capture.xhr --csv samples.csv [--session 1A2B3C4D]
    python3 tools/xhr_decode.py capture.xhr --rulings rulings.csv

WHAT IT AUDITS
  missing packets    a batch_seq gap, attributed to the locomotive's own ring
                     (its drop counter moved) or to the network (it did not)
  duplicates         the same batch_seq twice in one session
  reorder            a batch_seq lower than one already seen
  corruption         bad magic, wrong version, truncation, CRC mismatch
  tick gaps          any sample whose measured dt is not ~1 ms, including the
                     saturated ones, listed with the sample they follow
  sessions           a reboot mid-capture. Sessions are NEVER joined: sample
                     sequence, batch sequence and millis() all restart at
                     zero, so splicing two would invent a continuous trace
                     that never existed.

EVERY GAP IS REPORTED AS A GAP. A CSV row is written only for a sample that
actually arrived, and --csv marks each discontinuity with a gap_before column
so a plot cannot accidentally draw a straight line across missing evidence.
"""

import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xhr_format as F   # noqa: E402


class SessionAudit(object):
    def __init__(self, session_id, loco_id):
        self.session_id = session_id
        self.loco_id = loco_id
        self.seen = {F.REC_SAMPLES: set(), F.REC_RULING: set(), F.REC_STATUS: set()}
        self.high = {F.REC_SAMPLES: 0, F.REC_RULING: 0, F.REC_STATUS: 0}
        self.duplicates = collections.Counter()
        self.reordered = collections.Counter()
        self.samples = 0
        self.rulings = []
        self.status = []
        self.first_t0_ms = None
        # The end of the LAST batch, not its start. Dividing samples by the
        # span between batch starts drops the final batch's own 100 ms and
        # reports a rate above nominal -- 1008 Hz for a perfect 1000 Hz
        # stream, and 1500 Hz for a three-batch one.
        self.last_end_ms = None
        self.measured_us = 0
        self.tick_gaps = []       # (sample_seq, dt_us, saturated)
        self.late = 0
        self.last_ring_drops = None
        self.ring_drop_steps = 0

    def note(self, hdr):
        rt = hdr.rec_type
        seq = hdr.batch_seq
        if seq in self.seen[rt]:
            self.duplicates[rt] += 1
            return False
        if seq < self.high[rt]:
            self.reordered[rt] += 1
        self.seen[rt].add(seq)
        if seq > self.high[rt]:
            self.high[rt] = seq
        if self.last_ring_drops is not None and hdr.ring_drops_lo != self.last_ring_drops:
            self.ring_drop_steps += 1
        self.last_ring_drops = hdr.ring_drops_lo
        return True

    def missing(self, rt):
        """Sequence numbers absent between 1 and the highest seen."""
        if not self.seen[rt]:
            return []
        out, have = [], self.seen[rt]
        for s in range(1, self.high[rt] + 1):
            if s not in have:
                out.append(s)
        return out


def decode(path, want_session=None):
    audits = collections.OrderedDict()
    bad = []
    frames = 0
    for recv_us, data in F.iter_capture(path):
        frames += 1
        try:
            hdr, payload = F.parse_record(data)
        except F.BadRecord as e:
            bad.append((frames, recv_us, len(data), str(e)))
            continue
        if want_session is not None and hdr.session_id != want_session:
            continue
        a = audits.get(hdr.session_id)
        if a is None:
            a = audits[hdr.session_id] = SessionAudit(hdr.session_id, hdr.loco_id)
        if not a.note(hdr):
            continue                      # duplicate: counted, not double-decoded
        if hdr.rec_type == F.REC_SAMPLES:
            if a.first_t0_ms is None:
                a.first_t0_ms = hdr.t0_ms
            a.samples += hdr.n_items
            batch_us = 0
            for i, s in enumerate(F.iter_samples(hdr, payload)):
                batch_us += s["dt_us"]
                # The first sample of a batch measures its dt across the batch
                # boundary, so it is a real tick gap like any other.
                if s["late"] or s["dt_us"] > 1250 or (i and s["dt_us"] < 750):
                    a.tick_gaps.append((s["sample_seq"], s["dt_us"], s["dt_saturated"]))
                if s["late"]:
                    a.late += 1
            # Measured, not assumed: the batch's span is the sum of the dt its
            # own samples carried.
            a.measured_us += batch_us
            a.last_end_ms = hdr.t0_ms + batch_us / 1000.0
        elif hdr.rec_type == F.REC_RULING:
            r = F.parse_ruling(payload)
            r["_hdr_t0_ms"] = hdr.t0_ms
            a.rulings.append(r)
        elif hdr.rec_type == F.REC_STATUS:
            st = F.parse_status(payload)
            a.status.append(st)
    return audits, bad, frames


def report(path, audits, bad, frames):
    size = os.path.getsize(path)
    print("capture: %s  (%.2f MB, %d frames)" % (path, size / 1048576.0, frames))
    if not audits:
        print("  NO DECODABLE RECORDS")
    if len(audits) > 1:
        print("  ** %d SESSIONS — the locomotive rebooted during this capture." % len(audits))
        print("  ** They are reported separately and MUST NOT be joined: sample")
        print("  ** sequence, batch sequence and millis() all restart at zero.")
    for sid, a in audits.items():
        print("")
        print("session %08X  loco %d" % (sid, a.loco_id))
        span_s = ((a.last_end_ms - a.first_t0_ms) / 1000.0
                  if a.first_t0_ms is not None and a.last_end_ms is not None else 0.0)
        print("  samples        %d received over %.1f s of locomotive time" % (a.samples, span_s))
        if span_s > 0:
            # Below 1000 Hz here means samples are ABSENT -- either lost
            # datagrams (counted above) or ticks the locomotive never took
            # (the tick-gap list below). It is never a resampling artefact.
            print("  received rate  %.1f Hz (of a nominal 1000 Hz)" % (a.samples / span_s))
        for rt in (F.REC_SAMPLES, F.REC_RULING, F.REC_STATUS):
            miss = a.missing(rt)
            n = F.REC_NAME[rt]
            if not a.seen[rt]:
                continue
            print("  %-8s      %d of %d datagrams; %d missing, %d duplicate, %d reordered"
                  % (n, len(a.seen[rt]), a.high[rt], len(miss),
                     a.duplicates[rt], a.reordered[rt]))
            if miss:
                shown = ", ".join(str(m) for m in miss[:20])
                print("               missing seq: %s%s"
                      % (shown, " ..." if len(miss) > 20 else ""))
                if rt == F.REC_SAMPLES:
                    print("               = %d ms of trace absent. NOT interpolated."
                          % (len(miss) * 100))
        if a.ring_drop_steps:
            print("  ON-BOARD LOSS  the locomotive's ring-drop counter moved %d time(s):"
                  % a.ring_drop_steps)
            print("                 that loss happened before the radio, so a bigger")
            print("                 receive buffer would not have saved it.")
        print("  tick gaps      %d samples not within 750-1250 us of their predecessor"
              " (%d flagged late by the locomotive)" % (len(a.tick_gaps), a.late))
        for seq, dt, sat in a.tick_gaps[:10]:
            print("                 sample %d: dt %d us%s" % (seq, dt, "  SATURATED (>= 65.5 ms)" if sat else ""))
        if len(a.tick_gaps) > 10:
            print("                 ... and %d more" % (len(a.tick_gaps) - 10))
        if a.status:
            s = a.status[-1]
            print("  last status    %.1f Hz measured on board, ring high water %d/%d,"
                  % (s["measured_hz"], s["ring_high_water"], 40))
            print("                 ring drops %d, ruling drops %d, udp failures %d"
                  % (s["cum_sample_ring_drops"], s["cum_ruling_ring_drops"],
                     s["cum_udp_failures"]))
            print("                 worst tick gap %d us, worst tick body %d us"
                  % (s["max_gap_us"], s["max_tick_body_us"]))
            print("                 free heap %d B, hall stack free %d B, net stack free %d B"
                  % (s["free_heap"], s["hall_stack_free_bytes"], s["net_stack_free_bytes"]))
            print("                 rssi %d dBm, mqtt %s" % (s["rssi"], "up" if s["mqtt"] else "DOWN"))
        if a.rulings:
            adv = sum(1 for r in a.rulings if r["ruling"] == "ADVANCED")
            print("  rulings        %d (%d advanced)" % (len(a.rulings), adv))
            for r in a.rulings:
                if r["ruling"] not in ("ADVANCED",):
                    print("                 %-12s mm %s pol %s peak %d dur %d ms sample %d"
                          % (r["ruling"],
                             "?" if r["nav_mm_after"] == F.MM_NA else r["nav_mm_after"],
                             r["polarity"], r["peak"], r["duration_ms"], r["sample_seq"]))
    if bad:
        print("")
        print("UNREADABLE DATAGRAMS: %d" % len(bad))
        for fr, _us, ln, why in bad[:20]:
            print("  frame %d (%d bytes): %s" % (fr, ln, why))
        if len(bad) > 20:
            print("  ... and %d more" % (len(bad) - 20))


def write_csv(path, out, want_session):
    n = 0
    with open(out, "w") as fh:
        fh.write("session,sample_seq,t_ms,t_us,dt_us,gap_before,raw,pwm_actual,"
                 "pwm_commanded,dir,estop,auto,may_adapt,passage_open,late,"
                 "baseline,nav_mm,nav_dir,st_phase\n")
        expect = {}
        for _recv_us, data in F.iter_capture(path):
            try:
                hdr, payload = F.parse_record(data)
            except F.BadRecord:
                continue
            if hdr.rec_type != F.REC_SAMPLES:
                continue
            if want_session is not None and hdr.session_id != want_session:
                continue
            for s in F.iter_samples(hdr, payload):
                exp = expect.get(hdr.session_id)
                # gap_before is the number of samples known to be missing
                # immediately before this one. A plotting tool that honours it
                # cannot draw a line across evidence that was never received.
                gap = 0 if exp is None else max(0, s["sample_seq"] - exp)
                expect[hdr.session_id] = s["sample_seq"] + 1
                fh.write("%08X,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%s,%d,%s\n"
                         % (hdr.session_id, s["sample_seq"], hdr.t0_ms, s["t_us"],
                            s["dt_us"], gap, s["raw"], s["pwm_actual"],
                            s["pwm_commanded"], s["dir"], s["estop"], s["auto"],
                            s["may_adapt"], s["passage_open"], s["late"],
                            hdr.baseline,
                            "" if hdr.nav_mm == F.MM_NA else hdr.nav_mm,
                            hdr.nav_dir, hdr.phase_name))
                n += 1
    print("wrote %s (%d samples)" % (out, n))


def write_rulings(path, out, want_session):
    n = 0
    with open(out, "w") as fh:
        fh.write("session,t_ms,sample_seq,ruling,opened_ms,closed_ms,duration_ms,"
                 "peak,polarity,amplitude_ratio,gap_ms,entry_baseline,close_baseline,"
                 "shadow_baseline,is_magnet,nav_mm_before,nav_mm_after,nav_dir,"
                 "st_phase,pwm_actual,post_stop_successor\n")
        for _recv_us, data in F.iter_capture(path):
            try:
                hdr, payload = F.parse_record(data)
            except F.BadRecord:
                continue
            if hdr.rec_type != F.REC_RULING:
                continue
            if want_session is not None and hdr.session_id != want_session:
                continue
            r = F.parse_ruling(payload)
            fh.write("%08X,%d,%d,%s,%d,%d,%d,%d,%s,%.4f,%d,%d,%d,%d,%d,%s,%s,%d,%s,%d,%d\n"
                     % (hdr.session_id, r["t_ms"], r["sample_seq"], r["ruling"],
                        r["opened_ms"], r["closed_ms"], r["duration_ms"], r["peak"],
                        r["polarity"], r["amplitude_ratio"], r["gap_ms"],
                        r["entry_baseline"], r["close_baseline"], r["shadow_baseline"],
                        r["is_magnet"],
                        "" if r["nav_mm_before"] == F.MM_NA else r["nav_mm_before"],
                        "" if r["nav_mm_after"] == F.MM_NA else r["nav_mm_after"],
                        r["nav_dir"], r["st_phase"], r["pwm_actual"],
                        r["post_stop_successor"]))
            n += 1
    print("wrote %s (%d rulings)" % (out, n))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--report", action="store_true", help="audit summary (default)")
    ap.add_argument("--csv", help="write the sample trace here")
    ap.add_argument("--rulings", help="write X18's marker rulings here")
    ap.add_argument("--session", help="restrict to one session id (hex, from --report)")
    args = ap.parse_args()

    want = int(args.session, 16) if args.session else None
    if args.report or not (args.csv or args.rulings):
        audits, bad, frames = decode(args.capture, want)
        report(args.capture, audits, bad, frames)
    if args.csv:
        write_csv(args.capture, args.csv, want)
    if args.rulings:
        write_rulings(args.capture, args.rulings, want)


if __name__ == "__main__":
    main()
