"""Offline MQTT/Flask and rendered-JavaScript tests; never contact a broker."""
import importlib.util
import ast
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SERVER = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dashboard", SERVER / "ngr_app_v1_11_2.py")
app = importlib.util.module_from_spec(spec)
with patch("threading.Thread.start"):
    spec.loader.exec_module(app)


class SpeedTest(unittest.TestCase):
    def test_telemetry_buffer_bound(self):
        sketch = SERVER.parent / 'firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/NAVI_COHERENCE_0_6_IR_HEALTH.ino'
        source = sketch.read_text().split('static void serviceIr(){', 1)[1]
        quoted = source.split('const int n=snprintf(b,sizeof(b),', 1)[1].split('movement.paired()', 1)[0]
        fmt = ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', quoted))
        # Conservative upper bound: even byte/bool %u fields receive 10 digits.
        sizes = {'%u':10, '%lu':10, '%llu':20, '%02X':2, '%016llX':16, '%s':399}
        worst = re.sub(r'%016llX|%02X|%llu|%lu|%u|%s', lambda m: '9' * sizes[m.group()], fmt)
        self.assertLess(len(worst), 960)

    def test_canonical_constants(self):
        self.assertAlmostEqual(app.PKPH_PER_MM_S, 1 / 5.37325)
        self.assertEqual(f"{130 * app.PKPH_PER_MM_S:.1f}", "24.2")
        self.assertEqual(f"{212 * app.PKPH_PER_MM_S:.1f}", "39.5")
        # The requested approximate 39.4 example differs by < 0.1 pKPH.
        self.assertLess(abs(212 * app.PKPH_PER_MM_S - 39.4), 0.1)

    def test_render_and_telemetry(self):
        def feed(sub, value):
            msg = type("Msg", (), {"topic": "ngr/loco/9950012/" + sub,
                "payload": json.dumps(value).encode(), "retain": False})()
            app.on_mqtt_message(None, None, msg)
        feed("alert", {"est_mm_s": 130, "moving": 1, "uptime_ms": 10000})
        ir = {"ir_valid": 1, "ir_mmps": 212, "ir_pkph": 999,
              "ir_speed_reason": "MEASURED", "ir_coupled": 1}
        feed("telem/ir", ir)
        client = app.app.test_client()
        state = client.get("/loco/toby/state").get_json()
        self.assertEqual(state["pkph"], "24.2")
        self.assertEqual(json.loads(state["ir_link"]), ir)
        self.assertIsNotNone(state["ages"]["ir_link"])
        published = []
        with patch.object(app, "pub_loco", side_effect=lambda *args: published.append(args)):
            app.loco_state["9950012"]["auto"] = "1"
            self.assertEqual(client.post('/loco/toby/cmd/ir_coupled/1').status_code, 204)
            self.assertEqual(client.post('/loco/toby/cmd/ir_coupled/0').status_code, 204)
            self.assertEqual(client.post('/loco/toby/cmd/ir_coupled/yes').status_code, 400)
            self.assertEqual(published, [("9950012", "ir_coupled", "1"),
                                         ("9950012", "ir_coupled", "0")])
            self.assertEqual(client.post('/loco/toby/cmd/throttle/30').status_code, 423)
            app.loco_state["9950012"]["auto"] = "0"
        html = client.get("/loco/toby").get_data(as_text=True)
        self.assertIn("IR pKPH", html)
        self.assertIn('id="ir-coupled"', html)
        if os.environ.get("IR_SPEED_RENDER_DIR"):
            out = Path(os.environ["IR_SPEED_RENDER_DIR"])
            out.mkdir(parents=True, exist_ok=True)
            (out / "toby.html").write_text(html)
            (out / "state.json").write_text(json.dumps(state))
        # Parse every rendered script, including pre-existing dashboard code.
        with tempfile.TemporaryDirectory() as tmp:
            for i, script in enumerate(re.findall(r"<script[^>]*>(.*?)</script>", html, re.S)):
                path = Path(tmp) / f"script{i}.js"
                path.write_text(script)
                subprocess.run(["node", "--check", str(path)], check=True)
        fn = re.search(r"function irSpeedView\(s\) \{.*?\n\}", html, re.S).group()
        js = "const assert=require('assert'); const STALE_S=5; const PKPH_PER_MM_S=1/5.37325;\n"
        js += "function ageOf(s,k){return s.ages[k] ?? null;}\n" + fn
        js += """
const state=(v,age=0)=>({ir_link:JSON.stringify(v),ages:{ir_link:age,speed_view:0},speed_view:'130'});
const good={ir_valid:1,ir_mmps:212,ir_pkph:999,ir_coupled:1};
assert.equal(irSpeedView(state(good)).value,'39.5'); // raw source, not redundant pKPH
assert.equal(irSpeedView(state({...good,ir_mmps:130})).value,'24.2');
assert.equal(irSpeedView(state({...good,ir_mmps:0})).value,'0.0');
assert.equal(irSpeedView(state({...good,ir_valid:0})).value,'--');
assert.equal(irSpeedView(state({...good,ir_valid:'false'})).value,'--');
assert.equal(irSpeedView(state({...good,ir_mmps:null})).value,'--');
assert.equal(irSpeedView(state({...good,ir_mmps:'oops'})).value,'--');
assert.equal(irSpeedView(state({...good,ir_mmps:-1})).value,'--');
assert.equal(irSpeedView(state(good,6)).value,'--');
assert.equal(irSpeedView(state(good,null)).value,'--');
assert.equal(irSpeedView(state({paired:1})).value,'--'); // bare Hall number is not IR
assert.equal(irSpeedView({...state({paired:1}),speed_view:JSON.stringify(good)}).value,'39.5');
assert.equal(irSpeedView({...state({...good,ir_valid:0}),speed_view:JSON.stringify(good)}).value,'--');
assert.equal(irSpeedView({...state(good),ir_link:'{bad',speed_view:'130'}).value,'--');
console.log('IR display: 14 validity/conversion assertions passed');
"""
        subprocess.run(["node", "-e", js], check=True)


if __name__ == "__main__":
    unittest.main()
