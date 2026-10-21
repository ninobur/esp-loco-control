import unittest

from replay import DNAExperimental, Observation, QuorumExperimental, SimplifiedExperimental, run


class ReplayContractTests(unittest.TestCase):
    ROUTE = [1, -1, 1, -1]

    def test_dna_is_causal_and_retains_position_on_contradiction(self):
        result = run(DNAExperimental(self.ROUTE, 0), [Observation(1, -1), Observation(2, -1)])
        self.assertEqual([row["last_confirmed_mm"] for row in result], [1, 1])
        self.assertEqual(result[1]["uncertainty"], "contradicted")

    def test_quorum_refuses_ambiguous_polarity(self):
        row = run(QuorumExperimental(self.ROUTE, 0), [Observation(1, -1)])[0]
        self.assertEqual(row["uncertainty"], "no_quorum")

    def test_simplified_does_not_count_a_duplicate_key_twice(self):
        result = run(SimplifiedExperimental(self.ROUTE, 0), [Observation(1, -1), Observation(1, -1)])
        self.assertTrue(result[0]["recognized"])
        self.assertEqual(result[1]["note"], "duplicate event key suppressed")

    def test_rejects_future_before_past(self):
        with self.assertRaises(ValueError):
            run(DNAExperimental(self.ROUTE, 0), [Observation(2, -1), Observation(1, 1)])


if __name__ == "__main__":
    unittest.main()
