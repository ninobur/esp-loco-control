import binascii
import unittest

from ir_usb_bench import Counter, Decoder, WIRE


def packet(seq=1, raw=1234):
    data = WIRE.pack(0x31524942, 42, seq, seq*1000, raw, 0, 0, 0)
    return data[:-2] + binascii.crc_hqx(data[:-2], 0xffff).to_bytes(2, 'little')


class DecoderTests(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main()
