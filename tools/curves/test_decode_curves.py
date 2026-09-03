#!/usr/bin/env python3
"""Tests for decode_curves.py -- the failure cases the mirror does not contain.

    python3 tools/curves/test_decode_curves.py

Standard library only, no network, no Pi, no locomotive. Every log below is
synthesised: the mirror proves the decoder reads the real railway, and these
prove what it does when the railway misbehaves. Written 2026-09-03 after a
review found that the mirror runs were reported results rather than repeatable
tests.
"""
import base64, json, os, sqlite3, struct, sys, tempfile, unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decode_curves as dc


# --- building a synthetic log ----------------------------------------------
def arc(n=80, peak=200, centre=None):
    c = centre if centre is not None else n // 2
    return [int(peak * 2.718281828 ** (-((i - c) / (n / 6.0)) ** 2)) for i in range(n)]


def wave_frames(v, opened, closed, per_chunk=None, outcome=0, is_magnet=1,
                peak=None, resid=0.0710, gain=190, ratio=1.05, gap=0,
                polarity=1, slot_total=1, slot_index=0, trailer=None, quant=1):
    """The real wire format: one or more chunks, header + samples (+ trailer)."""
    n = len(v)
    per = per_chunk or n
    total = max(1, (n + per - 1) // per)
    out = []
    for ci in range(total):
        off = ci * per
        cnt = min(per, n - off)
        h = struct.pack(dc.HDR, slot_index, slot_total, ci, total, polarity, outcome,
                        is_magnet, 1, n, cnt, off, 1, peak if peak is not None else max(v),
                        gain, ratio, resid, gap, opened, closed)
        if quant > 1:
            body = struct.pack("<%db" % cnt, *[round(x / quant) for x in v[off:off + cnt]])
        else:
            body = struct.pack("<%dh" % cnt, *v[off:off + cnt])
        tr = b""
        if trailer is not None:
            tr = struct.pack(dc.TRAILER, dc.TRAILER_MAGIC, trailer.get("boot_id", 0xA1B2C3D4),
                             trailer.get("seq", 0), trailer.get("stitch_at", 0),
                             trailer.get("rest_level", 0), trailer.get("pre", 12),
                             quant, trailer.get("flags", 1))
        out.append(h + body + tr)
    return out


class Log:
    """A telemetry log built line by line, in the runlog's exact format."""

    def __init__(self, loco="9950012", day="20260904"):
        self.loco, self.day, self.lines = loco, day, []
        self.t = 0

    def _ts(self, bump=0.1):
        self.t += bump
        h, rem = divmod(self.t, 3600)
        m, s = divmod(rem, 60)
        return "2026-09-04T%02d:%02d:%06.3f" % (int(h), int(m), s)

    def raw(self, topic, payload, bump=0.1):
        self.lines.append("%s\tngr/loco/%s/%s\t%s" % (self._ts(bump), self.loco, topic, payload))
        return self

    def alert(self, uptime_ms, bump=1.0):
        return self.raw("alert", json.dumps({"level": "UNSET", "reason": "STATUS",
                                             "uptime_ms": uptime_ms}), bump)

    def bootid(self, sketch="NAVI_ONE_1_0X11_FIELDTEST", subtitle="Epiphany", truncate=None):
        p = json.dumps({"sketch": sketch, "subtitle": subtitle,
                        "build_class": "EXPERIMENTAL_FIELD_TEST", "field_accepted": 0,
                        "loco": self.loco, "entry": 38, "exit": 25})
        if truncate:
            p = p[:truncate]
        return self.raw("state/bootid", p)

    def wave(self, frames):
        for b in frames:
            self.raw("diag/waveform", json.dumps({"encoding": "base64", "byte_length": len(b),
                                                  "payload_b64": base64.b64encode(b).decode()}))
        return self

    def meta(self, **kw):
        j = dict(schema="station_curve_v1", phase_mask=1, phase_open="IDLE", phase_close="IDLE",
                 station="", stop_episode=0, stitch_at=0, rest_level=0, pre_samples=12,
                 decimation=1, outcome="MAGNET", is_magnet=1, shape_tested=1)
        j.update(kw)
        return self.raw("diag/wave_meta", json.dumps(j))

    def marker(self, peak, resid=0.0710, gap=0, event="AGREE", **kw):
        j = dict(event=event, mm=5, tgt=6, dir="CW", ruling="ADVANCED", why="MAGNET",
                 peak=peak, resid=resid, gap_ms=gap, stitched=0, paused_ms=0, trust="DECLARED")
        j.update(kw)
        return self.raw("mm/marker", json.dumps(j))

    def settle(self):
        """Let time pass, as a running railway does, so a passage that never
        got a marker ages past the hold-back and is written unmatched."""
        for _ in range(20):
            self.alert(int(self.t * 1000) + 100000, bump=1.0)
        return self

    def write(self, d, name=None):
        p = os.path.join(d, name or ("all_%s.log" % self.day))
        with open(p, "w") as f:
            f.write("\n".join(self.lines) + "\n")
        return p


def ingest(d, db=None):
    db = db or os.path.join(d, "c.sqlite")
    dc.cmd_ingest(d, db)
    return db


def rows(db, sql, args=()):
    c = sqlite3.connect(db)
    try:
        return c.execute(sql).fetchall() if not args else c.execute(sql, args).fetchall()
    finally:
        c.close()


def one(db, sql, args=()):
    return rows(db, sql, args)[0][0]


class Base(unittest.TestCase):
    def setUp(self):
        self._t = tempfile.TemporaryDirectory()
        self.d = self._t.name
        # silence cmd_ingest's progress lines
        self._out, sys.stdout = sys.stdout, open(os.devnull, "w")

    def tearDown(self):
        sys.stdout.close()
        sys.stdout = self._out
        self._t.cleanup()


# ---------------------------------------------------------------------------
class TestIdentity(Base):
    def test_same_times_in_two_boots_are_two_passages(self):
        """THE CRITICAL ONE. millis() restarts at every boot, so two passages
        from different boots can carry identical opened/closed times. They must
        never collapse into one row."""
        v = arc()
        L = Log().alert(1000).bootid().wave(wave_frames(v, 5000, 5200)).marker(max(v))
        L.alert(60000)
        L.alert(1000)                                   # <- reboot: uptime falls
        L.bootid().wave(wave_frames(v, 5000, 5200)).marker(max(v))
        L.alert(60000)
        L.write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 2)
        self.assertEqual(one(db, "SELECT COUNT(DISTINCT boot_id) FROM passages"), 2)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects"), 0)

    def test_millis_rollover_splits_and_never_merges(self):
        """A rollover reads as a boot. That splits one boot in two, which is
        the safe direction: it can never merge two passages into one row."""
        v = arc()
        L = Log().alert(4294900000).wave(wave_frames(v, 4294900100, 4294900300)).marker(max(v))
        L.alert(1200)                                   # wrapped
        L.wave(wave_frames(v, 400, 600)).marker(max(v))
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 2)
        self.assertEqual(one(db, "SELECT COUNT(DISTINCT boot_id) FROM passages"), 2)

    def test_delivery_delay_does_not_invent_a_boot(self):
        """The 2026-09-02 morning: the broker link flaps and alerts arrive late,
        so wall-clock minus uptime wanders by ~50 s. Uptime itself stays
        monotone, so it is still one boot."""
        L = Log().alert(1000)
        for u in (2000, 3000, 4000, 90000, 91000, 92000, 200000):
            L.alert(u, bump=40.0)                       # arrival far behind uptime
        v = arc()
        L.wave(wave_frames(v, 200100, 200300)).marker(max(v))
        L.alert(260000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(DISTINCT boot_id) FROM boots"), 1)

    def test_partial_epoch_is_flagged(self):
        """A log that begins mid-boot cannot prove where the boot started."""
        v = arc()
        L = Log().alert(500000).wave(wave_frames(v, 500100, 500300)).marker(max(v))
        L.alert(560000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT boot_partial FROM passages"), 1)
        self.assertEqual(one(db, "SELECT partial FROM boots"), 1)

    def test_duplicate_sequence_in_one_boot_is_refused(self):
        """(loco, boot_id, passage_seq) is the canonical identity: a firmware
        bug that reissued a number must not pass silently."""
        v = arc()
        L = Log().alert(1000)
        w = arc(peak=150)
        L.wave(wave_frames(v, 1000, 1200, trailer=dict(seq=7)))
        L.marker(max(v))
        L.wave(wave_frames(w, 2000, 2200, gap=1000, trailer=dict(seq=7)))
        L.marker(max(w), gap=1000)
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 1)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects WHERE reason LIKE 'insert refused%'"), 1)


class TestChunks(Base):
    def test_two_chunks_reassemble_in_order(self):
        v = arc(400)
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1400, per_chunk=332)).marker(max(v))
        L.settle().write(self.d)
        db = ingest(self.d)
        blob = one(db, "SELECT samples FROM passages")
        self.assertEqual(list(struct.unpack("<%dh" % len(v), blob)), v)

    def test_identical_duplicate_chunk_is_an_arrival_not_a_row(self):
        v = arc()
        f = wave_frames(v, 1000, 1200)
        L = Log().alert(1000).wave(f).wave(f).marker(max(v))
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 1)
        self.assertEqual(one(db, "SELECT copies FROM passages"), 2)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects"), 0)

    def test_conflicting_duplicate_chunk_is_refused(self):
        v = arc()
        f = wave_frames(v, 1000, 1200, per_chunk=40)
        bad = wave_frames([x + 1 for x in v], 1000, 1200, per_chunk=40,
                          peak=max(v), resid=0.0710, gain=190, ratio=1.05)
        L = Log().alert(1000)
        L.wave([f[0], bad[0]])                          # same chunk index, different bytes
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects WHERE reason LIKE '%different content%'"), 1)

    def test_missing_chunk_never_becomes_a_passage(self):
        v = arc(400)
        f = wave_frames(v, 1000, 1400, per_chunk=332)
        L = Log().alert(1000).wave([f[0]]).settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)

    def test_chunk_index_beyond_total_is_refused(self):
        v = arc()
        b = bytearray(wave_frames(v, 1000, 1200)[0])
        b[2] = 3                                        # chunkIndex 3 of chunkTotal 1
        L = Log().alert(1000).wave([bytes(b)]).alert(60000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)
        self.assertIn("chunk 3 of 1", one(db, "SELECT reason FROM rejects"))

    def test_chunk_offset_past_the_end_is_refused(self):
        v = arc()
        h = struct.pack(dc.HDR, 0, 1, 0, 1, 1, 0, 1, 1, len(v), len(v), 40, 1,
                        max(v), 190, 1.05, 0.071, 0, 1000, 1200)
        b = h + struct.pack("<%dh" % len(v), *v)        # coff 40 + 80 > 80
        L = Log().alert(1000).wave([b]).alert(60000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)
        self.assertIn("chunk covers", one(db, "SELECT reason FROM rejects"))

    def test_body_shorter_than_the_header_claims_is_refused(self):
        v = arc()
        b = wave_frames(v, 1000, 1200)[0][:-20]
        L = Log().alert(1000).wave([b]).alert(60000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)
        self.assertIn("B follow", one(db, "SELECT reason FROM rejects"))

    def test_chunks_that_disagree_about_the_slot_are_refused(self):
        v = arc(400)
        f = wave_frames(v, 1000, 1400, per_chunk=332)
        b = bytearray(f[1])
        struct.pack_into("<H", b, 16, 999)              # peakCounts, at byte 16
        L = Log().alert(1000).wave([f[0], bytes(b)]).alert(60000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)
        self.assertIn("disagrees", one(db, "SELECT reason FROM rejects"))

    def test_byte_length_disagreeing_with_the_payload_is_refused(self):
        v = arc()
        b = wave_frames(v, 1000, 1200)[0]
        L = Log().alert(1000)
        L.raw("diag/waveform", json.dumps({"byte_length": len(b) + 5,
                                           "payload_b64": base64.b64encode(b).decode()}))
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertIn("byte_length", one(db, "SELECT reason FROM rejects"))

    def test_closed_before_opened_is_refused(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 5000, 4000)).settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)


class TestTrailer(Base):
    def test_int16_trailer_round_trips(self):
        v = arc()
        L = Log().alert(1000)
        L.wave(wave_frames(v, 1000, 1200, trailer=dict(stitch_at=33, rest_level=-7, pre=12, seq=4)))
        L.marker(max(v)).alert(60000).write(self.d)
        db = ingest(self.d)
        r = rows(db, "SELECT stitch_at, stitch_src, rest_level, rest_src, pre_samples,"
                     " pre_src, splice, splice_src, passage_seq FROM passages")[0]
        self.assertEqual(r, (33, "wire", -7, "wire", 12, "wire", 33, "wire", 4))

    def test_int8_trailer_widens_by_its_scale(self):
        v = arc()
        L = Log().alert(1000)
        L.wave(wave_frames(v, 1000, 1200, quant=2, trailer=dict(seq=1)))
        L.marker(max(v)).alert(60000).write(self.d)
        db = ingest(self.d)
        blob, q = rows(db, "SELECT samples, quant FROM passages")[0]
        got = list(struct.unpack("<%dh" % len(v), blob))
        self.assertEqual(q, 2)
        self.assertLessEqual(max(abs(a - b) for a, b in zip(got, v)), 1)

    def test_illegal_quant_is_refused(self):
        v = arc()
        b = bytearray(wave_frames(v, 1000, 1200, trailer=dict(seq=1))[0])
        b[-2] = 200                                     # quant 200 > MAX_QUANT
        L = Log().alert(1000).wave([bytes(b)]).alert(60000).write(self.d)
        db = ingest(self.d)
        self.assertIn("quant", one(db, "SELECT reason FROM rejects"))


class TestMarkers(Base):
    def test_two_events_that_both_fit_attach_to_neither(self):
        """Never choose silently. Two identical events, one passage: the row is
        written, flagged ambiguous, and carries no marker."""
        v = arc()
        L = Log().alert(1000)
        L.marker(max(v), gap=0)
        L.marker(max(v), gap=0)                         # indistinguishable
        L.wave(wave_frames(v, 1000, 1200))
        L.settle().write(self.d)
        db = ingest(self.d)
        r = rows(db, "SELECT match_ambiguous, match_candidates, marker_ts, mm FROM passages")[0]
        self.assertEqual((r[0], r[1], r[2], r[3]), (1, 2, None, None))

    def test_one_event_fitting_two_passages_is_given_to_neither(self):
        """The mirror of the case above. Two DIFFERENT passages that look
        identical and one event between them: attaching it to whichever was
        written first would be a silent guess, so neither takes it and both
        record the ambiguity."""
        v = arc()
        L = Log().alert(1000)
        L.wave(wave_frames(v, 1000, 1200))
        L.wave(wave_frames(v, 3000, 3200))
        L.marker(max(v))
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 2)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages WHERE marker_ts IS NOT NULL"), 0)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages WHERE match_ambiguous"), 2)

    def test_the_second_event_of_a_passage_is_recorded(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200, outcome=3, is_magnet=0, resid=0.1967))
        L.marker(max(v), resid=0.1967, event="NOT_A_MAGNET", why="WRONG_SHAPE", stitched=1)
        L.marker(max(v), resid=0.1967, event="STITCHED_REFUSED", stitched=1)
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(rows(db, "SELECT event, event2 FROM passages")[0],
                         ("NOT_A_MAGNET", "STITCHED_REFUSED"))

    def test_match_method_and_confidence_are_recorded(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200)).marker(max(v))
        L.settle().write(self.d)
        db = ingest(self.d)
        m, c = rows(db, "SELECT match_method, match_confidence FROM passages")[0]
        self.assertEqual(m, "heuristic")
        self.assertGreater(c, 0.99)


class TestMeta(Base):
    def test_meta_after_the_waveform(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200)).marker(max(v))
        L.meta(seq=9, open_ms=1000, close_ms=1200, phase_open="RAMP", phase_close="DWELL",
               phase_mask=24, station="Arches", stitch_at=41, rest_level=118, stop_episode=1)
        L.settle().write(self.d)
        db = ingest(self.d)
        r = rows(db, "SELECT passage_seq, phase_open, phase_close, station, stitch_at,"
                     " stitch_src, rest_level, splice, splice_src, stop_episode FROM passages")[0]
        self.assertEqual(r, (9, "RAMP", "DWELL", "Arches", 41, "wire", 118, 41, "wire", 1))

    def test_meta_before_the_waveform(self):
        v = arc()
        L = Log().alert(1000)
        L.meta(seq=3, open_ms=1000, close_ms=1200, station="Bamboo", stitch_at=17)
        L.wave(wave_frames(v, 1000, 1200)).marker(max(v))
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(rows(db, "SELECT passage_seq, station, stitch_at FROM passages")[0],
                         (3, "Bamboo", 17))

    def test_meta_without_a_waveform_is_still_kept(self):
        L = Log().alert(1000)
        L.meta(seq=1, open_ms=10, close_ms=20)
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM wave_meta"), 1)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 0)


class TestMalformed(Base):
    def test_truncated_bootid_is_salvaged_and_flagged(self):
        """NAVI_ONE 1.0X11's bootid is cut at 399 bytes by the char b[400]
        buffer, so json.loads throws. The build must not be lost."""
        v = arc()
        L = Log().alert(1000).bootid(truncate=60)
        L.wave(wave_frames(v, 1000, 1200)).marker(max(v)).alert(60000).write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT build FROM passages"), "NAVI_ONE_1_0X11_FIELDTEST")
        self.assertEqual(one(db, "SELECT bootid_truncated FROM boots"), 1)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects WHERE topic='state/bootid'"), 1)

    def test_malformed_json_is_recorded_not_swallowed(self):
        L = Log().alert(1000)
        L.raw("diag/waveform", "{not json")
        L.raw("mm/marker", "{also not json")
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects"), 2)

    def test_raw_era_binary_is_recorded_as_lost(self):
        L = Log().alert(1000)
        L.raw("diag/waveform", "\x00\x06\x00\x01 not base64 at all")
        L.settle().write(self.d)
        db = ingest(self.d)
        self.assertIn("raw-era", one(db, "SELECT reason FROM rejects"))

    def test_a_line_still_being_written_is_not_read(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200)).marker(max(v)).alert(60000)
        p = L.write(self.d)
        with open(p, "a") as f:
            f.write("2026-09-04T00:02:00.000\tngr/loco/9950012/diag/wave")   # no newline
        db = ingest(self.d)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM passages"), 1)
        self.assertEqual(one(db, "SELECT COUNT(*) FROM rejects"), 0)


class TestIncremental(Base):
    def test_rerun_adds_nothing(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200)).marker(max(v)).alert(60000)
        L.write(self.d)
        db = ingest(self.d)
        before = rows(db, "SELECT * FROM passages")
        ingest(self.d, db)
        self.assertEqual(rows(db, "SELECT * FROM passages"), before)

    def test_growing_log_equals_one_shot(self):
        """The Pi appends while the timer runs. Reading half a log, then the
        rest, must give exactly what reading it once gives."""
        v = arc()
        L = Log().alert(1000)
        L.wave(wave_frames(v, 1000, 1200)).marker(max(v)).alert(30000)
        L.wave(wave_frames(v, 5000, 5200)).marker(max(v), gap=3800).alert(60000)
        full = "\n".join(L.lines) + "\n"
        half = full[:full.index("\n", len(full) // 2) + 1]
        d2 = os.path.join(self.d, "b")
        os.makedirs(d2)
        with open(os.path.join(self.d, "all_20260904.log"), "w") as f:
            f.write(half)
        db = ingest(self.d)
        with open(os.path.join(self.d, "all_20260904.log"), "w") as f:
            f.write(full)
        ingest(self.d, db)
        with open(os.path.join(d2, "all_20260904.log"), "w") as f:
            f.write(full)
        db2 = ingest(d2)
        cols = ("loco, boot_id, source, opened_ms, closed_ms, passage_seq, n, peak,"
                " marker_ts, match_method, match_ambiguous, copies, splice, splice_src")
        self.assertEqual(rows(db, "SELECT %s FROM passages ORDER BY opened_ms" % cols),
                         rows(db2, "SELECT %s FROM passages ORDER BY opened_ms" % cols))

    def test_a_rewritten_shorter_log_is_reread(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200)).marker(max(v)).alert(60000)
        L.write(self.d)
        db = ingest(self.d)
        Log().alert(1000).write(self.d)                 # truncated/rotated in place
        ingest(self.d, db)
        self.assertEqual(one(db, "SELECT offset FROM progress"),
                         os.path.getsize(os.path.join(self.d, "all_20260904.log")))


class TestExport(Base):
    def test_export_is_the_line_format_the_cpp_tools_read(self):
        v = arc()
        L = Log().alert(1000).wave(wave_frames(v, 1000, 1200)).marker(max(v))
        L.wave(wave_frames(v, 3000, 3200, gap=1800, outcome=3, is_magnet=0, resid=0.2))
        L.marker(max(v), resid=0.2, gap=1800, event="NOT_A_MAGNET", why="WRONG_SHAPE")
        L.settle().write(self.d)
        db = ingest(self.d)
        out = os.path.join(self.d, "p.txt")
        dc.cmd_export(db, out)
        lines = open(out).read().strip().split("\n")
        self.assertEqual(len(lines), 1)                 # the refusal is not exported
        f = lines[0].split()
        self.assertEqual(int(f[1]), len(v))
        self.assertEqual([int(x) for x in f[3:]], v)
        dc.cmd_export(db, out, True)
        self.assertEqual(len(open(out).read().strip().split("\n")), 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
