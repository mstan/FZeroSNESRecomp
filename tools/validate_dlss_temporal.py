"""GPU acceptance test: measured flow direction, history, reset, and resizing."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from PIL import Image

from dlss_worker import Bridge, History


def validate(root, output):
    output.mkdir(parents=True, exist_ok=False)
    bridge = Bridge(root)
    args = SimpleNamespace(style=1, intensity=1, tone=1, structure=1, channel_order='rgb')
    rng = np.random.default_rng(716)
    texture = rng.integers(20, 230, (120, 160, 3), dtype=np.uint8)
    rgb = np.array(Image.fromarray(texture).resize((640, 480), Image.Resampling.BILINEAR))
    records = []
    for i in range(12):
        current = np.roll(rgb, i * 8, axis=1)
        out, ms = bridge.process(current, i == 0, True, args)
        stats = list(bridge.motion)
        assert stats[4] == (0 if i == 0 else 1), stats
        if i:
            assert abs(stats[0] + 8) < 1, stats
            assert abs(stats[1]) < 1, stats
            assert stats[3] > 0.95, stats
        records.append(dict(frame=i, milliseconds=ms, motion=stats))
        Image.fromarray(out).save(output / f'{i:06d}.png')
    shifted = np.roll(rgb, 88, axis=1)
    # Resetting the same frame must remove measured flow/history.
    reset_out, _ = bridge.process(shifted, True, True, args)
    assert bridge.motion[4] == 0
    history_difference = float(np.abs(out.astype(float) - reset_out).mean())
    assert history_difference > 0, 'History has no observable effect'
    # A new size creates a new feature and starts with zero motion.
    resized = np.array(Image.fromarray(rgb).resize((853, 480)))
    bridge.process(resized, True, True, args)
    assert bridge.motion[4] == 0
    bridge.process(np.roll(resized, 8, axis=1), False, True, args)
    assert bridge.motion[4] == 1 and abs(bridge.motion[0] + 8) < 1
    args.intensity = 0
    identity, _ = bridge.process(resized, False, True, args)
    assert np.array_equal(identity, resized)
    args.intensity = 1
    bridge.process(resized, False, True, args)
    assert bridge.motion[4] == 0, 'Strength bypass must invalidate history'
    history = History()
    black, white = np.zeros_like(rgb), np.full_like(rgb, 255)
    assert history.reset_for(black)
    assert not history.reset_for(black)
    assert history.reset_for(white)
    assert history.reset_for(white, True)
    report = dict(**bridge.info(), frames=records, history_difference=history_difference,
                  translation='8 pixels right -> approximately -8 pixels backward flow',
                  resize=True, reset=True, zero_strength_identity=True)
    (output / 'report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    code = 0
    try:
        validate(args.root, args.output)
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    finally:
        # Same isolated-process lifetime as dlss_worker; NGX detach can hang.
        import sys
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(code)
