"""Run the real audio backend against an explicitly clocked native output mock.

No include path beyond the repository root: src/audio_ps5.cpp was a RetroArch
audio_driver_t and needed the frontend's headers to compile, and the driver table
is gone, so the file is self-contained now. If this ever needs an -I again, that
is the signal that frontend coupling has come back.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class AudioPs5(unittest.TestCase):
    def test_native_backend(self):
        with tempfile.TemporaryDirectory() as td:
            binary = str(Path(td) / 'audio-test')
            subprocess.run(['c++', '-std=c++17', '-O2', '-pthread', '-Wall', '-Wextra', '-Werror',
                            'tests/audio_ps5_test.cpp', '-o', binary], cwd=ROOT, check=True)
            subprocess.run([binary], cwd=ROOT, check=True, timeout=15)
