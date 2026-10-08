"""Focused contract checks for latest cumulative Type-6 observation ingress."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / 'firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED'


class PulseIntegration(unittest.TestCase):
    def compile_run(self, args=()):
        with tempfile.TemporaryDirectory(prefix='navi-pulse-') as tmp:
            exe = Path(tmp) / 'test'
            subprocess.run([
                'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-I', str(SKETCH),
                str(SKETCH / 'tests/test_pulse_observation.cpp'), '-o', str(exe),
            ], check=True)
            return subprocess.check_output([str(exe), *args], text=True)

    def test_cumulative_contract_and_telemetry_json(self):
        row = json.loads(self.compile_run(['json']))
        self.assertEqual(row['authority'], 'OBSERVATION_ONLY')
        self.assertEqual(row['valid'], 1)
        self.assertGreater(row['distance_advanced_um'], 0)
        self.assertGreater(row['physical_interval_us'], 0)
        self.assertEqual(row['boot_changes'], 1)

    def test_type6_cannot_enter_operational_navi_path(self):
        source = (SKETCH / 'NAVI_EYES_WIDE_OPEN_INTEGRATED.ino').read_text()
        pulse_start = source.index('static void servicePulseIngress()')
        pulse_end = source.index('\nstatic const char* eventName', pulse_start)
        pulse_service = source[pulse_start:pulse_end]
        self.assertIn('pulseObservation.receive(rx)', pulse_service)
        self.assertNotIn('navi.observeIr', pulse_service)
        self.assertNotIn('noteObservationLoss', pulse_service)
        self.assertIn('pulseQ=xQueueCreate(1', source)
        self.assertNotIn('pulseTransportQ', source)
        self.assertNotIn('pulseLogQ', source)
        self.assertNotIn('pulseStatusQ', source)


if __name__ == '__main__':
    unittest.main()
