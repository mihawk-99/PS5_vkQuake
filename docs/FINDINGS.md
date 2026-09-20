# What was measured, and what it forces

Append-only. New entries go at the end with the date they were measured, and an
entry that a later measurement supersedes is marked as superseded rather than
rewritten. Each entry states what was measured, how, what it forces, and the
boundary it holds within — the revision or version it was true of.

---

## 2026-09-20: The PS5 target shares the host's ELF header, so no object can be checked one at a time

**Measured.** An object compiled through `tooling/prospero-clang18` for
`x86_64-sie-ps5` and an object compiled by the host's `cc` are indistinguishable
by their ELF headers: both are `ELF 64-bit LSB relocatable`, `Machine: Advanced
Micro Devices X86-64`, `OS/ABI: UNIX - System V`, `Type: REL (Relocatable file)`,
`Flags: 0x0`. Measured by compiling `build/vkquake/obj/wad.o` through the wrapper
and `build/spike/host.o` with `cc`, then reading both with `readelf -h`.

**Consequence.** The pipeline cannot ask "is this a target object?" of an object.
A host object that reaches the title's archive stages cleanly and fails only on
the console, which is the failure the RetroArch pipeline's invariant names from
the other side. What can be decided is that the *toolchain* is the target
toolchain, so `tools/build-vkquake-engine.sh` does that instead: the wrapper
passes `-femulated-tls`, the host compiler does not, and a file with thread-local
storage compiled through the wrapper must therefore define `__emutls_v.*`. The
engine does use TLS — `common.c`'s `com_token`, `com_filesize` and the `va()`
buffers are `THREAD_LOCAL` — so the canary tests the mechanism the engine depends
on and not a synthetic one. Verified both ways: `llvm-nm` on the target's
`common.o` shows `U __emutls_get_address` and six `__emutls_v.*` definitions, and
the same file compiled for the host shows none.

**Boundary.** The payload SDK v0.42 toolchain on this host, 2026-09-20. A future
SDK whose target changes its OSABI, or a host that is not x86-64, changes this.

---

## 2026-09-20: `__get_cpuid(7, ...)` returns zeroes on this toolchain, which silently disabled AVX2

**Measured.** In a program compiled with clang 22.1.8, `__get_cpuid(7, &eax, &ebx,
&ecx, &edx)` returns `ok=1` with all four registers zero, while
`__cpuid_count(7, 0, ...)` on the same machine returns `ebx=239c27eb`. The host's
`/proc/cpuinfo` lists `avx2` and `__builtin_cpu_supports("avx2")` is 1.

**Consequence.** `SDL_HasAVX2` in `platform/ps5/sdl_ps5.c` read `false` on a CPU
that has AVX2, and would have read false on the console's Zen 2 as well, turning
the engine's AVX2 paths off with nothing to show for it. The queries now use
`__cpuid_count` with an explicit subleaf. The test that was meant to catch this
checked only that the four answers were mutually consistent — SSE, SSE2 and AVX
true, AVX2 false, every implication holding — and passed on the broken code.
Consistency is not correctness: `tests/sdl_ps5_test.c` now also compares each
answer against `__builtin_cpu_supports`, an independent source for the same fact,
and that is the check that fails on the old code.

**Boundary.** clang 22.1.8, the toolchain the SDK's dispatcher resolves to on this
host. The finding is about one intrinsic's behaviour, not about the target.

---

## 2026-09-20: `src/video_ps5.cpp` is not a Vulkan driver, and the port cannot reuse it as one

**Measured.** `src/video_ps5.cpp` implements RetroArch's `video_driver_t` and
contains no Vulkan calls at all; the string "Vulkan" appears in it once, in a
comment. The pixels come from `src/display.cpp`, which opens `sceVideoOut`,
allocates 32 MiB of direct memory, registers two 1920x1080 RGBA8_SRGB buffers in
it with tiled addressing, writes into the mapped memory and presents with
`sceVideoOutSubmitFlip` after flushing the cache with `_mm_clflush`. Measured by
reading both files.

The Vulkan path in this repository is RetroArch's own upstream
`gfx/drivers/vulkan.c` and `gfx/drivers_context/khr_display_ctx.c`, patched in
place by `tools/apply-port-patches.py` to create a `VkDisplayPlaneSurfaceKHR`, and
presenting through the out-of-repo implementation in `../PS5_Vulkan`.

**Consequence.** The port inherits the *pattern* — a display-plane surface through
`VK_KHR_display`, and the four driver archives linked whole — and not a reusable
PS5 Vulkan module. The good news is where the seam is: vkQuake resolves every
Vulkan entry point through one pointer, `fpGetInstanceProcAddr`, which it obtains
from `SDL_Vulkan_GetVkGetInstanceProcAddr`, and `libps5vk.ps5.a` defines exactly
that symbol. Three SDL calls carry all of the platform-specific Vulkan work — the
instance extension list, that entry point, and surface creation. `display.cpp`
remains useful for bringing the shell up before the renderer works.

**Boundary.** The baseline as copy-pasted, and `../PS5_Vulkan` at its current
revision.

---

## 2026-09-20: The PS5 Vulkan driver is a Vulkan 1.0 device with one extension, and vkQuake asks for almost nothing more

**Measured.** In `../PS5_Vulkan`, `driver/ps5vk_physical_device.c` declares exactly
one device extension (`.KHR_swapchain = true`) and exactly one feature
(`.robustBufferAccess = true`); the device `apiVersion` is 1.0 while the instance
reports 1.3; there is one queue family with one queue and one memory type, host
visible and host coherent, at most 4 GiB. `maxColorAttachments` is 4,
`maxImageDimension2D` is 4096, and there is no `samplerAnisotropy`. Its own README
records that 53 of 179 required formats still miss a required feature bit, that no
CTS run has happened, and that storage images and texel buffers are not
implemented.

In `vkQuake/Quake/gl_vidsdl.c`, `VK_KHR_swapchain` is the only hard device
requirement (`device_extensions[32] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}`), and
every other extension, feature and optional capability is enabled only after
querying for it: FP16 shaders, subgroup operations, dedicated allocation,
present-wait and ray query each have a guard, and the core features it requests
are copied from what the device reports.

**Consequence.** The two fit for the core render path. What does not fit is
specific and bounded: vkQuake's default texture upload is
`TRANSFER_DST | SAMPLED` through a staging buffer, which the driver supports, but
its dynamic lightmap update is a compute pipeline writing a
`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` with no CPU fallback
(`R_FlushUpdateLightmaps`, `r_brush.c`), and storage images are the driver's Phase
D2. SSAO, screen effects and texture warp need them too, and those are
cvar-disableable. The decision recorded in `docs/PLAN.md` is to close the lightmap
gap on the engine side.

**Boundary.** `../PS5_Vulkan` as it stands on this date, and vkQuake 1.36.0
(`1b948e29`). Both will move.

## The stencil buffer is load-bearing, so the depth gap has no engine-side fix

**Measured.** vkQuake requires one of the two spec-mandated combined depth-stencil
formats and `../PS5_Vulkan` reports neither, which stops the port inside device
initialisation (`docs/PHASE_LOG.md`, the depth-stencil entry). The open question
that entry left was whether the port could route around it by accepting the
depth-only `VK_FORMAT_D32_SFLOAT` the driver does report. It cannot.

vkQuake uses the stencil functionally, for the sky occlusion trick, in
`QUAKE/gl_rmisc.c`. One pipeline rasterises sky geometry with
`colorWriteMask = 0`, `stencilTestEnable = VK_TRUE`, `compareOp = ALWAYS`,
`passOp = REPLACE` and `reference = 0x1` — it writes stencil and no colour
(`:3384-3395`). A second draws the skybox with `depthTestEnable = VK_FALSE`,
`depthWriteEnable = VK_FALSE`, `stencilTestEnable = VK_TRUE`,
`compareOp = EQUAL`, `writeMask = 0x0` and `reference = 0x1`, so the skybox
survives only where sky was actually rasterised (`:3424-3436`).

It is not one pipeline's local trick. The engine builds a parallel render-pass
set indexed by `MAIN_RENDER_PASS_STENCIL_CLEAR`, whose variants differ only in
stencil load and clear semantics, and pipeline creation for the sky, the world,
the OIT and the MBOIT passes loops over all of them.

What the engine does *not* do is use a separate `pStencilAttachment`: every
subpass names only `pDepthStencilAttachment` (`gl_vidsdl.c:1706-1749`), so the
driver's refusal of separate stencil attachments is not the code path that fails.
The format table is.

**Consequence.** Two things follow, and both change the plan.

First, there is no engine-side workaround worth having. A local patch letting the
engine accept depth-only `D32_SFLOAT` would leave the sky pass writing and testing
a stencil aspect that does not exist — either invalid at render-pass creation or
silently wrong on screen. Editing the renderer to emulate the trick some other way
would break the port's central invariant, that upstream sources stay unmodified
and the port describes the console to the renderer rather than rewriting the
renderer. So this is a genuine stop for M2's render-pass step, and its fix belongs
in `../PS5_Vulkan` or nowhere.

Second, the fix is larger than the format table. The driver has no stencil path at
all: `DB_STENCIL_INFO` is the constant "stencil disabled" word, the stencil read
and write bases and `DB_STENCIL_CLEAR` are written zero, `stencilTestEnable` is
refused at pipeline creation, and a stencil clear is refused by name. An earlier
entry in `docs/PHASE_LOG.md` estimated this at roughly twenty lines and suggested
ignoring the stencil aspect; that estimate was wrong and that entry is superseded.

**Boundary.** `../PS5_Vulkan` at `23bcea1`, vkQuake 1.36.0 (`1b948e29`).

**Requested.** `docs/PS5_VULKAN_REQUESTS.md`, R1, with the code paths, the
specification requirement and the acceptance test.
