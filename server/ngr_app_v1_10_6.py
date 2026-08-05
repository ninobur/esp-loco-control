# ============================================================================
# NGR dashboard v1.10.6
#
# THE THROTTLE FAILURE OF 2026-08-04. v1.10.4 was deployed at 19:42 and
# reverted at 20:21 as broken: the throttle could not be moved at all. That
# fault is NOT REPRODUCED on the bench. Every state I could construct —
# offline, stale, E-STOP engaged, E-STOP cleared, AUTO, mid-drag — leaves the
# slider enabled, unobstructed and publishing. So rather than guess at a
# cause, this version does two things: removes the one genuinely new
# mechanism that could plausibly produce it, and makes the page able to
# report its own failure next time.
#
#  1. THE PAGE NOW SHOWS ITS OWN ERRORS. There is no console on a phone in a
#     garden. A JavaScript exception in pollState() would silently stop every
#     subsequent DOM update — the exact signature of "the controls stopped
#     working" — and leave no trace the operator could report. window.onerror
#     and unhandledrejection now paint a red banner at the top of the page
#     with the message, the line, and the time. If it happens again we will
#     know what it was instead of guessing.
#
#  2. ONE THROTTLE REQUEST IN FLIGHT AT A TIME. v1.10.3 made the slider
#     publish on `oninput`, which is correct and stays. But a real finger
#     drag on iOS emits ~60 events/second, and each one fired its own fetch()
#     at a Flask dev server on a Raspberry Pi. iOS Safari allows ~6
#     concurrent connections per host; beyond that requests queue, and
#     pollState()'s own fetch queues behind them. That is the only mechanism
#     in the 1.10.x line that scales with how hard the operator drags, which
#     makes it the best-fitting suspect for a control that felt dead under
#     use and fine under test.
#
#     The fix is transport backpressure, NOT a debounce and NOT a
#     confirmation wait: the first move publishes immediately, and while a
#     request is outstanding the newest value is held and sent the instant it
#     completes. The operator never waits on the locomotive, no value is ever
#     delayed behind a timer, and the final position of the slider is always
#     the last thing published. At most one throttle request exists at a
#     time instead of sixty.
#
# If the throttle still fails after this, the banner will say why, and that
# is the point of shipping it.
#
# v1.10.5
# ONE FIX — in MANUAL, nothing disables a control except AUTO.
#
# The last blocker was SET INTERVAL, disabled while the locomotive reported
# motion. That was the one refusal v1.10.2's inventory kept, to protect
# declaration integrity after the 2026-08-01 mid-motion declare corruption.
# The operator has ruled it advisory: a computer refusing a manual operator
# is the thing the bicameral doctrine forbids, and the operator decides when
# it is safe to declare. The note stays; the block is gone.
#
# The rule, stated once so it is not eroded again: STALE TELEMETRY, UNKNOWN
# MOTION, REPORTED MOTION AND MISSING CONFIRMATION MAY PRODUCE A NOTE. THEY
# MAY NEVER DISABLE ANYTHING. The only permitted disable in MANUAL is none;
# the only disable at all is the AUTO chamber, which is a boundary between
# chambers rather than a judgement about the operator.
#
# Throttle, brake and the direction buttons already obeyed this from v1.10.2
# onward — verified, not assumed: every `.disabled =` assignment and every
# `locked` class toggle now tests `isCto` and nothing else. E-STOP is
# untouched and remains ungated in every state.
#
# v1.10.4
# Four bugs from v1.10.3 field testing.
#
# BUG 1 (SAFETY) — E-STOP DID NOT ZERO THE THROTTLE. The slider kept showing
#   its old value, so clearing E-STOP could resume at speed. Now: E-STOP
#   publishes estop FIRST (unchanged, still first out), then forces the
#   throttle control to 0 and publishes cmd/throttle 0. Both engage and clear
#   zero it, so the slider reads 0 whenever E-STOP has been touched. The
#   throttle-zero is published UNGATED, like estop itself — under AUTO the
#   server still answers 423, which is harmless, but the display is corrected
#   either way.
#
# BUG 2 — DIRECTION BOUNCED BACK TO NEUTRAL. Two independent mechanisms, and
#   only the first is a dashboard defect:
#     (a) OURS: updateDirButtons() cleared every button when the locomotive
#         had not reported a direction, wiping the optimistic light from the
#         tap; and _fresh_state() defaults direction to "1" = NEUTRAL, so the
#         latent value the display fell back toward was Neutral. Fixed: the
#         operator's last commanded direction stays lit until the locomotive
#         actually reports one, and the reported value is then mirrored.
#     (b) THE LOCOMOTIVE, correctly: QUORUM REFUSES cmd/direction while
#         motorIsMoving() (actualPwm>0 || commandedPwm>0) or in AUTO, and
#         publishes DIR_REFUSED to state/warning. It then keeps reporting its
#         previous direction — which after an E-stop is NEUTRAL, because the
#         firmware forces NEUTRAL on both estop engage and clear. So raising
#         the throttle and THEN selecting Forward is refused, and the
#         dashboard was faithfully mirroring a refusal it never explained.
#         Now the disagreement is named on screen instead of looking like a
#         glitch. Display only — nothing is gated by it.
#
# BUG 3 — INA219 (Voltage/Current/Power) BLANK. Root cause is NOT the
#   dashboard binding: no locomotive publishes this telemetry any more.
#   QUORUM builds 28 topics and NOT ONE is a telem/ topic. INA219 support was
#   dropped at SOLONAV 2.1 (SOLONAV_1_3 has 7 references, SOLONAV_2_1 has
#   zero) and QUORUM inherits the gap, exactly as it inherited the missing
#   cmd/brake channel. The "—" is therefore CORRECT: it is v1.10.0's rule
#   working as designed — show nothing rather than a remembered number. The
#   old 15.4 V / 1.8 W that used to appear were retained MQTT ghosts from the
#   r12/SOLONAV 1.x era, which is the very failure v1.10.0 was built to stop.
#   The tiles are relabelled so the absence reads as "not published by this
#   firmware" rather than as a dead feed. No values are fabricated.
#
# BUG 4 — command path re-verified as direct. See the implementation report:
#   throttle/brake/direction still publish on `oninput`/`onclick` with nothing
#   between input and fetch, and the server still takes no lock on the manual
#   path. The field impression that the display "waits" was Bug 2(a) — the
#   optimistic light being wiped — not a command-path regression.
#
# v1.10.3 — OPERATOR RULING: manual control is sovereign and DUMB. In MANUAL,
# every control publishes its command immediately on input — tap to publish,
# nothing between. The BICAMERAL doctrine applies to the dashboard command
# path, not only to the firmware. E-STOP's directness is the model; every
# manual control now matches it.
#
# Evidence that the latency was ours: E-STOP, which bypasses all logic, was
# more responsive than throttle decrease, which did not. Signal strength was
# surveyed for five hours with zero dropout, so coverage is not the cause.
#
# What was actually in the way (full audit in the implementation report):
#
#   * THROTTLE AND BRAKE PUBLISHED ON `onchange`, NOT `oninput`. On a range
#     input `change` fires when the finger LIFTS. Dragging the throttle down
#     published nothing until release — the whole drag was silent. That is
#     the reported latency, and it is not the network.
#   * DIRECTION carried a 1-second same-value bounce guard in the command
#     path, so a deliberate re-press of the same direction inside a second
#     was silently swallowed, and it set a "WAITING FOR CONFIRMATION…" state
#     before publishing.
#   * THE SERVER TOOK `mqtt_lock` on the manual path to read one string.
#     `on_mqtt_message()` holds that same lock for its entire body — every
#     JSON parse of every inbound message — so a manual command could block
#     behind telemetry parsing. E-STOP returns before the lock and never
#     waits. That asymmetry is exactly the one the operator measured.
#
# All three are gone. Input handlers now read the value and publish it, and
# nothing else. Confirmation echoes remain ONLY as passive display: buttons
# light from the locomotive's confirmed state, but nothing waits on it.
#
# BRAKE — root cause is NOT in the dashboard. See the report: the firmware
# has no `cmd/brake` channel at all. SOLONAV dropped it and QUORUM inherits
# that; `subscribeAll()` does not subscribe to it and no handler exists. The
# dashboard has been publishing into the void. The publish path is fixed
# here for consistency, but the brake CANNOT work until the firmware has a
# channel — and firmware is out of scope for this change.
#
# v1.10.2 — operator ruling: the dashboard must never say no to a manual
# operator. Field evidence 2026-08-03 (Toby tab):
#
#  * PER-TAB BINDING. Element bindings were already per-slug, but every
#    confirmation string, status line and echo label said "OTTO" regardless
#    of tab. All wording is now generic or uses the active tab's name.
#  * DECLARATION MIRRORS THE LOCOMOTIVE. Declared/undeclared state comes
#    from the loco's reported nav state and nothing else. A manual stop
#    changes nothing; re-declaration is indicated only when the LOCOMOTIVE
#    reports UNSET (its reboot, direction change, or the operator's own
#    choice). The dashboard never generates the requirement itself.
#  * THE THROTTLE IS NEVER LOCKED IN MANUAL. The startup-order gate on
#    throttle, brake and motor direction is removed entirely. Undeclared
#    operation is signalled by loud truth — status line "POSITION NOT
#    DECLARED — NAVIGATION WILL NOT TRACK", empty MM tile — not by disabled
#    controls. AUTO still requires a declared position (the firmware's GO
#    refusal is legitimate and stays). E-STOP ungated always.
#  * CONFIRMATION NOTICES INFORM, NEVER GATE. "WAITING FOR CONFIRMATION…"
#    stays (bound to the active loco, so it actually resolves); on timeout
#    "NO CONFIRMATION — COMMAND MAY NOT HAVE ARRIVED" without disabling
#    anything. Direction echo binds to the active loco's state/direction
#    integer (published by SOLONAV 2_22 and QUORUM alike), with the
#    DIRECTION nav event as the freshness stamp where present.
#  * POLARITY AGREEMENT TILE, per locomotive: session AGREE/DISAGREE counts
#    with percentage and the last ten verdicts as green/red ticks with mm
#    numbers, live from the active loco's nav events, reset on its epoch.
#    (A noisy cable read 27% and a flipped sensor 100% this week.)
#
#  Refusal inventory (every v1.10.1 block reachable in MANUAL, with
#  disposition) is in docs/DASHBOARD_1_10_2_IMPLEMENTATION_REPORT.md.
#
# v1.10.1 — the 2026-08-01 16:03 field run, plus QUORUM 1.0 bindings:
#
#  * GATES ENFORCE STARTUP ORDER ONLY. At 16:08:03 the navigator went LOST
#    and the dashboard re-locked the throttle on a MOVING locomotive; the
#    last accepted command (54) stalled Otto with the operator locked out.
#    Now: once a session reaches a confirmed declare and the throttle
#    unlocks, throttle/brake/motor-direction/mode NEVER re-lock this session
#    — not on any nav state, not on stale telemetry. Those conditions change
#    the status line, loudly. Re-lock only on page load, Otto reboot
#    (epoch), or operator session end. SET INTERVAL stays motion-gated.
#  * MOTOR-DIRECTION CONFIRMATION ECHO. The operator pressed Forward 12
#    times in 55 s because the button never lit. Buttons now illuminate from
#    Otto's confirmed state/direction (0=REV 1=NEU 2=FWD); the firmware also
#    publishes a {"event":"DIRECTION"} nav event on EVERY accepted press
#    (even same-value), which stamps the freshness that clears the "waiting"
#    treatment. Press-lockout prevents re-tap double-publishes. (The
#    DIRECTION event's motor_dir string reports NEU as "REV" — firmware
#    quirk — so the integer topic is the binding, the event only freshness.)
#  * THE SLIDER NEVER FIGHTS THE OPERATOR. Slider = commanded value, stays
#    put; Otto's actual PWM is a separate read-only figure beside it.
#  * EXPLICIT RECOVERY PATH. Position lost mid-run: status says so and that
#    manual control is retained; once Otto reports stopped, SET INTERVAL
#    unlocks for re-declare without a page reload.
#  * QUORUM 1.0 VOCABULARY (firmware/QUORUM/QUORUM.ino, spec R20). Nav
#    states UNSET/NORMAL/EVALUATING/NO_QUORUM (legacy TRACKING/LOST still
#    understood — Toby runs SOLONAV_2_14); loopstat carries miss_streak, not
#    conf; confidence is gone entirely; alert carries viable[] and a
#    repurposed candidate_mm; decision events (QUORUM_OPEN/CLOSED/TIED/
#    ADOPTED/REOPENED, PHANTOM_REJECTED, NO_QUORUM, FORCED_OFFSET,
#    FIXTURE_REJECTED) arrive on state/nav and show in the packet log.
#
# GOVERNING PRINCIPLE — same one the locomotives live by: the dashboard
# renders only what Otto has actually said, timestamped, and says nothing
# when he is silent. No control asserts state Otto has not confirmed.
#
# What changed from v1.9.5 (each traced to a documented field failure):
#
#  * RETAINED MESSAGES ARE NEVER RENDERED AS LIVE DATA. The broker holds
#    retained telem/voltage 15.40, telem/power 1.79, mm/speed pkph 12.73 from
#    firmware that no longer publishes those topics; every reconnect replayed
#    them into the tiles as if fresh (the identical-values-15-hours-apart
#    failure). paho marks store replays with msg.retain — they are dropped
#    for state and appear only in the packet log tagged (retained).
#  * EVERY VALUE CARRIES ITS ARRIVAL TIME. /state reports per-field ages;
#    tiles gray out past 5 s and show "N s ago"; a field never received this
#    session renders "—". A dead feed looks dead.
#  * MOTION STATE (MOVING / STOPPED / UNKNOWN) from Otto's 1 Hz alert
#    (moving = actualPwm>0), never from the throttle slider.
#  * ONE PRESS, ONE PUBLISH. The throttle slider double-sent every release
#    (debounced oninput sender + onchange sender — verified as same-second
#    duplicate POSTs in the Pi journal). oninput is display-only now.
#  * START-ORDER ENFORCEMENT RESTORED, gated on Otto's confirmations:
#    session direction usable first; SET INTERVAL enabled only once Otto has
#    confirmed the direction and reports STOPPED; throttle unlocks only on
#    Otto's nav_ready / TRACKING. Re-locks on page load, Otto reboot
#    (live bootid or uptime reset), or nav returning to UNSET.
#  * E-STOP IS NEVER GATED. v1.9.5's /cmd route returned 423 for ALL
#    subtopics in AUTO — including estop. estop is exempt now.
#  * MM and KpH wired to Otto's own words: alert.dead_reckoned_mm while
#    TRACKING, and alert.est_mm_s scaled to prototype km/h.
#  * %CERT tile removed (QUORUM replaces the tally). PROFILE MEMORY panel
#    removed — the profile service still runs but SOLONAV v2.22 never
#    publishes profile/request, so nothing consumes it (git preserves it).
#    DNA tile stays, showing "—" until the DNA features exist.
#  * Packet log collapsed by default; 1 Hz republishes (loopstat, alert
#    STATUS broadcasts, unchanged state re-publishes) hidden behind a toggle;
#    cmd/*, nav events, alerts and markers always shown.
#  * Headers and values sized for daylight on an iPhone.
#
# MQTT topic and payload formats are UNCHANGED. No firmware changes.
# ============================================================================

from flask import Flask, render_template_string, request, jsonify, redirect, url_for, make_response
from markupsafe import Markup
import threading
import time
import subprocess
import os
import glob
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

LOCO_IDS = ("9950011", "9950012", "2095111")

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
        "online": "0", "throttle": "0", "direction": "1", "brake": "0",
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
    }


# Nav states in which Otto's position is usable. QUORUM 1.0 publishes
# NORMAL/EVALUATING/NO_QUORUM/UNSET; legacy TRACKING/LOST (Toby, SOLONAV_2_14)
# is still understood.
USABLE_NAV = ("TRACKING", "NORMAL", "EVALUATING")


loco_state = {lid: _fresh_state() for lid in LOCO_IDS}
loco_rx    = {lid: {} for lid in LOCO_IDS}    # field -> time.monotonic() of last LIVE message
loco_epoch = {lid: 0 for lid in LOCO_IDS}     # bumped on Otto reboot; client re-locks on change

mqtt_lock = threading.Lock()
mqtt_conn = None

loco_log = {lid: deque(maxlen=300) for lid in LOCO_IDS}
dispatch_log = deque(maxlen=200)

# Fields whose ages the client renders.
AGE_FIELDS = ("heard", "voltage", "current", "power", "lowvolt", "pwm", "pkph",
              "mm", "nav", "moving", "session_dir", "nav_ready", "start_interval",
              "marker", "throttle", "direction", "estop", "auto", "warning")

# state/<x> payloads copied verbatim into loco_state. The firmware publishes
# these on change (retained), so a live arrival is a confirmation event.
SIMPLE_STATE = {
    "state/throttle": "throttle", "state/direction": "direction",
    "state/brake": "brake", "state/estop": "estop", "state/auto": "auto",
    "state/lowvolt": "lowvolt", "state/warning": "warning",
    "state/block": "block", "state/ce": "ce",
    "state/session_direction": "session_dir", "state/nav_ready": "nav_ready",
    "state/start_interval": "start_interval",
}


def _touch(lid, *fields):
    now = time.monotonic()
    for f in fields:
        loco_rx[lid][f] = now


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
        for lid in LOCO_IDS:
            base = f"ngr/loco/{lid}"
            client.subscribe(f"{base}/online")
            client.subscribe(f"{base}/state/#")
            client.subscribe(f"{base}/telem/#")
            client.subscribe(f"{base}/alert")
            client.subscribe(f"{base}/mm/#")
            client.subscribe(f"{base}/cmd/#")   # so the packet log shows every command once
    else:
        print(f"MQTT connect failed rc={rc}")


def on_mqtt_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore")
    parts = topic.split("/")
    if len(parts) < 4 or parts[0] != "ngr" or parts[1] != "loco":
        return
    lid = parts[2]
    if lid not in loco_state:
        return
    sub = "/".join(parts[3:])
    retained = bool(msg.retain)

    with mqtt_lock:
        st = loco_state[lid]

        # Store replays are remembered numbers, not Otto speaking. Log, never render.
        if retained:
            _log(lid, sub, payload, retained=True)
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
                if ev == "DIRECTION":
                    _touch(lid, "direction")
                # v1.10.2: polarity agreement tally (Change 6). AGREE/DISAGREE
                # ride state/nav on every firmware generation.
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
            # A LIVE bootid means Otto just (re)connected after boot — the
            # firmware publishes it once per MQTT connect.
            try:
                sketch = json.loads(payload).get("sketch", "")
            except Exception:
                sketch = ""
            _reset_session(lid, "live bootid (%s)" % (sketch or "unknown sketch"))
            st = loco_state[lid]
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
        st = dict(loco_state[lid])
        st["verdicts"] = list(st["verdicts"])   # snapshot under the lock
        rx = dict(loco_rx[lid])
        ep = loco_epoch[lid]
    now = time.monotonic()
    st["ages"] = {f: (round(now - rx[f], 1) if f in rx else None) for f in AGE_FIELDS}
    st["epoch"] = ep
    return st


# ============================================================================
# CAL LOGGER — per-loco mosquitto_sub processes
# ============================================================================
CAL_LOG_DIR = "/home/david/NGR/logs"

cal_procs = {}   # lid -> Popen or None
cal_files = {}   # lid -> filename of current session

def cal_start(lid):
    if cal_procs.get(lid) and cal_procs[lid].poll() is None:
        return False, "already running"
    topic = f"ngr/loco/{lid}/mm/cal"
    from datetime import datetime
    fname = f"{CAL_LOG_DIR}/cal_{lid}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
    try:
        proc = subprocess.Popen(
            ["mosquitto_sub", "-h", "127.0.0.1", "-t", topic],
            stdout=open(fname, "a"),
            stderr=subprocess.DEVNULL
        )
        cal_procs[lid] = proc
        cal_files[lid] = fname
        return True, fname
    except Exception as e:
        return False, str(e)

def cal_stop(lid):
    proc = cal_procs.get(lid)
    if proc and proc.poll() is None:
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        cal_procs[lid] = None
        return True, cal_files.get(lid, "")
    cal_procs[lid] = None
    return False, "not running"

def cal_status(lid):
    proc = cal_procs.get(lid)
    running = bool(proc and proc.poll() is None)
    return {"running": running, "file": cal_files.get(lid, "") if running else ""}

def cal_list():
    """Return list of CAL log files sorted newest first."""
    files = sorted(glob.glob(f"{CAL_LOG_DIR}/cal_*.txt"), reverse=True)
    result = []
    for f in files[:20]:
        size = os.path.getsize(f)
        result.append({"name": os.path.basename(f), "size": size})
    return result

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
# Nav
# ============================================================================
NAV_STYLE = """
.nav-bar { display:flex; gap:6px; padding:10px 10px 0; max-width:700px; margin:0 auto; }
.nav-btn { flex:1; text-align:center; padding:10px 4px; border-radius:10px;
           background:rgba(40,40,40,0.7); color:#ccc; text-decoration:none;
           font-size:14px; font-weight:bold; border:2px solid #555; }
.nav-btn.active      { background:rgba(60,60,60,0.95); color:#fff; border-color:#aaa; }
.nav-btn.placeholder { opacity:0.4; pointer-events:none; }
"""

def nav(active):
    def c(n):
        if n == active: return "active"
        if n in ("oscar",): return "placeholder"
        return ""
    html = (f'<div class="nav-bar">'
            f'<a href="/"      class="nav-btn {c("console")}">Console</a>'
            f'<a href="/otto"  class="nav-btn {c("otto")}">Otto</a>'
            f'<a href="/toby"  class="nav-btn {c("toby")}">Toby</a>'
            f'<a href="/oscar" class="nav-btn {c("oscar")}">Oscar</a>'
            f'<a href="/hans"  class="nav-btn {c("hans")}">Hans</a>'
            f'</div>')
    return Markup(html)


# ============================================================================
# Shared CSS
# ============================================================================
SHARED_CSS = """
body { margin:0; font-family:Arial,sans-serif;
  background:linear-gradient(135deg,#a8a8a8 0%,#d9d9d9 20%,#8f8f8f 50%,#d7d7d7 80%,#9c9c9c 100%);
  color:#111; }
.container { max-width:700px; margin:auto; padding:10px; }
.panel { background:rgba(40,40,40,0.92); border-radius:14px; padding:14px;
  margin-bottom:14px; box-shadow:0 4px 10px rgba(0,0,0,0.35);
  border:2px solid #666; position:relative; }
.panel:before,.panel:after { content:""; width:12px; height:12px; border-radius:50%;
  background:#bbb; border:2px solid #555; position:absolute; top:8px; }
.panel:before { left:8px; } .panel:after { right:8px; }
h2 { margin-top:0; text-align:center; color:#f1f1f1; letter-spacing:1px; }
.badge { font-size:13px; font-weight:bold; padding:4px 10px; border-radius:20px; letter-spacing:1px; }
.badge-online  { background:#2a7a2a; color:#baffba; border:1px solid #4fc34f; }
.badge-offline { background:#5a2020; color:#ffb3b3; border:1px solid #b32020; }
.badge-stale   { background:#5a4a10; color:#ffe0a0; border:1px solid #b39020; }
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:8px; margin-bottom:10px; }
.telem-cell { background:#222; border-radius:8px; padding:8px 10px; }
.telem-lbl  { font-size:12px; color:#ddd; margin-bottom:2px; font-weight:bold; }
.telem-val  { font-size:20px; font-weight:bold; color:#4fc34f; }
.telem-val.warn { color:#ffc040; }
.estop-btn { width:100%; padding:18px; border-radius:50px; border:3px solid #b32020;
  background:rgba(40,40,40,0.92); color:#ff5050; font-size:20px; font-weight:bold;
  letter-spacing:3px; cursor:pointer; text-align:center; text-decoration:none; display:block; }
.estop-btn.active { background:#b32020; color:white; }
.estop-btn:hover  { background:#8a1515; color:white; border-color:#e04040; }
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
.cmd-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;}
.col-header{text-align:center;color:#aaa;font-size:12px;font-weight:bold;
  letter-spacing:1px;padding-bottom:6px;border-bottom:1px solid #444;margin-bottom:6px;}
button{border:none;border-radius:10px;padding:12px 4px;width:100%;
  font-size:14px;font-weight:bold;cursor:pointer;margin-bottom:6px;
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.25),0 2px 4px rgba(0,0,0,0.4);}
button:last-child{margin-bottom:0;}
.btn-go    {background:#3fa34d;color:white;}
.btn-stop  {background:#d9a21b;color:white;}
.btn-ce    {background:#2d6ea8;color:white;}
.btn-end   {background:#7a3a00;color:#ffd080;border:2px solid #aa6000;}
.btn-estop {width:100%;padding:18px;border-radius:50px;border:3px solid #b32020;
  background:rgba(40,40,40,0.92);color:#e04040;font-size:20px;font-weight:bold;
  letter-spacing:3px;cursor:pointer;}
.btn-estop:hover{background:#8a1515;color:white;border-color:#e04040;}
.status-val.on{color:#7f7;} .status-val.off{color:#555;}
</style></head><body>
{{ nav_html }}
<div class="container">

  <div class="panel">
    <h2>DISPATCHER</h2>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;">
      <div>
        <div style="text-align:center;color:#aaa;font-size:12px;font-weight:bold;
          letter-spacing:2px;margin-bottom:8px;border-bottom:1px solid #444;padding-bottom:6px;">OTTO</div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">STATUS</div>
          <div style="font-size:15px;font-weight:bold;" id="otto-mode">
            {{ 'CE' if (otto_auto=='1' and otto_ce=='1') else ('CTO' if otto_auto=='1' else 'MAN') }}
          </div>
        </div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">BLOCK</div>
          <div style="font-size:15px;font-weight:bold;color:#f1f1f1;" id="otto-block">{{ otto_block }}</div>
        </div>
        <div style="text-align:center;">
          <div style="font-size:10px;color:#666;">ONLINE</div>
          <div class="status-val {{ 'on' if otto_online=='1' else 'off' }}" id="otto-online">
            {{ 'YES' if otto_online=='1' else 'NO' }}</div>
        </div>
      </div>
      <div>
        <div style="text-align:center;color:#aaa;font-size:12px;font-weight:bold;
          letter-spacing:2px;margin-bottom:8px;border-bottom:1px solid #444;padding-bottom:6px;">TOBY</div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">STATUS</div>
          <div style="font-size:15px;font-weight:bold;" id="toby-mode">
            {{ 'CE' if (toby_auto=='1' and toby_ce=='1') else ('CTO' if toby_auto=='1' else 'MAN') }}
          </div>
        </div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">BLOCK</div>
          <div style="font-size:15px;font-weight:bold;color:#f1f1f1;" id="toby-block">{{ toby_block }}</div>
        </div>
        <div style="text-align:center;">
          <div style="font-size:10px;color:#666;">ONLINE</div>
          <div class="status-val {{ 'on' if toby_online=='1' else 'off' }}" id="toby-online">
            {{ 'YES' if toby_online=='1' else 'NO' }}</div>
        </div>
      </div>
    </div>
  </div>

  <div class="panel">
    <h2>OPERATIONS</h2>
    <div class="cmd-grid">
      <div>
        <div class="col-header">BOTH</div>
        <button class="btn-go"   onclick="dc('go')">GO</button>
        <button class="btn-stop" onclick="dc('stop')">STOP</button>
      </div>
      <div>
        <div class="col-header">OTTO</div>
        <button class="btn-go"   onclick="dc('go/9950011')">GO</button>
        <button class="btn-stop" onclick="dc('stop/9950011')">STOP</button>
      </div>
      <div>
        <div class="col-header">TOBY</div>
        <button class="btn-go"   onclick="dc('go/9950012')">GO</button>
        <button class="btn-stop" onclick="dc('stop/9950012')">STOP</button>
      </div>
    </div>
  </div>

  <div class="panel">
    <div class="cmd-grid">
      <div>
        <button class="btn-ce"  onclick="dc('ce')">CIRCUIT EXPRESS</button>
      </div>
      <div style="grid-column:span 2;">
        <button class="btn-end" onclick="endCto()">END CTO — RETURN TO MANUAL</button>
      </div>
    </div>
  </div>

  <div class="panel">
    <button class="btn-estop" onclick="dc('estop')">&#9888; E-STOP</button>
  </div>

  <div class="panel">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
      <span style="color:#888;font-size:11px;letter-spacing:1px;">PACKET LOG</span>
      <button onclick="clearLog('dispatch-log')" style="background:#333;border:1px solid #555;
        color:#aaa;font-size:11px;padding:4px 12px;border-radius:6px;cursor:pointer;">CLEAR</button>
    </div>
    <div id="dispatch-log" style="color:#fff;background:#0a0a0a;font-size:11px;
      font-family:monospace;padding:8px;border-radius:6px;max-height:300px;overflow-y:auto;">
      <div style="color:#aaa;">Waiting for packets...</div>
    </div>
  </div>

</div>
<script>
function dc(cmd){fetch('/dispatcher/cmd/'+cmd,{method:'POST'}).catch(e=>console.error(e));}
function endCto(){
  if(confirm('End CTO and return both locomotives to manual control?')){
    fetch('/dispatcher/endcto',{method:'POST'}).catch(e=>console.error(e));
  }
}
function pollStatus(){
  fetch('/dispatcher/state').then(r=>r.json()).then(s=>{
    document.getElementById('otto-mode').textContent = s.otto_auto==='1'?(s.otto_ce==='1'?'CE':'CTO'):'MAN';
    document.getElementById('toby-mode').textContent = s.toby_auto==='1'?(s.toby_ce==='1'?'CE':'CTO'):'MAN';
    document.getElementById('otto-online').textContent = s.otto_online==='1'?'YES':'NO';
    document.getElementById('toby-online').textContent = s.toby_online==='1'?'YES':'NO';
    document.getElementById('otto-block').textContent = s.otto_block||'--';
    document.getElementById('toby-block').textContent = s.toby_block||'--';
  }).catch(e=>{});
}
var logPausedUntil = {};
function clearLog(id){
  document.getElementById(id).innerHTML='<div style="color:#aaa;font-size:11px;">Cleared — resuming in 5s</div>';
  logPausedUntil[id] = Date.now()+5000;
}
function pollLog(){
  if(logPausedUntil['dispatch-log'] && Date.now()<logPausedUntil['dispatch-log']) return;
  fetch('/dispatcher/log').then(r=>r.json()).then(entries=>{
    if(!entries.length) return;
    const names={'9950011':'Otto','9950012':'Toby'};
    document.getElementById('dispatch-log').innerHTML=entries.map(e=>
      e.ts+'  '+(names[e.loco]||e.loco)+'  '+e.topic+'  '+e.value
    ).join('<br>');
  }).catch(e=>{});
}
setInterval(pollStatus,3000);
setInterval(pollLog,1000);
</script>
</body></html>"""


# ============================================================================
# Loco page template
# ============================================================================
LOCO_HTML = """<!DOCTYPE html>
<html><head>
<title>NGR — {{ name }}</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
{{ nav_style }}
{{ shared_css }}
.container { max-width:700px; margin:auto; padding:8px; }
.panel-tight { background:rgba(40,40,40,0.92); border-radius:12px; padding:10px 12px;
  margin-bottom:8px; box-shadow:0 3px 8px rgba(0,0,0,0.3); border:2px solid #666; }
.header-wrap { position:relative; display:flex; align-items:center;
  justify-content:flex-end; width:100%; }
.loco-title { position:absolute; left:50%; transform:translateX(-50%);
  font-size:22px; font-weight:bold; color:#ffffff; letter-spacing:2px; }

/* Section headers — readable outdoors at arm's length */
.sec-hdr { text-align:center; color:#ffffff; font-size:17px; font-weight:800;
  letter-spacing:2px; margin-bottom:8px; }

/* Plain-language status line */
.status-line { text-align:center; color:#ffffff; font-size:17px; font-weight:800;
  letter-spacing:1px; margin-top:8px; min-height:20px; line-height:1.3; }
.status-line.bad  { color:#ffb3b3; }
.status-line.warn { color:#ffe0a0; }
.status-line.ok   { color:#baffba; }
.warning-line { text-align:center; color:#ffc040; font-size:14px; font-weight:bold;
  min-height:16px; margin-top:4px; }

/* Motion indicator */
.motion-bar { text-align:center; font-size:24px; font-weight:900; letter-spacing:4px;
  padding:12px 4px; border-radius:10px; margin-top:8px; }
.motion-bar.moving  { background:#7a2508; color:#ffc9a8; border:2px solid #ff7040; }
.motion-bar.stopped { background:#14421a; color:#a8ffa8; border:2px solid #2a7a2a; }
.motion-bar.unknown { background:#333;    color:#cccccc; border:2px solid #777; }

.mode-row { display:flex; gap:6px; }
.mode-btn { flex:1; text-align:center; padding:10px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#bbb;
  font-size:15px; font-weight:bold; letter-spacing:1px; text-decoration:none; display:block; }
.mode-btn.man-active  { background:rgba(50,50,70,0.95); color:#ccd8ff; border-color:#88a; }
.mode-btn.auto-active { background:rgba(40,70,40,0.95); color:#7f7; border-color:#4a4; }
.mode-btn.locked { opacity:0.35; pointer-events:none; }
.cto-note { text-align:center; color:#7f7; font-size:12px; letter-spacing:1px; margin-top:4px; }
.dir-row { display:flex; gap:6px; }
.dir-btn { flex:1; text-align:center; padding:11px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#ccc;
  font-size:15px; font-weight:bold; cursor:pointer; text-decoration:none; }
.dir-btn.active-rev { background:#5a2020; color:#ffb3b3; border-color:#b32020; }
.dir-btn.active-neu { background:#4a4a10; color:#ffd080; border-color:#aa8800; }
.dir-btn.active-fwd { background:#1a4a1a; color:#7fff7f; border-color:#2a7a2a; }
.dir-btn.waiting    { border-color:#d9a21b; color:#ffe0a0; background:rgba(90,74,16,0.35); }
.dir-btn.locked { opacity:0.35; pointer-events:none; }
.sdir-row { display:flex; gap:6px; }
.sdir-btn { flex:1; text-align:center; padding:11px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#ccc;
  font-size:15px; font-weight:bold; cursor:pointer; text-decoration:none; }
.sdir-btn.active { background:#1a3a5a; color:#b8e2ff; border-color:#2a6aa8; }
.sdir-btn.locked { opacity:0.35; pointer-events:none; }
.navready-badge { text-align:center; font-size:13px; font-weight:bold; letter-spacing:1px;
  margin-top:6px; padding:6px; border-radius:6px; }
.navready-badge.ready    { color:#a8ffa8; background:rgba(42,122,42,0.25); }
.navready-badge.notready { color:#ffb3b3; background:rgba(90,32,32,0.25); }

/* Gate reason lines — say in words why a control is locked */
.gate-note { text-align:center; color:#ffe0a0; font-size:14px; font-weight:bold;
  min-height:17px; margin-top:6px; }

/* ---- START INTERVAL SLIDER ---- */
.interval-slider-wrap { padding:4px 0 8px; }
.interval-display {
  text-align:center; font-size:30px; font-weight:bold;
  color:#d0b8ff; letter-spacing:3px; margin-bottom:8px;
  font-family:monospace;
}
.interval-display span { font-size:14px; color:#aaa; font-weight:normal; letter-spacing:1px; }
input.interval-slider {
  width:100%; height:32px; cursor:pointer;
  -webkit-appearance:none; appearance:none;
  border-radius:16px; outline:none; border:none;
  background:linear-gradient(to right,#4a2a8a 0%,#7050c0 100%);
}
input.interval-slider::-webkit-slider-thumb {
  -webkit-appearance:none; appearance:none;
  width:36px; height:36px; border-radius:50%;
  background:#ddd; border:2px solid #aaa; cursor:pointer;
  box-shadow:0 2px 6px rgba(0,0,0,0.5);
}
input.interval-slider:disabled { opacity:0.35; }
.interval-tick-row {
  display:flex; justify-content:space-between;
  color:#999; font-size:10px; font-family:monospace;
  padding:0 2px; margin-top:2px;
}
.interval-send-btn {
  display:block; width:100%; margin-top:10px;
  padding:12px; border-radius:8px; border:2px solid #7050c0;
  background:rgba(60,30,100,0.5); color:#d0b8ff;
  font-size:16px; font-weight:bold; letter-spacing:1px;
  cursor:pointer; text-align:center;
}
.interval-send-btn:hover { background:rgba(80,50,130,0.8); }
.interval-send-btn:disabled { opacity:0.35; cursor:default; }
.interval-badge { text-align:center; font-size:13px; font-weight:bold; letter-spacing:1px;
  padding:6px; border-radius:6px; margin-top:6px; }
.interval-badge.set     { color:#d0b8ff; background:rgba(60,30,100,0.35); }
.interval-badge.notset  { color:#bbb;    background:rgba(30,30,30,0.4); }
.interval-badge.waiting { color:#ffe0a0; background:rgba(90,74,16,0.35); }
/* ---- END INTERVAL SLIDER ---- */

/* ---- THROTTLE BIG NUMBER ---- */
.throttle-display {
  text-align:center; font-size:30px; font-weight:bold;
  color:#7fff9f; letter-spacing:3px; margin-bottom:8px;
  font-family:monospace;
}
.throttle-display span { font-size:14px; color:#aaa; font-weight:normal; letter-spacing:1px; }
/* ---- END THROTTLE BIG NUMBER ---- */

/* ---- CURRENT MM / KPH / DNA PANEL ---- */
.mm-landmark {
  text-align:center; font-size:15px; color:#9fd6ff; font-weight:bold;
  letter-spacing:1px; margin-bottom:2px; min-height:18px;
}
.mm-countdown {
  text-align:center; font-size:13px; color:#8ec8f0; font-weight:bold;
  letter-spacing:1px; margin-bottom:8px; min-height:15px; font-style:italic;
}
.big-num { font-size:52px; font-weight:bold; font-family:monospace; line-height:1; }
.big-num.stale { color:#777 !important; }
.age-chip { font-size:12px; color:#999; min-height:14px; text-align:center; font-weight:bold; }
/* ---- END CURRENT MM PANEL ---- */

.slider-row { display:flex; align-items:center; gap:8px; }
.slider-lbl { color:#ddd; font-size:14px; font-weight:bold; letter-spacing:1px; min-width:70px; }
.slider-val { font-size:18px; font-weight:bold; min-width:34px; text-align:right; }
.slider-val.green  { color:#4fc34f; }
.slider-val.yellow { color:#ffc040; }
input.green-slider, input.yellow-slider {
  flex:1; height:28px; cursor:pointer;
  -webkit-appearance:none; appearance:none;
  border-radius:14px; outline:none; border:none; background:#333;
}
input.green-slider:disabled { opacity:0.35; }
input.green-slider::-webkit-slider-thumb,
input.yellow-slider::-webkit-slider-thumb {
  -webkit-appearance:none; appearance:none;
  width:32px; height:32px; border-radius:50%;
  background:#eee; border:2px solid #aaa; cursor:pointer;
  box-shadow:0 2px 6px rgba(0,0,0,0.5);
}
.estop-wrap { display:flex; justify-content:center; }
.estop-btn { width:80%; padding:14px; border-radius:50px; border:3px solid #b32020;
  background:rgba(40,40,40,0.92); color:#ff5050; font-size:19px; font-weight:bold;
  letter-spacing:3px; cursor:pointer; text-align:center; text-decoration:none; display:block; }
.estop-btn.active { background:#b32020; color:white; }
.estop-btn:hover  { background:#8a1515; color:white; border-color:#e04040; }
/* CAL recording */
.cal-row { display:flex; gap:8px; align-items:center; }
.cal-btn { flex:1; text-align:center; padding:10px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#ccc;
  font-size:14px; font-weight:bold; cursor:pointer; text-decoration:none; display:block; }
.cal-btn.recording { background:#3a0a0a; color:#ff6060; border-color:#b32020;
  animation: pulse 1.5s infinite; }
.cal-btn.idle { background:rgba(30,50,30,0.8); color:#7f7; border-color:#4a4; }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.6} }
.cal-file { font-size:11px; color:#999; font-family:monospace; margin-top:4px;
  white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr; gap:6px; margin-bottom:4px; }
.telem-cell { background:#222; border-radius:7px; padding:7px 4px; text-align:center; }
.telem-lbl  { font-size:12px; color:#ddd; margin-bottom:2px; font-weight:bold; }
.telem-val  { font-size:19px; font-weight:bold; color:#4fc34f; }
.telem-val.warn  { color:#ffc040; }
.telem-val.stale { color:#777; }
.telem-age  { font-size:10px; color:#999; min-height:12px; font-weight:bold; }
.block-display { text-align:center; color:#f1f1f1; font-size:14px; font-weight:bold;
  margin-top:4px; letter-spacing:2px; }
.log-btn { background:#333; border:1px solid #555; color:#ccc; font-size:12px;
  padding:5px 12px; border-radius:6px; cursor:pointer; font-weight:bold; }
.log-btn.on { background:#1a3a5a; color:#b8e2ff; border-color:#2a6aa8; }
</style></head><body>
{{ nav_html }}
<div class="container">

  <!-- Header: name, ONLINE badge, status line, motion -->
  <div class="panel-tight">
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

  <!-- 1. Session Direction -->
  <div class="panel-tight" id="panel-sdir">
    <div class="sec-hdr">STEP 1 — SESSION DIRECTION</div>
    <div class="sdir-row">
      <button onclick="sendSessionDir('CW')" id="sdir-cw"  class="sdir-btn">CLOCKWISE</button>
      <button onclick="sendSessionDir('CCW')" id="sdir-ccw" class="sdir-btn">COUNTER-CLOCKWISE</button>
    </div>
    <div class="navready-badge notready" id="navready-badge">SESSION DIRECTION NOT CONFIRMED THIS SESSION</div>
  </div>

  <!-- 2. Start Interval -->
  <div class="panel-tight" id="panel-interval">
    <div class="sec-hdr">STEP 2 — START INTERVAL</div>
    <div class="interval-slider-wrap">
      <div class="interval-display" id="interval-display">
        <span style="color:#999;">— slide to select —</span>
      </div>
      <input type="range" min="0" max="170" step="1" value="85"
             id="interval-slider" class="interval-slider"
             oninput="onIntervalSlide(this.value)" />
      <div class="interval-tick-row">
        <span>000</span><span>017</span><span>034</span><span>051</span>
        <span>068</span><span>085</span><span>102</span><span>119</span>
        <span>136</span><span>153</span><span>170</span>
      </div>
      <button class="interval-send-btn" id="interval-send" onclick="sendInterval()">
        SET INTERVAL
      </button>
      <div class="gate-note" id="interval-gate-note"></div>
    </div>
    <div class="interval-badge notset" id="interval-badge">NO INTERVAL CONFIRMED THIS SESSION</div>
  </div>

  <!-- 3. Motor Direction — illuminates only on this loco's confirmed
       state/direction echo (SOLONAV 2_22 and QUORUM alike) -->
  <div class="panel-tight" id="panel-dir">
    <div class="sec-hdr">STEP 3 — MOTOR DIRECTION</div>
    <div class="dir-row">
      <button onclick="sendDir(0)" id="dir-btn-0" class="dir-btn">Reverse</button>
      <button onclick="sendDir(1)" id="dir-btn-1" class="dir-btn">Neutral</button>
      <button onclick="sendDir(2)" id="dir-btn-2" class="dir-btn">Forward</button>
    </div>
    <div class="gate-note" id="dir-gate-note"></div>
  </div>

  <!-- 4. Mode -->
  <div class="panel-tight">
    <div class="sec-hdr">STEP 4 — MODE</div>
    <div class="mode-row" id="mode-row">
      <span class="mode-btn man-active" id="mode-man">MANUAL</span>
      <a href="/{{ slug }}/mode/1" class="mode-btn" id="mode-auto">AUTO</a>
    </div>
    <div class="cto-note" id="cto-note" style="display:none;">AUTO — Dispatcher in control</div>
  </div>

  <!-- Throttle. NEVER locked in MANUAL (v1.10.2 operator ruling): undeclared
       operation is signalled by the status line, not by a disabled control.
       The slider is the OPERATOR'S commanded value and never moves by itself;
       the loco's actual PWM is the separate read-only figure. -->
  <div class="panel-tight" id="panel-throttle">
    <div class="sec-hdr">THROTTLE</div>
    <div class="throttle-display" id="throttle-display">0<span> / 255</span></div>
    <div style="text-align:center;font-size:14px;font-weight:bold;color:#9fd6ff;margin-bottom:6px;">
      {{ name }} PWM: <span id="thr-actual" style="font-family:monospace;">&mdash;</span>
      <span id="thr-actual-age" style="color:#999;font-size:11px;"></span>
    </div>
    <div class="slider-row">
      <input type="range" min="0" max="255" value="0" step="1"
             id="throttle-slider" class="green-slider"
             oninput="sendThrottle(this.value);
                      document.getElementById('thr-val').textContent=this.value;
                      document.getElementById('throttle-display').innerHTML=this.value+'<span> / 255</span>';" />
      <span class="slider-val green" id="thr-val">0</span>
    </div>
    <div class="gate-note" id="throttle-gate-note"></div>
  </div>

  <!-- Brake -->
  <div class="panel-tight">
    <div class="slider-row">
      <span class="slider-lbl">BRAKE</span>
      <input type="range" min="0" max="255" value="0" step="1"
             id="brake-slider" class="yellow-slider"
             style="direction:rtl;"
             oninput="sendCmd('brake',this.value);
                      document.getElementById('brake-val').textContent=this.value" />
      <span class="slider-val yellow" id="brake-val">0</span>
    </div>
  </div>

  <!-- E-STOP — never gated, works in every state -->
  <div class="panel-tight">
    <div class="estop-wrap">
      <button onclick="toggleEstop()" id="estop-btn" class="estop-btn">E-STOP</button>
    </div>
  </div>

  <!-- CAL Recording -->
  <div class="panel-tight">
    <div class="sec-hdr">CAL RECORDING</div>
    <div class="cal-row">
      <button id="cal-btn" class="cal-btn idle" onclick="toggleCal()">
        &#9679; START RECORDING
      </button>
    </div>
    <div class="cal-file" id="cal-file"></div>
  </div>

  <!-- Telemetry. The INA219 tiles are wired to telem/voltage|current|power
       and stay ready for them, but NO CURRENT FIRMWARE PUBLISHES THOSE
       TOPICS — INA219 support was dropped at SOLONAV 2.1 and QUORUM inherits
       the gap. The footnote below says so, because an unexplained "—" reads
       as a dead sensor when it is actually an absent feature. -->
  <div class="panel-tight">
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
      <div class="telem-cell">
        <div class="telem-lbl">Low V</div>
        <div class="telem-val stale" id="telem-lowvolt">&mdash;</div>
        <div class="telem-age" id="telem-lowvolt-age"></div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">PWM</div>
        <div class="telem-val stale" id="telem-pwm">&mdash;</div>
        <div class="telem-age" id="telem-pwm-age"></div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">pKPH</div>
        <div class="telem-val stale" id="telem-pkph">&mdash;</div>
        <div class="telem-age" id="telem-pkph-age"></div>
      </div>
    </div>
    <div id="ina-note" style="display:none;text-align:center;color:#ffe0a0;
      font-size:12px;font-weight:bold;margin-top:6px;line-height:1.35;">
      VOLTAGE / CURRENT / POWER NOT PUBLISHED BY THIS FIRMWARE —
      INA219 telemetry was removed at SOLONAV 2.1. Not a sensor fault.
    </div>
    <div class="block-display" id="block-display"></div>
  </div>

  <!-- Current MM / KpH / DNA -->
  <div class="panel-tight">
    <div style="display:flex;justify-content:space-around;align-items:flex-end;margin-bottom:4px;">
      <div style="flex:1;display:flex;flex-direction:column;align-items:center;">
        <div class="big-num stale" id="mm-display" style="color:#9fd6ff;">&mdash;</div>
        <div class="age-chip" id="mm-display-age"></div>
        <div style="color:#9fd6ff;font-size:22px;font-weight:bold;letter-spacing:2px;margin-top:2px;">MM</div>
      </div>
      <div style="flex:1;display:flex;flex-direction:column;align-items:center;">
        <div class="big-num stale" id="kph-display" style="color:#ffd080;">&mdash;</div>
        <div class="age-chip" id="kph-display-age"></div>
        <div style="color:#ffd080;font-size:22px;font-weight:bold;letter-spacing:2px;margin-top:2px;">KpH</div>
      </div>
      <div style="flex:1;display:flex;flex-direction:column;align-items:center;">
        <!-- DNA features do not exist yet: placeholder by design -->
        <div class="big-num stale" id="dna-display">&mdash;</div>
        <div class="age-chip"></div>
        <div style="color:#999;font-size:22px;font-weight:bold;letter-spacing:2px;margin-top:2px;">DNA</div>
      </div>
    </div>
    <div class="mm-landmark" id="mm-landmark"></div>
    <div class="mm-countdown" id="mm-countdown"></div>
  </div>

  <!-- Polarity agreement — session AGREE/DISAGREE from THIS loco's nav
       events, reset on its epoch. Permanent instrumentation: a noisy cable
       read 27% and a flipped sensor 100% in the same week. -->
  <div class="panel-tight">
    <div class="sec-hdr">POLARITY AGREEMENT</div>
    <div style="display:flex;justify-content:space-around;align-items:center;">
      <div style="text-align:center;">
        <div class="big-num stale" id="agree-pct" style="font-size:44px;">&mdash;</div>
        <div style="color:#ddd;font-size:14px;font-weight:bold;letter-spacing:1px;margin-top:2px;">AGREE %</div>
      </div>
      <div style="text-align:left;color:#ddd;font-size:15px;font-weight:bold;line-height:1.7;">
        <span id="agree-n" style="color:#7fff7f;font-family:monospace;">0</span> agree<br>
        <span id="disagree-n" style="color:#ff8080;font-family:monospace;">0</span> disagree
      </div>
    </div>
    <div id="verdict-ticks" style="display:flex;gap:5px;justify-content:center;margin-top:10px;flex-wrap:wrap;min-height:34px;"></div>
  </div>

  <!-- Packet log — collapsed by default, periodic republishes filtered -->
  <div class="panel-tight">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;gap:6px;">
      <span style="color:#ddd;font-size:13px;font-weight:bold;letter-spacing:1px;">PACKET LOG</span>
      <span style="flex:1;"></span>
      <button class="log-btn" id="log-toggle" onclick="toggleLogOpen()">EXPAND</button>
      <button class="log-btn" id="log-filter" onclick="toggleLogFilter()" style="display:none;">SHOW PERIODIC</button>
      <button class="log-btn" id="log-clear"  onclick="clearLog('loco-log')" style="display:none;">CLEAR</button>
    </div>
    <div id="loco-log" style="display:none;color:#fff;background:#0a0a0a;font-size:11px;
      font-family:monospace;padding:8px;border-radius:6px;max-height:300px;overflow-y:auto;">
      <div style="color:#aaa;font-size:11px;">Waiting for packets...</div>
    </div>
  </div>

</div>
<script>
// ==========================================================================
// All rendering below is driven by /{{ slug }}/state — the ACTIVE TAB's
// locomotive, and nothing else. v1.10.2 operator ruling: the dashboard must
// never say no to a manual operator. Controls are disabled only by the AUTO
// chamber (dispatcher control); everything else is loud truth in the status
// line. Declared/undeclared state MIRRORS the locomotive's reported nav
// state; the dashboard never generates a re-declaration requirement itself.
// ==========================================================================
// ---- FAULT BANNER (v1.10.6) -----------------------------------------------
// There is no console on a phone in a garden. An exception inside pollState()
// would stop every later DOM update — controls freeze, values stop moving —
// and leave the operator nothing to report but "it broke". Paint it on the
// page instead. This runs BEFORE anything else so it can catch failures in
// the code below it.
function showFault(msg){
  try {
    var b = document.getElementById('fault-banner');
    if(!b) return;
    var t = new Date().toTimeString().slice(0,8);
    b.style.display = '';
    b.textContent = '⚠ DASHBOARD FAULT ' + t + ' — ' + msg +
                    '  (controls still work; reload to clear)';
  } catch(e) {}
}
window.addEventListener('error', function(e){
  showFault((e.message||'error') + ' @' + (e.lineno||'?'));
});
window.addEventListener('unhandledrejection', function(e){
  showFault('promise: ' + (e.reason && e.reason.message ? e.reason.message : e.reason));
});

var LOCO = '{{ name }}'.toUpperCase();
var STALE_S = 5;          // seconds after which a value is stale
var isCto = false;
var lastEpoch = null;     // server session epoch — a bump means the loco rebooted
// v1.10.3: pendingDir and its confirmation timer are DELETED. Nothing in the
// dashboard waits on a direction echo any more — the button lights optimistically
// on tap and is re-rendered passively from the locomotive's confirmed state.
// v1.10.4 (BUG 2): the last direction the OPERATOR commanded. It keeps the
// button lit until the locomotive actually reports a direction, so a missing
// echo can never blank the selection or let it fall back to the Neutral
// default. It is a display memory only — nothing is gated on it.
var lastCommandedDir = null;

function ageOf(s, f){
  var a = s.ages ? s.ages[f] : null;
  return (a === null || a === undefined) ? null : a;
}
function isFresh(s, f){ var a = ageOf(s, f); return a !== null && a <= STALE_S; }

// value+age -> tile. Never received: em-dash. Stale: gray + "N s ago".
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

// ---- CAL recording ----
var calRunning = false;
function toggleCal() {
  var action = calRunning ? 'stop' : 'start';
  fetch('/{{ slug }}/cal/' + action, {method:'POST'})
    .then(r => r.json())
    .then(data => { calRunning = data.running; updateCalBtn(data); })
    .catch(e => console.error(e));
}
function updateCalBtn(data) {
  var btn  = document.getElementById('cal-btn');
  var file = document.getElementById('cal-file');
  if (data.running) {
    btn.textContent  = '● RECORDING — TAP TO STOP';
    btn.className    = 'cal-btn recording';
    file.textContent = data.file ? data.file.split('/').pop() : '';
  } else {
    btn.textContent  = '● START RECORDING';
    btn.className    = 'cal-btn idle';
    file.textContent = data.file ? 'Saved: ' + data.file.split('/').pop() : '';
  }
}
function pollCalStatus() {
  fetch('/{{ slug }}/cal/status')
    .then(r => r.json())
    .then(data => { calRunning = data.running; updateCalBtn(data); })
    .catch(e => {});
}
setInterval(pollCalStatus, 4000);
pollCalStatus();
// ---- End CAL recording ----

// ---- Interval slider: purely local until SET INTERVAL is pressed ----
var pendingInterval = null;   // staged by the slider, sent on button press
var pendingSend = null;       // {interval, sentAt} — informative only

function mmToInterval(mm) {
  var lo = mm % 171;
  var hi = (lo + 1) % 171;
  return ('000'+lo).slice(-3) + '-' + ('000'+hi).slice(-3);
}
function onIntervalSlide(val) {
  var mm = parseInt(val);
  pendingInterval = mmToInterval(mm);
  var lo = ('000'+mm).slice(-3);
  var hi = ('000'+((mm+1)%171)).slice(-3);
  document.getElementById('interval-display').innerHTML =
    '<b>'+lo+'</b><span> — </span><b>'+hi+'</b>';
}

function sendInterval() {
  var btn = document.getElementById('interval-send');
  if (btn.disabled) return;                  // AUTO chamber only — nothing else disables it
  if (!pendingInterval) {
    document.getElementById('interval-badge').textContent = 'SLIDE TO SELECT AN INTERVAL FIRST';
    document.getElementById('interval-badge').className = 'interval-badge notset';
    return;
  }
  // Double-tap glitch guard only (1 s); pending confirmation never blocks.
  if (pendingSend && Date.now() - pendingSend.sentAt < 1000) return;
  pendingSend = { interval: pendingInterval, sentAt: Date.now() };
  document.getElementById('interval-badge').textContent = 'WAITING FOR CONFIRMATION…';
  document.getElementById('interval-badge').className = 'interval-badge waiting';
  fetch('/{{ slug }}/startinterval/'+pendingSend.interval, {method:'POST'})
    .catch(e => console.error(e));
}
// ---- End interval logic ----

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
var MM_TOTAL = 171;
function mmCountdown(currentMmStr) {
  var cur = parseInt(currentMmStr, 10);
  if (isNaN(cur)) return '';
  var best = null;
  var bestDist = MM_TOTAL + 1;
  for (var i = 0; i < MM_STATIONS.length; i++) {
    var st = MM_STATIONS[i];
    var ahead = (st.mm - cur + MM_TOTAL) % MM_TOTAL;
    var behind = (cur - st.mm + MM_TOTAL) % MM_TOTAL;
    if (ahead === 0) return '';
    if (ahead > 0 && ahead <= 5 && ahead < bestDist) {
      bestDist = ahead;
      best = { label: st.name + ' → ' + ahead, type: 'ahead' };
    }
    if (behind > 0 && behind <= 5) {
      if (best === null || best.type !== 'ahead') {
        best = { label: '← ' + st.name + ' ' + behind, type: 'behind' };
      }
    }
  }
  return best ? best.label : '';
}
// ---- End station MM countdown ----

// The whole manual command path: read the value, publish it. The isCto test
// is the CHAMBER boundary (dispatcher in control), not a manual gate — in
// MANUAL it is always false. Nothing else may be added here.
function sendCmd(sub, val) {
  if (isCto) return;
  fetch('/{{ slug }}/cmd/'+sub+'/'+encodeURIComponent(val),{method:'POST'}).catch(e=>console.error(e));
}

// THROTTLE — publishes immediately, but never stacks requests (v1.10.6).
// A finger drag emits ~60 input events a second and v1.10.3-1.10.5 fired a
// fetch for every one. iOS allows ~6 connections per host, so the surplus
// queued and pollState()'s own fetch queued behind them. This is NOT a
// debounce and NOT a confirmation wait: the first move goes out at once, and
// if one is already in flight the NEWEST value is held and sent the moment it
// returns. The last thing published is always where the slider ended up.
var thrInFlight = false, thrPending = null;
function sendThrottle(v) {
  document.getElementById('thr-val').textContent = v;
  document.getElementById('throttle-display').innerHTML = v + '<span> / 255</span>';
  if (isCto) return;
  if (thrInFlight) { thrPending = v; return; }      // coalesce to the latest
  thrInFlight = true;
  fetch('/{{ slug }}/cmd/throttle/' + encodeURIComponent(v), {method:'POST'})
    .catch(function(e){ console.error(e); })
    .then(function(){
      thrInFlight = false;
      if (thrPending !== null) { var p = thrPending; thrPending = null; sendThrottle(p); }
    });
}

// E-STOP: never gated — fires in every state including UNSET, LOST and stale.
function toggleEstop() {
  var btn = document.getElementById('estop-btn');
  var isActive = btn.classList.contains('active');
  if (isActive) {
    btn.textContent = 'E-STOP';
    btn.classList.remove('active');
  } else {
    btn.textContent = 'E-STOP ACTIVE — TAP TO CLEAR';
    btn.classList.add('active');
  }
  // E-STOP GOES FIRST, always. Nothing is added ahead of this line.
  fetch('/{{ slug }}/cmd/estop/' + (isActive ? '0' : '1'), {method:'POST'})
    .catch(e => {
      console.error(e);
      if (isActive) {
        btn.textContent = 'E-STOP ACTIVE — TAP TO CLEAR';
        btn.classList.add('active');
      } else {
        btn.textContent = 'E-STOP';
        btn.classList.remove('active');
      }
    });
  // BUG 1 (SAFETY): zero the throttle behind it. A slider still showing 120
  // after an E-STOP is a loaded gun — clearing E-STOP could resume at speed.
  // Fired on BOTH engage and clear, so the control reads 0 whenever E-STOP
  // has been touched. UNGATED like estop itself: no isCto test, no state
  // check. The firmware already zeroes PWM on estop; this makes the
  // OPERATOR'S control agree with it.
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
  fetch('/{{ slug }}/cmd/throttle/0', {method:'POST'}).catch(e => console.error(e));
}

// TAP PUBLISHES. Nothing between the tap and the fetch — no bounce guard, no
// pending state, no confirmation wait. v1.10.2 swallowed a deliberate
// same-value re-press inside 1 s and set a WAITING state first; both are
// gone. The button shows it registered the tap AT ONCE (optimistic), and the
// locomotive's confirmed echo re-renders it passively in pollState().
function sendDir(d) {
  if (isCto) return;                                  // AUTO chamber, not a manual gate
  fetch('/{{ slug }}/cmd/direction/'+d, {method:'POST'}).catch(e => console.error(e));
  lastCommandedDir = String(d);                       // display memory (BUG 2)
  updateDirButtons(d, true);                          // optimistic, after the publish
}

function sendSessionDir(d) {
  if (isCto) return;
  fetch('/{{ slug }}/sessiondir/'+d, {method:'GET'}).catch(e => console.error(e));
  // No optimistic highlight: the button lights only on this loco's confirmed
  // state/session_direction.
}

function updateDirButtons(d, confirmed) {
  // Illumination = THIS loco's confirmed state/direction, once confirmed this
  // session. Staleness does not un-light it; only an epoch reset clears it.
  var classes = {0:'active-rev', 1:'active-neu', 2:'active-fwd'};
  [0,1,2].forEach(function(i){
    var btn = document.getElementById('dir-btn-'+i);
    if(!btn) return;
    btn.classList.remove('active-rev','active-neu','active-fwd','waiting');
    if(confirmed && String(i)===String(d)) btn.classList.add(classes[i]);
  });
}

function fmt1(v){ var n=parseFloat(v); return isNaN(n)?v:n.toFixed(1); }

function resetLocalSession(){
  // The LOCOMOTIVE rebooted (its epoch bumped): drop everything staged
  // locally. This is the only dashboard-side reset there is.
  pendingSend = null;
  lastCommandedDir = null;      // the loco rebooted; forget what we commanded
  document.getElementById('dir-gate-note').textContent = '';
  document.getElementById('interval-badge').textContent = 'NO INTERVAL DECLARED';
  document.getElementById('interval-badge').className = 'interval-badge notset';
}

function pollState(){
  fetch('/{{ slug }}/state').then(r=>r.json()).then(s=>{
    // ---- Session epoch: the loco rebooted -> local staging clears ----
    if (lastEpoch === null) { lastEpoch = s.epoch; }
    else if (s.epoch !== lastEpoch) { lastEpoch = s.epoch; resetLocalSession(); }

    isCto = (s.auto === '1');

    var heardAge = ageOf(s, 'heard');
    var alive = heardAge !== null && heardAge <= STALE_S;

    // ---- ONLINE badge: driven by whether the loco is actually heard ----
    var badge = document.getElementById('online-badge');
    if (alive) { badge.textContent='ONLINE'; badge.className='badge badge-online'; }
    else if (heardAge !== null) { badge.textContent='STALE'; badge.className='badge badge-stale'; }
    else { badge.textContent='OFFLINE'; badge.className='badge badge-offline'; }

    // ---- Motion: the loco's reported PWM, never the slider ----
    var motion = !isFresh(s,'moving') ? 'UNKNOWN' : (s.moving==='1' ? 'MOVING' : 'STOPPED');
    var mb = document.getElementById('motion-bar');
    if (motion==='MOVING')      { mb.textContent='MOVING';  mb.className='motion-bar moving'; }
    else if (motion==='STOPPED'){ mb.textContent='STOPPED'; mb.className='motion-bar stopped'; }
    else { mb.textContent = heardAge===null ? 'UNKNOWN — NO TELEMETRY' : 'UNKNOWN — STALE';
           mb.className='motion-bar unknown'; }

    // ---- Declaration MIRRORS the locomotive's reported nav state. A manual
    // stop changes nothing; only the LOCO reporting UNSET undeclares. ----
    var declared = (s.nav==='TRACKING' || s.nav==='NORMAL' || s.nav==='EVALUATING');
    var sdirConfirmed = (s.session_dir==='CW' || s.session_dir==='CCW')
                        && ageOf(s,'session_dir') !== null;
    var intervalConfirmed = s.start_interval && s.start_interval!=='UNSET'
                        && ageOf(s,'start_interval') !== null;
    var navReady = s.nav_ready === '1' && declared;

    // ---- Interval confirmation round-trip (informative only) ----
    if (pendingSend) {
      if (intervalConfirmed && s.start_interval === pendingSend.interval) {
        pendingSend = null;   // the loco said it back — banner set below
      } else if (Date.now() - pendingSend.sentAt > 12000) {
        pendingSend = null;
        document.getElementById('interval-badge').textContent =
          'NO CONFIRMATION — COMMAND MAY NOT HAVE ARRIVED';
        document.getElementById('interval-badge').className = 'interval-badge notset';
      }
    }
    var ibadge = document.getElementById('interval-badge');
    if (pendingSend) {
      ibadge.textContent = 'WAITING FOR CONFIRMATION…';
      ibadge.className = 'interval-badge waiting';
    } else if (intervalConfirmed) {
      ibadge.textContent = 'INTERVAL SET — ' + s.start_interval + ' — CONFIRMED';
      ibadge.className = 'interval-badge set';
    } else if (declared) {
      ibadge.textContent = 'POSITION DECLARED';
      ibadge.className = 'interval-badge set';
    } else if (ibadge.className.indexOf('waiting') !== -1) {
      ibadge.textContent = 'NO INTERVAL DECLARED';
      ibadge.className = 'interval-badge notset';
    }

    // ---- SET INTERVAL: a REMINDER, never a refusal (v1.10.5). Motion used
    // to disable this button to protect declaration integrity after the
    // 2026-08-01 mid-motion declare corruption. The operator has ruled that
    // protection advisory: the note stays, the block goes. The operator
    // decides when it is safe to declare. AUTO remains a chamber boundary,
    // not a manual gate. ----
    var intervalGateNote = '';
    if (isCto)                     intervalGateNote = 'AUTO — DISPATCHER IN CONTROL';
    else if (motion === 'MOVING')  intervalGateNote = LOCO + ' REPORTS MOVING — CONFIRM STOPPED BEFORE DECLARING';
    else if (motion === 'UNKNOWN') intervalGateNote = 'MOTION UNKNOWN — CONFIRM ' + LOCO + ' IS STOPPED BEFORE DECLARING';
    else if (pendingSend)          intervalGateNote = 'WAITING FOR CONFIRMATION…';
    document.getElementById('interval-gate-note').textContent = intervalGateNote;
    document.getElementById('interval-send').disabled = isCto;   // AUTO only
    document.getElementById('interval-slider').disabled = isCto;

    // ---- MANUAL controls are NEVER locked (v1.10.2). AUTO (dispatcher
    // control) is the only disable; it is a chamber, not a gate. ----
    document.getElementById('throttle-slider').disabled = isCto;
    document.getElementById('brake-slider').disabled = isCto;
    document.getElementById('throttle-gate-note').textContent =
      isCto ? 'AUTO — DISPATCHER IN CONTROL' : '';

    // ---- Session direction buttons: highlight = confirmation only ----
    document.getElementById('sdir-cw').classList.toggle('active',  sdirConfirmed && s.session_dir==='CW');
    document.getElementById('sdir-ccw').classList.toggle('active', sdirConfirmed && s.session_dir==='CCW');
    document.getElementById('sdir-cw').classList.toggle('locked',  isCto);
    document.getElementById('sdir-ccw').classList.toggle('locked', isCto);
    var navBadge = document.getElementById('navready-badge');
    if (navReady) {
      navBadge.textContent = 'NAV READY — ' + s.session_dir;
      navBadge.className = 'navready-badge ready';
    } else if (sdirConfirmed) {
      navBadge.textContent = 'DIRECTION ' + s.session_dir + ' CONFIRMED';
      navBadge.className = 'navready-badge notready';
    } else {
      navBadge.textContent = 'SESSION DIRECTION NOT CONFIRMED THIS SESSION';
      navBadge.className = 'navready-badge notready';
    }

    // ---- Motor direction: PASSIVE DISPLAY ONLY. The button is lit from this
    // loco's reported state/direction integer (SOLONAV 2_22 and QUORUM
    // alike). Nothing waits on it, nothing times out on it, and no control's
    // behaviour depends on it — an echo that never arrives costs the operator
    // a light, never a command.
    //
    // BUG 2 (v1.10.4): if the locomotive has NEVER reported a direction, hold
    // the operator's last commanded value lit instead of clearing every
    // button. v1.10.3 blanked them, and the state default is "1" = NEUTRAL,
    // so the display drifted toward Neutral on a missing echo. Mirror the
    // locomotive whenever it actually speaks; fall back to the operator's own
    // command, never to a default.
    var dirReported = ageOf(s,'direction') !== null;
    if (dirReported) updateDirButtons(s.direction, true);
    else if (lastCommandedDir !== null) updateDirButtons(lastCommandedDir, true);
    else updateDirButtons(null, false);

    // When the locomotive reports a DIFFERENT direction from the one last
    // commanded, it refused the change — QUORUM refuses cmd/direction while
    // motorIsMoving() or in AUTO, and says so on state/warning as
    // DIR_REFUSED. Name it, so a refusal does not read as a glitch. Display
    // only: the buttons stay live and the command path is untouched.
    var dirNote = document.getElementById('dir-gate-note');
    if (dirReported && lastCommandedDir !== null && s.direction !== lastCommandedDir) {
      var want = {0:'REVERSE',1:'NEUTRAL',2:'FORWARD'}[lastCommandedDir] || lastCommandedDir;
      var got  = {0:'REVERSE',1:'NEUTRAL',2:'FORWARD'}[s.direction] || s.direction;
      dirNote.textContent = want + ' NOT ACCEPTED — ' + LOCO + ' REPORTS ' + got
                          + ' (STOP FIRST: DIRECTION IS REFUSED WHILE MOVING)';
    } else {
      dirNote.textContent = '';
    }
    [0,1,2].forEach(function(i){
      document.getElementById('dir-btn-'+i).classList.toggle('locked', isCto);
    });

    // ---- Mode row ----
    var man = document.getElementById('mode-man');
    var auto = document.getElementById('mode-auto');
    if (isCto) {
      man.className = 'mode-btn locked'; auto.className = 'mode-btn auto-active';
      auto.removeAttribute('href');
      document.getElementById('cto-note').style.display = '';
    } else {
      man.className = 'mode-btn man-active'; auto.className = 'mode-btn';
      auto.setAttribute('href', '/{{ slug }}/mode/1');
      document.getElementById('cto-note').style.display = 'none';
    }

    // ---- Actual PWM readout (the slider is never written) ----
    var actEl = document.getElementById('thr-actual');
    var actAge = document.getElementById('thr-actual-age');
    var pwmAge = ageOf(s,'pwm');
    if (pwmAge === null || s.pwm === '--') {
      actEl.innerHTML = '&mdash;'; actEl.style.color = '#777'; actAge.textContent = '';
    } else if (pwmAge > STALE_S) {
      actEl.textContent = s.pwm; actEl.style.color = '#777';
      actAge.textContent = Math.round(pwmAge) + ' s ago';
    } else {
      actEl.textContent = s.pwm; actEl.style.color = '#9fd6ff'; actAge.textContent = '';
    }

    // ---- Telemetry tiles: value + age, dash when silent ----
    setTile('telem-voltage', fmt1(s.voltage), ageOf(s,'voltage'));
    setTile('telem-current', fmt1(s.current), ageOf(s,'current'));
    setTile('telem-power',   fmt1(s.power),   ageOf(s,'power'));
    setTile('telem-lowvolt', s.lowvolt==='--' ? '--' : (s.lowvolt==='1'?'LOW':'OK'), ageOf(s,'lowvolt'));
    document.getElementById('telem-lowvolt').classList.toggle('warn', s.lowvolt==='1' && isFresh(s,'lowvolt'));
    setTile('telem-pwm',  s.pwm,  ageOf(s,'pwm'));
    setTile('telem-pkph', s.pkph, ageOf(s,'pkph'));
    // BUG 3: explain the INA219 blanks rather than leaving them to be read as
    // a dead feed. Shown only once the locomotive is otherwise talking — if
    // nothing at all has arrived, the status line already says so, and this
    // note would be noise on top of it.
    var inaNote = document.getElementById('ina-note');
    if (inaNote) {
      var inaSilent = ageOf(s,'voltage') === null && ageOf(s,'current') === null
                   && ageOf(s,'power') === null;
      inaNote.style.display = (inaSilent && heardAge !== null) ? '' : 'none';
    }
    document.getElementById('block-display').textContent =
      (s.block && s.block !== '--') ? '{{ name }}: ' + s.block : '';

    // ---- MM / KpH — this loco's position and speed ----
    setTile('mm-display',  s.mm,   ageOf(s,'mm'));
    setTile('kph-display', s.pkph !== '--' ? String(Math.round(parseFloat(s.pkph))) : '--', ageOf(s,'pkph'));
    var mmLive = isFresh(s,'mm') && s.mm !== '--';
    document.getElementById('mm-landmark').textContent = mmLive ? (s.landmark || '') : '';
    document.getElementById('mm-countdown').textContent = mmLive ? mmCountdown(s.mm) : '';

    // ---- Polarity agreement tile (v1.10.2, Change 6) ----
    var tot = (s.agree_n||0) + (s.disagree_n||0);
    var pctEl = document.getElementById('agree-pct');
    if (tot > 0) {
      var pct = Math.round(100*(s.agree_n||0)/tot);
      pctEl.textContent = pct;
      pctEl.classList.remove('stale');
      pctEl.style.color = pct>=90 ? '#7fff7f' : (pct>=60 ? '#ffd080' : '#ff8080');
    } else {
      pctEl.innerHTML = '&mdash;';
      pctEl.classList.add('stale');
      pctEl.style.color = '';
    }
    document.getElementById('agree-n').textContent = s.agree_n||0;
    document.getElementById('disagree-n').textContent = s.disagree_n||0;
    var ticks = (s.verdicts||[]).map(function(v){
      var ok = v[1] === 1;
      var mm = (v[0]>=0) ? ('000'+v[0]).slice(-3) : '???';
      return '<span style="display:inline-block;min-width:30px;text-align:center;'
           + 'padding:3px 2px;border-radius:5px;font-family:monospace;font-size:12px;font-weight:bold;'
           + (ok ? 'background:#14421a;color:#7fff7f;border:1px solid #2a7a2a;'
                 : 'background:#5a2020;color:#ffb3b3;border:1px solid #b32020;')
           + '">' + mm + '</span>';
    }).join('');
    document.getElementById('verdict-ticks').innerHTML = ticks;

    // ---- Warning line (the firmware clears it itself after 20 s) ----
    document.getElementById('warning-line').textContent =
      (alive && s.warning) ? s.warning : '';

    // ---- E-STOP button reflects the loco's confirmed estop state ----
    var estopBtn = document.getElementById('estop-btn');
    if (s.estop === '1' && ageOf(s,'estop') !== null) {
      estopBtn.textContent = 'E-STOP ACTIVE — TAP TO CLEAR';
      estopBtn.className = 'estop-btn active';
    } else {
      estopBtn.textContent = 'E-STOP';
      estopBtn.className = 'estop-btn';
    }

    // ---- Plain-language status line. Loss of position, silence and
    // undeclared operation are LOUD here; they never remove controls. ----
    var line, cls;
    if (heardAge === null) {
      line = LOCO + ' SILENT — NO TELEMETRY THIS SESSION'; cls = 'bad';
    } else if (heardAge > STALE_S) {
      line = 'TELEMETRY STALE — LAST HEARD ' + Math.round(heardAge) + ' s AGO'; cls = 'bad';
    } else if (s.estop === '1') {
      line = 'E-STOP ACTIVE'; cls = 'bad';
    } else if (s.nav === 'UNSET') {
      line = 'POSITION NOT DECLARED — NAVIGATION WILL NOT TRACK'; cls = 'warn';
    } else if (s.nav === 'LOST') {
      line = 'POSITION LOST — MANUAL CONTROL RETAINED — STOP AND RE-DECLARE WHEN READY';
      cls = 'bad';
    } else if (s.nav === 'NO_QUORUM') {
      line = 'POSITION UNCERTAIN — QUORUM UNRESOLVED — MANUAL CONTROL RETAINED'
           + (motion === 'STOPPED' ? ' — RE-DECLARE INTERVAL TO CONTINUE'
                                   : ' — STOP AND RE-DECLARE WHEN READY');
      cls = 'bad';
    } else if (pendingSend) {
      line = 'WAITING FOR CONFIRMATION…'; cls = 'warn';
    } else if (s.nav === 'EVALUATING') {
      line = 'CHECKING POSITION — QUORUM EVALUATING — MM ' + s.mm; cls = 'warn';
    } else {
      line = 'TRACKING — MM ' + s.mm; cls = 'ok';
    }
    var sl = document.getElementById('status-line');
    sl.textContent = line;
    sl.className = 'status-line ' + cls;
  }).catch(e=>{});
}

// ---- Packet log: collapsed by default, periodic republishes hidden ----
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
  document.getElementById(id).innerHTML='<div style="color:#aaa;font-size:11px;">Cleared — resuming in 5s</div>';
  logPausedUntil[id]=Date.now()+5000;
}
function renderLog(){
  if(!logOpen) return;
  if(logPausedUntil['loco-log'] && Date.now()<logPausedUntil['loco-log']) return;
  var shown = showPeriodic ? logEntries : logEntries.filter(function(e){ return !e.p; });
  if(!shown.length){
    document.getElementById('loco-log').innerHTML =
      '<div style="color:#aaa;font-size:11px;">No events' +
      (logEntries.length ? ' (periodic republishes hidden — tap SHOW PERIODIC)' : '') + '</div>';
    return;
  }
  document.getElementById('loco-log').innerHTML = shown.map(function(e){
    return e.ts+'  '+e.topic+'  '+e.value;
  }).join('<br>');
}
function pollLog(){
  if(!logOpen) return;   // don't poll a hidden log
  fetch('/{{ slug }}/log').then(r=>r.json()).then(entries=>{
    logEntries = entries;
    renderLog();
  }).catch(e=>{});
}

setInterval(pollState,1000);
setInterval(pollLog,1000);
pollState();
</script>
</body></html>"""


# ============================================================================
# Loco render helper
# ============================================================================
def render_loco(lid, name, slug, nav_active):
    return render_template_string(
        LOCO_HTML,
        name=name, lid=lid, slug=slug,
        nav_html=nav(nav_active), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )


# ============================================================================
# Root
# ============================================================================
@app.route("/")
def index():
    return redirect(url_for("console"))


# ============================================================================
# Dispatcher routes
# ============================================================================
@app.route("/console")
def console():
    with mqtt_lock:
        oa = loco_state["9950011"]["auto"]
        ta = loco_state["9950012"]["auto"]
        oo = loco_state["9950011"]["online"]
        to = loco_state["9950012"]["online"]
        ob = loco_state["9950011"]["block"]
        tb = loco_state["9950012"]["block"]
        oc = loco_state["9950011"]["ce"]
        tc = loco_state["9950012"]["ce"]
    return render_template_string(
        CONSOLE_HTML,
        otto_auto=oa, toby_auto=ta, otto_online=oo, toby_online=to,
        otto_block=ob, toby_block=tb, otto_ce=oc, toby_ce=tc,
        nav_html=nav("console"), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )

@app.route("/dispatcher/state")
def dispatcher_state():
    with mqtt_lock:
        return jsonify({
            "otto_auto":    loco_state["9950011"]["auto"],
            "toby_auto":    loco_state["9950012"]["auto"],
            "otto_online":  loco_state["9950011"]["online"],
            "toby_online":  loco_state["9950012"]["online"],
            "otto_block":   loco_state["9950011"]["block"],
            "toby_block":   loco_state["9950012"]["block"],
            "otto_ce":      loco_state["9950011"]["ce"],
            "toby_ce":      loco_state["9950012"]["ce"],
        })

@app.route("/dispatcher/cmd/<path:subcmd>", methods=["POST"])
def dispatcher_cmd(subcmd):
    allowed = {"go","stop","go/9950011","go/9950012","stop/9950011","stop/9950012","ce","estop"}
    if subcmd in allowed:
        pub_dispatcher(subcmd)
    return "", 204

@app.route("/dispatcher/endcto", methods=["POST"])
def dispatcher_endcto():
    pub_loco("9950011", "dispatcher_release", "1")
    pub_loco("9950012", "dispatcher_release", "1")
    pub_dispatcher("stop")
    return "", 204


# ============================================================================
# Shared command path
# ============================================================================
def _cmd(lid, subtopic, value):
    """E-STOP is NEVER gated: it publishes in every state — UNSET, LOST,
    stale, and AUTO (v1.9.5 returned 423 for estop in AUTO; that was wrong).
    Everything else respects AUTO."""
    if subtopic == "estop":
        pub_loco(lid, "estop", value)
        return "", 204
    # NO LOCK ON THE MANUAL PATH (v1.10.3). This used to be
    # `with mqtt_lock: if loco_state[lid]["auto"] == "1"` — and
    # on_mqtt_message() holds that same lock for its ENTIRE body, every JSON
    # parse of every inbound message, so a manual command could block behind
    # telemetry parsing. E-STOP returns above without ever touching it, which
    # is precisely the asymmetry the operator measured between E-STOP and
    # throttle. A bare dict read is atomic under the GIL, and the value could
    # change immediately after release anyway, so the lock bought nothing.
    if loco_state[lid]["auto"] == "1":
        return "", 423
    if subtopic in {"throttle", "brake", "direction"}:
        pub_loco(lid, subtopic, value)
    return "", 204


# ============================================================================
# Otto routes
# ============================================================================
@app.route("/otto")
def otto():
    return render_loco("9950011", "Otto", "otto", "otto")

@app.route("/otto/state")
def otto_state():
    return jsonify(state_payload("9950011"))

@app.route("/otto/cmd/<subtopic>/<value>", methods=["POST"])
def otto_cmd(subtopic, value):
    return _cmd("9950011", subtopic, value)

@app.route("/otto/dir/<int:d>")
def otto_dir(d):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return "", 423
    if d in (0,1,2):
        pub_loco("9950011", "direction", d)
    return "", 204

@app.route("/otto/sessiondir/<d>")
def otto_sessiondir(d):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return "", 423
    if d in ("CW","CCW"):
        pub_loco("9950011", "session_direction", d)
    return "", 204

@app.route("/otto/startinterval/<interval>", methods=["GET","POST"])
def otto_startinterval(interval):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return ("", 423) if request.method == "POST" else redirect("/otto")
    if re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco("9950011", "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/otto")

@app.route("/otto/mode/<int:m>")
def otto_mode(m):
    if m == 1:
        pub_loco("9950011", "auto", "1")
    return redirect("/otto")

@app.route("/otto/estop")
def otto_estop():
    with mqtt_lock:
        cur = loco_state["9950011"]["estop"]
    pub_loco("9950011", "estop", "0" if cur=="1" else "1")
    time.sleep(0.15)
    return redirect("/otto")

@app.route("/otto/cal/<action>", methods=["GET","POST"])
def otto_cal(action):
    if action == "start":
        ok, msg = cal_start("9950011")
        st = cal_status("9950011")
        st["file"] = msg if ok else st.get("file","")
        return jsonify(st)
    elif action == "stop":
        cal_stop("9950011")
        return jsonify(cal_status("9950011"))
    else:  # status
        return jsonify(cal_status("9950011"))


# ============================================================================
# Toby routes
# ============================================================================
@app.route("/toby")
def toby():
    return render_loco("9950012", "Toby", "toby", "toby")

@app.route("/toby/state")
def toby_state():
    return jsonify(state_payload("9950012"))

@app.route("/toby/cmd/<subtopic>/<value>", methods=["POST"])
def toby_cmd(subtopic, value):
    return _cmd("9950012", subtopic, value)

@app.route("/toby/dir/<int:d>")
def toby_dir(d):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return "", 423
    if d in (0,1,2):
        pub_loco("9950012", "direction", d)
    return "", 204

@app.route("/toby/sessiondir/<d>")
def toby_sessiondir(d):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return "", 423
    if d in ("CW","CCW"):
        pub_loco("9950012", "session_direction", d)
    return "", 204

@app.route("/toby/startinterval/<interval>", methods=["GET","POST"])
def toby_startinterval(interval):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return ("", 423) if request.method == "POST" else redirect("/toby")
    if re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco("9950012", "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/toby")

@app.route("/toby/mode/<int:m>")
def toby_mode(m):
    if m == 1:
        pub_loco("9950012", "auto", "1")
    return redirect("/toby")

@app.route("/toby/estop")
def toby_estop():
    with mqtt_lock:
        cur = loco_state["9950012"]["estop"]
    pub_loco("9950012", "estop", "0" if cur=="1" else "1")
    time.sleep(0.15)
    return redirect("/toby")

@app.route("/toby/cal/<action>", methods=["GET","POST"])
def toby_cal(action):
    if action == "start":
        ok, msg = cal_start("9950012")
        st = cal_status("9950012")
        st["file"] = msg if ok else st.get("file","")
        return jsonify(st)
    elif action == "stop":
        cal_stop("9950012")
        return jsonify(cal_status("9950012"))
    else:
        return jsonify(cal_status("9950012"))


# ============================================================================
# Log feeds
# ============================================================================
@app.route("/otto/log")
def otto_log():
    with mqtt_lock:
        return jsonify(list(loco_log["9950011"]))

@app.route("/toby/log")
def toby_log():
    with mqtt_lock:
        return jsonify(list(loco_log["9950012"]))

@app.route("/dispatcher/log")
def dispatcher_log():
    with mqtt_lock:
        return jsonify(list(dispatch_log))


# ============================================================================
# Placeholders
# ============================================================================
@app.route("/oscar")
def oscar():
    return "<h2 style='font-family:Arial;padding:2rem;color:#ccc;background:#222;min-height:100vh;margin:0;'>Oscar — coming soon</h2>"

# ============================================================================
# Hans routes
# ============================================================================
@app.route("/hans")
def hans():
    return render_loco("2095111", "Hans", "hans", "hans")

@app.route("/hans/state")
def hans_state():
    return jsonify(state_payload("2095111"))

@app.route("/hans/cmd/<subtopic>/<value>", methods=["POST"])
def hans_cmd(subtopic, value):
    return _cmd("2095111", subtopic, value)

@app.route("/hans/dir/<int:d>")
def hans_dir(d):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return "", 423
    if d in (0,1,2):
        pub_loco("2095111", "direction", d)
    return "", 204

@app.route("/hans/sessiondir/<d>")
def hans_sessiondir(d):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return "", 423
    if d in ("CW","CCW"):
        pub_loco("2095111", "session_direction", d)
    return "", 204

@app.route("/hans/startinterval/<interval>", methods=["GET","POST"])
def hans_startinterval(interval):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return ("", 423) if request.method == "POST" else redirect("/hans")
    if re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco("2095111", "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/hans")

@app.route("/hans/mode/<int:m>")
def hans_mode(m):
    if m == 1:
        pub_loco("2095111", "auto", "1")
    return redirect("/hans")

@app.route("/hans/estop")
def hans_estop():
    with mqtt_lock:
        cur = loco_state["2095111"]["estop"]
    pub_loco("2095111", "estop", "0" if cur=="1" else "1")
    time.sleep(0.15)
    return redirect("/hans")

@app.route("/hans/cal/<action>", methods=["GET","POST"])
def hans_cal(action):
    if action == "start":
        ok, msg = cal_start("2095111")
        st = cal_status("2095111")
        st["file"] = msg if ok else st.get("file","")
        return jsonify(st)
    elif action == "stop":
        cal_stop("2095111")
        return jsonify(cal_status("2095111"))
    else:
        return jsonify(cal_status("2095111"))

@app.route("/hans/log")
def hans_log():
    with mqtt_lock:
        return jsonify(list(loco_log["2095111"]))


# ============================================================================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, threaded=True)
