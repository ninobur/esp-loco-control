"""v1.12.0 NAVI_EWO dashboard tests. Offline: synthetic and firmware-produced
MQTT payloads only; never contacts a broker or a locomotive.

Firmware-produced payloads come from compiling the integrated sketch's own
output-only console adapter (tests/test_review_regressions.cpp json mode), so
the state/nav and telem/ir shapes are the ones the candidate really emits.
"""
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch

SERVER = Path(__file__).resolve().parents[1]
ROOT = SERVER.parent
FW = ROOT / "firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED"
spec = importlib.util.spec_from_file_location("dashboard_v1_12_0", SERVER / "ngr_app_v1_12_0.py")
app = importlib.util.module_from_spec(spec)
with patch("threading.Thread.start"):
    spec.loader.exec_module(app)

SKETCH = "NAVI_EYES_WIDE_OPEN_INTEGRATED_R2"
_ids = iter(range(9900100, 9900999))


def firmware_console_lines():
    """unset nav, declared nav (MM012), moving IR, stopped IR at PWM 0 with
    INADEQUATE_CONTRAST, stale IR — straight from the C++ adapter."""
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "regressions"
        subprocess.run(["c++", "-std=c++17", str(FW / "tests/test_review_regressions.cpp"),
                        "-o", str(exe)], check=True)
        out = subprocess.check_output([str(exe), "json"], text=True)
    return [json.loads(line) for line in out.splitlines()]


FW_UNSET, FW_NAV, FW_IR_MOVING, FW_IR_STOPPED, FW_IR_STALE = firmware_console_lines()


def loopstat(**over):
    """Every key the firmware's serviceStatus() publishes (checked against the
    sketch in test_contract_keys_match_firmware)."""
    d = {"build": SKETCH, "mm": 12, "target": 13, "dir": 1, "target_distance_mm": 300,
         "target_polarity": 1, "position_reliable": 1, "reference_ready": 1,
         "reference": 1880, "median5": 1884, "hall_support": 0, "spatial_phase": 0,
         "ir_applicable": 1, "degraded": 0, "ir_health_fault": 0, "ir_readiness": 0,
         "ir_boot": "000000000000002A", "ir_epoch": 3, "ir_epoch_active": 1,
         "ir_seq": 90, "ir_pulses": 400, "ir_reason": 6, "ir_age_ms": 40, "ir_span": 800,
         "ir_gap": 0, "ir_sat": 0, "ir_abort": 0, "ir_inferred_added": 0,
         "ir_inferred_removed": 0, "ir_calibration": 1, "hall_seen": 100000,
         "ir_seen": 900, "hall_q_drop": 0, "ir_q_drop": 0, "ir_invalid": 0,
         "nsr_hall_drop": 0, "nsr_ir_drop": 0, "nsr_navi_drop": 0, "nsr_native_drop": 0,
         "event_drop": 0, "pwm0_ir_motion": 0, "confirmed": 4, "missed": 0, "pwm": 90,
         "auto": 0, "running": 0, "estop": 0, "lowvolt": 0, "pub_drop": 0}
    d.update(over)
    return d


def nav(**over):
    d = dict(FW_NAV)
    d.update(over)
    return d


def event(kind, **over):
    d = {"event": kind, "hall_serial": 500, "mm": 12, "target": 13, "dir": 1,
         "ir_um": 3000000, "median5": 1960, "reference": 1880, "degraded": 0,
         "opening_serial": 498, "opening_ir_um": 2990000, "position_reliable": 1,
         "spatial_phase": 0, "decision_us": 123456789, "consumption_id": 77, "ir_seq": 91}
    d.update(over)
    return d


def feed(lid, sub, value, retain=False):
    payload = value if isinstance(value, str) else json.dumps(value)
    msg = type("Msg", (), {"topic": "ngr/loco/%s/%s" % (lid, sub),
                           "payload": payload.encode(), "retain": retain})()
    app.on_mqtt_message(None, None, msg)


def connect(boot_id="1111111111111111", auto="0", **stat):
    """A fresh EWO locomotive with a declared, reliable position."""
    lid = str(next(_ids))
    feed(lid, "state/bootid", {"sketch": SKETCH, "class": "X", "boot_id": boot_id})
    feed(lid, "online", "1")
    feed(lid, "state/auto", auto)
    feed(lid, "state/estop", "0")
    feed(lid, "state/session_direction", "CW")
    feed(lid, "state/direction", "2")
    feed(lid, "state/nav_ready", "1")
    feed(lid, "state/loopstat", loopstat(**stat))
    feed(lid, "state/nav", nav())
    feed(lid, "telem/ir", FW_IR_MOVING)
    return lid


def view(lid):
    return app.state_payload(lid)["ewo_view"]


def age(lid, field, seconds):
    app.loco_rx[lid][field] = time.monotonic() - seconds


class EwoDashboard(unittest.TestCase):
    # ---- position / target ------------------------------------------------
    def test_fresh_connection_valid_position(self):
        lid = connect()
        s = app.state_payload(lid)
        v = s["ewo_view"]
        self.assertEqual(s["nav"], "NORMAL")
        self.assertEqual(s["mm"], "012")
        self.assertEqual(v["mm"], "012")
        self.assertEqual(v["mode"], "NORMAL")
        self.assertEqual(v["mode_text"], "HALL + IR NORMAL")
        self.assertEqual(v["status"], {"text": "MM012 → SEEKING MM013", "cls": "ok"})
        self.assertEqual(v["alerts"], [])

    def test_current_target(self):
        lid = connect(target_distance_mm=315, target_polarity=-1)
        t = view(lid)["target"]
        self.assertEqual(t, {"mm": "013", "polarity": "S", "distance_mm": 315, "dir": "CW"})
        self.assertEqual(view(lid)["result"]["text"], "SEEKING MM013")

    def test_not_declared(self):
        lid = connect()
        feed(lid, "state/nav", FW_UNSET)
        feed(lid, "state/loopstat", loopstat(position_reliable=0))
        v = view(lid)
        self.assertEqual(v["mode"], "NOT_DECLARED")
        self.assertIsNone(v["target"])
        self.assertIsNone(v["mm"])
        self.assertEqual(app.state_payload(lid)["mm"], "--")

    # ---- control and motion -----------------------------------------------
    def test_manual_versus_auto(self):
        lid = connect()
        self.assertEqual(view(lid)["control"], "MANUAL")
        feed(lid, "state/auto", "1")
        feed(lid, "state/loopstat", loopstat(auto=1, running=0))
        self.assertEqual(view(lid)["control"], "AUTO ENROLLED — NOT RUNNING")
        feed(lid, "state/loopstat", loopstat(auto=1, running=1))
        self.assertEqual(view(lid)["control"], "AUTO ACTIVE")
        self.assertEqual(app._mode_of(app.loco_state[lid]), "RUN")
        # The server-side manual lock follows AUTO running, as it did via alert.
        with patch.object(app, "pub_loco") as pub:
            self.assertEqual(app._cmd(lid, "throttle", "40")[1], 423)
            self.assertEqual(app._cmd(lid, "estop", "1")[1], 204)
            pub.assert_called_once_with(lid, "estop", "1")

    def test_auto_enrolled_stationary_at_station(self):
        lid = connect(auto="1", running=1)
        feed(lid, "telem/ir", FW_IR_STOPPED)
        feed(lid, "state/station", {"event": "DWELL_BEGIN", "station": "Patio",
                                    "phase": "DWELL", "off": 0, "pwm": 0})
        v = view(lid)
        self.assertEqual(v["control"], "AUTO ACTIVE")
        self.assertEqual(v["motion"]["state"], "STOPPED")
        self.assertEqual(v["station"]["text"], "PATIO — STOPPED — DWELL")

    def test_moving_versus_stopped_and_ir_speed(self):
        lid = connect()
        s = app.state_payload(lid)
        self.assertEqual(s["ewo_view"]["motion"]["state"], "MOVING")
        expected = "%.1f" % (FW_IR_MOVING["ir_mmps"] * app.PKPH_PER_MM_S)
        self.assertEqual(s["pkph"], expected)
        self.assertIn(expected, s["ewo_view"]["motion"]["text"])
        self.assertEqual(json.loads(s["ir_link"])["ir_valid"], 1)
        feed(lid, "telem/ir", FW_IR_STOPPED)
        self.assertEqual(view(lid)["motion"]["state"], "STOPPED")
        self.assertEqual(app.state_payload(lid)["pkph"], "0.0")

    def test_no_ir_speed_is_not_a_motion_claim(self):
        lid = str(next(_ids))
        feed(lid, "state/bootid", {"sketch": SKETCH, "boot_id": "22"})
        feed(lid, "state/loopstat", loopstat(pwm=0))
        feed(lid, "state/nav", nav())
        feed(lid, "telem/ir", FW_IR_STALE)
        v = view(lid)
        self.assertEqual(v["motion"]["state"], "UNKNOWN")
        self.assertEqual(v["motion"]["text"], "MOTION NOT MEASURED — NO IR SPEED (PWM 0)")
        self.assertEqual(v["alerts"], [])

    # ---- Hall + IR modes ----------------------------------------------------
    def test_degraded_ir(self):
        lid = connect()
        feed(lid, "state/nav", nav(degraded=1))
        feed(lid, "state/loopstat", loopstat(degraded=1, ir_applicable=0))
        v = view(lid)
        self.assertEqual(v["mode"], "DEGRADED")
        self.assertEqual(v["mm"], "012")    # confident locomotive: still knows where it is
        self.assertEqual(v["status"]["cls"], "warn")
        self.assertIn("IR DEGRADED — HALL NAVIGATION", v["status"]["text"])
        self.assertEqual(v["alerts"], [{"level": "warn", "text": "IR DEGRADED — HALL NAVIGATION"}])
        self.assertNotIn("LOST", json.dumps(v))
        feed(lid, "state/nav", nav(degraded=0))
        feed(lid, "state/loopstat", loopstat())
        self.assertEqual(view(lid)["mode"], "NORMAL")

    def test_stationary_inadequate_contrast_is_not_an_alarm(self):
        lid = connect(ir_reason=1, pwm=0)          # raw INADEQUATE_CONTRAST
        self.assertEqual(FW_IR_STOPPED["ir_reason"], 1)
        feed(lid, "telem/ir", FW_IR_STOPPED)
        v = view(lid)
        self.assertEqual(v["mode"], "NORMAL")
        self.assertEqual(v["alerts"], [])
        self.assertEqual(v["status"]["cls"], "ok")
        operator = json.dumps({k: v[k] for k in v if k != "diag"})
        self.assertNotIn("CONTRAST", operator)
        raw = dict(v["diag"]["sections"][2][1])["Raw optical reason"]
        self.assertEqual(raw, "INADEQUATE_CONTRAST")

    # ---- conclusions ----------------------------------------------------------
    def test_missed_magnet(self):
        lid = connect()
        e = event("MISSED_MAGNET", mm=30, target=30)
        feed(lid, "nav/evidence", e)
        feed(lid, "mm/marker", e)                    # same conclusion, second topic
        feed(lid, "state/nav", nav(mm=30, target=31))
        feed(lid, "state/loopstat", loopstat(mm=30, target=31, missed=1))
        v = view(lid)
        self.assertEqual(v["result"]["text"], "MISSED MM030 — SEEKING MM031")
        self.assertIn({"level": "warn", "text": "MISSED MM030 — SEEKING MM031"}, v["alerts"])
        self.assertEqual(v["mode"], "NORMAL")         # a Missed Magnet is not lost
        self.assertEqual(len(app.state_payload(lid)["ewo_events"]), 1)
        age(lid, "ewo_result", app.EWO_RESULT_SHOW_S + 1)
        self.assertEqual(view(lid)["result"]["text"], "SEEKING MM031")

    def test_target_confirmed_normal_and_degraded(self):
        lid = connect()
        feed(lid, "nav/evidence", event("TARGET_CONFIRMED", mm=13, target=13))
        self.assertEqual(view(lid)["result"]["text"], "TARGET CONFIRMED MM013 — HALL + IR")
        feed(lid, "nav/evidence", event("TARGET_CONFIRMED", mm=14, target=14, degraded=1,
                                        decision_us=2))
        self.assertEqual(view(lid)["result"]["text"],
                         "TARGET CONFIRMED MM014 — DEGRADED HALL CONFIRMATION")

    def test_shrug_is_not_displayed(self):
        lid = connect()
        feed(lid, "nav/evidence", event("HALL_SUPPORT"))
        v = view(lid)
        self.assertEqual(v["alerts"], [])
        self.assertEqual(v["result"]["kind"], "SEEKING")

    def test_position_unreliable_then_reanchor(self):
        lid = connect()
        feed(lid, "nav/evidence", event("PWM_ZERO_IR_DISPLACEMENT", position_reliable=0))
        warn = "IR measured motion at PWM=0; map/IR relationship unreliable"
        feed(lid, "state/warning", warn)
        feed(lid, "state/nav", nav(state="UNSET", position_reliable=0, degraded=1))
        feed(lid, "state/loopstat", loopstat(position_reliable=0, degraded=1, pwm0_ir_motion=1))
        s = app.state_payload(lid)
        v = s["ewo_view"]
        self.assertEqual(v["mode"], "UNRELIABLE")
        self.assertIsNone(v["mm"])                   # no guessed position
        self.assertEqual(s["mm"], "--")
        self.assertEqual(v["last_mm"], "012")
        self.assertEqual(v["status"]["text"],
                         "POSITION REFERENCE UNRELIABLE — AWAITING MM RE-ANCHOR")
        self.assertEqual(v["target"]["mm"], "013")   # still seeking the next MM
        self.assertEqual(v["warning"], warn)
        # Re-anchor: NAVI confirms the next MM (degraded) and says reliable.
        feed(lid, "nav/evidence", event("TARGET_CONFIRMED", mm=13, target=13, degraded=1,
                                        decision_us=9))
        feed(lid, "nav/evidence", event("POSITION_REANCHORED", mm=13, target=13, decision_us=9))
        feed(lid, "state/nav", nav(mm=13, target=14))
        feed(lid, "state/loopstat", loopstat(mm=13, target=14, pwm0_ir_motion=1))
        v = view(lid)
        self.assertEqual(v["mode"], "NORMAL")
        self.assertEqual(v["mm"], "013")
        self.assertEqual(v["alerts"], [])
        self.assertEqual(v["warning"], "")           # firmware's sticky text superseded
        self.assertEqual(v["warning_superseded"], warn)

    def test_boot_reference_states(self):
        lid = connect()
        feed(lid, "state/nav", nav(reference_ready=0, boot_positions=3))
        v = view(lid)
        self.assertEqual(v["mode"], "REFERENCE_PENDING")
        self.assertIn("3/5", v["mode_text"])
        self.assertEqual(app._ewo_pill(v, True), ["REF 3/5", "q-eval"])
        feed(lid, "state/nav", nav(reference_ready=0, boot_positions=2, boot_incomplete=1))
        v = view(lid)
        self.assertEqual(v["mode"], "REFERENCE_INCOMPLETE")
        self.assertEqual(v["status"]["cls"], "bad")
        self.assertTrue(any("CANNOT CONFIRM" in a["text"] for a in v["alerts"]))

    # ---- stations -------------------------------------------------------------
    def test_station_sequence(self):
        lid = connect(auto="1", running=1)
        seq = [("ARMED", "APPROACH", "PATIO — APPROACHING"),
               ("APPROACH", "APPROACH", "PATIO — APPROACHING"),
               ("ZONE", "ZONE", "PATIO — IN STATION ZONE"),
               ("ZERO_RAMP", "ZERO_RAMP", "PATIO — STOPPING"),
               ("DWELL_BEGIN", "DWELL", "PATIO — STOPPED — DWELL"),
               ("DEPART", "DEPART", "PATIO — DEPARTING"),
               ("DEPARTED", "IDLE", "DEPARTED PATIO")]
        for ev, phase, text in seq:
            feed(lid, "state/station", {"event": ev, "station": "Patio", "phase": phase,
                                        "off": 0, "pwm": 0})
            self.assertEqual(view(lid)["station"]["text"], text, ev)
        console = app.app.test_client().get("/dispatcher/state").get_json()
        col = next(l for l in console["locos"] if l["id"] == lid)
        self.assertEqual(col["ewo_station"], "DEPARTED PATIO")

    def test_start_inside_station(self):
        # Declared inside the zone: the station controller's first order is
        # ZONE (or an immediate ramp). The dashboard displays it; it does not
        # work out which station the locomotive is in.
        lid = connect(auto="1", running=1)
        feed(lid, "telem/ir", FW_IR_STOPPED)
        feed(lid, "state/station", {"event": "ZONE", "station": "Patio", "phase": "ZONE",
                                    "off": -1, "pwm": 45})
        self.assertEqual(view(lid)["station"]["text"], "PATIO — IN STATION ZONE")
        feed(lid, "state/station", {"event": "ZERO_RAMP", "station": "Patio",
                                    "phase": "ZERO_RAMP", "off": 0, "pwm": 0})
        self.assertEqual(view(lid)["station"]["text"], "PATIO — STOPPING")

    def test_station_failure_and_withdrawal(self):
        lid = connect(auto="1", running=1)
        feed(lid, "state/station", {"event": "MISSED", "station": "Patio", "phase": "IDLE",
                                    "off": 4, "pwm": 90})
        feed(lid, "state/loopstat", loopstat(auto=0, running=0))
        feed(lid, "state/auto", "0")
        w = "Station approach failed: controlled stop; Manual available."
        feed(lid, "state/warning", w)
        v = view(lid)
        self.assertIn({"level": "bad", "text": "STATION APPROACH FAILED — PATIO MISSED"},
                      v["alerts"])
        self.assertEqual(v["warning"], w)
        self.assertEqual(v["control"], "MANUAL")

    # ---- safety ------------------------------------------------------------------
    def test_estop(self):
        lid = connect()
        feed(lid, "state/estop", "1")
        v = view(lid)
        self.assertEqual(v["status"], {"text": "E-STOP ACTIVE", "cls": "bad"})
        self.assertIn({"level": "bad", "text": "E-STOP ACTIVE"}, v["alerts"])
        console = app.app.test_client().get("/dispatcher/state").get_json()
        self.assertEqual(next(l for l in console["locos"] if l["id"] == lid)["estop"], "1")

    def test_low_voltage_withdrawal(self):
        lid = connect(auto="1", running=1)
        feed(lid, "state/lowvolt", "1")
        feed(lid, "state/warning", "LOW VOLTAGE: controlled stop")
        v = view(lid)
        self.assertEqual(v["status"]["text"], "LOW VOLTAGE — CONTROLLED STOP")
        self.assertEqual(v["warning"], "LOW VOLTAGE: controlled stop")

    # ---- session / comms ------------------------------------------------------
    def test_reboot_and_reconnect(self):
        lid = connect(boot_id="AAAA")
        feed(lid, "nav/evidence", event("TARGET_CONFIRMED"))
        epoch = app.state_payload(lid)["epoch"]
        # Reconnect of the same boot (bootid republished live, same boot_id).
        feed(lid, "state/bootid", {"sketch": SKETCH, "boot_id": "AAAA"})
        self.assertEqual(app.state_payload(lid)["epoch"], epoch)
        self.assertEqual(len(app.state_payload(lid)["ewo_events"]), 1)
        # Reboot: a new boot_id resets the session.
        feed(lid, "state/bootid", {"sketch": SKETCH, "boot_id": "BBBB"})
        s = app.state_payload(lid)
        self.assertEqual(s["epoch"], epoch + 1)
        self.assertEqual(s["ewo_events"], [])
        self.assertTrue(s["ewo"])
        # A retained replay followed by the same live boot is not a reboot.
        feed(lid, "state/bootid", {"sketch": SKETCH, "boot_id": "BBBB"}, retain=True)
        feed(lid, "state/bootid", {"sketch": SKETCH, "boot_id": "BBBB"})
        self.assertEqual(app.state_payload(lid)["epoch"], epoch + 1)
        # Legacy bootid without boot_id never resets (v1.10.11 finding A).
        legacy = str(next(_ids))
        feed(legacy, "state/bootid", {"sketch": "QUORUM_1_14"})
        feed(legacy, "state/bootid", {"sketch": "QUORUM_1_14"})
        self.assertEqual(app.state_payload(legacy)["epoch"], 0)

    def test_stale_and_lost_communications(self):
        lid = connect()
        age(lid, "ewo_nav", 8)
        v = view(lid)
        self.assertIn({"level": "warn", "text": "NAVI STATE NOT RECEIVED FOR 8 s"}, v["alerts"])
        for f in list(app.loco_rx[lid]):
            age(lid, f, 12)
        v = view(lid)
        self.assertEqual(v["status"]["text"], "TELEMETRY STALE — LAST HEARD 12 s AGO")
        self.assertEqual(v["motion"]["state"], "UNKNOWN")
        console = app.app.test_client().get("/dispatcher/state").get_json()
        col = next(l for l in console["locos"] if l["id"] == lid)
        self.assertFalse(col["heard"])
        self.assertEqual(col["ewo_pill"], ["UNKNOWN", "q-unset"])

    # ---- authority boundary ------------------------------------------------------
    def test_no_legacy_navigation_semantics_for_ewo(self):
        lid = connect()
        feed(lid, "nav/evidence", event("TARGET_CONFIRMED"))
        s = app.state_payload(lid)
        text = json.dumps(s["ewo_view"]).lower()
        for word in ("hallready", "x22", "quorum", "settle", "rearm", "lobe", "morpholog",
                     "recovery", "proximal", "candidate", "alternative", "lost"):
            self.assertNotIn(word, text)
        console = app.app.test_client().get("/dispatcher/state").get_json()
        col = next(l for l in console["locos"] if l["id"] == lid)
        self.assertTrue(col["ewo"])
        self.assertEqual(col["ewo_pill"], ["HALL+IR", "q-ok"])
        self.assertEqual(col["ewo_target"], "013")

    def test_legacy_firmware_keeps_v1_11_behaviour(self):
        lid = str(next(_ids))
        feed(lid, "alert", {"nav": "NORMAL", "dead_reckoned_mm": 44, "moving": 1,
                            "est_mm_s": 130, "session_dir": "CW", "auto": "0",
                            "uptime_ms": 5000})
        s = app.state_payload(lid)
        self.assertIsNone(s["ewo_view"])
        self.assertEqual(s["mm"], "044")
        self.assertEqual(s["pkph"], "24.2")
        console = app.app.test_client().get("/dispatcher/state").get_json()
        col = next(l for l in console["locos"] if l["id"] == lid)
        self.assertFalse(col["ewo"])
        self.assertEqual(col["quorum"], "QUORUM")

    # ---- contract and page -------------------------------------------------------
    def test_contract_keys_match_firmware(self):
        ino = (FW / "NAVI_EYES_WIDE_OPEN_INTEGRATED.ino").read_text()
        compat = (FW / "NaviCompatibility.h").read_text()
        keys = lambda text: set(re.findall(r'\\"([a-z0-9_]+)\\":', text))
        status_fmt = ino.split("static void serviceStatus()", 1)[1].split('pub("state/loopstat"', 1)[0]
        self.assertEqual(keys(status_fmt), set(loopstat()))
        nav_fmt = compat.split("formatConsoleNav", 1)[1].split("formatConsoleIr", 1)[0]
        self.assertEqual(keys(nav_fmt), set(FW_NAV))
        event_fmt = ino.split("static void publishEvents()", 1)[1].split('pub("nav/evidence"', 1)[0]
        self.assertEqual(keys(event_fmt), set(event("X")))
        for topic in ("state/loopstat", "state/nav", "telem/ir", "nav/evidence", "mm/marker",
                      "state/station", "state/bootid", "state/trace", "diag/ir_link",
                      "state/auto", "state/estop", "state/lowvolt", "state/warning"):
            self.assertIn('"%s"' % topic, ino, topic)
        self.assertNotIn('pub("alert"', ino)       # why RUNNING/motion moved sources
        for name in ("INITIAL_REFERENCE", "SPATIAL_REFERENCE", "TARGET_CONFIRMED",
                     "MISSED_MAGNET", "PWM_ZERO_IR_DISPLACEMENT", "POSITION_REANCHORED"):
            self.assertIn('"%s"' % name, ino)

    def test_page_scripts_parse(self):
        html = app.app.test_client().get("/loco/%s" % connect()).get_data(as_text=True)
        self.assertIn('id="navi-panel"', html)
        self.assertIn('id="evidence-panel"', html)
        console = app.app.test_client().get("/console").get_data(as_text=True)
        node = shutil.which("node")
        if not node:
            self.skipTest("node not installed")
        with tempfile.TemporaryDirectory() as tmp:
            for page in (html, console):
                for i, script in enumerate(re.findall(r"<script[^>]*>(.*?)</script>", page, re.S)):
                    path = Path(tmp) / ("s%d.js" % i)
                    path.write_text(script)
                    subprocess.run([node, "--check", str(path)], check=True)


if __name__ == "__main__":
    unittest.main()
