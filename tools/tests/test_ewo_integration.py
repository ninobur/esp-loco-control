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
        self.assertEqual(unknown['ir_distance_state'], 'INTERVAL_KNOWN_IR_POSITION_UNKNOWN')
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
        js += "assert.equal(cls,'warn'); assert.match(line,/INTERVAL KNOWN - MM 000 HELD/);"
        js += "assert.match(line,/IR POSITION UNKNOWN/); assert.ok(!line.includes('RE-DECLARE'));"
        subprocess.run([node, '-e', js], check=True)

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
