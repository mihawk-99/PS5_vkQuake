"""Diagnostic counts, failure-path durability and allocator semantics.

Two cases left this file with the frontend. One checked that the XMB observer
hooks were inert in a normal build, and one drove the Vulkan image and idle
hooks through tests/memory_vulkan_test.cpp. The hooks, the menu they observed and
that test file are all gone, so what remains is the allocator's own behaviour:
the counts it keeps, and what survives its failure path.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class MemoryDiagnostics(unittest.TestCase):
    def test_counts_failure_logging_and_ownership(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'diagnostics-test'
            capture = Path(directory) / 'memory.log'
            subprocess.run(['c++', '-std=c++20', '-pthread', '-I.',
                            '-DPS5_MEMORY_DIAGNOSTICS', '-Wl,--wrap=clock_gettime',
                            'tests/memory_diagnostics_test.cpp',
                            'src/memory_ps5.cpp', 'src/memory_diagnostics.cpp',
                            '-o', str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary), str(capture)], check=True)
            text = capture.read_text()
            self.assertIn('session host-test', text)
            self.assertEqual(sum(line.startswith('sample ') for line in text.splitlines()), 1)
            self.assertIn('caller seq=', text)
            self.assertIn('ms=5000', text)
            self.assertEqual(text.count('failure op='), 5)
            self.assertIn('failure op=forced', text)
            final = next(line for line in text.splitlines() if line.startswith('final '))
            fields = dict(item.split('=') for item in final.split()[1:])
            for field in ('native_bytes', 'mapped_bytes', 'aligned_bytes',
                          'native_count', 'mapped_count', 'aligned_count'):
                self.assertEqual(fields[field], '0')
            self.assertEqual(fields['dropped'], '1')
            self.assertEqual(fields['failures'], '10006')
            self.assertEqual(fields['failure_records'], '5')
            self.assertEqual(fields['failure_suppressed'], '10001')
            rows = text.splitlines()
            # The first failure still names its owners, one group per route.
            owners = [dict(field.split('=') for field in line.split()[1:])
                      for line in rows if line.startswith('first-failure-caller ')]
            self.assertEqual(len(owners), 48)
            for route in range(3):
                group = [o for o in owners if o['route'] == str(route)]
                self.assertEqual(len(group), 16)
                self.assertEqual([int(o['bytes']) for o in group], list(range(1019, 1003, -1)))
                self.assertTrue(all(o['count'] == '1' for o in group))
            self.assertLess(capture.stat().st_size, 20000)
            self.assertEqual(fields['image_create'], fields['image_destroy'])
            self.assertEqual(fields['idle_failed'], '1')
