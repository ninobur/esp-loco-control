"""Offline actual C++ wire/adapter interoperability; no broker or hardware."""
import ast
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from tools import navi_sync_format as F
from tools.tests.test_navi_sync_format import datagram

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/tests'


class EwoIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        for name in ('emit_records', 'test_review_regressions', 'emit_telemetry_contract'):
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(TESTS / (name + '.cpp')), '-o', str(cls.root / name)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_actual_cpp_wire_and_decoder_causal_links(self):
        raw = subprocess.check_output([str(self.root / 'emit_records')])
        records = []
        while raw:
            size, = struct.unpack_from('<I', raw)
            record, raw = raw[4:4+size], raw[4+size:]
            records.append(record)
        self.assertEqual(len(records), 4)
        ih, ip = F.parse_record(records[0])
        self.assertEqual(F.parse_ir(ip)['pwm_actual'], 0)
        self.assertEqual(F.parse_ir(ip)['rx_us'], 100000)
        ch, cp = F.parse_record(records[1])
        consumed = list(F.iter_consumption(ch, cp))
        self.assertEqual([x['id'] for x in consumed], list(range(100,132)))
        self.assertEqual(consumed[0]['pwm'], 0)
        nh, np = F.parse_record(records[2])
        decision = F.parse_navi(np)
        self.assertEqual(nh.version, 2)
        self.assertEqual(decision['consumption_id'], consumed[-1]['id'])
        self.assertEqual(decision['t_us'], consumed[-1]['decision_us'])
        ah, ap = F.parse_record(records[3])
        self.assertEqual(F.parse_action(ap)['last_consumption_id'], decision['consumption_id'])
        capture = self.root / 'trace.nsr'
        with capture.open('wb') as fh:
            F.write_capture_header(fh, 1)
            for record in records:
                F.write_frame(fh, 2, record)
        destinations = ['--ir-jsonl', '--navi-jsonl', '--consumption-jsonl', '--action-jsonl']
        args = []
        for i, flag in enumerate(destinations):
            args += [flag, str(self.root / f'{i}.jsonl')]
        import sys
        result = subprocess.check_output([sys.executable, str(ROOT/'tools/navi_sync_decode.py'),
                                          str(capture), *args], text=True)
        self.assertEqual(json.loads(result)['consumption'], 32)
        self.assertEqual(json.loads((self.root/'3.jsonl').read_text())['payload'], 'ZONE')

    def test_controller_consumes_actual_firmware_adapter_output(self):
        data = subprocess.check_output([str(self.root/'test_review_regressions'), 'json'], text=True)
        unset, nav, _, _, _ = [json.loads(line) for line in data.splitlines()]
        moving, stopped, stale, unknown, reset = [json.loads(line) for line in
            subprocess.check_output([str(self.root/'emit_telemetry_contract')],
                                    text=True).splitlines()]
        source = (ROOT/'server/ngr_app_v1_11_2.py').read_text()
        tree = ast.parse(source)
        fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name=='_apply_nav_state')
        scope = {'loco_state': {'9950012': {'nav':'UNSET', 'mm':'--'}},
                 '_touch': lambda *args: None, 'USABLE_NAV': {'NORMAL','TRACKING','EVALUATING'}}
        exec(compile(ast.Module(body=[fn], type_ignores=[]), 'actual_controller', 'exec'), scope)
        self.assertEqual(unset['state'], 'UNSET')
        scope['_apply_nav_state']('9950012', nav['state'], nav['mm'])
        self.assertEqual(scope['loco_state']['9950012']['mm'], '012')
        self.assertEqual(nav['authority'], 'NAVI_EWO')
        self.assertEqual(moving['ir_valid'], 1)
        self.assertEqual(stopped['ir_mmps'], 0)
        self.assertEqual(stale['ir_valid'], 0)
        self.assertEqual(reset['ir_distance_state'], 'FRAME_LOST_REDECLARE')
        self.assertEqual(unknown['ir_distance_state'], 'PWM_ZERO_MOVEMENT_REDECLARE')
        self.assertEqual(unknown['state'], 'NORMAL')
        scope['_apply_nav_state']('9950012', unknown['state'], unknown['mm'])
        self.assertEqual(scope['loco_state']['9950012']['mm'], '000')
        node = shutil.which('node')
        self.assertIsNotNone(node, 'Node is required to exercise the actual dashboard renderer')
        js_fn = re.search(r'function irSpeedView\(s\) \{.*?\n\}', source, re.S).group()
        js = "const assert=require('assert'); const STALE_S=5, PKPH_PER_MM_S=1/5.37325;"
        js += 'function ageOf(s,k){return s.ages[k] ?? null;}\n' + js_fn
        js += '\nconst data=' + json.dumps([moving, stopped, stale]) + ';'
        js += "const view=v=>irSpeedView({ir_link:JSON.stringify(v),ages:{ir_link:0}});"
        js += "assert.notEqual(view(data[0]).value,'--'); assert.equal(view(data[1]).value,'0.0');"
        js += "assert.equal(view(data[2]).value,'--');"
        # Execute the actual status-line decision chain, without running a server.
        start = source.index('    var line, cls;')
        status = source[start:source.index('    var sl2 =', start)]
        js += '\nconst s=' + json.dumps({'mm': '000', 'nav': unknown['state'],
                                         'ir_distance_state': unknown['ir_distance_state']}) + ';'
        js += "const heardAge=0, LOCO='Otto', motion='STOPPED', ir={reason:'STOPPED'};"
        js += 'function isFresh(){return true;}\n' + status
        js += "assert.equal(cls,'bad'); assert.match(line,/MOVEMENT AT PWM=0 - LAST MM 000 HELD/);"
        js += "assert.match(line,/VERIFY\\/REPOSITION LOCO AND DECLARE POSITION/);"
        subprocess.run([node, '-e', js], check=True)

    def test_actual_sketch_pwm_zero_auto_withdrawal_and_admission(self):
        source = (TESTS.parent/'NAVI_EYES_WIDE_OPEN_INTEGRATED.ino').read_text()
        # Compile the actual shell functions with output-only hardware stubs.
        functions = []
        for name in ('withdraw', 'servicePwmZeroMovementHold', 'opsNow'):
            functions.append(re.search(r'^static (?:void|Ops) ' + name +
                                       r'\([^\n]*\) \{.*?^\}', source, re.M | re.S).group())
        loop = source[source.index('void loop() {'):]
        self.assertLess(loop.index('serviceIrIngress();'), loop.index('servicePwmZeroMovementHold();'))
        self.assertLess(loop.index('servicePwmZeroMovementHold();'), loop.index('handleCommand(command)'))
        self.assertLess(loop.index('servicePwmZeroMovementHold();'), loop.index('serviceStation();'))
        self.assertIn('else if (Refusal reason = admitAuto(o))', source)
        self.assertIn('if (Refusal reason = admitGo(o))', source)
        self.assertIn('pub("state/nav_ready", opsNow().positionKnown ? "1" : "0", true);', source)
        cpp = '#include "' + str(TESTS.parent/'NaviIntegratedCore.h') + '"\n'
        cpp += '#include "' + str(ROOT/'firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/Ops.h') + '"\n'
        cpp += r'''
#include <cassert>
#include <cstring>
using namespace navi_one;
using namespace navi_eyes;
static NaviIntegratedCore navi;
static bool autoEnrolled = true, autoRunning = true, estopped = false, lowVoltage = false;
static int motorDirection = 1, actualPwm = 0, commandedPwm = 40, sessionDir = 1;
static constexpr int SAFE_DIRECTION_CHANGE_PWM = 15, AUTO_STEP_DOWN_MS = 31;
static int stopRequests = 0;
static const char* warning = nullptr;
static bool publishedAutoOff = false, publishedNotReady = false;
static void requestPwm(int target, int, int) {
  assert(target == 0); commandedPwm = target; ++stopRequests;
}
static void pub(const char* topic, const char* payload, bool retained) {
  assert(retained && !strcmp(payload,"0"));
  if (!strcmp(topic,"state/auto")) publishedAutoOff = true;
  if (!strcmp(topic,"state/nav_ready")) publishedNotReady = true;
}
static void warn(const char* message, bool sticky) { assert(sticky); warning = message; }
'''
        cpp += '\n'.join(functions)
        cpp += r'''
static void ir(uint32_t seq, uint64_t pulses, uint8_t pwm) {
  ir_movement::WireSnapshot w;
  w.bootId = 42; w.sequence = seq; w.capturedUs = uint64_t(seq) * 100000;
  w.completedPulses = w.observedRises = pulses;
  w.nominalUm = pulses * w.pitchUm; w.opticalReason = ir_movement::TRACKING;
  navi.observeIr(w, w.capturedUs, pwm);
}
int main() {
  ir(1,0,40); navi.declare(45,-1,110000);
  servicePwmZeroMovementHold(); assert(stopRequests == 0);
  // Fill NAVI's event queue: withdrawal must not depend on event delivery.
  for (unsigned i=1;i<=80;++i) navi.noteObservationLoss(i,0,110000+i);
  assert(navi.eventLoss() > 0);
  ir(2,1,0); servicePwmZeroMovementHold();
  assert(stopRequests == 1 && !autoEnrolled && !autoRunning && commandedPwm == 0);
  assert(publishedAutoOff && publishedNotReady && warning == kPwmZeroMovementWarning);
  assert(!opsNow().positionKnown && admitAuto(opsNow()) != nullptr);
  autoEnrolled = true; assert(admitGo(opsNow()) != nullptr);
  servicePwmZeroMovementHold(); assert(!autoEnrolled && stopRequests == 2);
  commandedPwm = 40; // operator manual positioning remains available
  ir(3,1000,40); servicePwmZeroMovementHold();
  assert(commandedPwm == 40 && stopRequests == 2 && admitThrottle(opsNow()) == nullptr);
  assert(navi.mm() == 45 && navi.target().sequence == 44 && !opsNow().positionKnown);
  commandedPwm = 0; navi.declare(12,1,310000);
  assert(opsNow().positionKnown && admitAuto(opsNow()) == nullptr);
  autoEnrolled = true; assert(admitGo(opsNow()) == nullptr); autoRunning = true;
  ir(4,1000,0); servicePwmZeroMovementHold(); // ordinary station dwell
  assert(stopRequests == 2 && autoRunning && autoEnrolled);
  ir(5,1001,0); servicePwmZeroMovementHold(); // subsequent movement relatches
  assert(stopRequests == 3 && !autoRunning && !autoEnrolled);
}
'''
        path = self.root/'actual_pwm_zero_shell.cpp'
        path.write_text(cpp)
        exe = self.root/'actual_pwm_zero_shell'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', str(path), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

    def test_actual_type5_ingress_preserves_validity_without_reconfiguration(self):
        source = (TESTS.parent/'NAVI_EYES_WIDE_OPEN_INTEGRATED.ino').read_text()
        functions = [re.search(r'^static (?:void|uint16_t) ' + name +
                               r'\([^\{]*\{.*?^\}', source, re.M | re.S).group()
                     for name in ('movementCrc', 'serviceIrIngress')]
        cpp = '#include "' + str(TESTS.parent/'NaviIntegratedCore.h') + '"\n'
        cpp += r'''
#include <cassert>
#include <cstring>
#include <deque>
using namespace navi_eyes;
namespace navi_sync { enum class InputKind { Ir }; }
struct TestContext { int8_t navDir = 1; };
struct IrRx {
  uint8_t mac[6] = {1,2,3,4,5,6}; uint64_t receivedUs = 0;
  uint16_t length = 110; uint8_t bytes[110]{};
  uint8_t pwmAtReceive = 40, commandedAtReceive = 40;
  TestContext contextAtReceive;
};
static NaviIntegratedCore navi;
static int irQ = 1;
static constexpr int pdTRUE = 1;
static uint32_t seenIrFrames = 0, irPacketInvalid = 0, inputs = 0;
static uint64_t nowUs = 100000;
static std::deque<IrRx> queue;
static int xQueueReceive(int, IrRx* rx, int) {
  if (queue.empty()) return 0;
  *rx = queue.front(); queue.pop_front(); return pdTRUE;
}
static void serviceObservationLoss() {}
static uint64_t esp_timer_get_time() { return nowUs; }
template<class... Args> static void recordInput(Args...) { ++inputs; }
struct Recorder {
  unsigned count = 0;
  template<class... Args> void addIr(Args...) { ++count; }
};
static Recorder recording;
static Recorder* recorder = &recording;
'''
        cpp += '\n'.join(functions)
        cpp += r'''
static ir_movement::WireSnapshot packet(uint32_t seq) {
  ir_movement::WireSnapshot w;
  w.bootId = 42; w.sequence = seq; w.capturedUs = uint64_t(seq) * 100000;
  w.completedPulses = w.observedRises = seq;
  w.nominalUm = w.completedPulses * w.pitchUm;
  w.opticalReason = ir_movement::TRACKING;
  return w;
}
static void deliver(ir_movement::WireSnapshot w, bool badCrc = false, bool shortPacket = false) {
  w.crc = movementCrc(reinterpret_cast<const uint8_t*>(&w), offsetof(ir_movement::WireSnapshot, crc));
  if (badCrc) ++w.crc;
  IrRx rx; rx.receivedUs = nowUs += 100000;
  if (shortPacket) --rx.length;
  memcpy(rx.bytes, &w, sizeof(w)); queue.push_back(rx); serviceIrIngress();
}
int main() {
  deliver(packet(1)); navi.declare(0, 1, nowUs + 1);
  const auto epoch = navi.irMeasurementEpochId();
  for (unsigned fault = 0; fault < 12; ++fault) {
    auto w = packet(fault + 2);
    switch (fault) {
      case 0: w.magic = 0; break;
      case 1: ++w.version; break;
      case 2: w.type = 1; break;
      case 3: w.bootId = 0; break;
      case 4: w.pitchUm = 0; w.nominalUm = 0; break;
      case 5: ++w.pitchUm; w.nominalUm = w.completedPulses * w.pitchUm; break;
      case 6: w.distanceValidated = 1; break;
      case 7: w.opticalReason = 255; break;
      case 8: w.observedRises = w.completedPulses - 1; break;
      case 9: ++w.nominalUm; break;
      case 10:
        w.completedPulses = w.observedRises = UINT64_MAX / w.pitchUm + 1;
        w.nominalUm = w.completedPulses * w.pitchUm;
        break;
      case 11: break; // valid shape, bad CRC
    }
    deliver(w, fault == 11);
    assert(irPacketInvalid == fault + 1 && navi.irObservationCount() == 1);
    assert(navi.irMeasurementEpochId() == epoch && navi.relationshipReliable());
  }
  deliver(packet(20), false, true);
  assert(irPacketInvalid == 13 && inputs == 1 && recording.count == 1);
  assert(!navi.irApplicable(nowUs)); // invalid evidence cannot refresh the source
  auto w = packet(21); w.calibrationId = 99; deliver(w);
  assert(navi.irApplicable(nowUs) && navi.irMeasurementEpochId() == epoch);
  assert(navi.latestIr().calibrationId == 99 && navi.relationshipReliable());
  w.calibrationId = 100; deliver(w); // same sequence/time cannot become a new frame
  assert(navi.irHealthFault() == uint8_t(ngr_nav::IrHealthFault::OrderFault));
  assert(!navi.irMeasurementEpochActive() && !navi.relationshipReliable());
  assert(inputs == 3 && recording.count == 3 && seenIrFrames == 16);
}
'''
        path = self.root/'actual_type5_ingress.cpp'
        path.write_text(cpp)
        exe = self.root/'actual_type5_ingress'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', str(path), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

    def test_loss_identity_and_late_packets(self):
        def header(kind, seq, drops=0, loco=9950012, boot=42, session=7):
            h, _ = F.parse_record(datagram(F.REC_NAVI,1,b'\0'*F.NAVI_LEN))
            h.rec_type=kind; h.batch_seq=seq; h.ring_drops=drops
            h.loco_id=loco; h.loco_boot_id=boot; h.session_id=session
            return h
        tracker=F.LossTracker()
        self.assertIsNone(tracker.observe(header(F.REC_HALL,10,2)))
        self.assertIsNone(tracker.observe(header(F.REC_IR,10,99)))
        gap=tracker.observe(header(F.REC_HALL,12,2))
        self.assertEqual(gap['missing'],1)
        self.assertEqual(gap['recorder_drop_delta'],0)
        self.assertIsNone(tracker.observe(header(F.REC_HALL,11,1000)))
        self.assertIsNone(tracker.observe(header(F.REC_HALL,13,2)))
        gap=tracker.observe(header(F.REC_IR,13,100))
        self.assertEqual(gap['recorder_drop_delta'],1)
        for kw in ({'boot':43},{'loco':9950011},{'session':8}):
            self.assertIsNone(tracker.observe(header(F.REC_HALL,100,50,**kw)))
        tracker.observe(header(F.REC_ACTION,0xffffffff,0))
        self.assertIsNone(tracker.observe(header(F.REC_ACTION,0,0)))
        tracker.observe(header(F.REC_STATUS,1,4))
        gap=tracker.observe(header(F.REC_STATUS,3,8))
        self.assertEqual(gap['recorder_drop_delta'],0)


if __name__ == '__main__':
    unittest.main()
