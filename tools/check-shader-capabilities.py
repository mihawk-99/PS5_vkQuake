#!/usr/bin/env python3
"""Name the SPIR-V capabilities the deployed shaders declare.

    python3 tools/check-shader-capabilities.py

Why this exists. Every shader the engine builds is in `build/vkquake/generated`
as a C array of SPIR-V bytes, and the driver compiles those bytes at
`vkCreateGraphicsPipelines` time on the console. A capability the driver's
compiler cannot lower is not refused there: the fork's ACO reaches
`Unimplemented intrinsic` and aborts the whole title, minutes into start-up, with
no QUAKE ERROR to say which pipeline did it. Reading the declarations here costs
a second and names the shader before a run is spent.

`evidence/m2-subpass-input/` is the run that paid for this: 528 shader compiles
and 265 pipelines into start-up, then `SpvCapabilityInputAttachment` warned and
`acos_select_nir_intrinsics.cpp:5132` aborted, and the shader that did it was
`postprocess_frag` - the UI pass's second subpass, the one that writes the
swapchain image.

Capability numbers that matter so far:

    40  InputAttachment    `subpassLoad`; ../PS5_Vulkan warns and then aborts
    50  ImageQuery         `textureSize`; warned, compiled anyway (result=0)

A declaration here is not a failure by itself - the MSAA twins are declared and
never created. It is a list of what the driver will be asked to compile when the
matching pipeline is built.
"""

from __future__ import annotations

import glob
import os
import re
import sys

# Only the ones seen in vkQuake's shaders, so the table reads.
NAMES = {
    1: "Shader",
    35: "ImageGatherExtended",
    40: "InputAttachment",
    46: "StorageImageWriteWithoutFormat",
    49: "GroupNonUniform",
    50: "ImageQuery",
    61: "GroupNonUniformBallot",
    65: "GroupNonUniformShuffle",
    4472: "PhysicalStorageBufferAddresses (EXT)",
    5347: "PhysicalStorageBufferAddresses",
}

HEADER = 5  # magic, version, generator, bound, schema


def capabilities(path: str) -> list[int] | None:
    """The `OpCapability` operands of one generated shader, in order."""
    values = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", open(path).read())]
    words = [
        values[i] | values[i + 1] << 8 | values[i + 2] << 16 | values[i + 3] << 24
        for i in range(0, len(values) - 3, 4)
    ]
    if not words or words[0] != 0x07230203:
        return None
    out, i = [], HEADER
    while i < len(words):
        word_count, opcode = words[i] >> 16, words[i] & 0xFFFF
        if word_count == 0:
            break
        if opcode == 17 and word_count >= 2:  # OpCapability
            out.append(words[i + 1])
        i += word_count
    return out


def main() -> int:
    generated = sorted(glob.glob("build/vkquake/generated/*_spv.c"))
    if not generated:
        print("no generated shaders: run tools/build-vkquake-shaders.sh first")
        return 0

    interesting = 0
    for path in generated:
        caps = capabilities(path)
        if caps is None:
            print(f"{os.path.basename(path)}: not SPIR-V")
            continue
        beyond_shader = [c for c in caps if c != 1]
        if not beyond_shader:
            continue
        interesting += 1
        named = ", ".join(f"{NAMES.get(c, 'capability')} ({c})" for c in beyond_shader)
        print(f"{os.path.basename(path)}: {named}")

    print(f"\n{len(generated)} deployed shaders, {interesting} declaring more than Shader")
    return 0


if __name__ == "__main__":
    sys.exit(main())
