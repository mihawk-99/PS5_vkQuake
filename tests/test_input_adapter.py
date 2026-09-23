"""Exercise the production input adapter against the pinned engine's actual types."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class InputAdapter(unittest.TestCase):
    def test_pad_to_engine(self):
        with tempfile.TemporaryDirectory() as td:
            exe = str(Path(td) / 'input-test')
            subprocess.run(['cc', '-std=gnu11', '-O2', '-DTASK_AFFINITY_NOT_AVAILABLE',
                            '-Ivendor/vkQuake/Quake', '-Iplatform/ps5',
                            '-Ivendor/vkQuake/Windows/misc/include',
                            'tests/input_adapter_test.c', '-lm', '-o', exe], cwd=ROOT, check=True)
            subprocess.run([exe], cwd=ROOT, check=True, timeout=10)
