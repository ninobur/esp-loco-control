"""Focused host regression for lightweight cumulative Type-6 reception."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / "firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED"


class LightweightPulseTests(unittest.TestCase):
    def test_cumulative_receiver_contract(self):
        source = SKETCH / "tests/test_pulse_observation.cpp"
        with tempfile.TemporaryDirectory(prefix="navi-pulse-") as tmp:
            executable = Path(tmp) / "test"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-I", str(SKETCH),
                str(source), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)

    def test_no_transport_recovery_or_control_coupling(self):
        source = (SKETCH / "NAVI_EYES_WIDE_OPEN_INTEGRATED.ino").read_text()
        observation = (SKETCH / "NaviPulseObservation.h").read_text()
        self.assertIn("xQueueCreate(1,sizeof(navi_pulse::Rx))", source)
        self.assertIn("xQueueOverwrite(pulseQ,&pulse)", source)
        self.assertNotIn("pulseTransportQ", source)
        self.assertNotIn("pulseLogQ", source)
        self.assertNotIn("esp_now_send", source)
        self.assertIn("navi.observeIr(wire", source)  # Type-5 remains NAVI's operational input.
        self.assertNotIn("ReliablePulseTransport", observation)
        self.assertIn("OBSERVATION_ONLY", source)


if __name__ == "__main__":
    unittest.main()
