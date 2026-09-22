"""Run the loader's name aliasing against a driver of the console's shape.

The subject is platform/ps5/vk_loader.c, which the test includes rather than links:
it is C, the test is C, and what is checked is the behaviour of the real code and
not a restatement of it. The console this port runs on has no Vulkan loader, so the
alias a loader keeps for a promoted extension's entry point is this file's job -
and the run that found it died on the first frame that asked
(evidence/m2-loader/, "failed to find vkGetPhysicalDeviceProperties2").
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class VkLoader(unittest.TestCase):
    def test_alias(self):
        with tempfile.TemporaryDirectory() as td:
            binary = str(Path(td) / 'vk-loader-test')
            subprocess.run(['cc', '-std=gnu11', '-O1', '-Wall', '-Wextra', '-Werror',
                            'tests/vk_loader_test.c', '-o', binary], cwd=ROOT, check=True)
            result = subprocess.run([binary], cwd=ROOT, check=True, capture_output=True,
                                    text=True, timeout=60)
            # The checks print what they proved, so a passing run says what it
            # covered rather than only that nothing failed.
            self.assertIn('core vkGetPhysicalDeviceProperties2 resolves to the extension',
                          result.stdout)
            self.assertIn('a name with no extension twin is reported missing', result.stdout)
            self.assertIn('all checks passed', result.stdout)
