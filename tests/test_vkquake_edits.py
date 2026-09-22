"""Apply the port's edits to a copy of vkQuake, and fail loudly when the text moves.

The subject is platform/ps5/vkquake-edits.py, which is the second of the two shapes
docs/PLAN.md allows for changing upstream's behaviour — an edit applied to a copy. Two
things about it are worth a test rather than a console cycle:

  - it must be idempotent, because every build applies it and a build that re-applied an
    edit would corrupt the copy;
  - it must refuse to build when the text it matches is gone, because that is the failure
    that would otherwise compile something nobody has read — and a vkQuake revision bump
    is exactly when it happens.

The edits are exercised against a copy of the real text in a temporary directory, so the
test never touches vendor/.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / 'platform' / 'ps5' / 'vkquake-edits.py'

sys.path.insert(0, str(SCRIPT.parent))
import importlib.util

spec = importlib.util.spec_from_file_location('vkquake_edits', SCRIPT)
vkquake_edits = importlib.util.module_from_spec(spec)
# The module has to be registered before it runs: its dataclasses resolve their own
# module through sys.modules, and an unregistered one fails there rather than here.
sys.modules['vkquake_edits'] = vkquake_edits
spec.loader.exec_module(vkquake_edits)


def run(root: Path, *extra: str) -> subprocess.CompletedProcess:
    return subprocess.run(['python3', str(SCRIPT), '--root', str(root), *extra],
                          cwd=ROOT, capture_output=True, text=True, timeout=60)


class VkQuakeEdits(unittest.TestCase):
    def fake_tree(self) -> Path:
        """A copy of vkQuake holding exactly the text the edits match."""
        root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, root, ignore_errors=True)
        for edit in vkquake_edits.EDITS:
            source = root / edit.path
            source.parent.mkdir(parents=True, exist_ok=True)
            existing = source.read_text(encoding='utf-8') if source.is_file() else ''
            source.write_text(existing + 'before\n' + edit.before * edit.count + 'after\n',
                              encoding='utf-8')
        return root

    def test_applies_once_and_is_idempotent(self):
        root = self.fake_tree()
        first = run(root)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertIn('applied', first.stdout)

        for edit in vkquake_edits.EDITS:
            text = (root / edit.path).read_text(encoding='utf-8')
            self.assertIn(edit.after, text)
            self.assertNotIn(edit.before, text)
        # The swapchain's wish survives inside the intersection, so that edit is the
        # specification's requirement rather than a removal of vkQuake's own intent.
        vidsdl = (root / vkquake_edits.EDITS[0].path).read_text(encoding='utf-8')
        self.assertIn('VK_IMAGE_USAGE_TRANSFER_SRC_BIT', vidsdl)

        second = run(root)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertIn('already applied', second.stdout)
        for edit in vkquake_edits.EDITS:
            self.assertIn(edit.after, (root / edit.path).read_text(encoding='utf-8'))

    def test_check_writes_nothing(self):
        root = self.fake_tree()
        result = run(root, '--check')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('would apply', result.stdout)
        for edit in vkquake_edits.EDITS:
            self.assertIn(edit.before, (root / edit.path).read_text(encoding='utf-8'))

    def test_missing_text_fails_the_build(self):
        """A revision that moves the text must stop the build, not compile past it."""
        root = self.fake_tree()
        path = root / vkquake_edits.EDITS[0].path
        path.write_text('a revision that rewrote the line\n', encoding='utf-8')
        result = run(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn('does not hold the text this edit replaces', result.stderr)


if __name__ == '__main__':
    unittest.main()
