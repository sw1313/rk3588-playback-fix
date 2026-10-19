"""Evidence parsing must not confuse virtual CLIENT composition with HDMI."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'display_path', Path(__file__).parents[1] / 'decode-performance/collect_display_path.py')
display_path = importlib.util.module_from_spec(spec)
spec.loader.exec_module(display_path)


class DisplayPathTests(unittest.TestCase):
    def test_mirroring_does_not_mark_physical_output_as_client(self):
        displays = display_path.parse_displays('''
+ DisplayDevice{0, internal, primary, "Internal display"}
  isVirtual=false DisplayId=0
  isEnabled=true isSecure=true usesClientComposition=false usesDeviceComposition=true
+ DisplayDevice{123, virtual, "scrcpy"}
  isVirtual=true DisplayId=123
  isEnabled=true isSecure=false usesClientComposition=true usesDeviceComposition=false
''')
        self.assertEqual(len(displays), 2)
        self.assertFalse(displays[0]['virtual'])
        self.assertFalse(displays[0]['client_composition'])
        self.assertTrue(displays[1]['virtual'])
        self.assertTrue(displays[1]['client_composition'])

    def test_unknown_state_is_not_a_successful_device_composition(self):
        result = display_path.parse_displays('+ DisplayDevice{0, internal, "Internal display"}\n')
        self.assertIsNone(result[0]['device_composition'])
        self.assertIsNone(result[0]['enabled'])

    def test_pending_fences_and_duplicate_present_times_are_excluded(self):
        result = display_path.parse_latency('''16666667
0 0 0
1 1000000000 1
2 1016666667 2
3 1016666667 3
4 1050000000 4
5 9223372036854775807 5
''')
        self.assertEqual(result['present_samples'], 3)
        self.assertEqual(result['window_gaps_gt25ms'], 1)
        self.assertAlmostEqual(result['window_max_gap_ms'], 33.333333)

    def test_unavailable_latency_is_not_zero_gap_evidence(self):
        result = display_path.parse_latency('16666667\n0 0 0\n')
        self.assertEqual(result['present_samples'], 0)
        self.assertIsNone(result['window_max_gap_ms'])

    def test_missing_sensors_stay_unknown(self):
        result = display_path.parse_state('===gpu\ncur_freq\n400000000\nload\nPermission denied\n')
        self.assertEqual(result['gpu_freq_hz'], 400000000)
        self.assertIsNone(result['gpu_load_pct'])


if __name__ == '__main__':
    unittest.main()
