"""Run the SDL compatibility layer against the host's pthreads.

The subject is platform/ps5/sdl_ps5.c, which the test includes rather than links:
it is C, the test is C, and what is being checked is the behaviour of the real
code and not a restatement of it. The timeout is generous because two of the
checks sleep on purpose, and a short one would turn scheduler noise into a
failure - but it is present, because the recursive-mutex check is exactly the
kind of mistake that hangs instead of failing, and a hung test is worse than a
red one.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class SdlPs5(unittest.TestCase):
    def test_compatibility_layer(self):
        with tempfile.TemporaryDirectory() as td:
            binary = str(Path(td) / 'sdl-ps5-test')
            subprocess.run(['cc', '-std=gnu11', '-O1', '-pthread', '-Wall', '-Wextra', '-Werror',
                            'tests/sdl_ps5_test.c', '-o', binary], cwd=ROOT, check=True)
            result = subprocess.run([binary], cwd=ROOT, check=True, capture_output=True,
                                    text=True, timeout=60)
            # The checks print what they proved, so a passing run says what it
            # covered rather than only that nothing failed.
            self.assertIn('mutex is recursive', result.stdout)
            self.assertIn('all checks passed', result.stdout)
