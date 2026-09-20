"""Quake's game data must never be committable.

pak0.pak is retail content. This repository does not ship it, does not fetch it
and must never commit it, so the rule that keeps it out is checked rather than
trusted: .gitignore is a text file, and a text file that stops matching - because
an entry was reordered, a directory was renamed, or a negation was added above it
- takes the pak with it into the next commit, where it cannot be taken back.

The check is `git check-ignore` on the paths game data can plausibly occupy, plus
a scan of the index for anything pak-shaped that is already tracked. The second
half matters more than the first: an ignore rule does nothing for a file that was
committed before the rule existed, and `git add -f` overrides the rule entirely.
"""
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent

# Where the data lands in practice, and where it would land by accident. The
# development copy sits at id1/pak0.pak beside the engine, because that is the
# path vkQuake's own filesystem layer looks for (GAMENAME "id1"); the rest are
# the places a person or a build step might drop it.
CANDIDATE_PATHS = [
    'pak0.pak',
    'pak1.pak',
    'id1/pak0.pak',
    'id1/pak1.pak',
    'some/deep/id1/pak0.pak',
]


class GameDataIgnored(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if shutil.which('git') is None:
            raise unittest.SkipTest('git is not available')
        probe = subprocess.run(['git', 'rev-parse', '--is-inside-work-tree'], cwd=ROOT,
                               capture_output=True, text=True)
        if probe.returncode != 0:
            raise unittest.SkipTest('not a git work tree')

    def test_game_data_paths_are_ignored(self):
        result = subprocess.run(['git', 'check-ignore', '--stdin'], cwd=ROOT,
                                input='\n'.join(CANDIDATE_PATHS), capture_output=True, text=True)
        ignored = {line.strip() for line in result.stdout.splitlines() if line.strip()}
        not_ignored = [path for path in CANDIDATE_PATHS if path not in ignored]
        self.assertEqual(not_ignored, [],
                         f'these paths would be committed: {not_ignored}')

    def test_no_game_data_is_tracked(self):
        # The ignore rule cannot help a file that is already in the index, so the
        # index is checked directly. Anything ending in .pak is game data.
        result = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, 'git ls-files failed')
        tracked = [line for line in result.stdout.splitlines()
                   if line.lower().endswith('.pak') or '/id1/' in line.lower()]
        self.assertEqual(tracked, [], f'game data is tracked: {tracked}')

    def test_the_pak_on_disk_is_invisible_to_git(self):
        # If the owner has supplied the data, it must not appear in status at all
        # - untracked, modified or otherwise. A missing file is also a pass: this
        # repository is expected to be buildable without it.
        result = subprocess.run(['git', 'status', '--porcelain', '--untracked-files=all'],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, 'git status failed')
        mentioned = [line for line in result.stdout.splitlines() if '.pak' in line.lower()]
        self.assertEqual(mentioned, [], f'game data shows up in git status: {mentioned}')

    def test_the_development_copy_is_where_the_engine_looks(self):
        # Not a requirement on a clean checkout - the data is the owner's to
        # supply - but if it is present, it is in the one place vkQuake's own
        # COM_AddGameDirectory("id1") will find it.
        pak = ROOT / 'id1' / 'pak0.pak'
        if not pak.is_file():
            self.skipTest('no game data on this machine, which is the normal case')
        header = pak.read_bytes()[:12]
        self.assertEqual(header[:4], b'PACK', f'{pak} is not a Quake pak')
        import struct
        _, offset, length = struct.unpack('<4sii', header)
        self.assertEqual(length % 64, 0, 'the pak directory is not a whole number of entries')
        self.assertGreater(length // 64, 0, 'the pak declares no entries')
        self.assertLessEqual(offset + length, pak.stat().st_size,
                             'the pak directory runs past the end of the file')
