import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
import json
from pathlib import Path

from tools import navi_sync_format as F


def datagram(rec_type, n_items, payload):
    fields = (F.MAGIC, 1, rec_type, n_items, 9950012, 7, 3, 11, 1000,
              1234567, 0x1122334455667788, 0, 41, 1, 0, 1, 0)
    header = struct.pack(F.HDR_FMT, *fields)
    crc = zlib.crc32(header[:-4] + b"\0\0\0\0")
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return header[:-4] + struct.pack("<I", crc) + payload


class NaviSyncFormatTests(unittest.TestCase):
    def test_native_hall_record(self):
        payload = struct.pack(F.HALL_FMT, 0, 500, 501, 502, 503, 504, 502, 90, 90, 8, 0)
        header, body = F.parse_record(datagram(F.REC_HALL, 1, payload))
        self.assertEqual(header.first_item_seq, 11)
        row = next(F.iter_hall(header, body))
        self.assertEqual(row["raw"], [500, 501, 502, 503, 504])
        self.assertEqual(row["median"], 502)

    def test_ir_record_preserves_110_byte_wire_payload(self):
        wire = bytes(range(110))
        payload = struct.pack(F.IR_FMT, 1234600, bytes.fromhex("38182b308c2c"),
                              1, 2, 3, 90, 90, 41, 1, 0, 1, wire)
        header, body = F.parse_record(datagram(F.REC_IR, 1, payload))
        item = F.parse_ir(body)
        self.assertEqual(item["source_mac"], "38:18:2B:30:8C:2C")
        self.assertEqual(item["wire"], wire)
        self.assertEqual(header.rec_type, F.REC_IR)

    def test_corruption_is_rejected(self):
        payload = struct.pack(F.STATUS_FMT, 1000, 1234, *([0] * 8), 0, 0, 1, 1, 0, 0)
        broken = bytearray(datagram(F.REC_STATUS, 1, payload))
        broken[-1] ^= 1
        with self.assertRaises(F.BadRecord):
            F.parse_record(bytes(broken))

    def test_ewo_native_hall_and_navi_decision_records(self):
        native = struct.pack(F.NATIVE_HALL_FMT, 204, 5_123_456, 777, 40, 1)
        header, payload = F.parse_record(datagram(F.REC_HALL_NATIVE, 1, native))
        self.assertEqual(header.rec_type, F.REC_HALL_NATIVE)
        self.assertEqual(list(F.iter_native_hall(header, payload)), [{
            "serial": 204, "t_us": 5_123_456, "raw": 777,
            "pwm": 40, "direction": 1}])
        decision = struct.pack(F.NAVI_FMT, 5_123_456, 330_000, 329_000,
                               204, 202, 0, 0, 777, 700, 3, 17, 18,
                               1, 0, 1, 2)
        header, payload = F.parse_record(datagram(F.REC_NAVI, 1, decision))
        self.assertEqual(F.parse_navi(payload)["median5"], 777)
        self.assertEqual(F.parse_navi(payload)["opening_serial"], 202)
        self.assertEqual(F.parse_navi(payload)["position_reliable"], 1)

    def test_ewo_record_lengths_and_crc_are_checked(self):
        native = struct.pack(F.NATIVE_HALL_FMT, 1, 2, 3, 4, 5)
        with self.assertRaises(F.BadRecord):
            F.parse_record(datagram(F.REC_HALL_NATIVE, 2, native))
        broken = bytearray(datagram(F.REC_HALL_NATIVE, 1, native))
        broken[-1] ^= 1
        with self.assertRaises(F.BadRecord):
            F.parse_record(bytes(broken))

    def test_ewo_capture_decodes_native_hall_and_decision(self):
        native = struct.pack(F.NATIVE_HALL_FMT, 204, 5_123_456, 777, 40, 1)
        decision = struct.pack(F.NAVI_FMT, 5_123_456, 330_000, 329_000,
                               204, 202, 0, 0, 777, 700, 3, 17, 18,
                               1, 0, 1, 2)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "evidence.nsr"
            native_csv = root / "native.csv"
            navi_jsonl = root / "navi.jsonl"
            with capture.open("wb") as fh:
                F.write_capture_header(fh, 123)
                F.write_frame(fh, 124, datagram(F.REC_HALL_NATIVE, 1, native))
                F.write_frame(fh, 125, datagram(F.REC_NAVI, 1, decision))
            script = Path(__file__).resolve().parents[1] / "navi_sync_decode.py"
            result = subprocess.run(
                [sys.executable, str(script), str(capture),
                 "--native-hall-csv", str(native_csv),
                 "--navi-jsonl", str(navi_jsonl)],
                check=True, capture_output=True, text=True)
            self.assertEqual(json.loads(result.stdout)["native_hall_samples"], 1)
            self.assertEqual(json.loads(result.stdout)["navi"], 1)
            self.assertIn("204,5123456,777,40,1", native_csv.read_text())
            self.assertEqual(json.loads(navi_jsonl.read_text())["opening_serial"], 202)


if __name__ == "__main__":
    unittest.main()
