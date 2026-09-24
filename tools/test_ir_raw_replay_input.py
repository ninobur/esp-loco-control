"""Offline tests of raw replay extraction, including both CRC layers."""
import gzip
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from ir_scope_espnow_analyze import HDR, crc16

EXTRACTOR = Path(__file__).with_name('ir_raw_replay_input.py')


def line(start=0, corrupt=False):
    packet = HDR.pack(0x4952, 1, 1, 0x1234, start // 2, start, 2, 0,
                      1000, 2000, 1666, 1333, *([0] * 7))
    packet += struct.pack('<96H', 0x8000 | 1000, 0xA000 | 2000, *([0] * 94))
    packet += struct.pack('<H', crc16(packet))
    if corrupt:
        packet = packet[:56] + bytes([packet[56] ^ 1]) + packet[57:]
    # Correct outer CRC with an intentionally bad inner CRC in corrupt case.
    return f'1000.0 RX 1 -70 250 {crc16(packet):04x} {packet.hex()}\n'


class ExtractionTest(unittest.TestCase):
    def run_extract(self, content, compressed=False):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / ('capture.log.gz' if compressed else 'capture.log')
            if compressed:
                path.write_bytes(gzip.compress(content.encode()))
            else:
                path.write_text(content)
            return subprocess.run([sys.executable, str(EXTRACTOR), str(path), '--sid', '1234'],
                                  capture_output=True, text=True)

    def test_plain_and_gzip(self):
        for compressed in (False, True):
            result = self.run_extract(line() + line(4), compressed)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, '0 1000 8\n1 2000 10\n4 1000 8\n5 2000 10\n')
            self.assertIn('missing_ranges=1 missing_samples=2', result.stderr)

    def test_inner_crc_rejected(self):
        result = self.run_extract(line(corrupt=True))
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')
        self.assertIn('corrupt=1', result.stderr)

    def test_duplicate_rejected(self):
        result = self.run_extract(line() + line())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Duplicate/out-of-order', result.stderr)


if __name__ == '__main__':
    unittest.main()
