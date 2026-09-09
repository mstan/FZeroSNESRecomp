"""Isolated DLSS NR bridge runner. Native ABI from ComfyUI-DLSS5-NR v0.3.0 (MIT).

No ComfyUI/Torch dependency. Live mode uses a single-slot Windows shared-memory
mailbox; process mode provides a reproducible still/sequence capability test.
"""
import argparse
import ctypes as c
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import sys
import time

import numpy as np
from PIL import Image

MAX_PIXELS = 1280 * 960
HEADER = 1024
SIZE = HEADER + MAX_PIXELS * 8


class Bridge:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.handles = [os.add_dll_directory(str(self.root / p))
                        for p in ('native/bin', 'runtime', 'runtime/caller')]
        self.lib = c.CDLL(str(self.root / 'native/bin/dlss5nr_bridge.dll'))
        self.error = c.create_string_buffer(4096)
        self.lib.dlss5nr_init.argtypes = [c.c_int, c.c_wchar_p, c.c_char_p, c.c_int]
        self.lib.dlss5nr_init.restype = c.c_int
        self.lib.dlss5nr_process.argtypes = [c.POINTER(c.c_float)] * 2 + [c.c_int] * 4 + [c.c_float] * 4 + [c.c_int] * 3 + [c.c_char_p, c.c_int]
        self.lib.dlss5nr_process.restype = c.c_int
        self.lib.dlss5nr_gpu_name.restype = c.c_char_p
        self.lib.dlss5nr_version.restype = c.c_char_p
        self.lib.dlss5nr_shutdown.restype = None
        self.lib.dlss5nr_motion_stats.argtypes = [c.POINTER(c.c_float)]
        self.lib.dlss5nr_motion_stats.restype = None
        self.motion = (c.c_float * 5)()
        self.check(self.lib.dlss5nr_init(0, str(self.root / 'runtime'), self.error, len(self.error)))

    def check(self, result):
        if not result:
            raise RuntimeError(self.error.value.decode('utf-8', 'replace'))

    def info(self):
        runtime = self.root / 'runtime/nvngx_dlssnr.dll'
        with runtime.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        return dict(gpu=self.lib.dlss5nr_gpu_name().decode(),
                    bridge=self.lib.dlss5nr_version().decode(), runtime_sha256=digest,
                    optical_flow=bool(self.lib.dlss5nr_nvof_available()))

    def process(self, rgb, reset, temporal, args):
        if args.intensity == 0:
            # This runtime returns black at zero strength; use a defined
            # identity image and invalidate history before the next evaluation.
            self.bypassed = True
            self.motion = (c.c_float * 5)()
            return rgb.copy(), 0.0
        reset = reset or getattr(self, 'bypassed', False)
        self.bypassed = False
        src = np.ascontiguousarray(rgb, dtype=np.float32) / 255.0
        dst = np.empty_like(src)
        h, w, _ = src.shape
        start = time.perf_counter()
        self.check(self.lib.dlss5nr_process(
            src.ctypes.data_as(c.POINTER(c.c_float)), dst.ctypes.data_as(c.POINTER(c.c_float)),
            w, h, args.style, 3, args.intensity, args.tone, args.structure, -1.0,
            0, int(reset), int(temporal), self.error, len(self.error)))
        elapsed = (time.perf_counter() - start) * 1000
        self.lib.dlss5nr_motion_stats(self.motion)
        if not np.isfinite(dst).all():
            raise RuntimeError('Neural output contains non-finite pixels')
        # The pinned SF-v2 runtime returns RGB. Do not infer channel order from
        # a potentially black boot frame or from intentional neural relighting.
        self.swap = args.channel_order == 'bgr'
        if self.swap:
            dst = dst[..., ::-1]
        return (np.clip(dst, 0, 1) * 255).round().astype(np.uint8), elapsed


class History:
    def __init__(self):
        self.previous = None
        self.size = None

    def reset_for(self, rgb, requested=False):
        sample = np.asarray(Image.fromarray(rgb).resize((16, 16), Image.Resampling.BOX), dtype=np.float32)
        reset = bool(requested or self.size != rgb.shape or self.previous is None)
        if not reset:
            reset = bool(np.abs(sample - self.previous).mean() > 80)
        self.previous, self.size = sample, rgb.shape
        return reset


def live(args):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.OpenEventW.argtypes = [c.c_uint32, c.c_int, c.c_wchar_p]
    kernel.OpenEventW.restype = c.c_void_p
    kernel.WaitForMultipleObjects.argtypes = [c.c_uint32, c.POINTER(c.c_void_p), c.c_int, c.c_uint32]
    kernel.SetEvent.argtypes = [c.c_void_p]
    kernel.CloseHandle.argtypes = [c.c_void_p]
    handles = [kernel.OpenEventW(0x1F0003, False, args.mapping + suffix)
               for suffix in ('-stop', '-request', '-done')]
    if not all(handles):
        raise OSError(c.get_last_error(), 'OpenEvent failed')
    memory = mmap.mmap(-1, SIZE, tagname=args.mapping)
    log_root = Path(args.root) if Path(args.root).is_dir() else Path(__file__).resolve().parent
    log = open(log_root / 'live.jsonl', 'a', buffering=1)
    bridge = None
    try:
        bridge = Bridge(args.root)
        log.write(json.dumps(dict(event='init', **bridge.info())) + '\n')
        struct.pack_into('<I', memory, 12, 1)
        kernel.SetEvent(handles[2])
        history = History()
        waits = (c.c_void_p * 2)(*handles[:2])
        while True:
            result = kernel.WaitForMultipleObjects(2, waits, False, 0xFFFFFFFF)
            if result == 0:
                break
            if result != 1:
                raise OSError(c.get_last_error(), 'Mailbox wait failed')
            w, h, reset, _, _, sequence = struct.unpack_from('<6I', memory)
            if not (0 < w <= 1280 and 0 < h <= 960):
                raise ValueError('Invalid mailbox dimensions')
            packed = np.frombuffer(memory, '<u4', w * h, HEADER).copy().reshape(h, w)
            rgb = np.stack([(packed >> 16) & 255, (packed >> 8) & 255, packed & 255], axis=-1).astype(np.uint8)
            reset = history.reset_for(rgb, reset or not args.temporal)
            out, ms = bridge.process(rgb, reset, args.temporal, args)
            packed_out = (out[..., 0].astype(np.uint32) << 16) | (out[..., 1].astype(np.uint32) << 8) | out[..., 2]
            memory[HEADER + MAX_PIXELS * 4:HEADER + MAX_PIXELS * 4 + w * h * 4] = packed_out.tobytes()
            struct.pack_into('<I', memory, 12, 2)
            struct.pack_into('<I', memory, 16, round(ms))
            struct.pack_into('<I', memory, 24, sequence)
            struct.pack_into('<I', memory, 28, int(bridge.motion[4]))
            log.write(json.dumps(dict(event='evaluated', sequence=sequence, milliseconds=ms,
                                      reset=reset, temporal=args.temporal,
                                      width=w, height=h, swap_rb=getattr(bridge, 'swap', False),
                                      motion=list(bridge.motion))) + '\n')
            if sequence % 120 == 0:
                Image.fromarray(rgb).save(Path(args.root) / 'live-original.png')
                Image.fromarray(out).save(Path(args.root) / 'live-neural.png')
            kernel.SetEvent(handles[2])
    except Exception as error:
        message = str(error).encode('utf-8')[:899]
        memory[64:64 + len(message) + 1] = message + b'\0'
        struct.pack_into('<i', memory, 12, -1)
        log.write(json.dumps(dict(event='error', error=str(error))) + '\n')
        kernel.SetEvent(handles[2])
    finally:
        memory.close()
        for handle in handles:
            kernel.CloseHandle(handle)
        log.close()
        # This process owns every GPU object. Avoid the driver's hanging NGX
        # shutdown; process teardown releases them without delaying the game.
        kernel.GetCurrentProcess.restype = c.c_void_p
        kernel.TerminateProcess.argtypes = [c.c_void_p, c.c_uint]
        kernel.TerminateProcess(kernel.GetCurrentProcess(), 0)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode', choices=['probe', 'process', 'live'])
    p.add_argument('--root', required=True)
    p.add_argument('--mapping')
    p.add_argument('--input', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--width', type=int, default=640)
    p.add_argument('--height', type=int, default=480)
    p.add_argument('--style', type=int, default=1)
    p.add_argument('--temporal', action=argparse.BooleanOptionalAction, default=True)
    p.add_argument('--channel-order', choices=['rgb', 'bgr'], default='rgb')
    p.add_argument('--intensity', type=float, default=1)
    p.add_argument('--tone', type=float, default=1)
    p.add_argument('--structure', type=float, default=1)
    args = p.parse_args()
    if not all(np.isfinite(v) and 0 <= v <= 2 for v in (args.intensity, args.tone, args.structure)):
        p.error('strengths must be finite values between 0 and 2')
    if args.mode == 'live':
        if not args.mapping:
            p.error('live requires --mapping')
        return live(args)
    bridge = Bridge(args.root)
    try:
        info = bridge.info()
        print(json.dumps(info), flush=True)
        if args.mode == 'probe':
            return
        if not args.input or not args.output:
            p.error('process requires --input and --output')
        paths = sorted(args.input.glob('*.ppm')) if args.input.is_dir() else [args.input]
        if not paths:
            p.error('no input frames')
        args.output.mkdir(parents=True, exist_ok=False)
        timings = []
        motion = []
        history = History()
        for i, path in enumerate(paths):
            rgb = np.array(Image.open(path).convert('RGB').resize((args.width, args.height), Image.Resampling.NEAREST))
            reset = history.reset_for(rgb, not args.temporal)
            out, ms = bridge.process(rgb, reset, args.temporal, args)
            Image.fromarray(out).save(args.output / f'{i:06d}.png')
            if i == 0:
                Image.fromarray(rgb).save(args.output / 'original.png')
            timings.append(ms)
            motion.append(list(bridge.motion))
            print(json.dumps(dict(frame=i, milliseconds=ms, reset=reset, motion=motion[-1], changed=int(np.count_nonzero(out != rgb)))), flush=True)
        info.update(settings=vars(args) | {'input': str(args.input), 'output': str(args.output)},
                    milliseconds=timings, motion=motion, swap_rb=getattr(bridge, 'swap', False))
        (args.output / 'report.json').write_text(json.dumps(info, indent=2))
    finally:
        # The pinned bridge can hang in NGX shutdown on this driver. This
        # disposable CLI process owns no persistent GPU state; terminate it
        # after flushing results, bypassing DLL detach callbacks.
        if sys.exc_info()[1] is not None:
            print(str(sys.exc_info()[1]), file=sys.stderr)
        sys.stdout.flush()
        sys.stderr.flush()
        kernel = c.WinDLL('kernel32')
        kernel.GetCurrentProcess.restype = c.c_void_p
        kernel.TerminateProcess.argtypes = [c.c_void_p, c.c_uint]
        kernel.TerminateProcess(kernel.GetCurrentProcess(), 0 if sys.exc_info()[0] is None else 1)


if __name__ == '__main__':
    main()
