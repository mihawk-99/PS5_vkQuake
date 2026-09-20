#!/usr/bin/env python3
# PS5 vkQuake - fetch the title's own startup trace off the console.
#
#   python3 tools/fetch-trace.py [--output PATH] [--title TID] [--print]
#
# The title writes its startup trace to /app0/trace.txt inside its own folder
# (src/trace.cpp), which is where a console run explains itself: the driver's
# answers, the engine's own error text, and anything the process printed before
# it died. It is the only record of a run that the console's klog does not carry,
# because the title's stdout is redirected into it.
#
# This exists so that a run's trace becomes an artifact instead of a transcript.
# It writes the raw bytes to the ignored klog/ tree, which is what
# `python3 tools/evidence.py distil` reads to produce the committed record under
# evidence/. The trace is opened for append on the console, so the file holds
# every run since the folder was deployed; the tail is the newest run.
#
# The credential handling is tools/deploy-title.py's, imported rather than
# repeated, so there is one definition of where the console is and one of which
# title is being talked to.

from __future__ import annotations

import argparse
import importlib.util
import io
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from ps5_ftp import connect  # noqa: E402


def deploy_tool():
    """tools/deploy-title.py, imported by path.

    It is a script rather than a module, so its two shared helpers - where the
    console is, and which title is being deployed - are reached this way. The
    same import is how tools/run-title.sh reads them.
    """
    spec = importlib.util.spec_from_file_location(
        "deploy_title", ROOT / "tools" / "deploy-title.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def fetch(title: str, settings: dict) -> bytes:
    buffer = io.BytesIO()
    with connect(**settings) as ftp:
        ftp.retrbinary(f"RETR /data/homebrew/{title}/trace.txt", buffer.write)
    return buffer.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fetch the title's own startup trace off the console.")
    parser.add_argument("--output", type=Path,
                        help="where to write the raw capture "
                             "(default: klog/trace-<UTC stamp>.txt)")
    parser.add_argument("--title", help="the title id (default: the built one)")
    parser.add_argument("--print", dest="show", action="store_true",
                        help="print the whole trace, not just its tail")
    parser.add_argument("--tail", type=int, default=40,
                        help="how many trailing lines to print (default: 40)")
    args = parser.parse_args()

    tool = deploy_tool()
    title = args.title or tool.title_id()
    settings = tool.load_settings()

    try:
        raw = fetch(title, settings)
    except Exception as error:  # ftplib's errors are the connection's, not ours
        print(f"could not read {title}/trace.txt: {error}", file=sys.stderr)
        print("    (the title writes it on its first run; a folder that was "
              "deployed but never launched has none)", file=sys.stderr)
        return 1

    output = args.output
    if output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        output = ROOT / "klog" / f"trace-{stamp}.txt"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(raw)

    lines = raw.decode("utf-8", "replace").splitlines()
    try:
        shown = str(output.relative_to(ROOT))
    except ValueError:
        shown = str(output)
    print(f"==> fetch-trace: {title}/trace.txt -> {shown}")
    print(f"    {len(lines)} lines, {len(raw)} bytes")

    body = lines if args.show else lines[-args.tail:]
    if not args.show and len(lines) > len(body):
        print(f"    ... {len(lines) - len(body)} earlier lines not shown "
              f"(--print for all)")
    for line in body:
        print(f"    {line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
