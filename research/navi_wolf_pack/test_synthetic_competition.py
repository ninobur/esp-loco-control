import unittest
from synthetic_competition import N, ROUTE, SPACING, detect, simulate

class SyntheticTrackTests(unittest.TestCase):
    def test_complete_surveyed_map(self):
        self.assertEqual(len(ROUTE), N); self.assertEqual(len(SPACING), N)
        self.assertEqual(sum(SPACING), 52150)
    def test_truth_is_independent_and_complete(self):
        samples, truth, _ = simulate("clean", "otto", 7)
        self.assertEqual(len([x for x in truth if x.kind == "magnet"]), N)
        self.assertGreater(len(samples), N)
        self.assertGreater(len(detect(samples)), 100)
    def test_stop_has_stationary_truth_not_new_magnet(self):
        _, truth, _ = simulate("stops", "otto", 8)
        self.assertGreater(len([x for x in truth if x.kind == "dwell"]), 0)
        self.assertEqual(len([x for x in truth if x.kind == "magnet"]), N)

if __name__ == "__main__": unittest.main()
