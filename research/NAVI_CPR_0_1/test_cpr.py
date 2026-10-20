import unittest
import numpy as np
from cpr import Config, NativeRule, Segmenter, area_resample, describe
from prepare import NATIVE


def segment(y, config=Config()):
    s = Segmenter(config); events = []
    for k, x in enumerate(y):
        e, _, _ = s.feed((k+1)*.02, float(x))
        if e:
            events.append(e)
    s.finish()
    return s, events


class Invariants(unittest.TestCase):
    def test_native_threshold_and_second_sample(self):
        a = np.zeros(6, dtype=NATIVE)
        a['serial'] = np.arange(6); a['us'] = np.arange(6)*200
        a['adc'] = [1069, 1070, 1070, 1000, 930, 930]
        h = NativeRule().feed(a, 1000)
        self.assertEqual([(r['t'], r['polarity']) for r in h], [(.0004, 1), (.001, -1)])

    def test_native_loss_and_chunk_boundary(self):
        a = np.zeros(3, dtype=NATIVE)
        a['serial'] = [1, 3, 4]; a['us'] = [200, 600, 800]; a['adc'] = 1070
        rule = NativeRule()
        self.assertEqual(rule.feed(a[:1], 1000), [])
        h = rule.feed(a[1:], 1000)
        self.assertEqual(h[0]['t'], .0008)
        self.assertEqual(len(h), 1)

    def test_area_preserves_all_samples(self):
        x = np.zeros(101); x[53] = 101
        self.assertAlmostEqual(area_resample(x).mean(), x.mean())
        self.assertAlmostEqual(area_resample(np.ones(7)).min(), 1)

    def test_no_completion_on_peak_or_reversal(self):
        y = 1000 + np.r_[np.zeros(100), np.linspace(0, 180, 15),
            np.full(20, 180), np.linspace(180, -40, 15), np.full(15, -40),
            np.linspace(-40, 0, 15), np.zeros(80)]
        s, e = segment(y)
        self.assertEqual(len(e), 1)
        self.assertGreater(e[0]['end'], 3.4)
        self.assertEqual(sum(r['kind'] == 'provisional_open' for r in s.journal), 1)

    def test_separated_encounters(self):
        hill = 160*np.sin(np.linspace(0, np.pi, 31))
        _, e = segment(1000+np.r_[np.zeros(100), hill, np.zeros(40), -hill, np.zeros(60)])
        self.assertEqual(len(e), 2)
        self.assertEqual([describe(x)['polarity'] for x in e], [1, -1])

    def test_no_eof_or_gap_completion(self):
        for tail in ([], [np.nan]):
            s, e = segment(np.r_[np.full(100, 1000.), np.full(10, 1100.), tail])
            self.assertEqual(e, [])
            self.assertTrue(any(r['kind'] == 'incomplete' for r in s.journal))

    def test_horizon_bounded_and_no_recognition(self):
        s, e = segment(np.r_[np.full(100, 1000.), np.full(2000, 1100.)], Config(horizon_s=1))
        self.assertEqual(e, [])
        self.assertLessEqual(s.max_buffer, 69)
        self.assertTrue(any(r.get('reason') == 'horizon_limit' for r in s.journal))

    def test_prefix_causality(self):
        y = 1000+np.r_[np.zeros(100), 150*np.sin(np.linspace(0, np.pi, 41)), np.zeros(60)]
        short, _ = segment(y[:125]); full, _ = segment(y)
        a = [r for r in short.journal if r['kind'] != 'incomplete']
        b = [r for r in full.journal if r['t'] <= 2.5 and r['kind'] != 'incomplete']
        self.assertEqual(a, b)

    def test_constant_background_and_time_order(self):
        s, e = segment(np.full(10000, 1890.))
        self.assertEqual(e, [])
        self.assertEqual(s.max_buffer, 0)
        with self.assertRaises(ValueError):
            s.feed(1., 1890.)


if __name__ == '__main__':
    unittest.main(verbosity=2)
