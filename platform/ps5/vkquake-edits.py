#!/usr/bin/env python3
"""Apply this port's edits to its copy of vkQuake, in vendor/.

    python3 platform/ps5/vkquake-edits.py --root vendor/vkQuake   apply, and report
    python3 platform/ps5/vkquake-edits.py --root vendor/vkQuake --check
                                                                 report without writing

Why this file exists, and why it is the second of the two shapes rather than the first.
docs/PLAN.md's invariant lets upstream's behaviour change in exactly two ways: a file under
platform/ that is compiled *instead of* an upstream file, or an edit applied to a copy by a
file under platform/. This is the second shape: the ten excluded files are replacements, and
everything else upstream compiles as written.

There are two edits, and both are named accommodations with a retirement trigger.

**1. The swapchain asks for what the surface reports.** vkQuake hardcoded
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT and never consulted
VkSurfaceCapabilitiesKHR::supportedUsageFlags. Vulkan requires the request to be a subset of
that set (VUID-VkSwapchainCreateInfoKHR-imageUsage-01276) and ../PS5_Vulkan reports
COLOR_ATTACHMENT alone -- it honours exactly what it advertises -- so the run that reached a
swapchain died in the driver's assertion (evidence/m2-swapchain/). Intersecting is the
specification's requirement rather than a workaround, so nothing here needs retiring: a driver
that proves TRANSFER_SRC for swapchain images turns vkQuake's screenshot copy back on by
itself. The one thing lost until then is the screenshot, and docs/ACTIVE.md says so.

**2. The pipeline layouts fit the four sets this driver binds, and OIT is dropped.**
ps5vk_descriptor_options refuses a descriptor set index at or beyond
VkPhysicalDeviceLimits.maxBoundDescriptorSets -- four here -- and vkQuake's world and md5
layouts name five sets. The set that goes is the MBOIT input-attachment set, because OIT is
the family this port drops: its composite subpass reads input attachments and renders into two
colour attachments, and ../PS5_Vulkan implements neither yet (R2 and its MRT item). With OIT
dropped, the bmodel instance block moves from set 4 to set 3, which is what Shaders/world.vert
declares. r_oit's default goes to 0 so a frame never selects the dropped family, and
tools/build-vkquake-shaders.sh compiles the oit/mboit *variants* as their base shader, because
a shader that declares a set the layout no longer holds is a mismatch, not a warning. The
symbols stay, so Shaders/shaders.h's own set is unchanged.

Retirement: when ../PS5_Vulkan implements the input-attachment descriptor type, subpass input
reads and renderings into more than one colour attachment, revert this edit list, restore
Shaders/world.vert's `set = 4`, and rebuild the shader variants with their OIT defines. The
port then has the WBOIT/MBOIT path it was written for.

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
    """One exact-match replacement in one upstream file.

    `count` is how many occurrences the text must have: more than one is how a
    replacement with several identical sites is expressed, and any other number is
    a revision that moved something, which fails the build rather than passing.
    """

    path: str
    before: str
    after: str
    why: str
    count: int = 1


EDITS = (
    Edit(
        path="Quake/gl_vidsdl.c",
        before=USAGE_BEFORE,
        after=USAGE_AFTER,
        why="the swapchain asks for a usage the surface does not report",
    ),
    Edit(
        path="Quake/gl_rmain.c",
        before='cvar_t r_oit = {"r_oit", "1", CVAR_ARCHIVE};\n',
        after=(
            '/* PS5 vkQuake: OIT is dropped while ../PS5_Vulkan implements neither subpass\n'
            ' * input reads nor renderings into more than one colour attachment. The cvar is\n'
            ' * still settable -- a user who turns it on gets the driver\'s own refusal rather\n'
            ' * than a wrong picture. platform/ps5/vkquake-edits.py names the retirement. */\n'
            'cvar_t r_oit = {"r_oit", "0", CVAR_ARCHIVE};\n'
        ),
        why="OIT is dropped: the frame must not select the pass family the layouts no longer hold",
    ),
    Edit(
        path="Quake/gl_rmisc.c",
        before=(
            "\t\tVkDescriptorSetLayout world_descriptor_set_layouts[5] = {\n"
            "\t\t\tvulkan_globals.single_texture_set_layout.handle, vulkan_globals.single_texture_set_layout.handle, vulkan_globals.single_texture_set_layout.handle,\n"
            "\t\t\tvulkan_globals.mboit_input_attachment_set_layout.handle, vulkan_globals.bmodel_instances_set_layout.handle};\n"
        ),
        after=(
            "\t\t/* PS5 vkQuake: four sets, which is what ../PS5_Vulkan binds. The MBOIT input\n"
            "\t\t * attachment set is the one that goes, and the bmodel instance block moves from\n"
            "\t\t * set 4 to 3, which is what Shaders/world.vert now declares. */\n"
            "\t\tVkDescriptorSetLayout world_descriptor_set_layouts[4] = {\n"
            "\t\t\tvulkan_globals.single_texture_set_layout.handle, vulkan_globals.single_texture_set_layout.handle, vulkan_globals.single_texture_set_layout.handle,\n"
            "\t\t\tvulkan_globals.bmodel_instances_set_layout.handle};\n"
        ),
        why="the world layout names five sets; the driver binds four, and OIT's set is the one dropped",
    ),
    Edit(
        path="Quake/gl_rmisc.c",
        before="\t\tvulkan_globals.world_pipeline_layout.mboit_input_attachment_set = 3;\n",
        after="\t\tvulkan_globals.world_pipeline_layout.mboit_input_attachment_set = -1;\n",
        why="the world layout no longer holds the input-attachment set",
    ),
    Edit(
        path="Quake/gl_rmisc.c",
        before=(
            "\t\tVkDescriptorSetLayout md5_descriptor_set_layouts[5] = {\n"
            "\t\t\tvulkan_globals.single_texture_set_layout.handle, vulkan_globals.single_texture_set_layout.handle, vulkan_globals.ubo_set_layout.handle,\n"
            "\t\t\tvulkan_globals.joints_buffer_set_layout.handle, vulkan_globals.mboit_input_attachment_set_layout.handle};\n"
        ),
        after=(
            "\t\t/* PS5 vkQuake: the same four-set rule as the world layout above. */\n"
            "\t\tVkDescriptorSetLayout md5_descriptor_set_layouts[4] = {\n"
            "\t\t\tvulkan_globals.single_texture_set_layout.handle, vulkan_globals.single_texture_set_layout.handle, vulkan_globals.ubo_set_layout.handle,\n"
            "\t\t\tvulkan_globals.joints_buffer_set_layout.handle};\n"
        ),
        why="the md5 layout names five sets; the fifth is OIT's input-attachment set",
    ),
    Edit(
        path="Quake/gl_rmisc.c",
        before="\t\tvulkan_globals.md5_pipelines[MAIN_RENDER_PASS_STANDARD][0].layout.mboit_input_attachment_set = 4;\n",
        after="\t\tvulkan_globals.md5_pipelines[MAIN_RENDER_PASS_STANDARD][0].layout.mboit_input_attachment_set = -1;\n",
        why="the md5 layout no longer holds the input-attachment set",
    ),
    Edit(
        path="Shaders/world.vert",
        before="layout (std430, set = 4, binding = 0) restrict readonly buffer vertex_instances_buffer\n",
        after="layout (std430, set = 3, binding = 0) restrict readonly buffer vertex_instances_buffer\n",
        why="the bmodel instance block moved into the set the input attachment held",
    ),
    Edit(
        path="Shaders/world.vert",
        before="layout (std430, set = 4, binding = 1) restrict readonly buffer instances_buffer\n",
        after="layout (std430, set = 3, binding = 1) restrict readonly buffer instances_buffer\n",
        why="its second binding moves with the first",
    ),
    Edit(
        path="Quake/gl_warp.c",
        before='cvar_t r_waterwarpcompute = {"r_waterwarpcompute", "1", CVAR_ARCHIVE};\n',
        after=(
            '/* PS5 vkQuake: the water warp defaults to the raster path, because\n'
            ' * ../PS5_Vulkan\'s compute path accepts exactly one declared binding and a\n'
            ' * storage buffer at that (ps5vk_compute.c), while this kernel reads a texture\n'
            ' * and writes a storage image in two sets. The raster path is the same effect\n'
            ' * through the strip pipeline (R6). Retire this when the driver\'s compute path\n'
            ' * takes the bindings the graphics path already takes. */\n'
            'cvar_t r_waterwarpcompute = {"r_waterwarpcompute", "0", CVAR_ARCHIVE};\n'
        ),
        why="the water warp prefers compute, which this driver's dispatch path cannot bind yet",
    ),
    Edit(
        path="Quake/r_brush.c",
        before="world_pipeline_layout.handle, 4, 1, &vulkan_globals.bmodel_instances_desc_set",
        after="world_pipeline_layout.handle, 3, 1, &vulkan_globals.bmodel_instances_desc_set",
        why="the draw binds the bmodel block where the layout now holds it (two of four sites)",
        count=2,
    ),
    Edit(
        path="Quake/r_world.c",
        before="world_pipeline_layout.handle, 4, 1, &vulkan_globals.bmodel_instances_desc_set",
        after="world_pipeline_layout.handle, 3, 1, &vulkan_globals.bmodel_instances_desc_set",
        why="the same bind in the world draw (the other two sites)",
        count=2,
    ),
)


def apply_one(source: Path, edit: Edit, write: bool) -> str:
    """Return 'applied', 'already' or 'missing' for one edit, writing when asked."""
    text = source.read_text(encoding="utf-8")
    if text.count(edit.before) == 0 and text.count(edit.after) == edit.count:
        return "already"
    if text.count(edit.before) != edit.count:
        return "missing"
    if write:
        source.write_text(text.replace(edit.before, edit.after), encoding="utf-8")
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
