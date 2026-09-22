#!/usr/bin/env python3
"""Apply this port's edits to its copy of vkQuake, in vendor/.

    python3 platform/ps5/vkquake-edits.py --root vendor/vkQuake   apply, and report
    python3 platform/ps5/vkquake-edits.py --root vendor/vkQuake --check
                                                                 report without writing

Why this file exists, and why it is the second of the two shapes rather than the first.
docs/PLAN.md's invariant lets upstream's behaviour change in exactly two ways: a file under
platform/ that is compiled *instead of* an upstream file, or an edit applied to a copy by a
file under platform/. This is the second shape, and the first edit that needed it — the ten
excluded files are replacements, and everything else upstream compiles as written.

The edit, and the run it came from. vkQuake sets the swapchain's imageUsage to
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT and never consults
VkSurfaceCapabilitiesKHR::supportedUsageFlags. Vulkan requires the request to be a subset of
that set (VUID-VkSwapchainCreateInfoKHR-imageUsage-01276), and ../PS5_Vulkan reports
COLOR_ATTACHMENT alone because rendering is the only use its swapchain images have been
proven for — it honours exactly what it advertises. The run that reached a swapchain died in
the driver's own assertion on that field (evidence/m2-swapchain/), which is what this edit is
written from.

Why the intersection rather than dropping the bit. Asking for what the surface reports is the
specification's requirement, not an accommodation, so nothing here needs retiring: when the
driver proves TRANSFER_SRC for swapchain images, this code picks it up and vkQuake's
screenshot path — which copies from the presented image (gl_vidsdl.c, vkCmdCopyImageToBuffer)
— works again with no change. Until then a screenshot is the one feature that loses its
source, and docs/ACTIVE.md names it.

Every edit is an exact-match replacement with a required occurrence count, so a vkQuake
revision that moves or rewrites the text fails this script loudly instead of compiling
something nobody has read.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

# What the port asks for and what the surface reports, one line in the swapchain's create
# info. The replacement keeps vkQuake's own two bits as the wish and intersects it with the
# surface's advertisement, which is what the valid-usage rule says to do.
USAGE_BEFORE = (
    "\tswapchain_create_info.imageUsage = "
    "VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;\n"
)
USAGE_AFTER = (
    "\t/* PS5 vkQuake: the surface reports what its swapchain images are proven for, and\n"
    "\t * Vulkan requires the request to be a subset of that (the valid-usage rule on\n"
    "\t * VkSwapchainCreateInfoKHR::imageUsage). Asking for TRANSFER_SRC regardless is\n"
    "\t * what stopped the swapchain on this console: ../PS5_Vulkan reports\n"
    "\t * COLOR_ATTACHMENT alone and honours exactly that. The intersection needs no\n"
    "\t * retirement -- a driver that proves TRANSFER_SRC for swapchain images, and so\n"
    "\t * advertises it, turns the screenshot copy back on by itself. */\n"
    "\tswapchain_create_info.imageUsage =\n"
    "\t\t(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &\n"
    "\t\tvulkan_surface_capabilities.supportedUsageFlags;\n"
)


@dataclass(frozen=True)
class Edit:
    """One exact-match replacement in one upstream file."""

    path: str
    before: str
    after: str
    why: str


EDITS = (
    Edit(
        path="Quake/gl_vidsdl.c",
        before=USAGE_BEFORE,
        after=USAGE_AFTER,
        why="the swapchain asks for a usage the surface does not report (R4's field, R3's run)",
    ),
)


def apply_one(source: Path, edit: Edit, write: bool) -> str:
    """Return 'applied', 'already' or 'missing' for one edit, writing when asked."""
    text = source.read_text(encoding="utf-8")
    if edit.after in text:
        return "already"
    if text.count(edit.before) != 1:
        return "missing"
    if write:
        source.write_text(text.replace(edit.before, edit.after, 1), encoding="utf-8")
    return "applied"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", required=True, help="the upstream tree, e.g. vendor/vkQuake")
    parser.add_argument("--check", action="store_true", help="report only; write nothing")
    args = parser.parse_args(argv)

    root = Path(args.root)
    if not root.is_dir():
        print(f"error: {root} is not a directory; run tools/fetch-vkquake.sh first", file=sys.stderr)
        return 2

    failed = 0
    for edit in EDITS:
        source = root / edit.path
        if not source.is_file():
            print(f"error: {edit.path} is not in {root}", file=sys.stderr)
            failed += 1
            continue
        state = apply_one(source, edit, write=not args.check)
        if state == "missing":
            # The text the edit matches is gone: a revision moved it, or somebody edited
            # the copy. Either way this port cannot claim the behaviour it depends on.
            print(
                f"error: {edit.path} does not hold the text this edit replaces ({edit.why}).\n"
                f"       The copy must be re-fetched, or the edit rewritten for the revision:\n"
                f"       platform/ps5/vkquake-edits.py, EDITS",
                file=sys.stderr,
            )
            failed += 1
        elif state == "already":
            print(f"    {edit.path}: already applied")
        else:
            verb = "would apply" if args.check else "applied"
            print(f"    {edit.path}: {verb} -- {edit.why}")

    if failed:
        return 1
    if args.check:
        print(f"==> [edits] {len(EDITS)} edit(s) present in {root}")
    else:
        print(f"==> [edits] {len(EDITS)} edit(s) applied to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
