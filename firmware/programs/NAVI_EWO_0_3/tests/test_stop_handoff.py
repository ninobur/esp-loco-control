"""Hall-to-actuator regression using production NAVI, adapter and shell code."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parents[1]


class StopHandoff(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.tmp.name) / "stop_handoff"
        source = (HERE / "NAVI_EWO_0_3.ino").read_text()
        constants = [re.search(r'^static constexpr uint(?:8|16)_t ' + name +
                               r'\b.*?;', source, re.M).group()
                     for name in ('NAVI_BASE_CRUISE_PWM', 'AUTO_STEP_UP_MS',
                                  'STOP_RESTART_STEP_UP_MS', 'STOP_FINAL_STEP_DOWN_MS',
                                  'MANUAL_STEP_UP_MS', 'BRAKE_STEP_COAST_MS')]
        functions = [re.search(r'^static [^\n]+\b' + name +
                               r'\([^\{]*\{.*?^\}', source, re.M | re.S).group()
                     for name in ('brakeStepMs', 'requestPwm', 'serviceRamp',
                                  'withdraw', 'recordStop', 'serviceAutoCruise')]
        (Path(cls.tmp.name) / "stop_handoff_shell.h").write_text(
            '\n'.join(constants + functions))
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-I', cls.tmp.name,
                        str(HERE / 'tests/test_stop_handoff.cpp'),
                        '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, case):
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        result = subprocess.run([str(self.binary), case], env=env,
                                capture_output=True, text=True)
        print(result.stdout, end='', flush=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_no_ir_cw(self):
        self.run_case('no_ir_cw')

    def test_no_ir_ccw(self):
        self.run_case('no_ir_ccw')

    def test_stale_ir_cw(self):
        self.run_case('stale_ir_cw')

    def test_fine_ir_cw(self):
        self.run_case('fine_ir_cw')

    def test_manual(self):
        self.run_case('manual')

    def test_rejected_and_pending_evidence(self):
        self.run_case('rejected')

    def test_fine_to_accepted_mm(self):
        self.run_case('fine_to_mm')


if __name__ == '__main__':
    unittest.main()
