import ctypes as c
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from dlss_worker import Bridge, History


class FakeNative:
    def dlss5nr_process(self, src, dst, w, h, style, preset, intensity,
                        tone, structure, skin, mask, reset, temporal, error, cap):
        self.reset = reset
        self.temporal = temporal
        np.ctypeslib.as_array(dst, shape=(w * h * 3,))[:] = np.ctypeslib.as_array(src, shape=(w * h * 3,))
        return 1

    def dlss5nr_motion_stats(self, out):
        out[4] = int(self.temporal and not self.reset)


class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.bridge = Bridge.__new__(Bridge)
        self.bridge.lib = FakeNative()
        self.bridge.error = c.create_string_buffer(4096)
        self.bridge.motion = (c.c_float * 5)()
        self.args = SimpleNamespace(style=1, intensity=1, tone=1, structure=1, channel_order='rgb')
        self.rgb = np.full((32, 48, 3), [220, 30, 70], dtype=np.uint8)

    def test_rgb_and_temporal_contract(self):
        out, _ = self.bridge.process(self.rgb, False, True, self.args)
        np.testing.assert_array_equal(out, self.rgb)
        self.assertEqual(self.bridge.motion[4], 1)

    def test_black_first_frame_does_not_choose_channel_order(self):
        self.bridge.process(np.zeros_like(self.rgb), True, True, self.args)
        out, _ = self.bridge.process(self.rgb, False, True, self.args)
        np.testing.assert_array_equal(out, self.rgb)

    def test_zero_strength_identity_and_history_invalidation(self):
        self.args.intensity = 0
        out, ms = self.bridge.process(self.rgb, False, True, self.args)
        np.testing.assert_array_equal(out, self.rgb)
        self.assertEqual(ms, 0)
        self.args.intensity = 1
        self.bridge.process(self.rgb, False, True, self.args)
        self.assertTrue(self.bridge.lib.reset)
        self.bridge.process(self.rgb, False, True, self.args)
        self.assertFalse(self.bridge.lib.reset)

    def test_history_reset_cut_and_size(self):
        history = History()
        self.assertTrue(history.reset_for(self.rgb))
        self.assertFalse(history.reset_for(self.rgb))
        self.assertTrue(history.reset_for(self.rgb, True))
        self.assertTrue(history.reset_for(self.rgb[:16]))
        self.assertTrue(history.reset_for(np.full_like(self.rgb[:16], 255)))


if __name__ == '__main__':
    unittest.main()
