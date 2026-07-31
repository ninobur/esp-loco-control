#!/usr/bin/env python3
"""
ngr_runlog.py -- NGR per-run MQTT telemetry logger (Raspberry Pi ngr-pi, user david).

Replaces the ad-hoc mosquitto_sub logger. Three things it fixes, all of which
every analysis today had to reconstruct by hand:

  * ONE FILE PER RUN. A run starts on a nav DECLARED event, or on the first
    message after 120 s of marker silence for that locomotive. It closes on the
    next such boundary (or on 120 s of silence). Two runs were misattributed on
    2026-07-30 because a single 16,000-line file held at least six sessions.
  * A MILLISECOND TIMESTAMP ON EVERY LINE, from the Pi clock at receipt. Without
    it, ordering across topics is guesswork and no run can be located in time.
  * LOOPSTAT ALONGSIDE THE MARKERS IT EXPLAINS -- noise and network counters in
    the same file as the events they explain.

A rolling all_YYYYMMDD.log also gets every message, so nothing is lost if run
detection misfires. The per-run files are a convenience; the all_ log is the
system of record.

  ---------------------------------------------------------------------------
  THIS PROCESS IS A LOGGER. IT NEVER PUBLISHES.
  There is deliberately no publish() call anywhere in this file, so it is
  structurally incapable of commanding a locomotive. Do not add one.
  ---------------------------------------------------------------------------
"""

import os
import sys
import json
import time
import math
import signal
from datetime import datetime

import paho.mqtt.client as mqtt

# --- configuration --------------------------------------------------------
BROKER = "127.0.0.1"
PORT = 1883
CLIENT_ID = "ngr-runlog"
TOPIC = "ngr/loco/+/#"            # all topics, all locomotives
MARKER_SILENCE_S = 120.0          # marker gap that ends a run

# Paths are anchored to this file's directory, so runs/ and all_ land in the
# WorkingDirectory (/home/david/NGR/telemetry) regardless of where it is run
# from.
BASE = os.path.dirname(os.path.abspath(__file__))
RUNS_DIR = os.path.join(BASE, "runs")


# --- rolling all_ log -----------------------------------------------------
# Every message, appended, line-buffered + flushed. Rolls to a new file when
# the local date changes.
_all_fh = None
_all_date = None


def all_log(line):
    global _all_fh, _all_date
    d = datetime.now().strftime("%Y%m%d")
    if d != _all_date:
        if _all_fh is not None:
            try:
                _all_fh.close()
            except Exception:
                pass
        _all_fh = open(os.path.join(BASE, "all_%s.log" % d), "a", buffering=1)
        _all_date = d
    _all_fh.write(line)
    _all_fh.flush()


# --- small numeric helpers ------------------------------------------------
def _median(vals):
    if not vals:
        return None
    s = sorted(vals)
    n = len(s)
    m = n // 2
    return float(s[m]) if n % 2 else (s[m - 1] + s[m]) / 2.0


def _percentile(vals, q):
    """Linear-interpolated percentile, q in [0,1]. No numpy dependency."""
    if not vals:
        return None
    s = sorted(vals)
    if len(s) == 1:
        return float(s[0])
    idx = q * (len(s) - 1)
    lo = math.floor(idx)
    hi = math.ceil(idx)
    if lo == hi:
        return float(s[int(idx)])
    return s[lo] * (hi - idx) + s[hi] * (idx - lo)


def _round(x):
    return round(x, 1) if isinstance(x, (int, float)) else None


def _counter_delta(first, last):
    """Delta of a boot-cumulative counter over a run. If the loco rebooted
    mid-run the counter resets and last < first; report last in that case,
    since it is the run's own activity after the reset."""
    if first is None or last is None:
        return None
    return last - first if last >= first else last


# --- run and per-locomotive state ----------------------------------------
class Run(object):
    def __init__(self, loco, dt, now):
        self.loco = loco
        self.started_epoch = now
        self.started_iso = dt.isoformat(timespec="milliseconds")
        self.last_activity_epoch = now

        os.makedirs(RUNS_DIR, exist_ok=True)
        stamp = dt.strftime("%Y%m%d_%H%M%S")
        base_name = "%s_%s" % (loco, stamp)
        # Guard against two runs opening in the same second (e.g. a silence
        # close immediately followed by a DECLARED).
        i = 0
        while True:
            suffix = "" if i == 0 else "_%d" % i
            path = os.path.join(RUNS_DIR, base_name + suffix + ".log")
            if not os.path.exists(path):
                break
            i += 1
        self.path = path
        self.meta_path = os.path.join(RUNS_DIR, base_name + suffix + ".meta.json")
        self.fh = open(self.path, "a", buffering=1)

        self.marker_count = 0
        self.peaks = []
        self.dts = []
        self.fr_first = self.fr_last = None      # floor_rejects
        self.qd_first = self.qd_last = None      # queue_drops
        self.att_first = self.att_last = None    # mqtt_attempts
        self.max_loop_gap = None
        self.sketch = None
        self.session_dir = None
        self.declared_mm = None

    def write(self, line, now):
        self.fh.write(line)
        self.fh.flush()
        self.last_activity_epoch = now


class LocoState(object):
    # sketch and session_dir are per-locomotive persistent facts (sketch comes
    # from the RETAINED bootid, which the broker delivers before any DECLARED).
    # Held here so a run started later -- by a declaration or by silence -- is
    # still stamped with them, rather than losing them to a pre-declaration
    # idle run.
    __slots__ = ("run", "last_marker_epoch", "sketch", "session_dir")

    def __init__(self):
        self.run = None
        self.last_marker_epoch = None
        self.sketch = None
        self.session_dir = None


states = {}


def open_run(st, loco, dt, now):
    st.run = Run(loco, dt, now)
    # Carry the last-known per-locomotive facts onto the fresh run, so a run
    # opened by silence (with no DECLARED and no fresh bootid) is still labelled.
    st.run.sketch = st.sketch
    st.run.session_dir = st.session_dir
    sys.stderr.write("[ngr_runlog] run OPEN  %s\n" % os.path.basename(st.run.path))


def close_run(st):
    run = st.run
    if run is None:
        return
    st.run = None
    ended_iso = datetime.fromtimestamp(run.last_activity_epoch).isoformat(
        timespec="milliseconds")
    meta = {
        "loco": run.loco,
        "started": run.started_iso,
        "ended": ended_iso,
        "duration_s": round(run.last_activity_epoch - run.started_epoch, 3),
        "markers": run.marker_count,
        "declared_mm": run.declared_mm,
        "session_dir": run.session_dir,
        "sketch": run.sketch,
        "floor_rejects_delta": _counter_delta(run.fr_first, run.fr_last),
        "queue_drops_delta": _counter_delta(run.qd_first, run.qd_last),
        "max_loop_gap_ms": run.max_loop_gap,
        "mqtt_attempts_delta": _counter_delta(run.att_first, run.att_last),
        "peak_median": _round(_median(run.peaks)),
        "peak_p05": _round(_percentile(run.peaks, 0.05)),
        "dt_median": _round(_median(run.dts)),
    }
    try:
        with open(run.meta_path, "w") as f:
            json.dump(meta, f, indent=2)
            f.write("\n")
    except Exception as e:
        sys.stderr.write("[ngr_runlog] meta write failed: %s\n" % e)
    try:
        run.fh.close()
    except Exception:
        pass
    sys.stderr.write("[ngr_runlog] run CLOSE %s  markers=%d\n"
                     % (os.path.basename(run.path), run.marker_count))


def _track_counter(run, key, val):
    if not isinstance(val, (int, float)):
        return
    if getattr(run, key + "_first") is None:
        setattr(run, key + "_first", val)
    setattr(run, key + "_last", val)


# --- MQTT callbacks -------------------------------------------------------
def on_connect(client, userdata, flags, rc, properties=None):
    # Subscribe only. This logger never publishes.
    client.subscribe(TOPIC)
    sys.stderr.write("[ngr_runlog] connected rc=%s, subscribed %s\n" % (rc, TOPIC))


def on_message(client, userdata, msg):
    # One malformed payload must never take down the logger.
    try:
        dt = datetime.now()
        now = dt.timestamp()
        ts = dt.isoformat(timespec="milliseconds")

        topic = msg.topic
        try:
            payload = msg.payload.decode("utf-8", "replace")
        except Exception:
            payload = repr(msg.payload)
        # Keep it strictly one line per message.
        payload_line = payload.replace("\r", " ").replace("\n", " ")

        parts = topic.split("/")
        loco = parts[2] if len(parts) >= 3 else "unknown"

        data = None
        try:
            data = json.loads(payload)
        except Exception:
            data = None

        event = data.get("event") if isinstance(data, dict) else None
        is_nav = topic.endswith("/state/nav")
        is_marker = topic.endswith("/mm/marker")
        is_loopstat = topic.endswith("/state/loopstat")
        is_bootid = topic.endswith("/state/bootid")
        is_sessdir = topic.endswith("/state/session_direction")
        declared = is_nav and event == "DECLARED"

        st = states.get(loco)
        if st is None:
            st = LocoState()
            states[loco] = st

        # (1) Close an ACTIVE run (one that saw markers) after 120 s of marker
        #     silence. The marker_count>0 guard is what stops a fresh, still
        #     markerless run from being re-closed on every idle message.
        if (st.run is not None and st.run.marker_count > 0 and
                st.last_marker_epoch is not None and
                (now - st.last_marker_epoch) > MARKER_SILENCE_S):
            close_run(st)

        # (2) A declaration always begins a fresh run.
        if declared and st.run is not None:
            close_run(st)

        # (3) Ensure a run exists to log into (covers boot, post-silence, and
        #     post-declaration). This message becomes the run's first line.
        if st.run is None:
            open_run(st, loco, dt, now)

        run = st.run

        # (4) Log the line -- per-run file AND the rolling all_ log.
        line = "%s\t%s\t%s\n" % (ts, topic, payload_line)
        run.write(line, now)
        all_log(line)

        # (5) Accumulate the .meta.json summary numbers.
        if is_marker:
            st.last_marker_epoch = now
            run.marker_count += 1
            if isinstance(data, dict):
                p = data.get("peak")
                d = data.get("dt")
                if isinstance(p, (int, float)):
                    run.peaks.append(p)
                if isinstance(d, (int, float)):
                    run.dts.append(d)

        if is_loopstat and isinstance(data, dict):
            _track_counter(run, "fr", data.get("floor_rejects"))
            _track_counter(run, "qd", data.get("queue_drops"))
            _track_counter(run, "att", data.get("mqtt_attempts"))
            g = data.get("loop_max_gap_ms")
            if isinstance(g, (int, float)):
                run.max_loop_gap = g if run.max_loop_gap is None else max(run.max_loop_gap, g)

        if is_bootid and isinstance(data, dict):
            s = data.get("sketch")
            if s:
                st.sketch = s        # persist across runs
                run.sketch = s

        new_sd = None
        if isinstance(data, dict):
            sd = data.get("session_dir")
            if sd and sd != "UNSET":
                new_sd = sd
        if is_sessdir:
            v = payload.strip().strip('"')
            if v in ("CW", "CCW"):
                new_sd = v
        if new_sd:
            st.session_dir = new_sd   # persist across runs
            run.session_dir = new_sd

        if declared and isinstance(data, dict) and run.declared_mm is None:
            run.declared_mm = data.get("mm")

    except Exception as e:
        try:
            sys.stderr.write("[ngr_runlog] on_message error: %s\n" % e)
        except Exception:
            pass


# --- lifecycle ------------------------------------------------------------
def shutdown(signum, frame):
    # Close open runs so a graceful stop still writes their .meta.json.
    for st in list(states.values()):
        try:
            close_run(st)
        except Exception:
            pass
    if _all_fh is not None:
        try:
            _all_fh.flush()
            _all_fh.close()
        except Exception:
            pass
    sys.exit(0)


def main():
    os.makedirs(RUNS_DIR, exist_ok=True)
    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    # Same reconnect shape as ngr_app.py: connect + loop_forever inside
    # try/except, sleep 5, retry forever, never exit.
    while True:
        try:
            c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID)
            c.on_connect = on_connect
            c.on_message = on_message
            c.connect(BROKER, PORT, keepalive=60)
            c.loop_forever()
        except SystemExit:
            raise
        except Exception as e:
            sys.stderr.write("[ngr_runlog] MQTT error: %s\n" % e)
        time.sleep(5)


if __name__ == "__main__":
    main()
