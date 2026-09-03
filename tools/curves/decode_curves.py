#!/usr/bin/env python3
"""The magnet-curve database. Runs on the Pi, reads what the locomotives have
already published, writes one row per passage. Nothing here touches a radio,
a serial port, or a locomotive.

    python3 decode_curves.py ingest <telemetry dir> <curves.sqlite>
    python3 decode_curves.py export <curves.sqlite> <passages.txt> [--all]
    python3 decode_curves.py stats  <curves.sqlite>
    python3 decode_curves.py audit  <curves.sqlite>

    on the Pi:   decode_curves.py ingest /home/david/NGR/telemetry /home/david/NGR/curves/curves.sqlite
    on the Mac:  decode_curves.py ingest ~/ngr-telemetry/pi/NGR/telemetry /tmp/curves.sqlite

Decision 0072 (proposed) says what is kept and why. This file says how.

WHAT A ROW IS
  One captured passage, however it reached the log:

  * `wire`  -- NAVI_ONE's diag/waveform: the 40-byte header decoded and the
    int16 samples exactly as the firmware recorded them (oriented, entry-
    baseline-relative, decimated as the header says).
  * `wave8` -- QUORUM 1.13X's mm/wave (the 2026-08-28 survey): int8 samples
    at a stated scale, oriented the same way. The 187 survey magnets the
    two-sided calibration rests on are these.

  A `wire` row is enriched, when they exist, by:

  * diag/wave_meta -- NAVI_ONE_STATION_CURVES_0_1's companion JSON, which
    carries the passage's own sequence number, its station phase at open and
    at close, the station and marker offset, commanded and actual PWM, and
    stitchAt / restLevel / preSamples, which the binary header does not have.
  * mm/marker -- what the navigator did with the recognizer's answer.

  Nothing is smoothed, resampled or reconstructed. What was published is what
  is stored; `quant` says how coarse the copy is and `decimation` how many
  milliseconds each stored sample spans.

IDENTITY IS BOOT-SCOPED
  openedAtMs and closedAtMs are millis() values that restart at every boot, so
  they cannot identify a passage on their own. The key is

      (loco, boot_id, source, opened_ms, closed_ms)

  and where a passage carries its own sequence number, (loco, boot_id,
  passage_seq) is enforced UNIQUE on top of it -- the canonical identity the
  firmware should eventually put on the wire.

  There is no boot id on the wire today. state/bootid is published once in
  setup() but RETAINED, so the broker replays it to every reconnecting
  subscriber: a bootid line in the log is not proof of a boot, and older
  QUORUM firmware republished it on every connection. Its payload names the
  build, not the boot.

  So the boot is derived from the one signal that is dense, prompt and
  monotone: `alert` carries `uptime_ms` about once a second, in every build
  era and on both locomotives (checked: 100% of 35,689 alerts on 2026-08-28,
  of 21,625 on 2026-08-12, of 10,954 on 2026-09-03). A boot boundary is
  uptime going BACKWARDS by at least a second. Nothing else is used: the
  wall-clock-minus-uptime offset drifts by up to 50 s when the broker link
  flaps and delivery is delayed, which would split the morning of 2026-09-02
  into 46 boots that did not happen.

  A passage is attributed to the boot that was current AT ITS ARRIVAL, which
  is correct by construction: the device published it, so the boot that was
  running then is the boot that produced it. The boundary is detected at the
  first alert of the new boot, within about a second of it, and a locomotive
  that has just booted has judged nothing yet.

  `boot_src` says how the boot was established and `boot_partial` marks an
  epoch joined in progress (its first observed uptime is not ~1000 ms), so an
  uncertain attribution is never silent. A millis() rollover at 49.7 days
  would read as a new boot; that splits one boot in two and never merges two,
  which is the safe direction, and no uptime in the records exceeds 2 hours.

MARKER ASSOCIATION IS NEVER A SILENT GUESS
  `match_method` is 'seq' when the passage and the event share an identity
  (not on the wire yet), 'heuristic' when it was matched on peak, residual
  and gap within a window, and NULL when nothing matched. When more than one
  event fits, NOTHING is attached: `match_ambiguous` is 1, `match_candidates`
  counts them, and their timestamps go in `extra`. A marker already carried by
  a passage is never offered to a second one.

WHAT THE BINARY WIRE STILL DOES NOT CARRY
  stitchAt, restLevel, preSamples and the sequence number. wave_meta carries
  all four for the station-curves sketch; for every other build they are
  derived and labelled:
    pre_samples   'derived' -- 12 stored points at decimation 1, the rule
                  extract_passages.py uses
    splice        'derived:medstep30' -- the largest step of a median-of-three
                  copy if it is at least 30 counts, and ONLY on a passage the
                  marker event says was stitched (the arches_fixtures rule)
  A trailer carrying them on the binary wire is proposed, not built; it is
  decoded when present and the columns then say 'wire'.

THE OUTCOME CODE CHANGED MEANING AT X9
  Before NAVI_ONE 1.0X9 the wire's outcome byte was 0 MAGNET, 1 TOO_SOON,
  2 TOO_WEAK, 3 WRONG_SHAPE, 4 NO_CURVE. X9 inserted INSUFFICIENT at 4 and
  moved NO_CURVE to 5. The header has no version field, so the name is chosen
  by the build that published it; `outcome_code` keeps the raw byte.

IDEMPOTENT
  The byte offset each log was read to is remembered and rolled back to the
  first line of anything still incomplete -- a slot with chunks to come, or a
  passage published in the last ten seconds whose marker may still be on its
  way. Re-runs add nothing; an incremental run equals a one-shot run.

Standard library only: it runs on a stock Pi (checked: Python 3.13, SQLite
3.46 on ngr-pi). Tests: tools/curves/test_decode_curves.py
"""
import base64, collections, datetime, glob, json, os, re, sqlite3, struct, sys

HDR = "<8B6H2f3I"
HDR_LEN = struct.calcsize(HDR)
assert HDR_LEN == 40
# PROPOSED trailer after the samples (decision 0075, NOT BUILT): magic, bootId,
# passageSeq, stitchAt, restLevel, preSamples, quant, flags -- 16 bytes.
# quant 1 = int16 verbatim; quant q > 1 = int8, each unit q counts. flags bit0
# archive, bit1 refusal dump, bit2 window dump. bootId is generated at boot, so
# a passage carrying one needs no derived boot at all.
TRAILER = "<2sIHHhHBB"
TRAILER_LEN = struct.calcsize(TRAILER)
TRAILER_MAGIC = b"S1"
MAX_QUANT = 64

OUTCOME_PRE_X9 = {0: "MAGNET", 1: "TOO_SOON", 2: "TOO_WEAK", 3: "WRONG_SHAPE", 4: "NO_CURVE"}
OUTCOME_X9 = {0: "MAGNET", 1: "TOO_SOON", 2: "TOO_WEAK", 3: "WRONG_SHAPE", 4: "INSUFFICIENT", 5: "NO_CURVE"}
WAVE8_REJ = {0: "ADMITTED", 1: "FLOOR_40MS", 2: "SUB_THRESHOLD"}
# StPhase in Stations.h, one bit each, as station_curve_v1 publishes them
PHASES = ["IDLE", "APPROACH", "ZONE", "RAMP", "DWELL", "DEPART"]

MARKER_BEFORE_S = 1800.0    # a withdraw window can hold passages minutes old
MARKER_AFTER_S = 5.0        # a refusal's event follows its dump within ms
HOLD_BACK_S = 10.0          # unmatched and this fresh at end of file: wait
BOOT_DROP_MS = 1000         # uptime falling by at least this much is a boot

SCHEMA = """
CREATE TABLE IF NOT EXISTS passages (
  loco          TEXT NOT NULL,
  boot_id       TEXT NOT NULL,          -- '<loco>@<iso of the boot epoch's first message>'
  source        TEXT NOT NULL,          -- 'wire' (diag/waveform) or 'wave8' (mm/wave)
  opened_ms     INTEGER NOT NULL,       -- locomotive millis() at open: EVIDENCE, not identity
  closed_ms     INTEGER NOT NULL,       -- ... and at close
  passage_seq   INTEGER,                -- the passage's own number within the boot, when it carries one
  boot_src      TEXT NOT NULL,          -- how the boot was established
  boot_partial  INTEGER NOT NULL DEFAULT 0,  -- 1 = epoch joined in progress, attribution uncertain
  ts            TEXT NOT NULL,          -- wall clock of the first arrival, ISO
  build         TEXT, subtitle TEXT, build_class TEXT,
  copies        INTEGER NOT NULL DEFAULT 1,
  slot_index    INTEGER, slot_total INTEGER,
  outcome       TEXT, outcome_code INTEGER,
  is_magnet     INTEGER, shape_tested INTEGER, truncated INTEGER,
  polarity      TEXT,
  n             INTEGER NOT NULL,
  decimation    INTEGER NOT NULL,       -- ms per stored sample
  quant         INTEGER NOT NULL,       -- counts per LSB of the stored copy
  pre_samples   INTEGER, pre_src TEXT,
  peak          INTEGER, gain INTEGER, ratio REAL, resid REAL, gap_ms INTEGER,
  stitch_at     INTEGER, stitch_src TEXT,
  rest_level    INTEGER, rest_src TEXT,
  splice        INTEGER, splice_src TEXT,
  stop_episode  INTEGER,
  -- from diag/wave_meta (station_curve_v1)
  meta_schema   TEXT,
  phase_open TEXT, phase_close TEXT, phase_mask INTEGER, station TEXT,
  -- from mm/marker
  marker_ts     TEXT,
  match_method  TEXT, match_confidence REAL,
  match_ambiguous INTEGER NOT NULL DEFAULT 0, match_candidates INTEGER NOT NULL DEFAULT 0,
  mm INTEGER, tgt INTEGER, dir TEXT,
  event TEXT, ruling TEXT, why TEXT, event2 TEXT,
  ev_resid REAL, two_sided INTEGER, trunk INTEGER, why2 TEXT,
  stitched INTEGER, paused_ms INTEGER, trust TEXT, obs TEXT, expected TEXT,
  samples       BLOB NOT NULL,          -- little-endian int16, n of them, oriented
  meta          TEXT,                   -- the rest of wave_meta, verbatim JSON
  extra         TEXT,                   -- JSON: transport facts and match candidates
  src_file      TEXT NOT NULL, src_line INTEGER NOT NULL,
  PRIMARY KEY (loco, boot_id, source, opened_ms, closed_ms)
);
-- the canonical identity, enforced wherever a passage carries its own number
CREATE UNIQUE INDEX IF NOT EXISTS ux_passages_seq
  ON passages(loco, boot_id, passage_seq) WHERE passage_seq IS NOT NULL;
CREATE INDEX IF NOT EXISTS ix_passages_ts    ON passages(ts);
CREATE INDEX IF NOT EXISTS ix_passages_mm    ON passages(loco, mm, dir);
CREATE INDEX IF NOT EXISTS ix_passages_out   ON passages(outcome, stitched);
CREATE INDEX IF NOT EXISTS ix_passages_src   ON passages(src_file, src_line);
CREATE INDEX IF NOT EXISTS ix_passages_phase ON passages(phase_open, phase_close);

CREATE TABLE IF NOT EXISTS copies (
  src_file TEXT NOT NULL, src_line INTEGER NOT NULL,
  loco TEXT NOT NULL, boot_id TEXT NOT NULL, source TEXT NOT NULL,
  opened_ms INTEGER NOT NULL, closed_ms INTEGER NOT NULL,
  ts TEXT NOT NULL, slot_index INTEGER, slot_total INTEGER,
  PRIMARY KEY (src_file, src_line)
);

CREATE TABLE IF NOT EXISTS markers (
  loco TEXT NOT NULL, ts TEXT NOT NULL, event TEXT NOT NULL, boot_id TEXT,
  mm INTEGER, tgt INTEGER, dir TEXT, ruling TEXT, why TEXT,
  peak INTEGER, resid REAL, gap_ms INTEGER, stitched INTEGER, paused_ms INTEGER,
  two_sided INTEGER, trunk INTEGER, why2 TEXT,
  json TEXT NOT NULL, src_file TEXT NOT NULL, src_line INTEGER NOT NULL,
  PRIMARY KEY (loco, ts, event)
);
CREATE INDEX IF NOT EXISTS ix_markers_match ON markers(loco, peak, ts);

-- diag/wave_meta, kept whether or not its waveform ever arrived
CREATE TABLE IF NOT EXISTS wave_meta (
  loco TEXT NOT NULL, boot_id TEXT NOT NULL,
  opened_ms INTEGER NOT NULL, closed_ms INTEGER NOT NULL,
  ts TEXT NOT NULL, seq INTEGER, schema TEXT, json TEXT NOT NULL,
  src_file TEXT NOT NULL, src_line INTEGER NOT NULL,
  PRIMARY KEY (loco, boot_id, opened_ms, closed_ms)
);

CREATE TABLE IF NOT EXISTS boots (
  loco TEXT NOT NULL, boot_id TEXT NOT NULL,
  first_ts TEXT NOT NULL, last_ts TEXT, first_uptime_ms INTEGER, last_uptime_ms INTEGER,
  partial INTEGER NOT NULL DEFAULT 0, boot_src TEXT NOT NULL,
  build TEXT, subtitle TEXT, build_class TEXT, bootid_ts TEXT, bootid_json TEXT,
  bootid_truncated INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (loco, boot_id)
);

CREATE TABLE IF NOT EXISTS rejects (
  src_file TEXT NOT NULL, src_line INTEGER NOT NULL, ts TEXT, loco TEXT,
  topic TEXT, reason TEXT NOT NULL, PRIMARY KEY (src_file, src_line, reason)
);

CREATE TABLE IF NOT EXISTS progress (
  src_file TEXT PRIMARY KEY, offset INTEGER NOT NULL, line INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS context (loco TEXT PRIMARY KEY, json TEXT NOT NULL);
"""


# ---------------------------------------------------------------------------
def parse_ts(ts):
    return datetime.datetime.fromisoformat(ts[:23]).timestamp()


def build_outcomes(sketch):
    if not sketch or not sketch.startswith("NAVI_ONE_1_0X"):
        return OUTCOME_PRE_X9
    digits = ""
    for ch in sketch[len("NAVI_ONE_1_0X"):]:
        if ch.isdigit():
            digits += ch
        else:
            break
    return OUTCOME_X9 if (int(digits) if digits else 1) >= 9 else OUTCOME_PRE_X9


def phase_names(mask):
    if mask is None:
        return None
    return "|".join(p for i, p in enumerate(PHASES) if mask & (1 << i)) or None


def median3(v):
    m = list(v)
    for i in range(1, len(v) - 1):
        m[i] = sorted(v[i - 1:i + 2])[1]
    return m


def derived_splice(v):
    if len(v) < 3:
        return None
    m = median3(v)
    i = max(range(1, len(m)), key=lambda k: abs(m[k] - m[k - 1]))
    return i if abs(m[i] - m[i - 1]) >= 30 else None


def derived_pre(dec):
    return 12 if dec == 1 else max(1, 12 // dec)


_STR = re.compile(r'"([A-Za-z_][A-Za-z0-9_]*)"\s*:\s*"([^"]*)"')
_NUM = re.compile(r'"([A-Za-z_][A-Za-z0-9_]*)"\s*:\s*(-?\d+(?:\.\d+)?)')


def salvage_json(text):
    """A truncated JSON object still names its leading fields. NAVI_ONE
    1.0X11's state/bootid is cut at 399 bytes by the char b[400] buffer in
    setup(), so json.loads throws and the build would be lost; the same fault
    decision 0071 records for marker events. Pull out whatever complete
    key/value pairs are there and mark the row truncated."""
    out = {}
    for k, v in _STR.findall(text):
        out[k] = v
    for k, v in _NUM.findall(text):
        if k not in out:
            out[k] = float(v) if "." in v else int(v)
    return out


def marker_fits(mj, peak, resid, gap):
    if mj.get("peak") != peak:
        return False
    try:
        if abs(float(mj.get("resid", -1)) - resid) > 1e-3:
            return False
    except (TypeError, ValueError):
        return False
    if gap is not None and "gap_ms" in mj and mj["gap_ms"] != gap:
        return False
    return True


# ---------------------------------------------------------------------------
class Ingest:
    def __init__(self, db):
        self.db = db
        self.cur = db.cursor()
        self._sql = None
        self.ctx = {}
        for loco, js in self.cur.execute("SELECT loco, json FROM context"):
            self.ctx[loco] = json.loads(js)

    # -- boot epochs ---------------------------------------------------------
    def ctx_for(self, loco):
        return self.ctx.setdefault(loco, dict(
            boot_id=None, boot_src=None, partial=0, last_uptime=None,
            first_uptime=None, build=None, subtitle=None, build_class=None))

    def boot_of(self, loco, ts):
        """The boot current at `ts`. If none has been established yet, open a
        provisional one starting here and say so."""
        c = self.ctx_for(loco)
        if c["boot_id"] is None:
            self.start_boot(loco, ts, None, "derived:none", partial=1)
        return c["boot_id"]

    def start_boot(self, loco, ts, uptime, src, partial=0):
        c = self.ctx_for(loco)
        c.update(boot_id="%s@%s" % (loco, ts), boot_src=src, partial=partial,
                 last_uptime=uptime, first_uptime=uptime)
        self.cur.execute(
            "INSERT OR IGNORE INTO boots (loco, boot_id, first_ts, last_ts,"
            " first_uptime_ms, last_uptime_ms, partial, boot_src, build, subtitle, build_class)"
            " VALUES (?,?,?,?,?,?,?,?,?,?,?)",
            (loco, c["boot_id"], ts, ts, uptime, uptime, partial, src,
             c["build"], c["subtitle"], c["build_class"]))
        self.boots += self.cur.rowcount

    def on_uptime(self, loco, ts, uptime):
        """The one signal that delimits boots: uptime going backwards."""
        c = self.ctx_for(loco)
        prev = c["last_uptime"]
        if c["boot_id"] is None:
            self.start_boot(loco, ts, uptime, "derived:uptime",
                            partial=1 if uptime > 5000 else 0)
            return
        if prev is not None and uptime < prev - BOOT_DROP_MS:
            self.start_boot(loco, ts, uptime, "derived:uptime",
                            partial=1 if uptime > 5000 else 0)
            return
        if prev is None or uptime > prev:
            c["last_uptime"] = uptime
            if c["first_uptime"] is None:
                c["first_uptime"] = uptime
            self.cur.execute("UPDATE boots SET last_ts=?, last_uptime_ms=? WHERE loco=? AND boot_id=?",
                             (ts, uptime, loco, c["boot_id"]))

    # -- per-file ------------------------------------------------------------
    def ingest(self, path):
        name = os.path.basename(path)
        row = self.cur.execute("SELECT offset, line FROM progress WHERE src_file=?", (name,)).fetchone()
        start, line = (row[0], row[1]) if row else (0, 0)
        if os.path.getsize(path) < start:
            start, line = 0, 0
        self.file = name
        self.open = {}
        self.pending = []
        self.recent = collections.defaultdict(list)
        self.added = self.dups = self.markers = self.rej = self.boots = self.metas = 0
        last_ts = None
        end, end_line = start, line
        with open(path, "rb") as f:
            f.seek(start)
            while True:
                pos = f.tell()
                raw = f.readline()
                if not raw or not raw.endswith(b"\n"):
                    break                        # EOF, or a line still being written
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
                if kind == "alert":
                    self.on_alert(loco, ts, parts[2])
                elif kind == "state/bootid":
                    self.on_boot(loco, ts, parts[2], line)
                elif kind == "mm/marker":
                    self.on_marker(loco, ts, parts[2], line)
                elif kind == "diag/waveform":
                    self.on_waveform(loco, ts, parts[2], line, pos)
                elif kind == "diag/wave_meta":
                    self.on_wave_meta(loco, ts, parts[2], line)
                elif kind == "mm/wave":
                    self.on_wave8(loco, ts, parts[2], line)
        rollback = [(d["pos"], d["line"] - 1) for d in self.open.values()]
        t_end = parse_ts(last_ts) if last_ts else 0
        for p in self.pending:
            if last_ts and t_end - p["t"] < HOLD_BACK_S:
                rollback.append((p["pos"], p["line"] - 1))
            elif p.get("ambig"):
                self.insert(p, (None, None, None, None, 1, len(p["ambig"]), p["ambig"]))
            else:
                self.insert(p, self.resolve_marker(p))
        if rollback:
            end, end_line = min(rollback)
        self.cur.execute("INSERT OR REPLACE INTO progress VALUES (?,?,?)", (name, end, end_line))
        for loco, c in self.ctx.items():
            self.cur.execute("INSERT OR REPLACE INTO context VALUES (?,?)", (loco, json.dumps(c)))
        self.db.commit()
        return self.added, self.markers, self.metas, self.rej

    # -- handlers ------------------------------------------------------------
    def on_alert(self, loco, ts, payload):
        try:
            j = json.loads(payload)
        except Exception:
            j = salvage_json(payload.decode("utf-8", "replace"))
        u = j.get("uptime_ms")
        if isinstance(u, (int, float)):
            self.on_uptime(loco, ts, int(u))

    def on_boot(self, loco, ts, payload, line):
        text = payload.decode("utf-8", "replace")
        truncated = 0
        try:
            j = json.loads(text)
        except Exception as e:
            j = salvage_json(text)
            truncated = 1
            self.reject(line, ts, loco, "state/bootid",
                        "bootid JSON truncated or malformed (%d bytes): %s" % (len(text), e))
        if not j.get("sketch"):
            return
        c = self.ctx_for(loco)
        c["build"], c["subtitle"], c["build_class"] = j.get("sketch"), j.get("subtitle"), j.get("build_class")
        bid = self.boot_of(loco, ts)
        # bootid is RETAINED and replayed to reconnecting subscribers, so it
        # names the build of whatever boot is current -- it does not start one.
        self.cur.execute(
            "UPDATE boots SET build=?, subtitle=?, build_class=?, bootid_ts=COALESCE(bootid_ts,?),"
            " bootid_json=COALESCE(bootid_json,?), bootid_truncated=MAX(bootid_truncated,?)"
            " WHERE loco=? AND boot_id=?",
            (j.get("sketch"), j.get("subtitle"), j.get("build_class"), ts, text, truncated, loco, bid))

    def on_marker(self, loco, ts, payload, line):
        text = payload.decode("utf-8", "replace")
        try:
            j = json.loads(text)
        except Exception as e:
            self.reject(line, ts, loco, "mm/marker", "marker JSON: %s" % e)
            return
        bid = self.boot_of(loco, ts)
        self.cur.execute("INSERT OR IGNORE INTO markers VALUES (?,?,?,?, ?,?,?,?,?, ?,?,?,?,?, ?,?,?, ?,?,?)",
                         (loco, ts, j.get("event", ""), bid, j.get("mm"), j.get("tgt"), j.get("dir"),
                          j.get("ruling"), j.get("why"), j.get("peak"), j.get("resid"), j.get("gap_ms"),
                          j.get("stitched"), j.get("paused_ms"), j.get("two_sided"), j.get("trunk"),
                          j.get("why2"), text, self.file, line))
        self.markers += self.cur.rowcount
        t = parse_ts(ts)
        # the second event of a passage already written (NOT_A_MAGNET, then
        # STITCHED_REFUSED 60 ms later)
        for t1, ts1, j1 in reversed(self.recent[loco]):
            if t - t1 > 1.0:
                break
            if ts1 != ts and marker_fits(j1, j.get("peak"), _f(j.get("resid")), j.get("gap_ms")):
                self.cur.execute("UPDATE passages SET event2=? WHERE loco=? AND marker_ts=? AND event2 IS NULL",
                                 (j.get("event"), loco, ts1))
                break
        self.recent[loco].append((t, ts, j))
        # AT MOST ONE pending passage may claim this event, and only if it is
        # the only one that fits: two that fit are ambiguous and neither takes it.
        fits = [p for p in self.pending
                if p["loco"] == loco and t - p["t"] <= MARKER_AFTER_S
                and marker_fits(j, p["peak"], p["resid"], p["gap"])]
        if len(fits) == 1:
            self.pending.remove(fits[0])
            self.insert(fits[0], (ts, j, "heuristic", 1.0, 0, 1))
        elif len(fits) > 1:
            for p in fits:
                p.setdefault("ambig", []).append(ts)

    def on_wave_meta(self, loco, ts, payload, line):
        text = payload.decode("utf-8", "replace")
        try:
            j = json.loads(text)
            o, c = int(j["open_ms"]), int(j["close_ms"])
        except Exception as e:
            self.reject(line, ts, loco, "diag/wave_meta", "wave_meta JSON: %s" % e)
            return
        bid = self.boot_of(loco, ts)
        self.cur.execute("INSERT OR IGNORE INTO wave_meta VALUES (?,?,?,?,?,?,?,?,?,?)",
                         (loco, bid, o, c, ts, j.get("seq"), j.get("schema"), text, self.file, line))
        self.metas += self.cur.rowcount
        # the waveform may already be in; a meta arriving late still lands on it
        self.apply_meta(loco, bid, o, c, j)

    def apply_meta(self, loco, bid, o, c, j):
        self.cur.execute(
            "UPDATE passages SET passage_seq=?, meta_schema=?, phase_open=?, phase_close=?,"
            " phase_mask=?, station=?, stop_episode=?, stitch_at=?, stitch_src='wire',"
            " rest_level=?, rest_src='wire', pre_samples=?, pre_src='wire', meta=?"
            " WHERE loco=? AND boot_id=? AND source='wire' AND opened_ms=? AND closed_ms=?",
            (j.get("seq"), j.get("schema"), j.get("phase_open"), j.get("phase_close"),
             j.get("phase_mask"), j.get("station") or None, j.get("stop_episode"),
             j.get("stitch_at"), j.get("rest_level"), j.get("pre_samples"),
             json.dumps(j, separators=(",", ":")), loco, bid, o, c))
        if self.cur.rowcount:
            # a stitch boundary from the wire supersedes any derived splice
            self.cur.execute(
                "UPDATE passages SET splice=CASE WHEN stitch_at>0 THEN stitch_at ELSE NULL END,"
                " splice_src='wire' WHERE loco=? AND boot_id=? AND source='wire'"
                " AND opened_ms=? AND closed_ms=?", (loco, bid, o, c))

    def on_waveform(self, loco, ts, payload, line, pos):
        if not payload.lstrip().startswith(b"{"):
            self.reject(line, ts, loco, "diag/waveform",
                        "raw-era binary, bytes lost to UTF-8 replacement")
            return
        try:
            j = json.loads(payload)
            b = base64.b64decode(j["payload_b64"], validate=True)
        except Exception as e:
            self.reject(line, ts, loco, "diag/waveform", "payload: %s" % e)
            return
        if "byte_length" in j and j["byte_length"] != len(b):
            self.reject(line, ts, loco, "diag/waveform",
                        "byte_length %s but %d decoded" % (j["byte_length"], len(b)))
            return
        if len(b) < HDR_LEN:
            self.reject(line, ts, loco, "diag/waveform", "short: %d bytes" % len(b))
            return
        h = struct.unpack(HDR, b[:HDR_LEN])
        si, st, ci, ct, pol, outc, ism, shp = h[0:8]
        scount, ccount, coff, dec, peak, gain = h[8:14]
        ratio, resid = h[14:16]
        gap, opened, closed = h[16:19]
        body = b[HDR_LEN:]

        trailer, quant, width = None, 1, 2
        if len(body) >= TRAILER_LEN and body[-TRAILER_LEN:-TRAILER_LEN + 2] == TRAILER_MAGIC:
            t = struct.unpack(TRAILER, body[-TRAILER_LEN:])
            # magic, bootId, passageSeq, stitchAt, restLevel, preSamples, quant, flags
            if not 1 <= t[6] <= MAX_QUANT:
                self.reject(line, ts, loco, "diag/waveform", "trailer quant %d out of range" % t[6])
                return
            trailer, quant = t[1:], t[6]
            body = body[:-TRAILER_LEN]
            width = 1 if quant > 1 else 2

        # ---- chunk invariants. A malformed chunk is refused, never folded in.
        why = None
        if ct < 1 or ci >= ct:
            why = "chunk %d of %d" % (ci, ct)
        elif ccount < 1 or scount < 1:
            why = "sampleCount %d chunkSampleCount %d" % (scount, ccount)
        elif coff + ccount > scount:
            why = "chunk covers %d..%d of %d samples" % (coff, coff + ccount - 1, scount)
        elif len(body) != ccount * width:
            why = "chunk claims %d samples (%d B) but %d B follow" % (ccount, ccount * width, len(body))
        elif dec < 1:
            why = "decimation %d" % dec
        elif closed < opened:
            why = "closed %d before opened %d" % (closed, opened)
        if why:
            self.reject(line, ts, loco, "diag/waveform", why)
            return

        bid = self.boot_of(loco, ts)
        key = (loco, bid, st, si, opened, closed)
        # every header field that describes the SLOT, not the chunk, must be
        # identical across the chunks of one slot
        inv = (pol, outc, ism, shp, scount, dec, peak, gain,
               round(ratio, 6), round(resid, 6), gap, ct, quant, trailer)
        d = self.open.get(key)
        if d is None:
            d = self.open[key] = dict(
                loco=loco, boot_id=bid, ts=ts, t=parse_ts(ts), line=line, pos=pos, source="wire",
                si=si, st=st, pol=pol, outc=outc, ism=ism, shp=shp, n=scount, dec=dec,
                peak=peak, gain=gain, ratio=ratio, resid=resid, gap=gap,
                opened=opened, closed=closed, s={}, trailer=trailer, quant=quant,
                inv=inv, byte_len=0, chunks={})
        elif d["inv"] != inv:
            self.reject(line, ts, loco, "diag/waveform",
                        "chunk %d disagrees with the slot's other chunks" % ci)
            return
        if ci in d["chunks"]:
            if d["chunks"][ci] != (coff, ccount, bytes(body)):
                self.reject(line, ts, loco, "diag/waveform",
                            "chunk %d repeated with different content" % ci)
                return
            self.copy_line(line, d)             # a duplicated chunk is an arrival
            return
        d["chunks"][ci] = (coff, ccount, bytes(body))
        d["byte_len"] += len(b)
        if width == 1:
            vals = [x * quant for x in struct.unpack("<%db" % ccount, body)]
        else:
            vals = struct.unpack("<%dh" % ccount, body)
        for i, x in enumerate(vals):
            d["s"][coff + i] = x
        if len(d["chunks"]) < ct:
            return                               # more chunks to come
        # complete: every index 0..ct-1 present, and contiguous cover of 0..n-1
        if sorted(d["chunks"]) != list(range(ct)) or sorted(d["s"]) != list(range(d["n"])):
            self.reject(line, ts, loco, "diag/waveform",
                        "slot has %d chunks and %d of %d samples, not contiguous"
                        % (len(d["chunks"]), len(d["s"]), d["n"]))
            del self.open[key]
            return
        del self.open[key]
        d["v"] = [d["s"][i] for i in range(d["n"])]
        del d["s"]
        pk = (loco, bid, "wire", opened, closed)
        for q in self.pending:
            if (q["loco"], q["boot_id"], q["source"], q["opened"], q["closed"]) == pk:
                self.copy_line(line, d)      # the same passage, published again
                return
        if self.cur.execute("SELECT 1 FROM passages WHERE loco=? AND boot_id=? AND source=?"
                            " AND opened_ms=? AND closed_ms=? LIMIT 1", pk).fetchone():
            self.copy_line(line, d)
            return
        ev = self.resolve_marker(d)
        if ev is not None:
            self.insert(d, ev)
        else:
            self.pending.append(d)

    def on_wave8(self, loco, ts, payload, line):
        text = payload.decode("utf-8", "replace")
        try:
            j = json.loads(text)
            raw = base64.b64decode(j["d"], validate=True)
        except Exception as e:
            self.reject(line, ts, loco, "mm/wave", "mm/wave payload: %s" % e)
            return
        sc = j.get("sc") or 1
        n = j.get("n")
        if n is not None and n != len(raw):
            self.reject(line, ts, loco, "mm/wave", "n=%s but %d samples" % (n, len(raw)))
            return
        v = [(x - 128) * sc for x in raw]
        if j.get("pol") == "S":
            v = [-x for x in v]
        rej, tr = j.get("rej"), j.get("tr")
        t = j.get("t", 0)
        self.insert(dict(
            loco=loco, boot_id=self.boot_of(loco, ts), ts=ts, t=parse_ts(ts), line=line,
            source="wave8", si=None, st=None, pol=1 if j.get("pol") == "N" else 0,
            outc=rej, ism=1 if rej == 0 else (0 if rej is not None else None),
            shp=0 if tr else 1, n=len(v), dec=1, peak=j.get("pk"), gain=None,
            ratio=None, resid=None, gap=None, opened=t, closed=t + (j.get("dur") or 0),
            v=v, trailer=None, quant=sc, pre=j.get("pre"), truncated=1 if tr else 0,
            extra=dict(mm=j.get("mm"), t=t, dur=j.get("dur"), pwm=j.get("pwm"), sc=sc,
                       pre=j.get("pre"), tr=tr, clip=j.get("clip"), rej=rej, drop=j.get("drop"))),
            None)

    # -- marker association --------------------------------------------------
    def resolve_marker(self, d):
        """Every event that fits, from this pass and from the table. One fit is
        a match; several are an ambiguity and NOTHING is attached."""
        seen, cands = set(), []
        for t, ts, mj in reversed(self.recent[d["loco"]]):
            if d["t"] - t > MARKER_BEFORE_S:
                break
            if ts in seen:
                continue
            if marker_fits(mj, d["peak"], d["resid"], d["gap"]) and not self.linked(d["loco"], ts):
                seen.add(ts)
                cands.append((abs(d["t"] - t), ts, mj))
        t0 = datetime.datetime.fromtimestamp(d["t"] - MARKER_BEFORE_S).isoformat(timespec="milliseconds")
        for ts, js in self.cur.execute(
                "SELECT ts, json FROM markers WHERE loco=? AND peak=? AND ts>=? AND ts<=?",
                (d["loco"], d["peak"], t0, d["ts"])).fetchall():
            if ts in seen:
                continue
            mj = json.loads(js)
            if marker_fits(mj, d["peak"], d["resid"], d["gap"]) and not self.linked(d["loco"], ts):
                seen.add(ts)
                cands.append((abs(d["t"] - parse_ts(ts)), ts, mj))
        if not cands:
            return None
        if len(cands) > 1:
            cands.sort()
            return (None, None, None, None, 1, len(cands), [c[1] for c in cands])
        dt, ts, mj = cands[0]
        return (ts, mj, "heuristic", round(max(0.0, 1.0 - dt / MARKER_BEFORE_S), 4), 0, 1)

    def linked(self, loco, marker_ts):
        return self.cur.execute("SELECT 1 FROM passages WHERE loco=? AND marker_ts=? LIMIT 1",
                                (loco, marker_ts)).fetchone() is not None

    # -- writing -------------------------------------------------------------
    def copy_line(self, line, d):
        self.cur.execute("INSERT OR IGNORE INTO copies VALUES (?,?,?,?,?,?,?,?,?,?)",
                         (self.file, line, d["loco"], d["boot_id"], d["source"],
                          d["opened"], d["closed"], d["ts"], d["si"], d["st"]))
        if self.cur.rowcount:
            self.cur.execute(
                "UPDATE passages SET copies=(SELECT COUNT(*) FROM copies WHERE loco=? AND boot_id=?"
                " AND source=? AND opened_ms=? AND closed_ms=?) WHERE loco=? AND boot_id=?"
                " AND source=? AND opened_ms=? AND closed_ms=?",
                (d["loco"], d["boot_id"], d["source"], d["opened"], d["closed"],
                 d["loco"], d["boot_id"], d["source"], d["opened"], d["closed"]))

    def insert(self, d, ev):
        loco, bid = d["loco"], d["boot_id"]
        c = self.ctx_for(loco)
        v = d["v"]
        if ev and ev[4]:                                    # ambiguous
            marker_ts, mj, method, conf, ambig, cands = None, {}, None, None, 1, ev[5]
            amb_list = ev[6]
        else:
            marker_ts, mj, method, conf, ambig, cands = (ev if ev else (None, {}, None, None, 0, 0))[:6]
            mj = mj or {}
            amb_list = None
        if d["source"] == "wire":
            outcome = build_outcomes(c["build"]).get(d["outc"], "CODE_%d" % d["outc"])
            quant, truncated = d["quant"], None
            if d["trailer"]:
                wire_boot, seq, stitch_at, rest_level, pre = d["trailer"][0:5]
                pre_src = stitch_src = rest_src = "wire"
            else:
                stitch_at = rest_level = seq = None
                stitch_src = rest_src = None
                pre, pre_src = derived_pre(d["dec"]), "derived"
            extra = dict(byte_len=d["byte_len"], chunks=len(d["chunks"]))
            if d["trailer"]:
                extra["flags"] = d["trailer"][6]
                extra["wire_boot_id"] = d["trailer"][0]
        else:
            outcome = WAVE8_REJ.get(d["outc"], None if d["outc"] is None else "REJ_%d" % d["outc"])
            quant, truncated = d["quant"], d["truncated"]
            stitch_at = rest_level = seq = None
            stitch_src = rest_src = None
            pre, pre_src = d["pre"], "wire"
            extra = dict(d["extra"])
        if amb_list:
            extra["marker_candidates"] = amb_list
        stitched = mj.get("stitched")
        if stitch_at is not None:
            splice, splice_src = (stitch_at or None), "wire"
        elif stitched:
            splice, splice_src = derived_splice(v), "derived:medstep30"
        else:
            splice, splice_src = None, None
        row = (loco, bid, d["source"], d["opened"], d["closed"], seq,
               c["boot_src"] or "derived:none", c["partial"] or 0, d["ts"],
               c["build"], c["subtitle"], c["build_class"],
               1, d["si"], d["st"],
               outcome, d["outc"],
               d["ism"], d["shp"], truncated,
               "N" if d["pol"] else "S",
               len(v), d["dec"], quant,
               pre, pre_src,
               d["peak"], d["gain"], d["ratio"], d["resid"], d["gap"],
               stitch_at, stitch_src, rest_level, rest_src,
               splice, splice_src,
               None,                                    # stop_episode
               None, None, None, None, None,            # meta_schema, phase_*, station
               marker_ts, method, conf, ambig, cands,
               mj.get("mm"), mj.get("tgt"), mj.get("dir"),
               mj.get("event"), mj.get("ruling"), mj.get("why"), None,
               mj.get("resid"), mj.get("two_sided"), mj.get("trunk"), mj.get("why2"),
               stitched, mj.get("paused_ms"), mj.get("trust"), mj.get("obs"), mj.get("expected"),
               struct.pack("<%dh" % len(v), *v), None,
               json.dumps(extra, separators=(",", ":")),
               self.file, d["line"])
        try:
            self.cur.execute(self.insert_sql(len(row)), row)
            self.added += 1
            if d["source"] == "wire":
                m = self.cur.execute(
                    "SELECT json FROM wave_meta WHERE loco=? AND boot_id=? AND opened_ms=? AND closed_ms=?",
                    (loco, bid, d["opened"], d["closed"])).fetchone()
                if m:
                    self.apply_meta(loco, bid, d["opened"], d["closed"], json.loads(m[0]))
        except sqlite3.IntegrityError as e:
            self.dups += 1
            old = self.cur.execute(
                "SELECT n, peak, src_file, src_line FROM passages WHERE loco=? AND boot_id=?"
                " AND source=? AND opened_ms=? AND closed_ms=?",
                (loco, bid, d["source"], d["opened"], d["closed"])).fetchone()
            if old is None:
                self.reject(d["line"], d["ts"], loco, d["source"], "insert refused: %s" % e)
            elif old[0] != len(v) or old[1] != d["peak"]:
                print("  KEY COLLISION %s %s opened %d closed %d: stored n=%d peak=%d,"
                      " %s:%d has n=%d peak=%d" % (loco, bid, d["opened"], d["closed"],
                                                   old[0], old[1], self.file, d["line"], len(v), d["peak"]),
                      file=sys.stderr)
                self.reject(d["line"], d["ts"], loco, d["source"],
                            "key collision with %s:%d" % (old[2], old[3]))
        self.copy_line(d["line"], d)

    def insert_sql(self, n):
        """Placeholders are generated, and their count is checked against the
        table, so a column added to the schema can never silently shift a
        value into the wrong field."""
        if self._sql is None:
            cols = [r[1] for r in self.cur.execute("PRAGMA table_info(passages)")]
            if len(cols) != n:
                raise RuntimeError("passages has %d columns, insert supplies %d: %s"
                                   % (len(cols), n, cols[min(n, len(cols)) - 1:]))
            self._sql = "INSERT INTO passages VALUES (%s)" % ",".join("?" * n)
        return self._sql

    def reject(self, line, ts, loco, topic, reason):
        self.cur.execute("INSERT OR IGNORE INTO rejects VALUES (?,?,?,?,?,?)",
                         (self.file, line, ts, loco, topic, reason))
        self.rej += self.cur.rowcount


def _f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return -1.0


# ---------------------------------------------------------------------------
def cmd_ingest(tdir, dbpath):
    d = os.path.dirname(os.path.abspath(dbpath))
    if d:
        os.makedirs(d, exist_ok=True)
    db = sqlite3.connect(dbpath)
    db.executescript(SCHEMA)
    ing = Ingest(db)
    tot = [0, 0, 0, 0]
    for path in sorted(glob.glob(os.path.join(tdir, "all_*.log"))):
        r = ing.ingest(path)
        tot = [a + b for a, b in zip(tot, r)]
        if any(r):
            print("  %-20s +%d passages  +%d markers  +%d meta  +%d rejects"
                  % (os.path.basename(path), r[0], r[1], r[2], r[3]))
    n = db.execute("SELECT COUNT(*) FROM passages").fetchone()[0]
    print("added %d passages; the database holds %d" % (tot[0], n))
    db.close()


def cmd_stats(dbpath):
    db = sqlite3.connect(dbpath)
    print("passages by build / source / outcome / stitched:")
    for r in db.execute("""SELECT COALESCE(build,'?'), source, COALESCE(outcome,'?'),
                                  COALESCE(stitched,0), COUNT(*), SUM(copies>1),
                                  SUM(marker_ts IS NULL), SUM(match_ambiguous)
                           FROM passages GROUP BY 1,2,3,4 ORDER BY 1,2,3,4"""):
        print("  %-30s %-5s %-13s st=%d %5d  (twice %d, no marker %d, ambiguous %d)" % r)
    print("markers %d  wave_meta %d  boots %d  rejects %d  copies %d" % tuple(
        db.execute("SELECT (SELECT COUNT(*) FROM markers),(SELECT COUNT(*) FROM wave_meta),"
                   "(SELECT COUNT(*) FROM boots),(SELECT COUNT(*) FROM rejects),"
                   "(SELECT COUNT(*) FROM copies)").fetchone()))
    for r in db.execute("SELECT topic, COUNT(*), MIN(reason) FROM rejects GROUP BY 1"):
        print("  rejects %-16s %d  e.g. %s" % r)


def cmd_audit(dbpath):
    """Everything a reader should distrust, counted."""
    db = sqlite3.connect(dbpath)
    q = lambda s: db.execute(s).fetchone()[0]
    print("boots            %d  (partial %d, bootid truncated %d, no build named %d)" % (
        q("SELECT COUNT(*) FROM boots"), q("SELECT COUNT(*) FROM boots WHERE partial"),
        q("SELECT COUNT(*) FROM boots WHERE bootid_truncated"),
        q("SELECT COUNT(*) FROM boots WHERE build IS NULL")))
    print("passages         %d  (boot uncertain %d)" % (
        q("SELECT COUNT(*) FROM passages"), q("SELECT COUNT(*) FROM passages WHERE boot_partial")))
    print("marker match     seq %d, heuristic %d, none %d, AMBIGUOUS %d" % (
        q("SELECT COUNT(*) FROM passages WHERE match_method='seq'"),
        q("SELECT COUNT(*) FROM passages WHERE match_method='heuristic'"),
        q("SELECT COUNT(*) FROM passages WHERE match_method IS NULL AND source='wire'"),
        q("SELECT COUNT(*) FROM passages WHERE match_ambiguous")))
    print("provenance       stitch wire %d derived %d none %d; pre wire %d derived %d" % (
        q("SELECT COUNT(*) FROM passages WHERE stitch_src='wire'"),
        q("SELECT COUNT(*) FROM passages WHERE splice_src LIKE 'derived%'"),
        q("SELECT COUNT(*) FROM passages WHERE splice_src IS NULL"),
        q("SELECT COUNT(*) FROM passages WHERE pre_src='wire'"),
        q("SELECT COUNT(*) FROM passages WHERE pre_src='derived'")))
    print("sequence gaps, per boot that numbers its passages:")
    any_seq = False
    for loco, bid, lo, hi, n in db.execute(
            "SELECT loco, boot_id, MIN(passage_seq), MAX(passage_seq), COUNT(passage_seq)"
            " FROM passages WHERE passage_seq IS NOT NULL GROUP BY 1,2 ORDER BY 2"):
        any_seq = True
        print("  %-34s seq %d..%d, %d present, %d MISSING" % (bid, lo, hi, n, hi - lo + 1 - n))
    if not any_seq:
        print("  (none: no build on the wire numbers its passages yet)")


def cmd_export(dbpath, out, everything=False):
    """One passage per line, the format tools/two_sided/*.cpp read:
         <tag> <n> <pre> v0 v1 v2 ...
    Default: accepted, complete, untruncated passages. --all: everything."""
    db = sqlite3.connect(dbpath)
    where = "" if everything else \
        "WHERE is_magnet=1 AND COALESCE(truncated,0)=0 AND ((source='wire' AND outcome_code=0) OR source='wave8')"
    n = 0
    with open(out, "w") as f:
        for src, sf, sl, pre, samples, extra, outcome, stitched in db.execute(
                "SELECT source, src_file, src_line, pre_samples, samples, extra, outcome, stitched"
                " FROM passages " + where + " ORDER BY src_file, src_line"):
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
            f.write("%s %d %d %s\n" % (tag, len(v), pre if pre is not None else 12,
                                       " ".join(str(s) for s in v)))
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
    elif len(a) >= 2 and a[0] == "audit":
        cmd_audit(a[1])
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
