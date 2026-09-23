import binascii
import os
import tempfile
import unittest

from ir_noon_analysis import parse_ir, reception, HDR, SAMPLES


class NoonAuditTests(unittest.TestCase):
    def test_loss_counts_unique_sequences_per_boot(self):
        rows=[dict(boot=b,seq=s,rssi=-80) for b,s in ((1,1),(1,3),(1,3),(2,50),(2,51))]
        result=reception(rows)
        self.assertEqual(result['received'],4)
        self.assertEqual(result['expected'],5)
        self.assertAlmostEqual(result['loss_pct'],20)

    def test_empty_reception(self):
        self.assertEqual(reception([]),{})

    def test_raw_crc_and_flags(self):
        header=HDR.pack(0x4952,1,1,123,7,576,96,0,300,1700,1200,800,0,0,0,0,18,0,0)
        payload=header+SAMPLES.pack(*([0x2000|1200]*96))
        payload+=binascii.crc_hqx(payload,65535).to_bytes(2,'little')
        good=f'1789931500 RX 500 -85 250 {binascii.crc_hqx(payload,65535):04x} {payload.hex()}\n'
        corrupt=bytearray(payload);corrupt[80]^=1
        bad=f'1789931501 RX 600 -85 250 {binascii.crc_hqx(corrupt,65535):04x} {corrupt.hex()}\n'
        with tempfile.NamedTemporaryFile(mode='w',delete=False) as f:
            f.write(good+bad);path=f.name
        try:
            raw,motion,errors=parse_ir(path)
        finally:
            os.unlink(path)
        self.assertEqual(len(raw),1)
        self.assertEqual(raw[0]['seq'],7)
        self.assertEqual(raw[0]['span'],1400)
        self.assertEqual(raw[0]['samples'][0]&4095,1200)
        self.assertEqual(motion,[])
        self.assertEqual(errors,{'payload':1})


if __name__=='__main__':
    unittest.main()
