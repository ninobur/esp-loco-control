import struct
import unittest
import zlib

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


if __name__ == "__main__":
    unittest.main()
