import unittest
from ir_measured_travel import evaluate,COUNTERS

def row(seq,count,reason='TRACKING',bad=0):
    r={k:0 for k in COUNTERS}
    r.update(boot_id=1,sequence=seq,captured_us=seq*100000,
             completed_pulses=count,observed_rises=count,pitch_um=9652,
             calibration_id=0,reason=reason,unreliable_samples=bad)
    return r

class MeasuredTravel(unittest.TestCase):
    def setUp(self):
        self.trial=dict(label='synthetic',boot_id=1,start_sequence=1,end_sequence=3,
                        truth_min_mm=965.2,truth_max_mm=965.2,truth_pulses=100,
                        truth_method='synthetic known full turns')
    def test_missing_reports_do_not_mean_missing_pulses(self):
        result=evaluate([row(1,5),row(3,105)],self.trial)
        self.assertEqual(result['net_count_error'],0)
        self.assertEqual(result['missing_snapshots'],1)
        self.assertFalse(result['field_distance_validated'])
    def test_hidden_bad_samples_preserved(self):
        result=evaluate([row(1,5),row(3,103,bad=50)],self.trial)
        self.assertEqual(result['net_count_error'],-2)
        self.assertTrue(result['unreliability_detected'])
    def test_reset_cannot_supply_endpoint(self):
        b=row(3,105);b['boot_id']=2
        with self.assertRaises(ValueError):evaluate([row(1,5),b],self.trial)
    def test_stationary_false_pulse(self):
        self.trial.update(truth_min_mm=0,truth_max_mm=0,truth_pulses=0)
        result=evaluate([row(1,0),row(3,1)],self.trial)
        self.assertEqual(result['net_count_error'],1)
        self.assertIsNone(result['net_count_error_fraction'])
    def test_counter_reversal_rejected(self):
        with self.assertRaises(ValueError):evaluate([row(1,5),row(3,4)],self.trial)

if __name__=='__main__':unittest.main()
