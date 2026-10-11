"""Compare operational code with immutable 0.1 commit, not a second model."""
from pathlib import Path
import re
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[4]
HERE=Path(__file__).resolve().parents[1]
BASE='firmware/programs/NAVI_EWO_0_1_MM045_STOP/'
def baseline(name):
    return subprocess.check_output(['git','show','85a2cf2:'+BASE+name],cwd=ROOT,text=True)
def function(source,name):
    return re.search(r'^static [^\n]+\b'+name+r'\([^\{]*\{.*?^\}',source,re.M|re.S).group()
class Preservation(unittest.TestCase):
    def test_headers_byte_equal(self):
        for name in ['NaviStopOverlay.h','EwoStationStop.h','NaviEstop.h','NaviCompatibility.h',
                     'NaviMapAdapter.h','NaviSyncRecorder.h','NaviPulseObservation.h','NaviPulseTelemetry.h']:
            self.assertEqual((HERE/name).read_text(),baseline(name),name)
    def test_actuator_manual_mqtt_type5_equal(self):
        old=baseline('NAVI_EWO_0_1_MM045_STOP.ino')
        new=(HERE/'NAVI_EWO_0_3.ino').read_text()
        for name in ['brakeStepMs','requestPwm','serviceRamp','withdraw','handleCommand',
                     'serviceBattery','mqttCallback','connectWifi','serviceIrIngress','recordStop']:
            if re.search(r'^static [^\n]+\b'+name+r'\(',old,re.M):
                self.assertEqual(function(new,name),function(old,name),name)
        old_constants=re.findall(r'^static constexpr uint(?:8|16)_t .+;',old,re.M)
        new_constants=re.findall(r'^static constexpr uint(?:8|16)_t .+;',new,re.M)
        self.assertEqual(old_constants,new_constants)
    def test_stop_algorithm_equal_except_explicit_geographic_revision(self):
        old=baseline('NAVI_EWO_0_1_MM045_STOP.ino')
        new=(HERE/'NAVI_EWO_0_3.ino').read_text()
        fn=function(new,'serviceAutoCruise')
        revision='''\n  static uint32_t consumedGeographicRevision=0;
  if(consumedGeographicRevision!=navi.geographicRevision()) {
    stopOverlay.reset();
    consumedGeographicRevision=navi.geographicRevision();
  }'''
        self.assertIn(revision,fn)
        pending_guard='// A pending DNA judgment reports uncertainty without adding an AUTO\n  // withdrawal threshold. The unchanged overlay holds its existing instruction\n  // when fine geography is unavailable; verified correction resets it above.\n  if (!navi.declared() || (!navi.positionReliable() && !navi.geographicJudgmentPending())) {'
        fn=fn.replace(pending_guard,'if (!navi.declared() || !navi.positionReliable()) {')
        self.assertEqual(fn.replace(revision,''),function(old,'serviceAutoCruise'))
    def test_governing_records_and_ir_calibration_equal(self):
        changed=subprocess.check_output(['git','diff','85a2cf2','--name-only','--','docs/decisions',
                  'firmware/common/IrMovementWire.h'],cwd=ROOT,text=True)
        self.assertEqual(changed,'')
        from textwrap import dedent
        self.assertIn('kInstalledPitchUm = 9652',(ROOT/'firmware/common/IrMovementWire.h').read_text())
if __name__=='__main__':unittest.main()
