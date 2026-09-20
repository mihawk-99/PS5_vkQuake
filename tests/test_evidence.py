"""The evidence gate must be able to tell a pass from a fail, and to say which run it read.

`tools/evidence.py` is the only gate that cannot be checked by rebuilding
something: it replays a record of what a console actually answered, and a
stranger is supposed to be able to read the committed record and agree. That
makes two properties worth holding down, because both fail silently.

The first is that `compare` actually fails. A gate whose needles never match
anything, or whose `must_not_contain` list is never populated, reports OK for a
capture that contradicts it, and the whole evidence directory becomes decoration.

The second is that `distil` records the right run. The title opens its trace for
append (`src/trace.cpp`), so the file holds every run since the folder was
deployed and the newest - the one being evidenced - is at the end. `--head`
would record the oldest run instead, which is why `--tail` exists; these tests
pin both, so a change that quietly restores head-only behaviour is caught.
"""
import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPEC = ROOT / "tools" / "evidence.py"

# A trace in the console's own shape: three appends, oldest first, with the
# newest run's conclusion last. The three runs are distinguishable by their
# verdicts, so a test can say which one was recorded.
THREE_RUNS = "\n".join([
    "OLDEST RUN: vkCreateInstance -> -9",
    "OLDEST RUN: gave up",
    "MIDDLE RUN: vkCreateDevice -> -3",
    "MIDDLE RUN: gave up",
    "NEWEST RUN: Vendor: AMD",
    "NEWEST RUN: Device: PS5 AGC GPU (ps5vk)",
    "NEWEST RUN: vkCreateDevice -> 0",
]) + "\n"


def evidence_module():
    spec = importlib.util.spec_from_file_location("evidence_under_test", SPEC)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Distil(unittest.TestCase):
    def setUp(self):
        self.module = evidence_module()
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.raw = Path(self.tmp.name) / "trace.txt"
        self.raw.write_text(THREE_RUNS, encoding="utf-8")
        self.out = Path(self.tmp.name) / "evidence"

    def distil(self, *extra):
        args = self.module.argparse.Namespace(
            raw=str(self.raw), step="s", note="", console_run="", revision="",
            command="", head=40, tail=0, must_contain=[], must_not_contain=[],
            human_check="")
        for item in extra:
            key, value = item
            setattr(args, key, value)
        saved = self.module.EVIDENCE
        self.module.EVIDENCE = self.out
        try:
            # distil prints the record it wrote, which is useful at a console and
            # noise in the gate; the tests read the file instead.
            with contextlib.redirect_stdout(io.StringIO()):
                self.module.distil(args)
        finally:
            self.module.EVIDENCE = saved
        return json.loads((self.out / "s" / "capture.json").read_text("utf-8"))

    def test_tail_records_the_newest_run(self):
        capture = self.distil(("tail", 3))
        self.assertEqual(capture["lines"], [
            "NEWEST RUN: Vendor: AMD",
            "NEWEST RUN: Device: PS5 AGC GPU (ps5vk)",
            "NEWEST RUN: vkCreateDevice -> 0",
        ])
        # The whole file is still accounted for: the record says how much it read.
        self.assertEqual(capture["raw_lines"], 7)

    def test_head_records_the_oldest_run(self):
        """The default, kept for captures that do not accumulate."""
        capture = self.distil(("head", 2))
        self.assertEqual(capture["lines"],
                         ["OLDEST RUN: vkCreateInstance -> -9",
                          "OLDEST RUN: gave up"])

    def test_tail_and_head_do_not_both_apply(self):
        """--tail wins, so a stale --head cannot truncate the newest run."""
        capture = self.distil(("tail", 2), ("head", 1))
        self.assertEqual(len(capture["lines"]), 2)
        self.assertTrue(all(line.startswith("NEWEST RUN") for line in capture["lines"]))


class Compare(unittest.TestCase):
    """compare is the gate, so its verdicts are the thing worth testing."""

    def setUp(self):
        self.module = evidence_module()
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)

    def write(self, step, lines, **expectation):
        directory = self.base / step
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "capture.json").write_text(
            json.dumps({"step": step, "lines": lines, "raw_capture": "klog/gone.txt"}),
            encoding="utf-8")
        (directory / "expectation.json").write_text(
            json.dumps({"step": step, "note": "", "must_contain": [],
                        "must_not_contain": [], "human_check": "",
                        **expectation}),
            encoding="utf-8")

    def compare(self):
        args = self.module.argparse.Namespace(directory=str(self.base))
        return self.module.compare(args)

    def test_a_met_expectation_passes(self):
        self.write("s", ["Vendor: AMD", "vkCreateDevice -> 0"],
                   must_contain=["Vendor: AMD", "vkCreateDevice -> 0"])
        self.assertEqual(self.compare(), 0)

    def test_a_missing_needle_fails(self):
        self.write("s", ["Vendor: AMD"], must_contain=["Vendor: Intel"])
        self.assertEqual(self.compare(), 1)

    def test_a_forbidden_needle_fails(self):
        """must_not_contain has to bite, or a regression can be recorded as a pass."""
        self.write("s", ["Vendor: AMD", "QUAKE ERROR: something broke"],
                   must_not_contain=["QUAKE ERROR"])
        self.assertEqual(self.compare(), 1)

    def test_an_empty_record_fails(self):
        """A capture that distilled to nothing must not read as OK."""
        self.write("s", [], must_contain=[])
        self.assertEqual(self.compare(), 1)

    def test_an_incomplete_step_fails(self):
        (self.base / "half").mkdir()
        (self.base / "half" / "capture.json").write_text("{}", encoding="utf-8")
        self.assertEqual(self.compare(), 1)

    def test_an_absent_raw_capture_is_not_a_failure(self):
        """klog/ is ignored and ages out; its absence says so without going red."""
        self.write("s", ["Vendor: AMD"], must_contain=["Vendor: AMD"])
        self.assertEqual(self.compare(), 0)


if __name__ == "__main__":
    unittest.main()
