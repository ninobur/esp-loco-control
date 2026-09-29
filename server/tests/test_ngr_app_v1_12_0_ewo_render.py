"""Rendered-page checks for the v1.12.0 EWO dashboard in headless Chromium.

Skipped unless the Python `playwright` package is installed. Uses the same
synthetic/firmware-produced MQTT payloads as test_ngr_app_v1_12_0_ewo. Set
EWO_DASHBOARD_SCREENSHOTS=<dir> to also save one PNG per scenario.
"""
import os
from pathlib import Path
import threading
import unittest

from server.tests import test_ngr_app_v1_12_0_ewo as t

try:
    from playwright.sync_api import sync_playwright
except ImportError:          # pragma: no cover - optional dependency
    sync_playwright = None

CHROMIUM = "/opt/pw-browsers/chromium"


@unittest.skipIf(sync_playwright is None, "playwright not installed")
class RenderedEwoDashboard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import logging
        from werkzeug.serving import make_server
        logging.getLogger("werkzeug").setLevel(logging.ERROR)
        cls.server = make_server("127.0.0.1", 0, t.app.app, threaded=True)
        cls.base = "http://127.0.0.1:%d" % cls.server.server_port
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.pw = sync_playwright().start()
        kw = {"executable_path": CHROMIUM} if os.path.exists(CHROMIUM) else {}
        cls.browser = cls.pw.chromium.launch(**kw)
        cls.shots = os.environ.get("EWO_DASHBOARD_SCREENSHOTS")
        if cls.shots:
            Path(cls.shots).mkdir(parents=True, exist_ok=True)

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.pw.stop()
        cls.server.shutdown()

    def open(self, path, name):
        page = self.browser.new_page(viewport={"width": 720, "height": 1600})
        self.addCleanup(page.close)
        errors = []
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.goto(self.base + path)
        page.wait_for_timeout(1300)          # one poll cycle
        if self.shots:
            page.screenshot(path=str(Path(self.shots) / (name + ".png")), full_page=True)
        self.assertEqual(errors, [])
        self.assertFalse(page.is_visible("#fault-banner"))
        return page

    def test_normal_moving(self):
        lid = t.connect()
        t.feed(lid, "nav/evidence", t.event("TARGET_CONFIRMED", mm=12, target=12))
        p = self.open("/loco/%s" % lid, "1_normal_moving")
        self.assertEqual(p.inner_text("#navi-mm"), "012")
        self.assertEqual(p.inner_text("#navi-target"), "013")
        self.assertEqual(p.inner_text("#navi-mode"), "HALL + IR NORMAL")
        self.assertIn("TARGET CONFIRMED MM012", p.inner_text("#navi-result"))
        self.assertIn("MOVING", p.inner_text("#motion-bar"))
        self.assertEqual(p.inner_text("#kph-label"), "TARGET")
        self.assertFalse(p.is_visible("#agree-panel"))
        self.assertEqual(p.inner_text("#mm-countdown"), "")
        p.click("#evidence-toggle")
        p.wait_for_timeout(1200)
        body = p.inner_text("#evidence-body")
        self.assertIn("Active Hall reference", body)
        self.assertIn("TARGET_CONFIRMED", body)
        if self.shots:
            p.screenshot(path=str(Path(self.shots) / "2_evidence_panel.png"), full_page=True)

    def test_degraded(self):
        lid = t.connect()
        t.feed(lid, "state/nav", t.nav(degraded=1))
        t.feed(lid, "state/loopstat", t.loopstat(degraded=1, ir_applicable=0))
        p = self.open("/loco/%s" % lid, "3_ir_degraded")
        self.assertEqual(p.inner_text("#navi-mode"), "IR DEGRADED — HALL NAVIGATION")
        self.assertEqual(p.inner_text("#navi-mm"), "012")

    def test_unreliable(self):
        lid = t.connect()
        t.feed(lid, "state/warning", "IR measured motion at PWM=0; map/IR relationship unreliable")
        t.feed(lid, "state/nav", t.nav(state="UNSET", position_reliable=0, degraded=1))
        t.feed(lid, "state/loopstat", t.loopstat(position_reliable=0, degraded=1,
                                                  pwm0_ir_motion=1, pwm=0))
        t.feed(lid, "telem/ir", t.FW_IR_STOPPED)
        p = self.open("/loco/%s" % lid, "4_position_unreliable")
        self.assertIn("AWAITING MM RE-ANCHOR", p.inner_text("#status-line"))
        self.assertIn("—", p.inner_text("#navi-mm"))
        self.assertIn("LAST ESTABLISHED MM012", p.inner_text("#navi-last"))

    def test_missed_and_station(self):
        lid = t.connect(auto="1", running=1)
        t.feed(lid, "nav/evidence", t.event("MISSED_MAGNET", mm=30, target=30))
        t.feed(lid, "state/nav", t.nav(mm=30, target=31))
        t.feed(lid, "state/loopstat", t.loopstat(mm=30, target=31, missed=1, running=1, auto=1))
        t.feed(lid, "state/station", {"event": "DWELL_BEGIN", "station": "Grillers",
                                      "phase": "DWELL", "off": 0, "pwm": 0})
        t.feed(lid, "telem/ir", t.FW_IR_STOPPED)
        p = self.open("/loco/%s" % lid, "5_missed_magnet_station_dwell")
        self.assertIn("MISSED MM030 — SEEKING MM031", p.inner_text("#navi-result"))
        self.assertIn("GRILLERS — STOPPED — DWELL", p.inner_text("#navi-station"))
        self.assertEqual(p.inner_text("#motion-bar"), "STOPPED")
        self.assertIn("AUTO ACTIVE", p.inner_text("#navi-control"))

    def test_console(self):
        t.connect()
        p = self.open("/console", "6_console")
        grid = p.inner_text("#cgrid")
        self.assertIn("HALL+IR", grid)
        self.assertIn("TGT", grid)
        self.assertIn("MANUAL", grid)
        self.assertIn("EWO.R2", grid)


if __name__ == "__main__":
    unittest.main()
