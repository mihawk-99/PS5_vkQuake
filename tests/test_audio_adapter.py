"""Exercise the production audio adapter against the pinned engine's actual types."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class AudioAdapter(unittest.TestCase):
    def test_engine_dma(self):
        with tempfile.TemporaryDirectory() as td:
            exe = str(Path(td) / 'audio-test')
            subprocess.run(['cc', '-std=gnu11', '-O2', '-DTASK_AFFINITY_NOT_AVAILABLE',
                            '-Ivendor/vkQuake/Quake', '-Iplatform/ps5',
                            '-Ivendor/vkQuake/Windows/misc/include',
                            'tests/audio_adapter_test.c', '-lm', '-o', exe], cwd=ROOT, check=True)
            subprocess.run([exe], cwd=ROOT, check=True, timeout=10)
