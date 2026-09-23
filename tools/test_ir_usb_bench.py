import binascii
import csv
import multiprocessing
import queue
import tempfile
import time
import unittest
from pathlib import Path

from ir_usb_bench import BATCH, READING, Counter, Decoder, WIRE, capture_process


def packet(seq=1, raw=1234):
    data = WIRE.pack(0x31524942, 42, seq, seq*1000, raw, 0, 0, 0)
    return data[:-2] + binascii.crc_hqx(data[:-2], 0xffff).to_bytes(2, 'little')


class DecoderTests(unittest.TestCase):
    def test_batched_timestamps_missed_slots_and_sequence_wrap(self):
        data = BATCH.pack(0x32524942, 99, 0xffffffff, 9000000000, 5, 16, 2)
        data += READING.pack(0, 700, 0) + READING.pack(3000, 1800, 2)
        data += bytes(14*READING.size)
        data += binascii.crc_hqx(data, 0xffff).to_bytes(2, 'little')
        self.assertEqual(len(data), 128)
        decoder, result = Decoder(), []
        for byte in data:
            result.extend(decoder.feed(bytes([byte])))
        self.assertEqual(result, [(99, 0xffffffff, 9000000000, 700, 5, 16),
                                  (99, 0, 9000003000, 1800, 7, 16)])
        corrupt = bytearray(data)
        corrupt[50] ^= 1
        decoder = Decoder()
        self.assertEqual(decoder.feed(corrupt + data), result)
        self.assertEqual(decoder.bad_crc, 1)

    def test_fragmented_record(self):
        decoder = Decoder()
        data = packet()
        found = []
        for byte in data:
            found.extend(decoder.feed(bytes([byte])))
        self.assertEqual(found, [(42, 1, 1000, 1234, 0, 0)])

    def test_corruption_and_boot_noise_resync(self):
        decoder = Decoder()
        corrupt = bytearray(packet())
        corrupt[22] ^= 1
        records = decoder.feed(b'ESP ROM boot\n' + corrupt + packet(2) + packet(3))
        self.assertEqual([r[1] for r in records], [2, 3])
        self.assertEqual(decoder.bad_crc, 1)

    def test_uint32_and_timestamp_bounds(self):
        decoder = Decoder()
        self.assertEqual(decoder.feed(packet(0xffffffff))[0][1], 0xffffffff)
        self.assertEqual(decoder.feed(packet(0))[0][1], 0)

    def test_hysteresis_and_gap(self):
        counter = Counter()
        for raw in [1800, 1800, 800, 1300, 1600, 1400, 1600]:
            counter.update(raw, 1000, 1500)
        self.assertEqual(counter.count, 1)
        counter.update(800, 1000, 1500)
        counter.update(1600, 1000, 1500, discontinuity=True)
        self.assertEqual(counter.count, 1)
        counter.update(800, 1000, 1500)
        counter.update(1600, 1000, 1500)
        self.assertEqual(counter.count, 2)

    def test_recorder_keeps_logging_with_full_display_queue(self):
        context = multiprocessing.get_context('spawn')
        output, commands, stop = context.Queue(1), context.Queue(), context.Event()
        with tempfile.TemporaryDirectory() as folder:
            path = str(Path(folder) / 'capture.csv')
            process = context.Process(target=capture_process,
                                      args=('', path, True, output, commands, stop))
            process.start()
            try:
                time.sleep(1.5)
                # Never drain the display queue: this models a hung GUI.
                stop.set()
                process.join(5)
                self.assertFalse(process.is_alive())
                self.assertEqual(process.exitcode, 0)
                with open(path) as stream:
                    rows = list(csv.DictReader(stream))
                self.assertGreater(len(rows), 400)
                self.assertEqual([int(r['seq']) for r in rows], list(range(len(rows))))
                self.assertTrue(all(r['demo'] == '1' for r in rows))
            finally:
                if process.is_alive():
                    process.terminate()
                    process.join()


if __name__ == '__main__':
    unittest.main()
