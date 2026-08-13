# ============================================================================
# NGR dashboard v1.11.0 — THE ROSTER IS NOT A LIST
#
# Layout redesign from the operator's own sketches (Dashboard.csv,
# Console.csv, 2026-08-12). Full design record, including what was rejected
# and why: docs/dashboard-redesign/README.md.
#
# NO AUTHORITY RULE IS CHANGED BY THIS VERSION. P2-P14 stand exactly as
# v1.10.10 left them; the drop-retained rule (v1.10.0), the P8 provisional
# seed, the never-fabricate-a-value rule (v1.10.9) and the E-STOP ordering
# (v1.10.8) are carried over untouched. What changed is where things sit on
# the screen, which locomotives can appear on it, and one released-set bug.
#
#   DISCOVERY — there is no roster. A locomotive appears on the console when
#     it is HEARD. LOCO_NAMES is a display lookup and nothing more: an
#     unknown id still gets a column, labelled with its own number, and a
#     fourth or fifth locomotive needs no code change to show up.
#
#     THE DISCOVERY GATE: only a NON-RETAINED message may create a
#     locomotive. A wildcard subscription hands us the broker's entire
#     retained backlog on connect, including locomotives switched off for
#     months; creating on "first message seen" would fill the console with
#     ghost columns at every restart. That is the v1.10.0 stale-tile failure
#     and the v1.10.11 zombie-last-will failure arriving by a new door.
#     Retained messages may UPDATE a locomotive that already exists (the P8
#     seed still works exactly as before); they may never bring one into
#     being.
#
#     A COLUMN NEVER DISAPPEARS. Nothing is ever pruned from loco_state. A
#     locomotive that goes quiet keeps its column and greys — losing the
#     column of the locomotive that most needs watching is not an option.
#
#     ORDER IS STABLE, not arrival order: known names in their listed order,
#     then anything else by id. BEGIN must be where it was yesterday.
#
#   CONSOLE — three commands across the top (E-STOP ALL, END AO, CIRCUIT
#     EXPRESS), then one column per discovered locomotive. Each column:
#     name and a tiny ONLINE/STALE link chip, the mode chip, BEGIN / PAUSE /
#     E-STOP, then QUORUM, then % agreement, then MM / KPH / PWM / V.
#
#     LEAD / TRAIL chips were designed for the chip row and removed before
#     first run: nothing publishes the roles, and deriving them from position
#     is only safe behind an envelope-overlap test (two locomotives on the
#     same piece of track otherwise get labelled confidently and wrongly).
#     The design survives in docs/dashboard-redesign/mockup_v7.html.
#
#     QUORUM OUTRANKS THE LINK. A locomotive can be talking steadily and
#     have no idea where it is; that is the more important fact and it is
#     shown first. EVALUATING is a real third state and is not collapsed
#     into either QUORUM or NO QUORUM — collapsing it up claims a certainty
#     the locomotive has not got, collapsing it down stops a locomotive that
#     is still working it out.
#
#     NOTHING MAY MOVE WHEN A LOCOMOTIVE GOES QUIET. The mode chip holds a
#     fixed row and reads UNKNOWN rather than collapsing to a hyphen; the
#     link chip has a floor width; no status string may wrap.
#     The two columns are meant to be read across, and a column that
#     re-flows as its locomotive gets into trouble breaks exactly when it is
#     needed. A stale locomotive's figures are kept but dimmed: true once,
#     not true now.
#
#   RELEASE SET FIXED — v1.10.11's dispatcher_endcto() released Otto and
#     Toby by hardcoded id and then fanned stop/ across all of LOCO_IDS.
#     Hans was in that tuple, so END AO stopped Hans and never released it.
#     Both halves now walk the discovered set, so the bug cannot recur by
#     someone forgetting to extend a tuple.
#
#   LOCO PAGE — the once-per-session setup (SESSION ORIENTATION, SET
#     LOCATION, AUTO) moves to a STARTUP block at the bottom. The operating
#     controls and the figures own the top of the screen, and E-STOP rises
#     from the eighth card to the fifth.
#
#     Throttle shows COMMANDED beside PWM, both large, so the gap between
#     what was asked for and what the locomotive is doing is one glance.
#
#     DIRECTION is FOR / REV. Neutral is still a state the firmware reports
#     (direction=1) and is still displayed when reported — it is simply no
#     longer commandable from here.
#
#     AGREEMENT IS A ROLLING PERCENTAGE OF THE LAST TEN VERDICTS, not the
#     session total. Field case: 4015 agree / 30 disagree reads 99% while
#     eight of the last ten markers disagreed. The session total is kept
#     underneath in small text. Gaps in the tick row are LABELLED — a gap
#     means markers passed without a verdict, which happens in
#     NAV_EVALUATING and NAV_NO_QUORUM (QUORUM.ino acceptEvent) or across a
#     re-declaration. That gap is the scar of a position incident and the
#     old row threw it away by butting the ticks together.
#
#     REMOVED: CAL RECORDING (superseded by the continuous runlog), the DNA
#     placeholder, the Low V and pKPH tiles, and the MANUAL button (manual
#     is the default and cannot be chosen while auto is in force).
#
#   INA219 IS LIVE. v1.10.11 carried a comment claiming no firmware
#     publishes telem/voltage|current|power. That was stale: QUORUM v1.7
#     restored the service under decision 0012 and publishes all three every
#     5 s, retained. The comment is gone and Voltage/Current/Power are shown
#     on both pages without apology.
#
# ---------------------------------------------------------------------------
# Everything below this line is v1.10.11 and earlier. Its history is intact
# because the rules it records are still in force.
# ---------------------------------------------------------------------------

from flask import Flask, render_template_string, request, jsonify, redirect, url_for, make_response
from markupsafe import Markup
import threading
import time
# subprocess/os/glob went with CAL RECORDING — the continuous runlog
# (server/ngr_runlog.py) supersedes the per-loco mosquitto_sub processes.
import json
import re
import datetime
from collections import deque
import paho.mqtt.client as mqtt_client

app = Flask(__name__)

def is_authenticated(req):
    return True


# ============================================================================
# MQTT
# ============================================================================
MQTT_BROKER = "127.0.0.1"
MQTT_PORT   = 1883

# ---------------------------------------------------------------------------
# THE ROSTER IS NOT A LIST. A locomotive gets a column because it is HEARD,
# not because its id was typed in here. LOCO_NAMES is a DISPLAY lookup and
# nothing more: an id nobody has named still appears, labelled with its own
# number, so a fourth or fifth locomotive needs no code change to show up.
# Adding a name here only gives it a nicer label and a friendlier URL.
# ---------------------------------------------------------------------------
LOCO_NAMES = {
    "9950011": "Otto",
    "9950012": "Toby",
    "2095111": "Hans",
}

# A field older than this is stale: tile grays, gates re-lock. Otto's alert
# and loopstat both broadcast at 1 Hz, so 5 s means five missed heartbeats.
FRESH_S = 5.0

# alert.est_mm_s (layout mm/s) -> prototype km/h. Same convention as the old
# measured-speed path: (mm/ms) * 3.6 * 45.
PKPH_PER_MM_S = 3.6 * 45.0 / 1000.0


def _fresh_state():
    """Everything Otto has not said this session is '--' / UNSET. Nothing here
    is ever seeded from a remembered value."""
    return {
        "online": "0", "throttle": "0", "direction": "--", "brake": "0",
        "estop": "0", "auto": "0", "ce": "0", "lowvolt": "--", "warning": "",
        "voltage": "--", "current": "--", "power": "--", "block": "--",
        "session_dir": "UNSET", "nav_ready": "0", "start_interval": "UNSET",
        # from the 1 Hz alert / loopstat / nav events (QUORUM 1.0 vocabulary;
        # confidence is deleted with the tally navigator and never read)
        "nav": "UNSET", "moving": "--", "pwm": "--", "pkph": "--", "mm": "--",
        "landmark": "", "miss_streak": "--", "viable": [], "candidate_mm": "--",
        "nav_event": "", "nav_event_ts": "",
        # v1.10.2: polarity agreement tally — session counts and the last ten
        # verdicts [[mm, 1|0], ...], from nav AGREE/DISAGREE events, reset on
        # epoch (a fresh state IS the reset).
        "agree_n": 0, "disagree_n": 0, "verdicts": [],
        "sketch": "", "uptime_ms": None,
        # v1.10.10 P4: ENLISTED (autoEnrolled, from state/auto ONLY).
        # Tri-state: "--" = never proven this session; "0"/"1" as reported.
        # st["auto"] remains RUNNING (autoRunning, from the alert ONLY).
        "enlisted": "--",
        # P7: the locomotive's latest command response / station event, raw.
        "station_event": "", "station_note": "", "station_seq": None,
        "station_ts": "",
    }


# Nav states in which Otto's position is usable. QUORUM 1.0 publishes
# NORMAL/EVALUATING/NO_QUORUM/UNSET; legacy TRACKING/LOST (Toby, SOLONAV_2_14)
# is still understood.
USABLE_NAV = ("TRACKING", "NORMAL", "EVALUATING")


# Populated by discovery, NEVER pruned. A locomotive that goes quiet keeps
# its entry and greys on screen: a column vanishing mid-session is how you
# lose sight of the locomotive that most needs watching.
loco_state = {}     # lid -> state dict
loco_rx    = {}     # lid -> {field: time.monotonic() of last LIVE message}
loco_epoch = {}     # lid -> bumped on reboot; client re-locks on change

# v1.10.10 P8 — provisional retained-authority seed, per MQTT connection.
# The v1.10.0 drop-retained rule killed the ghost tiles and stays. But it
# also discards the locomotive's REAL current authority state on every
# reconnect, and the default that stands in is a fabrication (the v1.10.9
# Toby NEUTRAL bug; the 2026-08-07 MANUAL-while-enlisted bug). The firmware
# publishes the governing contract: retained state is interpretable ONLY
# while the retained last-will 'online' flag reads 1. online is a SIBLING
# of state/, delivered in no guaranteed order — so retained authority
# values are held PROVISIONALLY here and promoted into loco_state only
# when online=1 arrives; discarded on online=0. Never _touch()ed: a seed
# is reported state, not a live report. Live messages always supersede.
AUTHORITY_SEED_TOPICS = {
    "state/auto": "enlisted", "state/estop": "estop",
    "state/direction": "direction",
    "state/session_direction": "session_dir",
    "state/start_interval": "start_interval",
}
loco_seed = {}          # lid -> {sub: provisional value}
loco_seed_online = {}   # lid -> None until online seen

mqtt_lock = threading.Lock()
mqtt_conn = None

loco_log = {}   # lid -> deque(maxlen=300)
dispatch_log = deque(maxlen=200)

# Fields whose ages the client renders.
AGE_FIELDS = ("heard", "voltage", "current", "power", "lowvolt", "pwm", "pkph",
              "mm", "nav", "moving", "session_dir", "nav_ready", "start_interval",
              "marker", "throttle", "direction", "estop", "auto", "warning")

# state/<x> payloads copied verbatim into loco_state. The firmware publishes
# these on change (retained), so a live arrival is a confirmation event.
SIMPLE_STATE = {
    "state/throttle": "throttle", "state/direction": "direction",
    "state/brake": "brake", "state/estop": "estop",
    # v1.10.10 P4: state/auto is autoEnrolled — it feeds ENLISTED, never
    # the RUNNING flag. (The alert feeds RUNNING, never ENLISTED.)
    "state/auto": "enlisted",
    "state/lowvolt": "lowvolt", "state/warning": "warning",
    "state/block": "block", "state/ce": "ce",
    "state/session_direction": "session_dir", "state/nav_ready": "nav_ready",
    "state/start_interval": "start_interval",
}


def _touch(lid, *fields):
    now = time.monotonic()
    for f in fields:
        loco_rx[lid][f] = now


def _ensure_loco(lid):
    """Bring a locomotive into being. CALLED ONLY FROM THE LIVE PATH — see the
    discovery gate in on_mqtt_message(). Creating on any message would let the
    broker's retained backlog conjure columns for locomotives switched off
    months ago, which is the v1.10.0 stale-tile failure wearing a new hat."""
    if lid in loco_state:
        return False
    loco_state[lid] = _fresh_state()
    loco_rx[lid] = {}
    loco_epoch[lid] = 0
    loco_seed[lid] = {}
    loco_seed_online[lid] = None
    loco_log[lid] = deque(maxlen=300)
    return True


def _log(lid, subtopic, payload, periodic=False, retained=False):
    ts = datetime.datetime.now().strftime("%H:%M:%S")
    val = ("(retained) " + payload) if retained else payload
    entry = {"ts": ts, "topic": subtopic, "value": val, "p": 1 if (periodic or retained) else 0}
    loco_log[lid].appendleft(entry)
    dispatch_log.appendleft({"ts": ts, "loco": lid, "topic": subtopic, "value": val})


def _reset_session(lid, reason):
    """Otto rebooted (live bootid, or alert uptime went backwards): everything
    he confirmed belonged to the previous boot. Gates re-lock, banner clears."""
    loco_state[lid] = _fresh_state()
    loco_rx[lid] = {}
    loco_epoch[lid] += 1
    _log(lid, "dashboard", "SESSION RESET — %s" % reason)


def _apply_nav_state(lid, nav, mm=None):
    """Central nav bookkeeping: MM renders while Otto's position is usable
    (NORMAL/EVALUATING — position is held while evaluating — or legacy
    TRACKING); nav falling to UNSET re-locks the interval/throttle gates."""
    st = loco_state[lid]
    prev = st["nav"]
    st["nav"] = nav
    _touch(lid, "nav")
    if nav in USABLE_NAV:
        if mm is not None:
            try:
                st["mm"] = "%03d" % int(mm)
                _touch(lid, "mm")
            except (TypeError, ValueError):
                pass
    else:
        st["mm"] = "--"
    if nav == "UNSET" and prev != "UNSET":
        st["nav_ready"] = "0"
        st["start_interval"] = "UNSET"
        _touch(lid, "nav_ready", "start_interval")


def on_mqtt_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("MQTT connected")
        with mqtt_lock:
            for lid in list(loco_state):
                loco_seed[lid].clear()
                loco_seed_online[lid] = None    # P8: seeds are per-connection
        # ONE WILDCARD PER TOPIC FAMILY. The id slot is '+', so a locomotive
        # nobody has ever heard of is already subscribed to before it speaks.
        for pat in ("online", "state/#", "telem/#", "alert", "mm/#",
                    "cmd/#"):   # cmd so the packet log shows every command once
            client.subscribe(f"ngr/loco/+/{pat}")
    else:
        print(f"MQTT connect failed rc={rc}")


def on_mqtt_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore")
    parts = topic.split("/")
    if len(parts) < 4 or parts[0] != "ngr" or parts[1] != "loco":
        return
    lid = parts[2]
    sub = "/".join(parts[3:])
    retained = bool(msg.retain)

    with mqtt_lock:
        if lid not in loco_state:
            # ---- THE DISCOVERY GATE ----
            # A retained message is the BROKER replaying what a locomotive
            # said before it was switched off. It may never bring a column
            # into being, or every restart repopulates the console with
            # locomotives that are not there. A cmd/ echo is this dashboard's
            # own voice and may not conjure one either. Only something live,
            # from the locomotive itself, counts as an arrival.
            if retained or sub.startswith("cmd/"):
                return
            _ensure_loco(lid)
            _log(lid, "dashboard", "DISCOVERED — %s" % LOCO_NAMES.get(lid, lid))
        st = loco_state[lid]

        # Store replays are remembered numbers, not Otto speaking. Log, never
        # render — EXCEPT the P8 provisional authority seed. A retained value
        # is held aside and promoted only if the retained online flag proves
        # the locomotive alive (online=1); a powered-off locomotive keeps its
        # blank. online itself may be retained and is handled here too.
        if retained:
            _log(lid, sub, payload, retained=True)
            if sub == "online":
                loco_seed_online[lid] = payload
                if payload == "1":
                    # Promote everything held provisionally.
                    for k, v in loco_seed[lid].items():
                        st[AUTHORITY_SEED_TOPICS[k]] = v
                else:
                    # Last will fired: the locomotive is gone; seeds are void.
                    loco_seed[lid].clear()
                return
            if sub in AUTHORITY_SEED_TOPICS:
                loco_seed[lid][sub] = payload
                if loco_seed_online[lid] == "1":
                    st[AUTHORITY_SEED_TOPICS[sub]] = payload
            return

        # Commands are the dashboard speaking, not Otto: log them (that is how
        # one-press-one-publish is verified) but they carry no freshness.
        if sub.startswith("cmd/"):
            _log(lid, sub, payload)
            return

        _touch(lid, "heard")
        periodic = False

        if sub == "online":
            st["online"] = payload
            loco_seed_online[lid] = payload          # P8: live online governs too
            if payload == "1":
                for k, v in loco_seed[lid].items():
                    if st.get(AUTHORITY_SEED_TOPICS[k]) in ("--", "", None):
                        st[AUTHORITY_SEED_TOPICS[k]] = v
            else:
                loco_seed[lid].clear()

        elif sub == "state/station":
            # P7: the locomotive's command responses and station events, raw.
            # QUORUM 1.10 adds "seq" on *_REFUSED / STOP_IGNORED (P13).
            try:
                d = json.loads(payload)
                st["station_event"] = str(d.get("event", ""))
                st["station_note"]  = str(d.get("note", ""))
                st["station_seq"]   = d.get("seq")
                st["station_ts"]    = time.strftime("%H:%M:%S")
            except Exception:
                pass

        elif sub == "state/loopstat":
            periodic = True
            try:
                d = json.loads(payload)
                if "pwm" in d:
                    st["pwm"] = str(d["pwm"])
                    _touch(lid, "pwm")
                if "miss_streak" in d:      # QUORUM 1.0; replaces conf, which is not read
                    st["miss_streak"] = str(d["miss_streak"])
                _apply_nav_state(lid, d.get("nav", st["nav"]), d.get("mm"))
            except Exception:
                pass

        elif sub == "alert":
            try:
                d = json.loads(payload)
                periodic = (d.get("reason") == "STATUS")
                # Reboot detection: uptime going backwards means a new boot.
                up = d.get("uptime_ms")
                if isinstance(up, (int, float)):
                    prev_up = st["uptime_ms"]
                    if prev_up is not None and up < prev_up - 5000:
                        _reset_session(lid, "alert uptime reset (%d -> %d ms)" % (prev_up, up))
                        st = loco_state[lid]
                        _touch(lid, "heard")   # this alert IS the new boot speaking
                    st["uptime_ms"] = up
                # v1.10.11: the alert carries the LOCOMOTIVE'S cumulative
                # agree/disagree (since its boot). Those are the authority —
                # the console's event-derived tally could only drift or be
                # wiped. Self-healing after any gap. (Alert never writes
                # authority state — M4 — but tallies are telemetry, not
                # authority.)
                if "agree" in d:    st["agree_n"]    = int(d["agree"])
                if "disagree" in d: st["disagree_n"] = int(d["disagree"])
                if "moving" in d:
                    st["moving"] = str(d["moving"])
                    _touch(lid, "moving")
                if "pwm" in d:
                    st["pwm"] = str(d["pwm"])
                    _touch(lid, "pwm")
                if "est_mm_s" in d:
                    if str(d.get("moving")) == "0":
                        st["pkph"] = "0.0"
                    else:
                        st["pkph"] = "%.1f" % (float(d["est_mm_s"]) * PKPH_PER_MM_S)
                    _touch(lid, "pkph")
                sd = d.get("session_dir")
                if sd in ("CW", "CCW", "UNSET"):
                    st["session_dir"] = sd
                    _touch(lid, "session_dir")
                if "auto" in d:
                    # P4: the alert's auto field is autoRunning. It writes the
                    # RUNNING flag ONLY. The alert stream is never the source
                    # of record for any authority state (spec M4) — writing
                    # st["enlisted"] here is exactly the bug that made AUTO
                    # look dead for two days.
                    st["auto"] = str(d["auto"])
                    _touch(lid, "auto")
                if "viable" in d and isinstance(d["viable"], list):
                    st["viable"] = d["viable"]      # QUORUM candidate offsets
                if "candidate_mm" in d:
                    st["candidate_mm"] = str(d["candidate_mm"])
                lm = d.get("last_confirmed_landmark")
                if lm is not None:
                    st["landmark"] = lm
                nav = d.get("nav")
                if nav:
                    _apply_nav_state(lid, nav, d.get("dead_reckoned_mm"))
                    # nav_ready exactly as QUORUM derives it: a declared
                    # direction and a usable position (EVALUATING included).
                    st["nav_ready"] = "1" if (sd in ("CW", "CCW") and nav in USABLE_NAV) else "0"
                    _touch(lid, "nav_ready")
            except Exception:
                pass

        elif sub == "state/nav":
            try:
                d = json.loads(payload)
                ev = d.get("event", "")
                st["nav_event"] = ev
                st["nav_event_ts"] = datetime.datetime.now().strftime("%H:%M:%S")
                # QUORUM publishes a DIRECTION event on EVERY accepted
                # cmd/direction — including a same-value press, which
                # state/direction (publish-on-change) stays silent about.
                # That event is the per-press confirmation the buttons wait
                # on; the VALUE binding stays the state/direction integer.
                # A DIRECTION event says the direction CHANGED; it does not
                # carry the integer. It may refresh a value we already hold —
                # that is what makes a same-value re-press visible — but it
                # must never mark a value fresh that we have never received,
                # or the dashboard reports a direction the locomotive never
                # sent. (v1.10.9: that is exactly what happened to Toby.)
                if ev == "DIRECTION" and st["direction"] != "--":
                    _touch(lid, "direction")
                # v1.10.2: polarity agreement tally (Change 6). AGREE/DISAGREE
                # ride state/nav on every firmware generation.
                # v1.10.11: these increments are now only smoothing between
                # 1 Hz alerts — the alert's cumulative counts overwrite them
                # every second and are the authority.
                if ev in ("AGREE", "DISAGREE"):
                    ok = 1 if ev == "AGREE" else 0
                    if ok:
                        st["agree_n"] += 1
                    else:
                        st["disagree_n"] += 1
                    try:
                        vmm = int(d.get("mm", -1))
                    except (TypeError, ValueError):
                        vmm = -1
                    st["verdicts"].append([vmm, ok])
                    if len(st["verdicts"]) > 10:
                        st["verdicts"].pop(0)
                sd = d.get("session_dir")
                if sd in ("CW", "CCW", "UNSET"):
                    st["session_dir"] = sd
                    _touch(lid, "session_dir")
                if "miss_streak" in d:
                    st["miss_streak"] = str(d["miss_streak"])
                _apply_nav_state(lid, d.get("state", st["nav"]), d.get("mm"))
            except Exception:
                pass

        elif sub == "state/bootid":
            # v1.10.11 (field finding A, 2026-08-08): a live bootid means Otto
            # CONNECTED — the firmware republishes it on EVERY MQTT connect,
            # not only after a boot. Treating it as a reboot wiped the console
            # session 14+ times in one afternoon of link flapping (agree
            # tallies zeroed, pre-flight lost, AUTO withheld) while the
            # locomotive ran on undisturbed. Reboot detection lives in the
            # alert handler's uptime-regression test, which reconnects can
            # never trip. Here we only take the identity and the fact that he
            # is alive.
            try:
                sketch = json.loads(payload).get("sketch", "")
            except Exception:
                sketch = ""
            st["sketch"] = sketch
            st["online"] = "1"
            _touch(lid, "heard")

        elif sub in SIMPLE_STATE:
            field = SIMPLE_STATE[sub]
            val = payload
            # The firmware publishes "000-000" for no-interval; never show it as set.
            if field == "start_interval" and val == "000-000":
                val = "UNSET"
            periodic = (st[field] == val)   # unchanged republish (older firmware sends 1-2 Hz)
            st[field] = val
            _touch(lid, field)

        elif sub == "telem/voltage":
            st["voltage"] = payload
            _touch(lid, "voltage")
        elif sub == "telem/current":
            st["current"] = payload
            _touch(lid, "current")
        elif sub == "telem/power":
            st["power"] = payload
            _touch(lid, "power")

        elif sub == "mm/marker":
            _touch(lid, "marker")

        elif sub == "mm/speed":
            # Legacy measured-speed topic (older sketches). Live only.
            try:
                d = json.loads(payload)
                if d.get("source") == "SEGMENT_MEASURED" and d.get("pkph") is not None:
                    st["pkph"] = "%.1f" % float(d["pkph"])
                    _touch(lid, "pkph")
            except Exception:
                pass

        _log(lid, sub, payload, periodic=periodic)


def pub(topic, value):
    global mqtt_conn
    if mqtt_conn and mqtt_conn.is_connected():
        mqtt_conn.publish(topic, str(value), retain=False)

def pub_loco(lid, subtopic, value):
    pub(f"ngr/loco/{lid}/cmd/{subtopic}", value)

def pub_dispatcher(subcmd):
    pub(f"ngr/dispatcher/cmd/{subcmd}", "1")


def state_payload(lid):
    with mqtt_lock:
        if lid not in loco_state:
            # Named in LOCO_NAMES but never heard: a blank state, not an
            # error. Blank-until-proven (P8) is the same answer the page
            # gives for a locomotive that has gone quiet.
            st = _fresh_state()
            st["verdicts"] = []
            st["ages"] = {f: None for f in AGE_FIELDS}
            st["epoch"] = 0
            return st
        st = dict(loco_state[lid])
        st["verdicts"] = list(st["verdicts"])   # snapshot under the lock
        rx = dict(loco_rx[lid])
        ep = loco_epoch[lid]
    now = time.monotonic()
    st["ages"] = {f: (round(now - rx[f], 1) if f in rx else None) for f in AGE_FIELDS}
    st["epoch"] = ep
    return st


def mqtt_thread():
    global mqtt_conn
    while True:
        try:
            c = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2, client_id="ngr-flask")
            c.on_connect = on_mqtt_connect
            c.on_message = on_mqtt_message
            mqtt_conn = c
            c.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            c.loop_forever()
        except Exception as e:
            print(f"MQTT error: {e}")
            mqtt_conn = None
        time.sleep(5)

threading.Thread(target=mqtt_thread, daemon=True).start()


# ============================================================================
# Roster helpers — display only. NOT a gate on who may appear.
# ============================================================================
def loco_name(lid):
    return LOCO_NAMES.get(lid, lid)


def loco_slug(lid):
    return LOCO_NAMES.get(lid, lid).lower()


def discovered_order():
    """Stable order: the known names in their listed order, then anything else
    by id. Arrival order would shuffle between sessions, and BEGIN has to be
    where it was yesterday."""
    known = [l for l in LOCO_NAMES if l in loco_state]
    rest = sorted(l for l in loco_state if l not in LOCO_NAMES)
    return known + rest


def resolve_loco(ref):
    """A locomotive may be addressed by id or by known name. An id nobody has
    named is perfectly valid — under discovery the id IS the identity."""
    if ref in loco_state:
        return ref
    low = str(ref).lower()
    for lid in loco_state:
        if loco_slug(lid) == low:
            return lid
    for lid, nm in LOCO_NAMES.items():          # named but not yet heard
        if nm.lower() == low or lid == ref:
            return lid
    return None


# P4 authority, as the console shows it. RUNNING (autoRunning, from the 1 Hz
# alert) and ENLISTED (autoEnrolled, from state/auto) are NOT the same state
# and must never be merged: PAUSE moves RUNNING -> ENLISTED and deliberately
# keeps the locomotive enrolled, so a single "AUTO" would make a PAUSE
# invisible on screen.
def _mode_of(st):
    if st["auto"] == "1":
        return "CE" if st["ce"] == "1" else "RUN"
    if st["enlisted"] == "1":
        return "ENL"
    if st["enlisted"] == "--":
        return "NONE"        # never proven this session (P8 blank-until-proven)
    return "MAN"


# Nav states in which the locomotive has a position it trusts.
_QUORUM_OK = ("TRACKING", "NORMAL")


def _quorum_of(st, heard):
    """The console's quorum indicator. A locomotive we are not hearing gets
    UNKNOWN, never its last value — a stale reading is not a current one."""
    if not heard:
        return "UNKNOWN"
    nav = st["nav"]
    if nav in _QUORUM_OK:
        return "QUORUM"
    if nav == "EVALUATING":
        return "EVALUATING"
    if nav in ("NO_QUORUM", "LOST"):
        return "NO_QUORUM"
    return "UNSET"


def _rolling_agree(st):
    """Rolling percentage over the last ten verdicts, NOT the session total.
    A session reading 4015 agree / 30 disagree shows 99% while eight of the
    last ten markers disagreed; the recent number is the one that matters.
    The session totals are still published for the small print."""
    v = st["verdicts"]
    if not v:
        return None
    return int(round(100.0 * sum(1 for x in v if x[1] == 1) / len(v)))


# ============================================================================
# Nav bar — built from the discovered set, not a hardcoded list
# ============================================================================
NAV_STYLE = """
.nav-bar { display:flex; gap:6px; padding:10px 10px 0; max-width:700px; margin:0 auto;
           flex-wrap:wrap; }
.nav-btn { flex:1; min-width:64px; text-align:center; padding:12px 4px; border-radius:10px;
           background:rgba(40,40,40,0.7); color:#ccc; text-decoration:none;
           font-size:15px; font-weight:bold; border:2px solid #555; }
.nav-btn.active { background:rgba(60,60,60,0.95); color:#fff; border-color:#aaa; }
"""


def nav(active):
    parts = ['<a href="/console" class="nav-btn %s">Console</a>'
             % ("active" if active == "console" else "")]
    with mqtt_lock:
        order = discovered_order()
    for lid in order:
        parts.append('<a href="/loco/%s" class="nav-btn %s">%s</a>'
                     % (loco_slug(lid), "active" if active == lid else "", loco_name(lid)))
    return Markup('<div class="nav-bar">%s</div>' % "".join(parts))


# ============================================================================
# Shared CSS
# ============================================================================
SHARED_CSS = """
body { margin:0; font-family:Arial,sans-serif;
  background:linear-gradient(135deg,#a8a8a8 0%,#d9d9d9 20%,#8f8f8f 50%,#d7d7d7 80%,#9c9c9c 100%);
  background-attachment:fixed; color:#111; padding-bottom:40px; }
.container { max-width:700px; margin:auto; padding:8px; }
.panel { background:rgba(40,40,40,0.92); border-radius:12px; padding:10px 12px;
  margin-bottom:8px; box-shadow:0 3px 8px rgba(0,0,0,0.3); border:2px solid #666; }
.sec-hdr { text-align:center; color:#fff; font-size:17px; font-weight:800;
  letter-spacing:2px; margin-bottom:8px; }
.badge { font-size:13px; font-weight:bold; padding:5px 11px; border-radius:20px; letter-spacing:1px; }
.badge-online  { background:#2a7a2a; color:#baffba; border:1px solid #4fc34f; }
.badge-offline { background:#5a2020; color:#ffb3b3; border:1px solid #b32020; }
.badge-stale   { background:#5a4a10; color:#ffe0a0; border:1px solid #b39020; }
.estop-btn { width:100%; padding:20px; border-radius:50px; border:3px solid #b32020;
  background:rgba(40,40,40,0.92); color:#ff5050; font-size:21px; font-weight:bold;
  letter-spacing:3px; cursor:pointer; text-align:center; display:block; }
.estop-btn.active { background:#b32020; color:#fff; }
.estop-btn:disabled { opacity:0.35; }
.log-hdr { display:flex; justify-content:space-between; align-items:center;
  margin-bottom:6px; gap:6px; }
.log-btn { background:#333; border:1px solid #555; color:#ccc; font-size:12px;
  padding:6px 13px; border-radius:6px; cursor:pointer; font-weight:bold; }
.log-btn.on { background:#1a3a5a; color:#b8e2ff; border-color:#2a6aa8; }
.log-body { color:#fff; background:#0a0a0a; font-size:11px; font-family:monospace;
  padding:8px; border-radius:6px; max-height:260px; overflow-y:auto; }
"""


# ============================================================================
# Dispatcher console
# ============================================================================
CONSOLE_HTML = """<!DOCTYPE html>
<html><head>
<title>NGR Dispatcher Console</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
{{ nav_style }}
{{ shared_css }}
.cmdrow { display:grid; grid-template-columns:1.15fr 1fr 1fr; gap:8px; margin-bottom:12px; }
.cmdrow button { border-radius:10px; padding:16px 4px; font-size:14px; font-weight:bold;
  cursor:pointer; line-height:1.25; letter-spacing:1px;
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.22),0 2px 4px rgba(0,0,0,0.4); }
.b-estopall { background:rgba(30,30,30,0.95); color:#ff5050; border:3px solid #b32020;
  font-size:15px; letter-spacing:1px; }
.b-estopall:hover { background:#8a1515; color:#fff; }
.b-end { background:#7a3a00; color:#ffd080; border:2px solid #aa6000; }
.b-ce  { background:#2d6ea8; color:#fff; border:none; }

.cgrid { display:grid; gap:10px; }
.col-hdr { text-align:center; padding-bottom:7px; border-bottom:1px solid #444; margin-bottom:9px; }
.col-name { color:#fff; font-size:19px; font-weight:bold; letter-spacing:2px; }
/* The tiny link chip rides beside the name, where the width is already spare.
   Both header rows are FIXED HEIGHT so the columns' figures always line up,
   whatever the states say. */
.namerow { display:flex; gap:6px; align-items:center; justify-content:center;
  height:24px; overflow:hidden; }
.chiprow { display:flex; gap:5px; justify-content:center; align-items:center;
  flex-wrap:nowrap; margin-top:4px; height:20px; overflow:hidden; }
/* The chip takes the whole row at a fixed size, so no wording change can
   nudge anything sideways as a locomotive changes state or goes quiet.
   (LEAD / TRAIL chips were designed to share this row — removed 2026-08-12
   because nothing publishes the roles. The design is preserved in
   docs/dashboard-redesign/mockup_v7.html for whenever CTO/Bubble v2 does.) */
.chiprow .mode { flex:1 1 0; text-align:center; white-space:nowrap;
  overflow:hidden; text-overflow:ellipsis; }
.mode { font-size:10px; font-weight:bold; letter-spacing:1px; padding:3px 6px; border-radius:4px; }
.mode.m-run  { background:rgba(42,122,42,0.35); color:#baffba; border:1px solid #4fc34f; }
.mode.m-enl  { background:rgba(30,90,140,0.35); color:#b8e2ff; border:1px solid #2a6aa8; }
.mode.m-man  { background:rgba(60,60,80,0.5);   color:#ccd8ff; border:1px solid #8888aa; }
.mode.m-ce   { background:rgba(45,110,168,0.4); color:#cfe6ff; border:1px solid #2d6ea8; }
.mode.m-none { background:rgba(40,40,40,0.6);   color:#888;    border:1px solid #555; }
.link-tiny { font-size:9px; font-weight:bold; letter-spacing:1px; padding:2px 5px;
  border-radius:3px; white-space:nowrap; min-width:52px; text-align:center; }
.link-tiny.up   { background:rgba(42,122,42,0.22); color:#9ada9a; border:1px solid #3a7a3a; }
.link-tiny.down { background:rgba(140,110,20,0.4); color:#ffe0a0; border:1px solid #b39020; }

.cbtn { border:none; border-radius:9px; padding:15px 4px; width:100%;
  font-size:15px; font-weight:bold; cursor:pointer; margin-bottom:7px; color:#fff;
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.25),0 2px 4px rgba(0,0,0,0.4); }
.btn-go { background:#3fa34d; } .btn-stop { background:#d9a21b; }
.cbtn-estop { background:rgba(30,30,30,0.9); color:#ff5050; border:2px solid #b32020;
  border-radius:9px; padding:13px 4px; width:100%; font-size:14px; font-weight:bold;
  letter-spacing:1px; cursor:pointer; margin-bottom:7px; }
.cbtn-estop.active { background:#b32020; color:#fff; }
/* Status pills. No titles: every value names itself, including the unheard
   case, where a bare dash would say nothing at all. nowrap keeps each one a
   single line so the columns cannot fall out of step. */
.pill { border-radius:6px; padding:9px 4px; margin-bottom:5px; text-align:center; border:1px solid; }
.pill .pv { font-size:17px; font-weight:bold; letter-spacing:1px; line-height:1.15;
  white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.q-ok    { background:rgba(42,122,42,0.35);  border-color:#4fc34f; color:#baffba; }
.q-eval  { background:rgba(140,110,20,0.35); border-color:#d9a21b; color:#ffe0a0; }
.q-bad   { background:rgba(140,30,30,0.45);  border-color:#b32020; color:#ffb3b3; }
.q-unset { background:rgba(40,40,40,0.6);    border-color:#666;    color:#999; }
/* Label to the LEFT of the value, both at the same size: one line per figure
   instead of two, and the digits line up down the pair of columns. */
.readout { display:flex; align-items:baseline; justify-content:space-between; gap:6px;
  background:#222; border-radius:6px; padding:6px 10px; margin-bottom:4px; }
.readout .rl { font-size:23px; color:#9a9a9a; font-weight:bold; font-family:monospace;
  letter-spacing:1px; }
.readout .rv { font-size:23px; font-weight:bold; font-family:monospace; color:#eee; }
.readout .rv.stale { color:#777; }
.station-note { font-size:11px; font-weight:bold; min-height:14px; text-align:center; margin-top:2px; }
.empty-note { color:#aaa; font-size:14px; font-weight:bold; text-align:center;
  padding:26px 10px; line-height:1.6; }
</style></head><body>
{{ nav_html }}
<div class="container">

  <div class="panel">
    <!-- P12: SET broadcasts — everyone stops is right for an emergency.
         CLEAR is per-locomotive (a broadcast clear would execute the clear
         path on locomotives that were never stopped, including a manually
         running one). Displayed E-STOP state derives from retained
         state/estop ONLY — never from the last command sent. -->
    <div class="cmdrow">
      <button class="b-estopall" onclick="dc('estop')">&#9888; E-STOP ALL</button>
      <button class="b-end" onclick="endAo()">END AO</button>
      <button class="b-ce"  onclick="dc('ce')">CIRCUIT EXPRESS</button>
    </div>
    <div class="cgrid" id="cgrid"></div>
  </div>

  <div class="panel">
    <div class="log-hdr">
      <span style="color:#ddd;font-size:13px;font-weight:bold;letter-spacing:1px;">PACKET LOG</span>
      <button class="log-btn" onclick="clearLog()">CLEAR</button>
    </div>
    <div class="log-body" id="dispatch-log">
      <div style="color:#aaa;">Waiting for packets...</div>
    </div>
  </div>

</div>
<script>
var MODE_VIEW = {
  'MAN':  ['MANUAL',   'm-man'],
  'ENL':  ['ENLISTED', 'm-enl'],
  'RUN':  ['RUNNING',  'm-run'],
  'CE':   ['CE',       'm-ce'],
  // A dash is narrow, so a chip that shrinks to one re-centres the row and
  // the whole column shifts as a locomotive goes quiet. UNKNOWN holds the
  // width AND says what it means.
  'NONE': ['UNKNOWN',  'm-none']
};
var QUORUM_VIEW = {
  'QUORUM':     ['QUORUM',       'q-ok'],
  'EVALUATING': ['EVALUATING',   'q-eval'],
  'NO_QUORUM':  ['NO QUORUM',    'q-bad'],
  'UNSET':      ['NOT DECLARED', 'q-unset'],
  'UNKNOWN':    ['UNKNOWN',      'q-unset']
};

function dc(cmd){ fetch('/dispatcher/cmd/'+cmd,{method:'POST'}).catch(e=>console.error(e)); }
function endAo(){
  if(confirm('End auto operations and return every locomotive to manual control?')){
    fetch('/dispatcher/endcto',{method:'POST'}).catch(e=>console.error(e));
  }
}

function esc(s){
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function colHtml(l){
  var stale = !l.heard;
  var st = function(x){ return (x===null||x===undefined||x==='--'||stale) ? ' stale' : ''; };
  var val = function(x){ return (x===null||x===undefined||x==='--') ? '\\u2014' : x; };
  var m = MODE_VIEW[l.mode] || MODE_VIEW['NONE'];
  var q = QUORUM_VIEW[l.quorum] || QUORUM_VIEW['UNKNOWN'];
  var mm = (l.mm && l.mm !== '--') ? l.mm : '\\u2014';
  var kph = (l.pkph && l.pkph !== '--') ? String(Math.round(parseFloat(l.pkph))) : '\\u2014';
  var agr = (l.agree_pct === null || l.agree_pct === undefined) ? '\\u2014' : l.agree_pct;
  var estopped = (l.estop === '1');
  var stn = '';
  if (l.station && l.station.event){
    var refused = l.station.event.indexOf('REFUSED')>=0 || l.station.event==='STOP_IGNORED';
    stn = '<div class="station-note" style="color:'+(refused?'#f88':'#8c8')+';">'+
          esc(l.station.event)+(l.station.seq!=null?(' #'+l.station.seq):'')+'</div>';
  } else { stn = '<div class="station-note"></div>'; }

  return '<div>'+
    '<div class="col-hdr">'+
      '<div class="namerow">'+
        '<span class="col-name">'+esc(l.name).toUpperCase()+'</span>'+
        '<span class="link-tiny '+(l.heard?'up':'down')+'">'+(l.heard?'ONLINE':'STALE')+'</span>'+
      '</div>'+
      '<div class="chiprow">'+
        '<span class="mode '+m[1]+'">'+m[0]+'</span>'+
      '</div>'+
    '</div>'+
    '<button class="cbtn btn-go"   onclick="dc(\\'go/'+l.id+'\\')">BEGIN</button>'+
    '<button class="cbtn btn-stop" onclick="dc(\\'stop/'+l.id+'\\')">PAUSE</button>'+
    '<button class="cbtn-estop'+(estopped?' active':'')+'" onclick="dc(\\''+
      (estopped?'estopclear/':'estopset/')+l.id+'\\')">'+(estopped?'CLEAR':'E-STOP')+'</button>'+
    '<div class="pill '+q[1]+'"><div class="pv">'+q[0]+'</div></div>'+
    '<div class="readout"><span class="rl">%</span><span class="rv'+st(l.agree_pct)+'">'+agr+'</span></div>'+
    '<div class="readout"><span class="rl">MM</span><span class="rv'+st(l.mm)+'">'+mm+'</span></div>'+
    '<div class="readout"><span class="rl">KPH</span><span class="rv'+st(l.pkph)+'">'+kph+'</span></div>'+
    '<div class="readout"><span class="rl">PWM</span><span class="rv'+st(l.pwm)+'">'+val(l.pwm)+'</span></div>'+
    '<div class="readout"><span class="rl">V</span><span class="rv'+st(l.voltage)+'">'+val(l.voltage)+'</span></div>'+
    stn +
    '</div>';
}

function pollStatus(){
  fetch('/dispatcher/state').then(r=>r.json()).then(s=>{
    var locos = s.locos || [];
    var grid = document.getElementById('cgrid');
    if(!locos.length){
      grid.style.gridTemplateColumns = '1fr';
      grid.innerHTML = '<div class="empty-note">No locomotive has been heard '+
        'this session.<br>A column appears when one starts talking.</div>';
      return;
    }
    grid.style.gridTemplateColumns = 'repeat('+locos.length+',1fr)';
    grid.innerHTML = locos.map(colHtml).join('');
  }).catch(e=>{});
}

var logPausedUntil = 0;
function clearLog(){
  document.getElementById('dispatch-log').innerHTML =
    '<div style="color:#aaa;">Cleared \\u2014 resuming in 5s</div>';
  logPausedUntil = Date.now()+5000;
}
function pollLog(){
  if(Date.now() < logPausedUntil) return;
  fetch('/dispatcher/log').then(r=>r.json()).then(entries=>{
    if(!entries.length) return;
    document.getElementById('dispatch-log').innerHTML = entries.map(e=>
      esc(e.ts+'  '+(e.name||e.loco)+'  '+e.topic+'  '+e.value)
    ).join('<br>');
  }).catch(e=>{});
}
setInterval(pollStatus,2000);
setInterval(pollLog,1000);
pollStatus(); pollLog();
</script>
</body></html>"""


# ============================================================================
# Loco page
# ============================================================================
LOCO_HTML = """<!DOCTYPE html>
<html><head>
<title>NGR &mdash; {{ name }}</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
{{ nav_style }}
{{ shared_css }}
.header-wrap { position:relative; display:flex; align-items:center;
  justify-content:flex-end; width:100%; }
.loco-title { position:absolute; left:50%; transform:translateX(-50%);
  font-size:24px; font-weight:bold; color:#fff; letter-spacing:2px;
  text-transform:uppercase; }
.status-line { text-align:center; color:#fff; font-size:18px; font-weight:800;
  letter-spacing:1px; margin-top:10px; min-height:22px; line-height:1.3; }
.status-line.bad  { color:#ffb3b3; }
.status-line.warn { color:#ffe0a0; }
.status-line.ok   { color:#baffba; }
.warning-line { text-align:center; color:#ffc040; font-size:14px; font-weight:bold;
  min-height:16px; margin-top:4px; }
.motion-bar { text-align:center; font-size:26px; font-weight:900; letter-spacing:4px;
  padding:14px 4px; border-radius:10px; margin-top:8px; }
.motion-bar.moving  { background:#7a2508; color:#ffc9a8; border:2px solid #ff7040; }
.motion-bar.stopped { background:#14421a; color:#a8ffa8; border:2px solid #2a7a2a; }
.motion-bar.unknown { background:#333;    color:#ccc;    border:2px solid #777; }

.dir-row { display:flex; gap:8px; }
.dir-btn { flex:1; text-align:center; padding:16px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#ccc;
  font-size:17px; font-weight:bold; cursor:pointer; letter-spacing:1px; }
.dir-btn.active-rev { background:#5a2020; color:#ffb3b3; border-color:#b32020; }
.dir-btn.active-fwd { background:#1a4a1a; color:#7fff7f; border-color:#2a7a2a; }
.dir-btn.locked { opacity:0.35; pointer-events:none; }
/* Neutral is still a state the firmware reports (direction=1). It is no
   longer commandable from here, so it needs somewhere to be SEEN. */
.neutral-chip { text-align:center; margin-top:8px; font-size:14px; font-weight:bold;
  letter-spacing:1px; padding:8px; border-radius:6px; display:none;
  background:#4a4a10; color:#ffd080; border:1px solid #aa8800; }
.gate-note { text-align:center; color:#ffe0a0; font-size:14px; font-weight:bold;
  min-height:17px; margin-top:8px; }

.thr-pair { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-bottom:10px; }
.thr-cell { text-align:center; }
.thr-num { font-size:38px; font-weight:bold; font-family:monospace; line-height:1; }
.thr-num span { font-size:14px; color:#aaa; font-weight:normal; }
.thr-num.stale { color:#777 !important; }
.thr-cap { font-size:12px; font-weight:bold; letter-spacing:2px; margin-top:4px; }
.slider-row { display:flex; align-items:center; gap:8px; }
.slider-lbl { color:#ddd; font-size:15px; font-weight:bold; letter-spacing:1px; min-width:70px; }
.slider-val { font-size:19px; font-weight:bold; min-width:36px; text-align:right; }
.slider-val.green  { color:#4fc34f; }
.slider-val.yellow { color:#ffc040; }
input[type=range] { flex:1; height:30px; cursor:pointer; -webkit-appearance:none;
  appearance:none; border-radius:15px; outline:none; border:none; background:#333; }
input[type=range]::-webkit-slider-thumb { -webkit-appearance:none; appearance:none;
  width:34px; height:34px; border-radius:50%; background:#eee; border:2px solid #aaa;
  cursor:pointer; box-shadow:0 2px 6px rgba(0,0,0,0.5); }
input[type=range]:disabled { opacity:0.35; }
input.interval-slider { height:34px; border-radius:17px;
  background:linear-gradient(to right,#4a2a8a 0%,#7050c0 100%); }

.num-row { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; }
.num-cell { text-align:center; }
.big-num { font-size:46px; font-weight:bold; font-family:monospace; line-height:1; }
.big-num.stale { color:#777 !important; }
.num-lbl { font-size:19px; font-weight:bold; letter-spacing:2px; margin-top:4px; }
.age-chip { font-size:11px; color:#999; min-height:13px; font-weight:bold; text-align:center; }
.mm-landmark { text-align:center; font-size:15px; color:#9fd6ff; font-weight:bold;
  letter-spacing:1px; margin-top:8px; min-height:18px; }
.mm-countdown { text-align:center; font-size:13px; color:#8ec8f0; font-weight:bold;
  letter-spacing:1px; font-style:italic; margin-top:2px; min-height:15px; }
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; }
.telem-cell { background:#222; border-radius:7px; padding:9px 4px; text-align:center; }
.telem-lbl  { font-size:13px; color:#ddd; margin-bottom:3px; font-weight:bold; }
.telem-val  { font-size:21px; font-weight:bold; color:#4fc34f; }
.telem-val.warn  { color:#ffc040; }
.telem-val.stale { color:#777; }
.telem-age  { font-size:10px; color:#999; min-height:12px; font-weight:bold; }
.block-display { text-align:center; color:#f1f1f1; font-size:14px; font-weight:bold;
  margin-top:6px; letter-spacing:2px; }

.tickmm { display:inline-block; min-width:34px; text-align:center; padding:4px 3px;
  border-radius:5px; font-family:monospace; font-size:13px; font-weight:bold; }
.tickmm.a { background:#14421a; color:#7fff7f; border:1px solid #2a7a2a; }
.tickmm.d { background:#5a2020; color:#ffb3b3; border:1px solid #b32020; }
/* A gap in the tick row is not missing data — it is markers that passed
   without a verdict, which happens in NAV_EVALUATING and NAV_NO_QUORUM or
   across a re-declaration. It is the scar of a position incident and is
   worth more than the ticks either side of it. */
.tickgap { display:inline-block; padding:4px 5px; font-size:11px; font-weight:bold;
  color:#ffd080; font-family:monospace; border-radius:5px;
  background:rgba(90,74,16,0.45); border:1px dashed #aa8800; }
.roll-lbl { color:#ddd; font-size:13px; font-weight:bold; letter-spacing:1px;
  margin-top:2px; line-height:1.35; text-align:center; }
.session-note { text-align:center; color:#999; font-size:12px; font-weight:bold; margin-top:10px; }

.startup-rule { text-align:center; color:#ddd; font-size:15px; font-weight:800;
  letter-spacing:4px; margin:22px 0 10px; position:relative; }
.startup-rule:before, .startup-rule:after { content:""; position:absolute; top:50%;
  width:26%; height:2px; background:rgba(40,40,40,0.55); }
.startup-rule:before { left:2%; } .startup-rule:after { right:2%; }
.sdir-row { display:flex; gap:8px; }
.sdir-btn { flex:1; text-align:center; padding:14px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#ccc;
  font-size:15px; font-weight:bold; cursor:pointer; }
.sdir-btn.active { background:#1a3a5a; color:#b8e2ff; border-color:#2a6aa8; }
.sdir-btn.locked { opacity:0.35; pointer-events:none; }
.badge-line { text-align:center; font-size:13px; font-weight:bold; letter-spacing:1px;
  margin-top:8px; padding:7px; border-radius:6px; }
.badge-line.ready    { color:#a8ffa8; background:rgba(42,122,42,0.25); }
.badge-line.notready { color:#ffb3b3; background:rgba(90,32,32,0.25); }
.badge-line.set      { color:#d0b8ff; background:rgba(60,30,100,0.35); }
.badge-line.notset   { color:#bbb;    background:rgba(30,30,30,0.4); }
.badge-line.waiting  { color:#ffe0a0; background:rgba(90,74,16,0.35); }
.interval-display { text-align:center; font-size:30px; font-weight:bold;
  color:#d0b8ff; letter-spacing:3px; margin-bottom:8px; font-family:monospace; }
.interval-display span { font-size:14px; color:#aaa; font-weight:normal; letter-spacing:1px; }
.interval-tick-row { display:flex; justify-content:space-between; color:#999;
  font-size:10px; font-family:monospace; padding:0 2px; margin-top:2px; }
.send-btn { display:block; width:100%; margin-top:10px; padding:13px; border-radius:8px;
  border:2px solid #7050c0; background:rgba(60,30,100,0.5); color:#d0b8ff;
  font-size:16px; font-weight:bold; letter-spacing:1px; cursor:pointer; }
.send-btn:disabled { opacity:0.35; cursor:default; }
.auto-btn { display:block; width:100%; padding:18px; border-radius:10px;
  border:2px solid #4a4; background:rgba(40,70,40,0.6); color:#7f7;
  font-size:19px; font-weight:bold; letter-spacing:3px; cursor:pointer;
  text-align:center; text-decoration:none; }
.auto-btn.on     { background:#1a4a1a; color:#7fff7f; border-color:#2a7a2a; }
.auto-btn.locked { opacity:0.4; pointer-events:none; }
</style></head><body>
{{ nav_html }}
<div class="container">

  <div class="panel">
    <div class="header-wrap">
      <span class="loco-title">{{ name }}</span>
      <span class="badge badge-offline" id="online-badge">OFFLINE</span>
    </div>
    <div id="fault-banner" style="display:none;background:#b32020;color:#fff;
      font-size:13px;font-weight:bold;padding:8px;border-radius:8px;margin-bottom:8px;
      line-height:1.35;word-break:break-word;"></div>
    <div class="status-line" id="status-line">NO TELEMETRY THIS SESSION</div>
    <div class="warning-line" id="warning-line"></div>
    <div class="motion-bar unknown" id="motion-bar">UNKNOWN</div>
  </div>

  <!-- DIRECTION. FOR / REV only; NEUTRAL is reported, not commanded. -->
  <div class="panel" id="panel-dir">
    <div class="sec-hdr">DIRECTION</div>
    <div class="dir-row">
      <button onclick="sendDir(0)" id="dir-btn-0" class="dir-btn">REV</button>
      <button onclick="sendDir(2)" id="dir-btn-2" class="dir-btn">FOR</button>
    </div>
    <div class="neutral-chip" id="neutral-chip">REPORTING NEUTRAL</div>
    <div class="gate-note" id="dir-gate-note"></div>
  </div>

  <!-- THROTTLE. NEVER locked in MANUAL (v1.10.2 operator ruling): undeclared
       operation is signalled by the status line, not by a disabled control.
       The slider is the OPERATOR'S commanded value and never moves by itself;
       the loco's actual PWM is the separate read-only figure beside it. -->
  <div class="panel" id="panel-throttle">
    <div class="sec-hdr">THROTTLE</div>
    <div class="thr-pair">
      <div class="thr-cell">
        <div class="thr-num" id="throttle-display" style="color:#7fff9f;">0<span> / 255</span></div>
        <div class="thr-cap" style="color:#7fff9f;">COMMANDED</div>
      </div>
      <div class="thr-cell">
        <div class="thr-num" id="thr-actual" style="color:#9fd6ff;">&mdash;</div>
        <div class="thr-cap" style="color:#9fd6ff;">PWM <span id="thr-actual-age"
             style="color:#999;font-size:10px;font-weight:normal;"></span></div>
      </div>
    </div>
    <div class="slider-row">
      <input type="range" min="0" max="255" value="0" step="1"
             id="throttle-slider" oninput="sendThrottle(this.value)" />
      <span class="slider-val green" id="thr-val">0</span>
    </div>
    <div class="gate-note" id="throttle-gate-note"></div>
  </div>

  <div class="panel">
    <div class="slider-row">
      <span class="slider-lbl">BRAKE</span>
      <input type="range" min="0" max="255" value="0" step="1"
             id="brake-slider" style="direction:rtl;"
             oninput="sendCmd('brake',this.value);
                      document.getElementById('brake-val').textContent=this.value" />
      <span class="slider-val yellow" id="brake-val">0</span>
    </div>
  </div>

  <!-- E-STOP: manual-chamber control (R4/P5), never gated within it. -->
  <div class="panel">
    <button onclick="toggleEstop()" id="estop-btn" class="estop-btn">E-STOP</button>
  </div>

  <div class="panel">
    <div class="num-row">
      <div class="num-cell">
        <div class="big-num stale" id="mm-display" style="color:#9fd6ff;">&mdash;</div>
        <div class="age-chip" id="mm-display-age"></div>
        <div class="num-lbl" style="color:#9fd6ff;">MM</div>
      </div>
      <div class="num-cell">
        <div class="big-num stale" id="kph-display" style="color:#ffd080;">&mdash;</div>
        <div class="age-chip" id="kph-display-age"></div>
        <div class="num-lbl" style="color:#ffd080;">KPH</div>
      </div>
      <div class="num-cell">
        <div class="big-num stale" id="pwm-display" style="color:#7fff9f;">&mdash;</div>
        <div class="age-chip" id="pwm-display-age"></div>
        <div class="num-lbl" style="color:#7fff9f;">PWM</div>
      </div>
    </div>
    <div class="mm-landmark" id="mm-landmark"></div>
    <div class="mm-countdown" id="mm-countdown"></div>
  </div>

  <!-- INA219 telemetry. QUORUM v1.7 restored the service (decision 0012) and
       publishes all three every 5 s, retained. -->
  <div class="panel">
    <div class="telem-grid">
      <div class="telem-cell">
        <div class="telem-lbl">Voltage</div>
        <div class="telem-val stale" id="telem-voltage">&mdash;</div>
        <div class="telem-age" id="telem-voltage-age"></div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">Current</div>
        <div class="telem-val stale" id="telem-current">&mdash;</div>
        <div class="telem-age" id="telem-current-age"></div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">Power</div>
        <div class="telem-val stale" id="telem-power">&mdash;</div>
        <div class="telem-age" id="telem-power-age"></div>
      </div>
    </div>
    <div class="block-display" id="block-display"></div>
  </div>

  <div class="panel">
    <div class="sec-hdr">POLARITY AGREEMENT</div>
    <div style="display:flex;justify-content:space-around;align-items:center;">
      <div style="text-align:center;">
        <div class="big-num stale" id="agree-pct" style="font-size:44px;">&mdash;</div>
        <div class="roll-lbl">AGREE %<br><span style="color:#9fd6ff;">LAST 10 VERDICTS</span></div>
      </div>
      <div style="text-align:left;color:#ddd;font-size:15px;font-weight:bold;line-height:1.7;">
        <span id="agree-n" style="color:#7fff7f;font-family:monospace;">0</span> agree<br>
        <span id="disagree-n" style="color:#ff8080;font-family:monospace;">0</span> disagree
      </div>
    </div>
    <div id="verdict-ticks" style="display:flex;gap:5px;justify-content:center;
      margin-top:10px;flex-wrap:wrap;align-items:center;min-height:34px;"></div>
    <div class="session-note" id="session-note"></div>
  </div>

  <!-- ==================== STARTUP ==================== -->
  <div class="startup-rule">STARTUP</div>

  <div class="panel" id="panel-sdir">
    <div class="sec-hdr">SESSION ORIENTATION</div>
    <div class="sdir-row">
      <button onclick="sendSessionDir('CW')"  id="sdir-cw"  class="sdir-btn">CLOCKWISE</button>
      <button onclick="sendSessionDir('CCW')" id="sdir-ccw" class="sdir-btn">COUNTER-CLOCKWISE</button>
    </div>
    <div class="badge-line notready" id="navready-badge">SESSION DIRECTION NOT CONFIRMED THIS SESSION</div>
  </div>

  <div class="panel" id="panel-interval">
    <div class="sec-hdr">SET LOCATION</div>
    <div class="interval-display" id="interval-display">
      <span style="color:#999;">&mdash; slide to select &mdash;</span>
    </div>
    <input type="range" min="0" max="170" step="1" value="85"
           id="interval-slider" class="interval-slider"
           oninput="onIntervalSlide(this.value)" />
    <div class="interval-tick-row">
      <span>000</span><span>017</span><span>034</span><span>051</span>
      <span>068</span><span>085</span><span>102</span><span>119</span>
      <span>136</span><span>153</span><span>170</span>
    </div>
    <button class="send-btn" id="interval-send" onclick="sendInterval()">SET LOCATION</button>
    <div class="gate-note" id="interval-gate-note"></div>
    <div class="badge-line notset" id="interval-badge">NO LOCATION CONFIRMED THIS SESSION</div>
  </div>

  <!-- AUTO only. Manual is the default and cannot be chosen from here while
       auto is in force; P9 keeps release on the dispatcher console. -->
  <div class="panel">
    <a href="/loco/{{ slug }}/mode/1" class="auto-btn" id="auto-btn">AUTO</a>
    <div class="gate-note" id="auto-gate-note"></div>
  </div>

  <div class="panel">
    <div class="log-hdr">
      <span style="color:#ddd;font-size:13px;font-weight:bold;letter-spacing:1px;">PACKET LOG</span>
      <span>
        <button class="log-btn" id="log-filter" onclick="toggleLogFilter()" style="display:none;">SHOW PERIODIC</button>
        <button class="log-btn" id="log-clear"  onclick="clearLog('loco-log')" style="display:none;">CLEAR</button>
        <button class="log-btn" id="log-toggle" onclick="toggleLogOpen()">EXPAND</button>
      </span>
    </div>
    <div class="log-body" id="loco-log" style="display:none;"></div>
  </div>

</div>
<script>
// ==========================================================================
// All rendering below is driven by /loco/{{ slug }}/state — the ACTIVE TAB's
// locomotive, and nothing else. v1.10.2 operator ruling: the dashboard must
// never say no to a manual operator. Controls are disabled only by the AUTO
// chamber (dispatcher control); everything else is loud truth in the status
// line. Declared/undeclared state MIRRORS the locomotive's reported nav
// state; the dashboard never generates a re-declaration requirement itself.
// ==========================================================================
// ---- FAULT BANNER (v1.10.6) -----------------------------------------------
// There is no console on a phone in a garden. An exception inside pollState()
// would stop every later DOM update and leave the operator nothing to report
// but "it broke". Paint it on the page instead.
function showFault(msg){
  try {
    var b = document.getElementById('fault-banner');
    if(!b) return;
    var t = new Date().toTimeString().slice(0,8);
    b.style.display = '';
    b.textContent = '\\u26a0 DASHBOARD FAULT ' + t + ' \\u2014 ' + msg +
                    '  (controls still work; reload to clear)';
  } catch(e) {}
}
window.addEventListener('error', function(e){
  showFault((e.message||'error') + ' @' + (e.lineno||'?'));
});
window.addEventListener('unhandledrejection', function(e){
  showFault('promise: ' + (e.reason && e.reason.message ? e.reason.message : e.reason));
});

// A dropped request is ORDINARY. An exception thrown inside our own render
// code is not: that is the fault that makes the page look frozen. Only the
// second kind earns a banner, or the banner becomes noise and stops being
// read. (CODEX review of v1.10.6: a caught rejection fires neither
// window.onerror nor unhandledrejection.)
function pollFail(where, e){
  var m = (e && e.message) ? e.message : String(e);
  if (/failed to fetch|load failed|networkerror|network request failed|aborted/i.test(m)) return;
  showFault(where + ' \\u2014 ' + m);
}

var SLUG = '{{ slug }}';
var LOCO = '{{ name }}'.toUpperCase();
var STALE_S = 5;
var isCto = false;
var lastEpoch = null;
// v1.10.4 (BUG 2): the last direction the OPERATOR commanded. Display memory
// only — nothing is gated on it. It keeps the button lit until the locomotive
// actually reports a direction, so a missing echo can never blank the
// selection or let it fall back to a default.
var lastCommandedDir = null;

function ageOf(s, f){
  var a = s.ages ? s.ages[f] : null;
  return (a === null || a === undefined) ? null : a;
}
function isFresh(s, f){ var a = ageOf(s, f); return a !== null && a <= STALE_S; }

function setTile(id, val, age){
  var el = document.getElementById(id);
  var ageEl = document.getElementById(id + '-age');
  if(!el) return;
  if(age === null || val === null || val === undefined || val === '--' || val === ''){
    el.innerHTML = '&mdash;';
    el.classList.add('stale');
    if(ageEl) ageEl.textContent = '';
  } else if(age > STALE_S){
    el.textContent = val;
    el.classList.add('stale');
    if(ageEl) ageEl.textContent = Math.round(age) + ' s ago';
  } else {
    el.textContent = val;
    el.classList.remove('stale');
    if(ageEl) ageEl.textContent = '';
  }
}

// ---- Location slider: purely local until SET LOCATION is pressed ----
var pendingInterval = null;
var pendingSend = null;
var MM_TOTAL = 171;

function mmToInterval(mm) {
  var lo = mm % MM_TOTAL;
  var hi = (lo + 1) % MM_TOTAL;
  return ('000'+lo).slice(-3) + '-' + ('000'+hi).slice(-3);
}
function onIntervalSlide(val) {
  var mm = parseInt(val);
  pendingInterval = mmToInterval(mm);
  var lo = ('000'+mm).slice(-3);
  var hi = ('000'+((mm+1)%MM_TOTAL)).slice(-3);
  document.getElementById('interval-display').innerHTML =
    '<b>'+lo+'</b><span> \\u2014 </span><b>'+hi+'</b>';
}
function sendInterval() {
  var btn = document.getElementById('interval-send');
  if (btn.disabled) return;                  // AUTO chamber only
  if (!pendingInterval) {
    var b = document.getElementById('interval-badge');
    b.textContent = 'SLIDE TO SELECT A LOCATION FIRST';
    b.className = 'badge-line notset';
    return;
  }
  if (pendingSend && Date.now() - pendingSend.sentAt < 1000) return;  // double-tap guard
  pendingSend = { interval: pendingInterval, sentAt: Date.now() };
  var bb = document.getElementById('interval-badge');
  bb.textContent = 'WAITING FOR CONFIRMATION\\u2026';
  bb.className = 'badge-line waiting';
  fetch('/loco/'+SLUG+'/startinterval/'+pendingSend.interval, {method:'POST'})
    .catch(e => console.error(e));
}

// ---- Station MM countdown ----
var MM_STATIONS = [
  { name: "Southpoint", mm: 0   },
  { name: "Patio",      mm: 15  },
  { name: "Grillers",   mm: 63  },
  { name: "Westpoint",  mm: 72  },
  { name: "Northpoint", mm: 98  },
  { name: "Arches",     mm: 108 },
  { name: "Eastpoint",  mm: 140 },
  { name: "Bamboo",     mm: 157 }
];
function mmCountdown(currentMmStr) {
  var cur = parseInt(currentMmStr, 10);
  if (isNaN(cur)) return '';
  for (var i = 0; i < MM_STATIONS.length; i++) {
    var st = MM_STATIONS[i];
    var ahead = (st.mm - cur + MM_TOTAL) % MM_TOTAL;
    if (ahead === 0) return 'AT ' + st.name.toUpperCase();
    if (ahead > 0 && ahead <= 5) return st.name + ' \\u2192 ' + ahead;
  }
  return '';
}

// The whole manual command path: read the value, publish it. The isCto test
// is the CHAMBER boundary (dispatcher in control), not a manual gate — in
// MANUAL it is always false. Nothing else may be added here.
function sendCmd(sub, val) {
  if (isCto) return;
  fetch('/loco/'+SLUG+'/cmd/'+sub+'/'+encodeURIComponent(val),{method:'POST'})
    .catch(e=>console.error(e));
}

// THROTTLE — publishes immediately, but never stacks requests (v1.10.6).
// A finger drag emits ~60 input events a second; earlier versions fired a
// fetch for every one and iOS queued the surplus behind pollState. NOT a
// debounce and NOT a confirmation wait: the first move goes out at once, and
// if one is in flight the NEWEST value is held and sent when it returns.
var thrInFlight = false, thrPending = null;
function sendThrottle(v) {
  document.getElementById('thr-val').textContent = v;
  document.getElementById('throttle-display').innerHTML = v + '<span> / 255</span>';
  if (isCto) return;
  if (thrInFlight) { thrPending = v; return; }
  thrInFlight = true;
  fetch('/loco/'+SLUG+'/cmd/throttle/' + encodeURIComponent(v), {method:'POST'})
    .catch(function(e){ console.error(e); })
    .then(function(){
      thrInFlight = false;
      if (thrPending !== null) { var p = thrPending; thrPending = null; sendThrottle(p); }
    });
}

// E-STOP: fires in every state including UNSET, LOST and stale.
function toggleEstop() {
  var btn = document.getElementById('estop-btn');
  var isActive = btn.classList.contains('active');
  if (isActive) {
    btn.textContent = 'E-STOP';
    btn.classList.remove('active');
  } else {
    btn.textContent = 'E-STOP ACTIVE \\u2014 TAP TO CLEAR';
    btn.classList.add('active');
  }
  // E-STOP GOES FIRST, always. Nothing is added ahead of this line.
  fetch('/loco/'+SLUG+'/cmd/estop/' + (isActive ? '0' : '1'), {method:'POST'})
    .catch(e => {
      console.error(e);
      if (isActive) {
        btn.textContent = 'E-STOP ACTIVE \\u2014 TAP TO CLEAR';
        btn.classList.add('active');
      } else {
        btn.textContent = 'E-STOP';
        btn.classList.remove('active');
      }
    });
  // BUG 1 (SAFETY): zero the throttle behind it. A slider still showing 120
  // after an E-STOP is a loaded gun — clearing E-STOP could resume at speed.
  zeroThrottle();
}

// Force the throttle control to 0 and publish it. Used by E-STOP only.
function zeroThrottle() {
  var sl = document.getElementById('throttle-slider');
  if (sl) sl.value = 0;
  var v = document.getElementById('thr-val');
  if (v) v.textContent = '0';
  var d = document.getElementById('throttle-display');
  if (d) d.innerHTML = '0<span> / 255</span>';
  // AN E-STOP SUPERSEDES ANYTHING THE COALESCER IS HOLDING (v1.10.8). If a
  // request is still outstanding, queue a TRAILING ZERO rather than merely
  // dropping the pending value: the in-flight command may still reach the
  // broker after ours, so the last throttle published must be 0 whichever
  // way the ordering falls.
  thrPending = thrInFlight ? '0' : null;
  fetch('/loco/'+SLUG+'/cmd/throttle/0', {method:'POST'}).catch(e => console.error(e));
}

// TAP PUBLISHES. Nothing between the tap and the fetch — no bounce guard, no
// pending state, no confirmation wait. The button shows it registered the tap
// AT ONCE (optimistic); the locomotive's confirmed echo re-renders it
// passively in pollState().
function sendDir(d) {
  if (isCto) return;
  fetch('/loco/'+SLUG+'/cmd/direction/'+d, {method:'POST'}).catch(e => console.error(e));
  lastCommandedDir = String(d);
  updateDirButtons(d, true);
}

function sendSessionDir(d) {
  if (isCto) return;
  fetch('/loco/'+SLUG+'/sessiondir/'+d, {method:'POST'}).catch(e => console.error(e));
  // No optimistic highlight: the button lights only on this loco's confirmed
  // state/session_direction.
}

// Illumination = THIS loco's confirmed state/direction. NEUTRAL (1) has no
// button any more, so it is shown as a chip instead of silently lighting
// nothing — a reported state must remain visible even when it cannot be
// commanded.
function updateDirButtons(d, confirmed) {
  var ds = String(d);
  var rev = document.getElementById('dir-btn-0');
  var fwd = document.getElementById('dir-btn-2');
  rev.classList.remove('active-rev');
  fwd.classList.remove('active-fwd');
  if (confirmed && ds === '0') rev.classList.add('active-rev');
  if (confirmed && ds === '2') fwd.classList.add('active-fwd');
  document.getElementById('neutral-chip').style.display =
    (confirmed && ds === '1') ? 'block' : 'none';
}

function fmt1(v){ var n=parseFloat(v); return isNaN(n)?v:n.toFixed(1); }

function resetLocalSession(){
  // The LOCOMOTIVE rebooted (its epoch bumped): drop everything staged
  // locally. This is the only dashboard-side reset there is.
  pendingSend = null;
  lastCommandedDir = null;
  document.getElementById('dir-gate-note').textContent = '';
  var b = document.getElementById('interval-badge');
  b.textContent = 'NO LOCATION DECLARED';
  b.className = 'badge-line notset';
}

// Rolling agreement over the last ten verdicts. The session totals are still
// shown, in small print underneath, because they are what the locomotive
// itself is counting — but the headline number is the recent one.
function renderAgreement(s){
  var v = s.verdicts || [];
  var pctEl = document.getElementById('agree-pct');
  if (v.length) {
    var agree = 0;
    for (var i=0;i<v.length;i++) if (v[i][1] === 1) agree++;
    var pct = Math.round(100*agree/v.length);
    pctEl.textContent = pct;
    pctEl.classList.remove('stale');
    pctEl.style.color = pct>=90 ? '#7fff7f' : (pct>=70 ? '#ffd080' : '#ff8080');
    document.getElementById('agree-n').textContent = agree;
    document.getElementById('disagree-n').textContent = v.length - agree;
  } else {
    pctEl.innerHTML = '&mdash;';
    pctEl.classList.add('stale');
    pctEl.style.color = '';
    document.getElementById('agree-n').textContent = '0';
    document.getElementById('disagree-n').textContent = '0';
  }

  // Ticks oldest first, with any gap in mm named. A gap means markers passed
  // without a verdict — nav was evaluating or without quorum, or position was
  // re-declared. Butting the ticks together threw that away.
  //
  // st["verdicts"] is ALREADY oldest-first: the handler appends and pops(0).
  // Reversing it here computed every step backwards round the loop and
  // reported "168 silent" between two consecutive markers.
  var out = [], asc = v;
  for (var j=0;j<asc.length;j++){
    if (j>0){
      var step = (asc[j][0] - asc[j-1][0] + MM_TOTAL) % MM_TOTAL;
      if (asc[j][0] >= 0 && asc[j-1][0] >= 0 && step > 1){
        out.push('<span class="tickgap">'+(step-1)+' silent</span>');
      }
    }
    var mm = (asc[j][0] >= 0) ? ('000'+asc[j][0]).slice(-3) : '???';
    out.push('<span class="tickmm '+(asc[j][1]===1?'a':'d')+'">'+mm+'</span>');
  }
  document.getElementById('verdict-ticks').innerHTML = out.join('');

  var tot = (s.agree_n||0) + (s.disagree_n||0);
  document.getElementById('session-note').textContent = tot
    ? ('session total: ' + (s.agree_n||0) + ' agree / ' + (s.disagree_n||0) +
       ' disagree \\u2014 ' + Math.round(100*(s.agree_n||0)/tot) + '%')
    : '';
}

function pollState(){
  fetch('/loco/'+SLUG+'/state').then(r=>r.json()).then(s=>{
    // ---- Session epoch: the loco rebooted -> local staging clears ----
    if (lastEpoch === null) { lastEpoch = s.epoch; }
    else if (s.epoch !== lastEpoch) { lastEpoch = s.epoch; resetLocalSession(); }

    // P4: greying and authority display key on ENLISTED (state/auto =
    // autoEnrolled). s.auto is RUNNING (autoRunning, from the alert) and is
    // shown as activity, never used for authority.
    isCto = (s.enlisted === '1');
    var isRunning = (s.auto === '1');

    var heardAge = ageOf(s, 'heard');
    var alive = heardAge !== null && heardAge <= STALE_S;

    var badge = document.getElementById('online-badge');
    if (alive) { badge.textContent='ONLINE'; badge.className='badge badge-online'; }
    else if (heardAge !== null) { badge.textContent='STALE'; badge.className='badge badge-stale'; }
    else { badge.textContent='OFFLINE'; badge.className='badge badge-offline'; }

    var motion = !isFresh(s,'moving') ? 'UNKNOWN' : (s.moving==='1' ? 'MOVING' : 'STOPPED');
    var mb = document.getElementById('motion-bar');
    if (motion==='MOVING')      { mb.textContent='MOVING';  mb.className='motion-bar moving'; }
    else if (motion==='STOPPED'){ mb.textContent='STOPPED'; mb.className='motion-bar stopped'; }
    else { mb.textContent = heardAge===null ? 'UNKNOWN \\u2014 NO TELEMETRY' : 'UNKNOWN \\u2014 STALE';
           mb.className='motion-bar unknown'; }

    var declared = (s.nav==='TRACKING' || s.nav==='NORMAL' || s.nav==='EVALUATING');
    var sdirConfirmed = (s.session_dir==='CW' || s.session_dir==='CCW')
                        && ageOf(s,'session_dir') !== null;
    var intervalConfirmed = s.start_interval && s.start_interval!=='UNSET'
                        && ageOf(s,'start_interval') !== null;
    var navReady = s.nav_ready === '1' && declared;

    // ---- Location confirmation round-trip (informative only) ----
    if (pendingSend) {
      if (intervalConfirmed && s.start_interval === pendingSend.interval) {
        pendingSend = null;
      } else if (Date.now() - pendingSend.sentAt > 12000) {
        pendingSend = null;
        var ib0 = document.getElementById('interval-badge');
        ib0.textContent = 'NO CONFIRMATION \\u2014 COMMAND MAY NOT HAVE ARRIVED';
        ib0.className = 'badge-line notset';
      }
    }
    var ibadge = document.getElementById('interval-badge');
    if (pendingSend) {
      ibadge.textContent = 'WAITING FOR CONFIRMATION\\u2026';
      ibadge.className = 'badge-line waiting';
    } else if (intervalConfirmed) {
      ibadge.textContent = 'LOCATION SET \\u2014 ' + s.start_interval + ' \\u2014 CONFIRMED';
      ibadge.className = 'badge-line set';
    } else if (declared) {
      ibadge.textContent = 'POSITION DECLARED';
      ibadge.className = 'badge-line set';
    } else if (ibadge.className.indexOf('waiting') !== -1) {
      ibadge.textContent = 'NO LOCATION DECLARED';
      ibadge.className = 'badge-line notset';
    }

    // ---- SET LOCATION: a REMINDER, never a refusal (v1.10.5). The operator
    // decides when it is safe to declare. AUTO remains a chamber boundary,
    // not a manual gate. ----
    var intervalGateNote = '';
    if (isCto)                     intervalGateNote = 'AUTO \\u2014 DISPATCHER IN CONTROL';
    else if (motion === 'MOVING')  intervalGateNote = LOCO + ' REPORTS MOVING \\u2014 CONFIRM STOPPED BEFORE DECLARING';
    else if (motion === 'UNKNOWN') intervalGateNote = 'MOTION UNKNOWN \\u2014 CONFIRM ' + LOCO + ' IS STOPPED BEFORE DECLARING';
    else if (pendingSend)          intervalGateNote = 'WAITING FOR CONFIRMATION\\u2026';
    document.getElementById('interval-gate-note').textContent = intervalGateNote;
    document.getElementById('interval-send').disabled = isCto;
    document.getElementById('interval-slider').disabled = isCto;

    // ---- MANUAL controls are NEVER locked (v1.10.2). AUTO is the only
    // disable; it is a chamber, not a gate. ----
    document.getElementById('throttle-slider').disabled = isCto;
    document.getElementById('brake-slider').disabled = isCto;
    document.getElementById('throttle-gate-note').textContent =
      isCto ? 'AUTO \\u2014 DISPATCHER IN CONTROL' : '';

    document.getElementById('sdir-cw').classList.toggle('active',  sdirConfirmed && s.session_dir==='CW');
    document.getElementById('sdir-ccw').classList.toggle('active', sdirConfirmed && s.session_dir==='CCW');
    document.getElementById('sdir-cw').classList.toggle('locked',  isCto);
    document.getElementById('sdir-ccw').classList.toggle('locked', isCto);
    var navBadge = document.getElementById('navready-badge');
    if (navReady) {
      navBadge.textContent = 'NAV READY \\u2014 ' + s.session_dir;
      navBadge.className = 'badge-line ready';
    } else if (sdirConfirmed) {
      navBadge.textContent = 'DIRECTION ' + s.session_dir + ' CONFIRMED';
      navBadge.className = 'badge-line notready';
    } else {
      navBadge.textContent = 'SESSION DIRECTION NOT CONFIRMED THIS SESSION';
      navBadge.className = 'badge-line notready';
    }

    // ---- Motor direction: PASSIVE DISPLAY ONLY. Lit from this loco's
    // reported state/direction integer. Nothing waits on it and no control's
    // behaviour depends on it — an echo that never arrives costs a light,
    // never a command. "--" means we have never received one, which happens
    // routinely: the firmware publishes state/direction RETAINED and this
    // dashboard drops retained messages, so a console restart loses it until
    // the locomotive next changes direction. Treating "never heard" as a
    // report is what made Toby appear to sit in NEUTRAL (v1.10.9).
    var dirReported = ageOf(s,'direction') !== null && s.direction !== '--';
    if (dirReported) updateDirButtons(s.direction, true);
    else if (lastCommandedDir !== null) updateDirButtons(lastCommandedDir, true);
    else updateDirButtons(null, false);

    var dirNote = document.getElementById('dir-gate-note');
    if (dirReported && lastCommandedDir !== null && s.direction !== lastCommandedDir) {
      var nm = {'0':'REVERSE','1':'NEUTRAL','2':'FORWARD'};
      var want = nm[lastCommandedDir] || lastCommandedDir;
      var got  = nm[s.direction] || s.direction;
      var why  = (motion === 'MOVING')
               ? ' \\u2014 STOP FIRST: DIRECTION IS REFUSED WHILE MOVING'
               : (isCto ? ' \\u2014 AUTO IS IN CONTROL' : ' \\u2014 CHECK THE WARNING LINE');
      dirNote.textContent = want + ' NOT ACCEPTED, ' + LOCO + ' REPORTS ' + got + why;
    } else if (!dirReported && lastCommandedDir !== null) {
      dirNote.textContent = LOCO + ' HAS NOT REPORTED ITS DIRECTION \\u2014 SHOWING YOUR LAST COMMAND';
    } else {
      dirNote.textContent = '';
    }
    document.getElementById('dir-btn-0').classList.toggle('locked', isCto);
    document.getElementById('dir-btn-2').classList.toggle('locked', isCto);

    // ---- AUTO (P5/P6/P9/R8) ----
    // Rendered from REPORTED state only. Successful enlistment = AUTO lights
    // and the manual controls grey; a refusal = nothing changes (R8: the
    // absence of change IS the refusal signal). No release control here (P9)
    // — END lives on the dispatcher console.
    var autoBtn = document.getElementById('auto-btn');
    var autoNote = document.getElementById('auto-gate-note');
    // P6: withhold the enlistment request until pre-flight is supplied.
    var preflit = (s.session_dir==='CW'||s.session_dir==='CCW') &&
                  (s.start_interval && s.start_interval!=='UNSET' && s.start_interval!=='000-000');
    if (isCto) {
      autoBtn.className = 'auto-btn on locked';
      autoBtn.removeAttribute('href');
      autoBtn.textContent = isRunning ? 'AUTO \\u2014 RUNNING' : 'ENLISTED';
      autoNote.textContent = isRunning
        ? 'Dispatcher holds this locomotive. Release is on the dispatcher console.'
        : 'Dispatcher holds this locomotive. Release is on the dispatcher console.';
    } else {
      autoBtn.textContent = 'AUTO';
      if (preflit) {
        autoBtn.className = 'auto-btn';
        autoBtn.setAttribute('href', '/loco/'+SLUG+'/mode/1');
        autoNote.textContent = '';
      } else {
        autoBtn.className = 'auto-btn locked';
        autoBtn.removeAttribute('href');   // P6: request withheld, not refused
        autoNote.textContent = 'AUTO needs startup: set ORIENTATION (CW/CCW), then the location.';
      }
    }

    // ---- E-STOP control (R4/P5): manual-chamber only ----
    var esBtn = document.getElementById('estop-btn');
    if (esBtn) {
      esBtn.disabled = isCto;
      esBtn.title = isCto ? 'Enlisted \\u2014 E-STOP from the dispatcher console' : '';
      if (s.estop === '1') {
        esBtn.textContent = 'E-STOP ACTIVE \\u2014 TAP TO CLEAR';
        esBtn.className = 'estop-btn active';
      } else {
        esBtn.textContent = 'E-STOP';
        esBtn.className = 'estop-btn';
      }
    }

    // ---- R13: the slider zeroes on E-STOP so a clear cannot re-command a
    // stale value. The one sanctioned exception to "the slider never moves by
    // itself" (v1.10.2), by operator ruling. ----
    if (s.estop === '1') {
      var sl = document.getElementById('throttle-slider');
      if (sl && sl.value !== '0') { sl.value = 0; sendThrottle(0); }
    }

    // ---- Actual PWM, beside the commanded number (the slider is never
    // written from here) ----
    var actEl = document.getElementById('thr-actual');
    var actAge = document.getElementById('thr-actual-age');
    var pwmAge = ageOf(s,'pwm');
    if (pwmAge === null || s.pwm === '--') {
      actEl.innerHTML = '&mdash;'; actEl.style.color = '#777'; actAge.textContent = '';
    } else if (pwmAge > STALE_S) {
      actEl.textContent = s.pwm; actEl.style.color = '#777';
      actAge.textContent = Math.round(pwmAge) + 's ago';
    } else {
      actEl.textContent = s.pwm; actEl.style.color = '#9fd6ff'; actAge.textContent = '';
    }

    // ---- Telemetry (INA219 restored at QUORUM v1.7, decision 0012) ----
    setTile('telem-voltage', fmt1(s.voltage), ageOf(s,'voltage'));
    setTile('telem-current', fmt1(s.current), ageOf(s,'current'));
    setTile('telem-power',   fmt1(s.power),   ageOf(s,'power'));
    document.getElementById('telem-voltage').classList.toggle('warn',
      s.lowvolt === '1' && isFresh(s,'lowvolt'));
    document.getElementById('block-display').textContent =
      (s.block && s.block !== '--') ? LOCO + ': ' + s.block : '';

    // ---- MM / KPH / PWM ----
    setTile('mm-display',  s.mm, ageOf(s,'mm'));
    setTile('kph-display', s.pkph !== '--' ? String(Math.round(parseFloat(s.pkph))) : '--',
            ageOf(s,'pkph'));
    setTile('pwm-display', s.pwm, ageOf(s,'pwm'));
    var mmLive = isFresh(s,'mm') && s.mm !== '--';
    document.getElementById('mm-landmark').textContent = mmLive ? (s.landmark || '') : '';
    document.getElementById('mm-countdown').textContent = mmLive ? mmCountdown(s.mm) : '';

    renderAgreement(s);

    // ---- Warning line (the firmware clears it itself after 20 s) ----
    document.getElementById('warning-line').textContent = (alive && s.warning) ? s.warning : '';

    // ---- Plain-language status line. Loss of position, silence and
    // undeclared operation are LOUD here; they never remove controls. ----
    var line, cls;
    if (heardAge === null) {
      line = LOCO + ' SILENT \\u2014 NO TELEMETRY THIS SESSION'; cls = 'bad';
    } else if (heardAge > STALE_S) {
      line = 'TELEMETRY STALE \\u2014 LAST HEARD ' + Math.round(heardAge) + ' s AGO'; cls = 'bad';
    } else if (s.estop === '1') {
      line = 'E-STOP ACTIVE'; cls = 'bad';
    } else if (s.nav === 'UNSET') {
      line = 'POSITION NOT DECLARED \\u2014 NAVIGATION WILL NOT TRACK'; cls = 'warn';
    } else if (s.nav === 'LOST') {
      line = 'POSITION LOST \\u2014 MANUAL CONTROL RETAINED \\u2014 STOP AND RE-DECLARE WHEN READY';
      cls = 'bad';
    } else if (s.nav === 'NO_QUORUM') {
      line = 'POSITION UNCERTAIN \\u2014 QUORUM UNRESOLVED \\u2014 MANUAL CONTROL RETAINED'
           + (motion === 'STOPPED' ? ' \\u2014 RE-DECLARE LOCATION TO CONTINUE'
                                   : ' \\u2014 STOP AND RE-DECLARE WHEN READY');
      cls = 'bad';
    } else if (pendingSend) {
      line = 'WAITING FOR CONFIRMATION\\u2026'; cls = 'warn';
    } else if (s.nav === 'EVALUATING') {
      line = 'CHECKING POSITION \\u2014 QUORUM EVALUATING \\u2014 MM ' + s.mm; cls = 'warn';
    } else {
      line = 'TRACKING \\u2014 MM ' + s.mm; cls = 'ok';
    }
    var sl2 = document.getElementById('status-line');
    sl2.textContent = line;
    sl2.className = 'status-line ' + cls;
  }).catch(function(e){ pollFail('pollState', e); });
}

// ---- Packet log: collapsed by default, periodic republishes filtered ----
var logOpen = false;
var showPeriodic = false;
var logEntries = [];
var logPausedUntil = {};
function toggleLogOpen(){
  logOpen = !logOpen;
  document.getElementById('loco-log').style.display = logOpen ? '' : 'none';
  document.getElementById('log-toggle').textContent = logOpen ? 'COLLAPSE' : 'EXPAND';
  document.getElementById('log-filter').style.display = logOpen ? '' : 'none';
  document.getElementById('log-clear').style.display  = logOpen ? '' : 'none';
  if (logOpen) { pollLog(); }
}
function toggleLogFilter(){
  showPeriodic = !showPeriodic;
  var b = document.getElementById('log-filter');
  b.textContent = showPeriodic ? 'HIDE PERIODIC' : 'SHOW PERIODIC';
  b.classList.toggle('on', showPeriodic);
  renderLog();
}
function clearLog(id){
  document.getElementById(id).innerHTML =
    '<div style="color:#aaa;font-size:11px;">Cleared \\u2014 resuming in 5s</div>';
  logPausedUntil[id]=Date.now()+5000;
}
function renderLog(){
  if(!logOpen) return;
  if(logPausedUntil['loco-log'] && Date.now()<logPausedUntil['loco-log']) return;
  var shown = showPeriodic ? logEntries : logEntries.filter(function(e){ return !e.p; });
  if(!shown.length){
    document.getElementById('loco-log').innerHTML =
      '<div style="color:#aaa;font-size:11px;">No events' +
      (logEntries.length ? ' (periodic republishes hidden \\u2014 tap SHOW PERIODIC)' : '') + '</div>';
    return;
  }
  document.getElementById('loco-log').innerHTML = shown.map(function(e){
    return e.ts+'  '+e.topic+'  '+e.value;
  }).join('<br>');
}
function pollLog(){
  if(!logOpen) return;   // don't poll a hidden log
  fetch('/loco/'+SLUG+'/log').then(r=>r.json()).then(entries=>{
    logEntries = entries;
    renderLog();
  }).catch(function(e){ pollFail('pollLog', e); });
}

setInterval(pollState,1000);
setInterval(pollLog,1000);
pollState();
</script>
</body></html>"""


# ============================================================================
# Rendering
# ============================================================================
def render_loco(lid):
    return render_template_string(
        LOCO_HTML,
        name=loco_name(lid), lid=lid, slug=loco_slug(lid),
        nav_html=nav(lid), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )


# ============================================================================
# Root / console
# ============================================================================
@app.route("/")
def index():
    return redirect(url_for("console"))


@app.route("/console")
def console():
    return render_template_string(
        CONSOLE_HTML,
        nav_html=nav("console"), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )


@app.route("/dispatcher/state")
def dispatcher_state():
    # online = "heard recently", not the retained online flag. Each MQTT
    # reconnect strands a zombie last-will (the old session's online 0 firing
    # AFTER the new session's online 1), so the flag flips while the
    # locomotive streams continuously. Heard-age cannot be fooled that way.
    now = time.monotonic()
    with mqtt_lock:
        out = []
        for lid in discovered_order():
            st = loco_state[lid]
            t = loco_rx[lid].get("heard")
            heard = (t is not None and now - t <= 10)
            out.append({
                "id": lid,
                "name": loco_name(lid),
                "slug": loco_slug(lid),
                "heard": heard,
                "mode": _mode_of(st),
                "quorum": _quorum_of(st, heard),
                "estop": st["estop"],
                "mm": st["mm"],
                "pkph": st["pkph"],
                "pwm": st["pwm"],
                "voltage": st["voltage"],
                "agree_pct": _rolling_agree(st),
                "agree_n": st["agree_n"],
                "disagree_n": st["disagree_n"],
                "block": st["block"],
                "station": {k: st["station_" + k] for k in ("event", "note", "seq", "ts")},
            })
    return jsonify({"locos": out})


@app.route("/dispatcher/cmd/<path:subcmd>", methods=["POST"])
def dispatcher_cmd(subcmd):
    """P2: bare go/stop fan out per locomotive — QUORUM subscribes only to
    ngr/dispatcher/cmd/go/<id>, and the suffix-less topic is an ESP-NOW-era
    fossil the Dispatcher ESP32 used to fan out over radio. Flask is the only
    place that can live now. The fan-out list is the DISCOVERED set."""
    parts = subcmd.split("/")
    verb = parts[0]
    target = parts[1] if len(parts) > 1 else None
    with mqtt_lock:
        known = list(loco_state)
    if target is not None and target not in known:
        return "", 204
    if verb in ("go", "stop"):
        if target:
            pub_dispatcher("%s/%s" % (verb, target))
        else:
            for lid in known:
                pub_dispatcher("%s/%s" % (verb, lid))
    elif verb == "ce" and target is None:
        pub_dispatcher("ce")
    elif verb == "estop" and target is None:
        # P12: SET broadcasts — everyone stops is right for an emergency.
        pub_dispatcher("estop")
    elif verb == "estopset" and target:
        pub_loco(target, "estop", "1")
    elif verb == "estopclear" and target:
        # P12: CLEAR is PER-LOCOMOTIVE. A broadcast clear would execute the
        # clear path on locomotives that were never stopped.
        pub_loco(target, "estop", "0")
    return "", 204


@app.route("/dispatcher/endcto", methods=["POST"])
def dispatcher_endcto():
    """v1.11.0: BOTH halves walk the discovered set. v1.10.11 released Otto and
    Toby by hardcoded id and then fanned stop/ across all of LOCO_IDS — Hans
    was in that tuple, so END AO stopped Hans and never released it. Deriving
    the list from who is actually out there means the bug cannot recur by
    someone forgetting to extend a tuple."""
    with mqtt_lock:
        known = list(loco_state)
    for lid in known:
        pub_loco(lid, "dispatcher_release", "1")
    for lid in known:
        pub_dispatcher("stop/%s" % lid)
    return "", 204


@app.route("/dispatcher/log")
def dispatcher_log():
    with mqtt_lock:
        entries = list(dispatch_log)
    for e in entries:
        e = e.setdefault("name", loco_name(e["loco"]))
    return jsonify(entries)


# ============================================================================
# Per-locomotive command path
# ============================================================================
def _cmd(lid, subtopic, value):
    """E-STOP is NEVER gated: it publishes in every state — UNSET, LOST,
    stale, and AUTO (v1.9.5 returned 423 for estop in AUTO; that was wrong).
    Everything else respects AUTO."""
    if subtopic == "estop":
        pub_loco(lid, "estop", value)
        return "", 204
    # NO LOCK ON THE MANUAL PATH (v1.10.3). on_mqtt_message() holds mqtt_lock
    # for its ENTIRE body, every JSON parse of every inbound message, so a
    # manual command could block behind telemetry parsing. E-STOP returns
    # above without ever touching it, which is precisely the asymmetry the
    # operator measured between E-STOP and throttle. A bare dict read is
    # atomic under the GIL, and the value could change immediately after
    # release anyway, so the lock bought nothing.
    if loco_state.get(lid, {}).get("auto") == "1":
        return "", 423
    if subtopic in {"throttle", "brake", "direction"}:
        pub_loco(lid, subtopic, value)
    return "", 204


# ============================================================================
# Locomotive routes — ONE parametrised set. A locomotive is addressed by id
# or by known name; discovery means an unnamed id is a first-class address.
# ============================================================================
@app.route("/loco/<ref>")
def loco_page(ref):
    lid = resolve_loco(ref)
    if lid is None:
        return ("<h2 style='font-family:Arial;padding:2rem;color:#ccc;background:#222;"
                "min-height:100vh;margin:0;'>No locomotive %s &mdash; nothing by that "
                "name or id has been heard this session.</h2>" % Markup.escape(ref)), 404
    return render_loco(lid)


@app.route("/loco/<ref>/state")
def loco_state_route(ref):
    lid = resolve_loco(ref)
    if lid is None:
        return jsonify({"error": "unknown locomotive"}), 404
    return jsonify(state_payload(lid))


@app.route("/loco/<ref>/cmd/<subtopic>/<value>", methods=["POST"])
def loco_cmd(ref, subtopic, value):
    lid = resolve_loco(ref)
    if lid is None:
        return "", 404
    return _cmd(lid, subtopic, value)


@app.route("/loco/<ref>/sessiondir/<d>", methods=["GET", "POST"])
def loco_sessiondir(ref, d):
    lid = resolve_loco(ref)
    if lid is None:
        return "", 404
    if loco_state.get(lid, {}).get("auto") == "1":
        return ("", 423) if request.method == "POST" else redirect("/loco/" + loco_slug(lid))
    if d in ("CW", "CCW"):
        pub_loco(lid, "session_direction", d)
    return ("", 204) if request.method == "POST" else redirect("/loco/" + loco_slug(lid))


@app.route("/loco/<ref>/startinterval/<interval>", methods=["GET", "POST"])
def loco_startinterval(ref, interval):
    lid = resolve_loco(ref)
    if lid is None:
        return "", 404
    if loco_state.get(lid, {}).get("auto") == "1":
        return ("", 423) if request.method == "POST" else redirect("/loco/" + loco_slug(lid))
    if re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco(lid, "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/loco/" + loco_slug(lid))


@app.route("/loco/<ref>/mode/<int:m>")
def loco_mode(ref, m):
    lid = resolve_loco(ref)
    if lid is None:
        return "", 404
    if m == 1:
        pub_loco(lid, "auto", "1")
    return redirect("/loco/" + loco_slug(lid))


@app.route("/loco/<ref>/log")
def loco_log_route(ref):
    lid = resolve_loco(ref)
    if lid is None:
        return jsonify([])
    with mqtt_lock:
        return jsonify(list(loco_log.get(lid, [])))


# Legacy addresses. The old console linked to /otto, /toby, /hans and those
# links are in bookmarks and on a phone home screen.
@app.route("/<any_slug>")
def legacy_loco(any_slug):
    lid = resolve_loco(any_slug)
    if lid is None:
        return "", 404
    return redirect("/loco/" + loco_slug(lid))


# ============================================================================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, threaded=True)
