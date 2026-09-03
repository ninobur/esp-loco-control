#!/usr/bin/env python3
"""The magnet-curve database. Runs on the Pi, reads what the locomotives have
already published, writes one row per passage. Nothing here touches a radio,
a serial port, or a locomotive.

    python3 decode_curves.py ingest <telemetry dir> <curves.sqlite>
    python3 decode_curves.py export <curves.sqlite> <passages.txt> [--all]
    python3 decode_curves.py stats  <curves.sqlite>

    on the Pi:   decode_curves.py ingest /home/david/NGR/telemetry /home/david/NGR/curves/curves.sqlite
    on the Mac:  decode_curves.py ingest ~/ngr-telemetry/pi/NGR/telemetry /tmp/curves.sqlite

Decision 0072 (proposed) says what is kept and why. This file says how.

WHAT A ROW IS
  One captured passage, however it reached the log:

  * `wire`  -- NAVI_ONE's diag/waveform: the 40-byte header decoded, the
    int16 samples exactly as the firmware recorded them (oriented, entry-
    baseline-relative, decimated as the header says), the build that
    published it, and the mm/marker event it belongs to.
  * `wave8` -- QUORUM 1.13X's mm/wave (the 2026-08-28 survey): int8 samples
    at a stated scale, oriented the same way. The 187 survey magnets the
    two-sided calibration rests on are these.

  Nothing is smoothed, resampled or reconstructed. What was published is what
  is stored; `quant` says how coarse the copy is (1 for the wire, sc for the
  survey) and `decimation` how many milliseconds each stored sample spans.

THE KEY
  (loco, opened_ms, closed_ms). Both are the locomotive's millis() at open and
  at close, so a passage that is published twice -- the refusal dump
  (slotTotal 1) and again as a slot of the withdraw window -- lands on one
  row, and the second arrival is recorded in `copies`. A different boot would
  have to collide on both to the millisecond to alias; it has not happened in
  any day of the mirror and the ingest reports it if it ever does (a duplicate
  key whose sample count or peak differs).

WHAT THE WIRE DOES NOT CARRY YET
  stitchAt, restLevel and preSamples. They live in the Passage struct since
  2026-09-02 but not in WavHeader; adding them is a gate-5 wire-format change
  (proposed in docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260902.md as an
  8-byte trailer AFTER the samples, so no existing decoder moves). Until then:
    stitch_at, rest_level   NULL
    pre_samples             derived the way extract_passages.py derives it
                            (12 stored points at decimation 1), pre_src='derived'
    splice                  derived only for a passage the marker event says
                            was stitched: the largest step of a median-of-
                            three copy, if >= 30 counts (the arches_fixtures
                            rule). splice_src='derived:medstep30'.
  The trailer is decoded when it is present, and the columns then say 'wire'.

THE OUTCOME CODE CHANGED MEANING AT X9
  Before NAVI_ONE 1.0X9 the wire's outcome byte was 0 MAGNET, 1 TOO_SOON,
  2 TOO_WEAK, 3 WRONG_SHAPE, 4 NO_CURVE. X9 inserted INSUFFICIENT at 4 and
  moved NO_CURVE to 5. The header has no version field, so the name is chosen
  by the build that published it; `outcome_code` keeps the raw byte.

IDEMPOTENT
  The byte offset each log was read to is remembered (with its line number,
  so `src_line` matches a plain text-mode line count) and rolled back to the
  first line of anything still incomplete at the end of a pass -- a slot with
  chunks to come, or a passage published in the last ten seconds whose marker
  event may still be on its way. Every insert is INSERT OR IGNORE on a natural
  key. Run it from a timer every few minutes or once a night; re-runs add
  nothing.

Standard library only: it runs on a stock Pi (checked: Python 3.13, SQLite
3.46 on ngr-pi, 2026-09-02).
"""
import base64, collections, datetime, glob, json, os, sqlite3, struct, sys

HDR = "<8B6H2f3I"
HDR_LEN = struct.calcsize(HDR)
assert HDR_LEN == 40
# Proposed trailer AFTER the samples (docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_
# PROPOSAL_20260902.md): magic, stitchAt, restLevel, preSamples, quant, flags,
# seq. quant 1 = int16 samples verbatim; quant q > 1 = int8 samples, each q
# counts. flags bit0 archive, bit1 refusal dump, bit2 window dump. seq counts
# judged passages since boot, so the Pi can see a gap.
TRAILER = "<2sHhHBBH"
TRAILER_LEN = struct.calcsize(TRAILER)
TRAILER_MAGIC = b"S1"

OUTCOME_PRE_X9 = {0: "MAGNET", 1: "TOO_SOON", 2: "TOO_WEAK", 3: "WRONG_SHAPE", 4: "NO_CURVE"}
OUTCOME_X9 = {0: "MAGNET", 1: "TOO_SOON", 2: "TOO_WEAK", 3: "WRONG_SHAPE", 4: "INSUFFICIENT", 5: "NO_CURVE"}
WAVE8_REJ = {0: "ADMITTED", 1: "FLOOR_40MS", 2: "SUB_THRESHOLD"}

MARKER_BEFORE_S = 1800.0    # a withdraw window can hold passages minutes old
MARKER_AFTER_S = 5.0        # a refusal's event follows its dump within ms
HOLD_BACK_S = 10.0          # unmatched and this fresh at end of file: wait

SCHEMA = """
CREATE TABLE IF NOT EXISTS passages (
  loco          TEXT NOT NULL,
  opened_ms     INTEGER NOT NULL,       -- locomotive millis() at open
  closed_ms     INTEGER NOT NULL,       -- ... and at close
  source        TEXT NOT NULL,          -- 'wire' (diag/waveform) or 'wave8' (mm/wave)
  ts            TEXT NOT NULL,          -- wall clock of the first arrival, ISO
  boot          TEXT,                   -- ts of the last state/bootid before it
  build         TEXT,                   -- sketch name from that bootid
  subtitle      TEXT,                   -- BUILD_SUBTITLE, from X11 on
  build_class   TEXT,
  copies        INTEGER NOT NULL DEFAULT 1,
  slot_index    INTEGER, slot_total INTEGER,   -- of the first arrival
  outcome       TEXT, outcome_code INTEGER,
  is_magnet     INTEGER, shape_tested INTEGER, truncated INTEGER,
  polarity      TEXT,                   -- 'N' or 'S'
  n             INTEGER NOT NULL,       -- samples stored
  decimation    INTEGER NOT NULL,       -- ms per stored sample
  quant         INTEGER NOT NULL,       -- counts per LSB of the stored copy
  pre_samples   INTEGER, pre_src TEXT,
  peak          INTEGER, gain INTEGER, ratio REAL, resid REAL, gap_ms INTEGER,
  stitch_at     INTEGER,                -- first sample of the departure; NULL until the wire carries it
  rest_level    INTEGER,                -- level rested at during a pause; NULL until the wire carries it
  splice        INTEGER, splice_src TEXT,
  marker_ts     TEXT,                   -- the mm/marker event this passage belongs to
  mm INTEGER, tgt INTEGER, dir TEXT,
  event TEXT, ruling TEXT, why TEXT, event2 TEXT,
  ev_resid REAL, two_sided INTEGER, trunk INTEGER, why2 TEXT,
  stitched INTEGER, paused_ms INTEGER, trust TEXT, obs TEXT, expected TEXT,
  samples       BLOB NOT NULL,          -- little-endian int16, n of them, oriented
  extra         TEXT,                   -- JSON: whatever else the record carried
  src_file      TEXT NOT NULL, src_line INTEGER NOT NULL,
  PRIMARY KEY (loco, opened_ms, closed_ms)
);
CREATE INDEX IF NOT EXISTS ix_passages_ts   ON passages(ts);
CREATE INDEX IF NOT EXISTS ix_passages_mm   ON passages(loco, mm, dir);
CREATE INDEX IF NOT EXISTS ix_passages_out  ON passages(outcome, stitched);
CREATE INDEX IF NOT EXISTS ix_passages_src  ON passages(src_file, src_line);

-- every arrival of a passage, so 'copies' is a count of rows here
CREATE TABLE IF NOT EXISTS copies (
  src_file TEXT NOT NULL, src_line INTEGER NOT NULL,
  loco TEXT NOT NULL, opened_ms INTEGER NOT NULL, closed_ms INTEGER NOT NULL,
  ts TEXT NOT NULL, slot_index INTEGER, slot_total INTEGER,
  PRIMARY KEY (src_file, src_line)
);

-- every mm/marker event, published waveform or not: the accepted stitched arc
-- of 2026-09-02 11:03:54 has a row here and none in passages
CREATE TABLE IF NOT EXISTS markers (
  loco TEXT NOT NULL, ts TEXT NOT NULL, event TEXT NOT NULL,
  mm INTEGER, tgt INTEGER, dir TEXT, ruling TEXT, why TEXT,
  peak INTEGER, resid REAL, gap_ms INTEGER, stitched INTEGER, paused_ms INTEGER,
  two_sided INTEGER, trunk INTEGER, why2 TEXT,
  json TEXT NOT NULL, src_file TEXT NOT NULL, src_line INTEGER NOT NULL,
  PRIMARY KEY (loco, ts, event)
);
CREATE INDEX IF NOT EXISTS ix_markers_match ON markers(loco, peak, ts);

CREATE TABLE IF NOT EXISTS boots (
  loco TEXT NOT NULL, ts TEXT NOT NULL, sketch TEXT, subtitle TEXT,
  build_class TEXT, field_accepted INTEGER, json TEXT NOT NULL,
  PRIMARY KEY (loco, ts)
);

-- lines that carried a waveform and could not be decoded, with the reason
CREATE TABLE IF NOT EXISTS rejects (
  src_file TEXT NOT NULL, src_line INTEGER NOT NULL, ts TEXT, loco TEXT,
  reason TEXT NOT NULL, PRIMARY KEY (src_file, src_line)
);

CREATE TABLE IF NOT EXISTS progress (
  src_file TEXT PRIMARY KEY, offset INTEGER NOT NULL, line INTEGER NOT NULL
);
-- per-loco context that has to survive across files and runs
CREATE TABLE IF NOT EXISTS context (loco TEXT PRIMARY KEY, boot_ts TEXT, boot_json TEXT);
"""


# ---------------------------------------------------------------------------
def parse_ts(ts):
    """'2026-09-02T15:38:01.234' -> seconds since the epoch, local time."""
    return datetime.datetime.fromisoformat(ts[:23]).timestamp()


def build_outcomes(sketch):
    """Which numbering the outcome byte uses, from the build that sent it."""
    if not sketch or not sketch.startswith("NAVI_ONE_1_0X"):
        return OUTCOME_PRE_X9
    digits = ""
    for ch in sketch[len("NAVI_ONE_1_0X"):]:
        if ch.isdigit():
            digits += ch
        else:
            break
    return OUTCOME_X9 if (int(digits) if digits else 1) >= 9 else OUTCOME_PRE_X9


def median3(v):
    m = list(v)
    for i in range(1, len(v) - 1):
        m[i] = sorted(v[i - 1:i + 2])[1]
    return m


def derived_splice(v):
    """arches_fixtures.py's rule: the largest step of the median-of-three copy,
    if it is at least 30 counts. Only meaningful on a record with a join."""
    if len(v) < 3:
        return None
    m = median3(v)
    i = max(range(1, len(m)), key=lambda k: abs(m[k] - m[k - 1]))
    return i if abs(m[i] - m[i - 1]) >= 30 else None


def derived_pre(dec):
    """extract_passages.py's rule: PRE = 12 stored points at decimation 1."""
    return 12 if dec == 1 else max(1, 12 // dec)


def marker_fits(mj, peak, resid, gap):
    if mj.get("peak") != peak:
        return False
    if abs(float(mj.get("resid", -1)) - resid) > 1e-3:
        return False
    if gap is not None and "gap_ms" in mj and mj["gap_ms"] != gap:
        return False
    return True


# ---------------------------------------------------------------------------
class Ingest:
    def __init__(self, db):
        self.db = db
        self.cur = db.cursor()
        self.context = {}            # loco -> (boot_ts, boot_json dict)
        for loco, bts, bj in self.cur.execute("SELECT loco, boot_ts, boot_json FROM context"):
            self.context[loco] = (bts, json.loads(bj) if bj else {})

    # -- per-file state -----------------------------------------------------
    def ingest(self, path):
        name = os.path.basename(path)
        row = self.cur.execute("SELECT offset, line FROM progress WHERE src_file=?", (name,)).fetchone()
        start, line = (row[0], row[1]) if row else (0, 0)
        size = os.path.getsize(path)
        if size < start:
            start, line = 0, 0                      # rewritten: start over
        self.open = {}                              # (loco, st, si, opened) -> slot being assembled
        self.pending = []                           # complete passages waiting for a marker
        self.recent = collections.defaultdict(list) # loco -> [(t, mj)] markers of this pass
        self.added = self.dups = self.markers = self.rej = 0
        self.file = name
        last_ts = None
        end, end_line = start, line
        with open(path, "rb") as f:
            f.seek(start)
            while True:
                pos = f.tell()
                raw = f.readline()
                if not raw or not raw.endswith(b"\n"):
                    break                           # EOF, or a line still being written
                end, end_line = f.tell(), line + 1
                line += 1
                parts = raw[:-1].split(b"\t", 2)
                if len(parts) < 3:
                    continue
                try:
                    ts = parts[0].decode("ascii")
                    topic = parts[1].decode("ascii")
                except UnicodeDecodeError:
                    continue
                tp = topic.split("/")
                if len(tp) < 4 or tp[1] != "loco":
                    continue
                loco, kind = tp[2], "/".join(tp[3:])
                last_ts = ts
                if kind == "state/bootid":
                    self.on_boot(loco, ts, parts[2])
                elif kind == "mm/marker":
                    self.on_marker(loco, ts, parts[2], line)
                elif kind == "diag/waveform":
                    self.on_waveform(loco, ts, parts[2], line, pos)
                elif kind == "mm/wave":
                    self.on_wave8(loco, ts, parts[2], line)
        # anything still open rolls the offset back to its first line
        rollback = [(d["pos"], d["line"] - 1) for d in self.open.values()]
        t_end = parse_ts(last_ts) if last_ts else 0
        for p in self.pending:
            if last_ts and t_end - p["t"] < HOLD_BACK_S:
                rollback.append((p["pos"], p["line"] - 1))
            else:
                self.insert(p, None)
        if rollback:
            end, end_line = min(rollback)
        self.cur.execute("INSERT OR REPLACE INTO progress VALUES (?,?,?)", (name, end, end_line))
        for loco, (bts, bj) in self.context.items():
            self.cur.execute("INSERT OR REPLACE INTO context VALUES (?,?,?)", (loco, bts, json.dumps(bj)))
        self.db.commit()
        return self.added, self.dups, self.markers, self.rej

    # -- handlers -----------------------------------------------------------
    def on_boot(self, loco, ts, payload):
        try:
            j = json.loads(payload)
        except Exception:
            return
        self.context[loco] = (ts, j)
        self.cur.execute("INSERT OR IGNORE INTO boots VALUES (?,?,?,?,?,?,?)",
                         (loco, ts, j.get("sketch"), j.get("subtitle"), j.get("build_class"),
                          j.get("field_accepted"), payload.decode("utf-8", "replace")))

    def on_marker(self, loco, ts, payload, line):
        try:
            j = json.loads(payload)
        except Exception:
            return
        self.cur.execute("INSERT OR IGNORE INTO markers VALUES (?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?, ?,?,?)",
                         (loco, ts, j.get("event", ""), j.get("mm"), j.get("tgt"), j.get("dir"),
                          j.get("ruling"), j.get("why"), j.get("peak"), j.get("resid"), j.get("gap_ms"),
                          j.get("stitched"), j.get("paused_ms"), j.get("two_sided"), j.get("trunk"),
                          j.get("why2"), payload.decode("utf-8", "replace"), self.file, line))
        self.markers += self.cur.rowcount
        t = parse_ts(ts)
        # the second event of a passage already written (NOT_A_MAGNET, then
        # STITCHED_REFUSED 60 ms later): name it on the row
        for t1, ts1, j1 in reversed(self.recent[loco]):
            if t - t1 > 1.0:
                break
            if ts1 != ts and marker_fits(j1, j.get("peak"), float(j.get("resid", -1)), j.get("gap_ms")):
                self.cur.execute("UPDATE passages SET event2=? WHERE loco=? AND marker_ts=? AND event2 IS NULL",
                                 (j.get("event"), loco, ts1))
                break
        self.recent[loco].append((t, ts, j))
        # a passage waiting for exactly this event?
        keep = []
        for p in self.pending:
            if p["loco"] == loco and marker_fits(j, p["peak"], p["resid"], p["gap"]) and t - p["t"] <= MARKER_AFTER_S:
                self.insert(p, (ts, j))
            else:
                keep.append(p)
        self.pending = keep

    def on_waveform(self, loco, ts, payload, line, pos):
        if not payload.lstrip().startswith(b"{"):
            # The runlog wrote the binary raw before 2026-08-31 10:21 and the
            # bytes over 127 were replaced on the way in. Unrecoverable.
            self.reject(line, ts, loco, "raw-era binary, bytes lost to UTF-8 replacement")
            return
        try:
            j = json.loads(payload)
            b = base64.b64decode(j["payload_b64"], validate=True)
        except Exception as e:
            self.reject(line, ts, loco, "payload: %s" % e)
            return
        if len(b) < HDR_LEN:
            self.reject(line, ts, loco, "short: %d bytes" % len(b))
            return
        h = struct.unpack(HDR, b[:HDR_LEN])
        si, st, ci, ct, pol, outc, ism, shp = h[0:8]
        scount, ccount, coff, dec, peak, gain = h[8:14]
        ratio, resid = h[14:16]
        gap, opened, closed = h[16:19]
        body = b[HDR_LEN:]
        trailer, quant, width = None, 1, 2
        if len(body) >= TRAILER_LEN and body[-TRAILER_LEN:-TRAILER_LEN + 2] == TRAILER_MAGIC:
            _, stitch_at, rest_level, pre, quant, flags, seq = struct.unpack(TRAILER, body[-TRAILER_LEN:])
            trailer = (stitch_at, rest_level, pre, quant, flags, seq)
            body = body[:-TRAILER_LEN]
            width = 1 if quant > 1 else 2
        if len(body) < ccount * width:
            self.reject(line, ts, loco, "chunk claims %d samples, %d bytes follow" % (ccount, len(body)))
            return
        key = (loco, st, si, opened)
        d = self.open.get(key)
        if d is None:
            d = self.open[key] = dict(
                loco=loco, ts=ts, t=parse_ts(ts), line=line, pos=pos, source="wire",
                si=si, st=st, pol=pol, outc=outc, ism=ism, shp=shp, n=scount, dec=dec,
                peak=peak, gain=gain, ratio=ratio, resid=resid, gap=gap,
                opened=opened, closed=closed, s={}, trailer=trailer, quant=quant, byte_len=0, chunks=[])
        d["chunks"].append((ci, ct))
        d["byte_len"] += len(b)
        if width == 1:
            vals = [x * quant for x in struct.unpack("<%db" % ccount, body[:ccount])]
        else:
            vals = struct.unpack("<%dh" % ccount, body[:ccount * 2])
        for i, x in enumerate(vals):
            d["s"][coff + i] = x
        if len(d["s"]) < d["n"]:
            return                                  # more chunks to come
        del self.open[key]
        d["v"] = [d["s"][i] for i in sorted(d["s"])]
        del d["s"]
        # the marker event this passage belongs to, if it has already arrived
        ev = self.find_marker(d)
        if ev is not None:
            self.insert(d, ev)
        else:
            self.pending.append(d)

    def on_wave8(self, loco, ts, payload, line):
        try:
            j = json.loads(payload)
            raw = base64.b64decode(j["d"], validate=True)
        except Exception as e:
            self.reject(line, ts, loco, "mm/wave payload: %s" % e)
            return
        sc = j.get("sc") or 1
        v = [(x - 128) * sc for x in raw]
        if j.get("pol") == "S":
            v = [-x for x in v]                     # orient the pole positive
        n = len(v)
        rej, tr = j.get("rej"), j.get("tr")
        t = j.get("t", 0)
        d = dict(loco=loco, ts=ts, t=parse_ts(ts), line=line, source="wave8",
                 si=None, st=None, pol=1 if j.get("pol") == "N" else 0,
                 outc=rej, ism=1 if rej == 0 else (0 if rej is not None else None),
                 shp=0 if tr else 1, n=n, dec=1, peak=j.get("pk"), gain=None,
                 ratio=None, resid=None, gap=None,
                 opened=t, closed=t + (j.get("dur") or 0), v=v, trailer=None,
                 quant=sc, pre=j.get("pre"), truncated=1 if tr else 0,
                 extra=dict(mm=j.get("mm"), t=t, dur=j.get("dur"), pwm=j.get("pwm"),
                            sc=sc, pre=j.get("pre"), tr=tr, clip=j.get("clip"),
                            rej=rej, drop=j.get("drop")))
        self.insert(d, None)

    # -- matching and writing -----------------------------------------------
    def find_marker(self, d):
        """The nearest earlier marker event with this passage's peak and
        residual -- in this pass first, then in the table (a withdraw window
        at the start of a resumed pass refers to events before the offset)."""
        best = None
        for t, ts, mj in reversed(self.recent[d["loco"]]):
            if d["t"] - t > MARKER_BEFORE_S:
                break
            if marker_fits(mj, d["peak"], d["resid"], d["gap"]) and not self.linked(d["loco"], ts):
                best = (t, ts, mj)
                break
        if best is None:
            t0 = datetime.datetime.fromtimestamp(d["t"] - MARKER_BEFORE_S).isoformat(timespec="milliseconds")
            for ts, js in self.cur.execute(
                    "SELECT ts, json FROM markers WHERE loco=? AND peak=? AND ts>=? AND ts<=? ORDER BY ts DESC",
                    (d["loco"], d["peak"], t0, d["ts"])).fetchall():
                mj = json.loads(js)
                if marker_fits(mj, d["peak"], d["resid"], d["gap"]) and not self.linked(d["loco"], ts):
                    best = (parse_ts(ts), ts, mj)
                    break
        return None if best is None else (best[1], best[2])

    def linked(self, loco, marker_ts):
        """A marker event already carried by a passage row belongs to that
        passage; a second waveform with the same peak and residual is a
        different passage still waiting for its own event."""
        return self.cur.execute("SELECT 1 FROM passages WHERE loco=? AND marker_ts=? LIMIT 1",
                                (loco, marker_ts)).fetchone() is not None

    def second_event(self, d, ev):
        """The same passage can raise two events a few ms apart (NOT_A_MAGNET,
        then STITCHED_REFUSED). Keep the second's name."""
        if ev is None:
            return None
        ts, mj = ev
        t = parse_ts(ts)
        for t2, ts2, mj2 in self.recent[d["loco"]]:
            if ts2 != ts and abs(t2 - t) <= 1.0 and marker_fits(mj2, d["peak"], d["resid"], d["gap"]):
                return mj2.get("event")
        return None

    def insert(self, d, ev):
        loco = d["loco"]
        boot_ts, bj = self.context.get(loco, (None, {}))
        sketch = bj.get("sketch")
        v = d["v"]
        mj = ev[1] if ev else {}
        marker_ts = ev[0] if ev else None
        if d["source"] == "wire":
            outcome = build_outcomes(sketch).get(d["outc"], "CODE_%d" % d["outc"])
            quant, truncated = d["quant"], None
            if d["trailer"]:
                stitch_at, rest_level, pre = d["trailer"][:3]
                pre_src = "wire"
                extra_flags = d["trailer"][4]
            else:
                stitch_at = rest_level = None
                pre, pre_src = derived_pre(d["dec"]), "derived"
            extra = dict(byte_len=d["byte_len"], chunks=len(d["chunks"]))
            if d["trailer"]:
                extra["flags"], extra["seq"] = extra_flags, d["trailer"][5]
        else:
            outcome = WAVE8_REJ.get(d["outc"], None if d["outc"] is None else "REJ_%d" % d["outc"])
            quant, truncated = d["quant"], d["truncated"]
            stitch_at = rest_level = None
            pre, pre_src = d["pre"], "wire"
            extra = d["extra"]
        stitched = mj.get("stitched")
        if stitch_at is not None:
            splice, splice_src = (stitch_at if stitch_at else None), "wire"
        elif stitched:
            splice, splice_src = derived_splice(v), "derived:medstep30"
        else:
            splice, splice_src = None, None
        try:
            self.cur.execute("""INSERT INTO passages VALUES (
                ?,?,?,?,?,?,?,?,?, ?,?,?, ?,?, ?,?,?, ?, ?,?,?, ?,?, ?,?,?,?,?, ?,?, ?,?,
                ?, ?,?,?, ?,?,?,?, ?,?,?,?, ?,?,?,?,?, ?, ?, ?,?)""",
                (loco, d["opened"], d["closed"], d["source"], d["ts"], boot_ts, sketch,
                 bj.get("subtitle"), bj.get("build_class"),
                 1, d["si"], d["st"],
                 outcome, d["outc"],
                 d["ism"], d["shp"], truncated,
                 "N" if d["pol"] else "S",
                 len(v), d["dec"], quant,
                 pre, pre_src,
                 d["peak"], d["gain"], d["ratio"], d["resid"], d["gap"],
                 stitch_at, rest_level,
                 splice, splice_src,
                 marker_ts,
                 mj.get("mm"), mj.get("tgt"), mj.get("dir"),
                 mj.get("event"), mj.get("ruling"), mj.get("why"), self.second_event(d, ev),
                 mj.get("resid"), mj.get("two_sided"), mj.get("trunk"), mj.get("why2"),
                 stitched, mj.get("paused_ms"), mj.get("trust"), mj.get("obs"), mj.get("expected"),
                 struct.pack("<%dh" % len(v), *v),
                 json.dumps(extra, separators=(",", ":")),
                 self.file, d["line"]))
            self.added += 1
        except sqlite3.IntegrityError:
            self.dups += 1
            # already there: the refusal dump and the window copy, or a re-read
            old = self.cur.execute("SELECT n, peak, src_file, src_line FROM passages WHERE loco=? AND opened_ms=? AND closed_ms=?",
                                   (loco, d["opened"], d["closed"])).fetchone()
            if old and (old[0] != len(v) or old[1] != d["peak"]):
                print("  KEY COLLISION %s opened %d closed %d: stored n=%d peak=%d, %s:%d has n=%d peak=%d"
                      % (loco, d["opened"], d["closed"], old[0], old[1], self.file, d["line"], len(v), d["peak"]),
                      file=sys.stderr)
        self.cur.execute("INSERT OR IGNORE INTO copies VALUES (?,?,?,?,?,?,?,?)",
                         (self.file, d["line"], loco, d["opened"], d["closed"], d["ts"], d["si"], d["st"]))
        if self.cur.rowcount:
            self.cur.execute("UPDATE passages SET copies=(SELECT COUNT(*) FROM copies WHERE loco=? AND opened_ms=? AND closed_ms=?) "
                             "WHERE loco=? AND opened_ms=? AND closed_ms=?",
                             (loco, d["opened"], d["closed"], loco, d["opened"], d["closed"]))

    def reject(self, line, ts, loco, reason):
        self.cur.execute("INSERT OR IGNORE INTO rejects VALUES (?,?,?,?,?)", (self.file, line, ts, loco, reason))
        self.rej += self.cur.rowcount


# ---------------------------------------------------------------------------
def cmd_ingest(tdir, dbpath):
    os.makedirs(os.path.dirname(os.path.abspath(dbpath)), exist_ok=True)
    db = sqlite3.connect(dbpath)
    db.executescript(SCHEMA)
    ing = Ingest(db)
    tot = [0, 0, 0, 0]
    for path in sorted(glob.glob(os.path.join(tdir, "all_*.log"))):
        a, du, m, r = ing.ingest(path)
        for i, x in enumerate((a, du, m, r)):
            tot[i] += x
        if a or m or r:
            print("  %-20s +%d passages  +%d markers  +%d rejects" % (os.path.basename(path), a, m, r))
    n = db.execute("SELECT COUNT(*) FROM passages").fetchone()[0]
    print("added %d passages; the database holds %d" % (tot[0], n))
    db.close()


def cmd_stats(dbpath):
    db = sqlite3.connect(dbpath)
    print("passages by build / source / outcome / stitched:")
    for r in db.execute("""SELECT COALESCE(build,'?'), source, COALESCE(outcome,'?'), COALESCE(stitched,0), COUNT(*),
                                  SUM(copies>1), SUM(marker_ts IS NULL AND source='wire')
                           FROM passages GROUP BY 1,2,3,4 ORDER BY 1,2,3,4"""):
        print("  %-28s %-5s %-13s stitched=%d  %4d  (twice: %d, no marker: %d)" % r)
    print("markers: %d   boots: %d   rejects: %d   copies: %d" % tuple(
        db.execute("SELECT (SELECT COUNT(*) FROM markers),(SELECT COUNT(*) FROM boots),"
                   "(SELECT COUNT(*) FROM rejects),(SELECT COUNT(*) FROM copies)").fetchone()))
    for r in db.execute("SELECT src_file, COUNT(*), MIN(reason) FROM rejects GROUP BY 1"):
        print("  rejects %s: %d  e.g. %s" % r)
    for r in db.execute("SELECT src_file, offset, line FROM progress ORDER BY 1"):
        print("  progress %-20s offset %11d  line %8d" % r)


def cmd_export(dbpath, out, everything=False):
    """One passage per line, the format tools/two_sided/*.cpp read:
         <tag> <n> <pre> v0 v1 v2 ...
    Default: the accepted, complete, untruncated passages -- what
    extract_passages.py produced, from the database instead of the raw logs.
    --all: every passage, with the outcome in the tag."""
    db = sqlite3.connect(dbpath)
    where = "" if everything else \
        "WHERE is_magnet=1 AND COALESCE(truncated,0)=0 AND ((source='wire' AND outcome_code=0) OR source='wave8')"
    n = 0
    with open(out, "w") as f:
        for src, sf, sl, pre, samples, extra, outcome, stitched in db.execute(
                "SELECT source, src_file, src_line, pre_samples, samples, extra, outcome, stitched FROM passages "
                + where + " ORDER BY src_file, src_line"):
            v = struct.unpack("<%dh" % (len(samples) // 2), samples)
            day = sf.replace("all_", "").replace(".log", "")
            x = json.loads(extra) if src == "wave8" else {}
            if src == "wave8" and x.get("mm") is not None and x.get("t") is not None:
                tag = "survey_mm%d_t%d" % (x["mm"], x["t"])
            elif src == "wave8":
                tag = "wave8_%s_L%d" % (day, sl)
            else:
                tag = "telem_%s_L%d" % (day, sl)
            if everything:
                tag += "_%s%s" % (outcome, "_stitched" if stitched else "")
            f.write("%s %d %d %s\n" % (tag, len(v), pre if pre is not None else 12, " ".join(str(s) for s in v)))
            n += 1
    print("%d passages -> %s" % (n, out), file=sys.stderr)


def main():
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == "ingest":
        cmd_ingest(a[1], a[2])
    elif len(a) >= 3 and a[0] == "export":
        cmd_export(a[1], a[2], "--all" in a[3:])
    elif len(a) >= 2 and a[0] == "stats":
        cmd_stats(a[1])
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
