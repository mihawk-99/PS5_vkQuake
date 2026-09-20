"""The native directory reader, compiled on the host with its syscalls wrapped.

Split out of the inherited tests/test_platform_paths.py when the RetroArch
frontend code was stripped: that file held three cases, and two of them tested
the frontend (its browser roots and the application-path patch). This one tests
src/ps5_directory.cpp, which the vkQuake port keeps, so it survives the strip
with its subject unchanged.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class Ps5Directory(unittest.TestCase):
    def test_native_directory_records(self):
        with tempfile.TemporaryDirectory() as td:
            binary = str(Path(td) / 'directory-test')
            subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                            'tests/ps5_directory_test.cpp', '-o', binary,
                            '-Wl,--wrap=open', '-Wl,--wrap=close'], cwd=ROOT, check=True)
            subprocess.run([binary], cwd=ROOT, check=True, timeout=10)
