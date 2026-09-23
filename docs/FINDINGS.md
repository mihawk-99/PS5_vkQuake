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

## The driver's own audit classifies the clause it violates as "conditional"

**Measured.** Reading `../PS5_Vulkan` before sending it the stencil request turned
up why this gap survived: the driver's format audit cannot express it.

`tools/format_audit.py:140-151` walks the specification's required-format table
and splits every cell by its marker. A `{sym1}` cell is checked against the
reported table and filed as missing when the entry does not carry the feature. A
`{sym2}` or `{sym3}` cell is appended to `conditional` **without `carried` being
consulted at all**, and `main` returns `1 if args.check and missing else 0`
(`:171`) — so a conditional row cannot fail the gate. The tool's own output shows
the consequence: `VK_FORMAT_D32_SFLOAT` appears in the conditional list for
`VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` even though
`driver/ps5vk_image.c:442-446` carries that bit. A satisfied row and a violated
one are indistinguishable inside the bucket.

The reason it matters here is that the `DEPTH_STENCIL_ATTACHMENT` footnote in
`formats-v1.4.354.adoc` carries **two `must` clauses under one `{sym2}` marker**:
the bit "must: be supported for at least one of `VK_FORMAT_X8_D24_UNORM_PACK32`
and `VK_FORMAT_D32_SFLOAT`, and must: be supported for at least one of
`VK_FORMAT_D24_UNORM_S8_UINT` and `VK_FORMAT_D32_SFLOAT_S8_UINT`." The driver
satisfies the first through `D32_SFLOAT` and violates the second, because it
reports the bit for neither S8 format.

So the audit's headline "4 formats miss a required feature" is an undercount for
this footnote, and its three named obstacles — the descriptor type, the hardware's
fetch order and the compiler — do not include the fourth, the stencil registers
`docs/V0_FORMATS_AUDIT.md:238` quotes this row against.

**This is not an oversight in the driver's record.** `docs/V0_FORMATS_AUDIT.md:238`
carries the row, its reason and its closing path, and `:24` says conditional rows
are "listed as conditional so a conditional row is never mistaken for a closed
one". The classification is doing its job; what it cannot do is distinguish a
caveat from a disjunction, and this footnote is a disjunction. That is a tooling
gap, and it is separable from the driver work.

**A public source does not close the driver half.** ps5-opengl has no stencil path
either — its `src/` holds only `platform/` — so "nothing has recorded" holds for
it too. Mesa's `amdgfxregs.h` does carry the encodings, in the form the tree
already vendors at
`.deps/native/opengl-sdk/third_party/opengnm-psbc/src/amd/common/amdgfxregs.h`:
`DB_DEPTH_CONTROL` `0x028800` with `STENCIL_ENABLE` bit 0, `STENCILFUNC` bits 8-10
and the `V_028800_FRAG_*` compare enumeration (`:12877`); `DB_STENCIL_CONTROL`
`0x02842C` with `STENCILFAIL`/`STENCILZPASS`/`STENCILZFAIL` four bits each and the
`V_02842C_STENCIL_*` op enumeration (`:11487`); `DB_STENCILREFMASK` `0x028430` and
`_BF` `0x028434` (`:11523`, `:11537`); `DB_STENCIL_INFO` `0x028044` (`:10121`).

That is a partial lead, not the answer, and the distinction is worth keeping:
the driver does not address registers absolutely. It writes AGC register packets
in ps5-opengl's compacted numbering, where `DB_Z_INFO` is offset `0x010` and Mesa
puts the register at `0x028040`. Mesa therefore supplies field positions and
enumerations, not the offset mapping and not the measured enable word — the
analogue of the measured `0x80000183`.

**Boundary.** `../PS5_Vulkan` local `main` at `23bcea1`, one commit ahead of the
published `PS5Vulkan/main` (`2b494d2`) and unpushed, with uncommitted round-8
work in the tree. Also measured: the repository has two remotes with the same URL
(`PS5Vulkan` current, `main` 244 commits stale) and a `master` branch 308 behind.

## The title takes SIGSYS on its exit path, every run, and the console calls it a crash

**Measured.** The klog listener capture `klog/vkquake-listen-113523.log` holds
three runs of this title and **three identical SIGSYS records**. In each, the
kernel first records the process leaving through `exit()` and then reports a fatal
signal on a user thread:

```
# process pid=182, eboot.bin calls exit() exit_value=1.
#
# A user thread receives a fatal signal
#
# signal: 12 (SIGSYS)
...
# rip: 00000008000003ac  eflags: 00000246
#
# backtrace:
# 0000000000b8caab
# 0000000000ace520
# 0000000000a6ed7f
# 0000000000a6c09a
# 0000000000400190
```

All three records carry the **same rip**, `0x8000003ac`, which is inside
libkernel's syscall stubs rather than anywhere in the image. A repeated identical
instruction is a deterministic path, not a race.

The record for pid 182 is committed as `evidence/exit-sigsys/`, distilled from a
verbatim slice of the capture.

**It is not the error path's fault, and it is not `Sys_Error`'s.** The exit values
across the three runs are 1, 0 and 0 — the two runs that left through `exit(0)`
took the same signal at the same instruction as the one that left through
`exit(1)`. So the signal is on the exit path itself and fires whatever the exit
value is.

**What it costs.** The console answers a fatal signal the way it answers any
crash: `SCE_SHELL_UTIL_ERROR_APPLICATION_CRASH`, a coredump under
`devlog/system/sce_coredumps.0/PPSA99010_*/`, and a `gpudump.elf` run as part of
the same report pipeline. There is no GPU fault behind that — no fault message
appears anywhere in the capture, and the GPU dump is routine report tooling — but
the effect is that the klog of an ordinary run reads like a crash. The title's own
trace cannot show it: the trace ends at the message box, because the signal
arrives while `exit()` is unwinding, after everything the engine prints.

**This corrects an earlier entry.** The `W_LoadWadFile` run was recorded as dying
of SIGSYS *because* the game data was missing. Deploying `id1/pak0.pak` removed the
reason `Sys_Error` was called, which is why that run stopped failing — but the
signal itself is on the exit path and was never fixed. The earlier reading
attributed a symptom of the exit path to the reason for the exit. Both runs carry
`rip: 00000008000003ac`.

**Not symbolized, deliberately.** `build/title.map` is from a later build than the
binary that crashed — the build identity moved from `440199d7` to `c0677c1f`, and
the identity covers `src/`, `platform/ps5/`, three build scripts and the linked
archives. Symbolizing those five frames with a mismatched map is the failure mode
`tools/symbolize-crash.py` warns about in its own header, so the frames are
recorded raw. A re-run against a freshly built and deployed title is what makes
them readable.

**Boundary.** The listener capture `klog/vkquake-listen-113523.log` (not
committed; `klog/` is ignored), build identity `440199d7`, console clock
2026-01-14.

## The driver refuses a *layout*'s bindings, not a shader's — read 2026-09-22

`ps5vk_descriptor_options` (`../PS5_Vulkan/driver/ps5vk_pipeline.c:195-220`) iterates
the pipeline **layout** — `layout->set_layouts[set]`, every binding in each set — and
refuses one whose `stride` is 0, where the stride comes from
`ps5vk_descriptor_stride`'s switch over descriptor types. It never consults the shader's
used bindings. It is called from `ps5vk_CreateGraphicsPipelines` for the vertex stage
(`:1263`) and the fragment stage (`:1265`).

**What that forces.** A pipeline is refused for a descriptor type its *layout* names,
whether or not its SPIR-V mentions it. vkQuake's `basic_pipeline_layout` is
`{single_texture, mboit_input_attachment}` (`vendor/vkQuake/Quake/gl_rmisc.c`), and the
input-attachment layout is three `INPUT_ATTACHMENT` bindings at `FRAGMENT` stage — so
**every** pipeline built against it is refused, including the plain opaque
`basic_alphatest` that never reads one. The first `vkCreateGraphicsPipelines` vkQuake
makes (`R_CreateBasicPipelines`, the main-pass variant) is therefore refused, and the
driver's descriptor table has no entry for `INPUT_ATTACHMENT` (10), `SAMPLER` (0) or
`SAMPLED_IMAGE` (2) — the last two reachable only through this port's GUI and lightmap
compute layouts. Reported as R2 in `docs/PS5_VULKAN_REQUESTS.md`, with the predicted
refusal sentence in `docs/PHASE_LOG.md`.

**How it was found, so it is not re-derived:** by reading the driver's refusal path and
vkQuake's layout definitions together. No console run has produced the sentence yet; the
prediction is what the next run is for.

## `attachmentCount` is not a colour-attachment count — read 2026-09-22

`render_pass_create_info.attachmentCount = use_mboit ? … : use_wboit ? … : (resolve ? 3 : 2)`
(`vendor/vkQuake/Quake/gl_vidsdl.c`) counts **all** of a render pass's attachments, and
for the plain main pass those are one colour attachment plus one depth attachment:
`attachmentCount = 2` while `subpass_descriptions[0].colorAttachmentCount = 1`.

`resolve` is `sample_count != VK_SAMPLE_COUNT_1_BIT`, and `vid_fsaa` — the cvar that
raises the sample count (`:2150-2169`) — defaults to `"0"`. So with defaults the plain
pass has exactly one colour attachment. The two-colour shapes are the OIT variants,
selected at runtime by `r_oit`, which defaults to `"1"` = `OIT_MODE_WBOIT`: subpass 1
writes `accum` + `reveal` (`WBOIT_COLOR_ATTACHMENT_COUNT = 2`) and subpass 2 reads two
input attachments.

**What it forced.** A reading of the port's next gate that said `colorAttachmentCount > 1`
(MRT) was next. It is not: the refusal the port meets first is the descriptor one above,
and MRT restores OIT rather than unblocking the world draw — which is why OIT is being
dropped as a registered accommodation, with the driver's MRT item and input attachments
as its retirement trigger. A resolve attachment would make the plain pass two colour
attachments too, so MSAA keeps MRT on the list; it is off by default.

## 2026-09-23 — R17/R18 accepted; map advances to 32-bit indices

Driver R17 3be25f1 and R18 aafd697 are verified on PS5. R18 PID 214 has
531 PASS, zero FAIL: every pixel in 19 mip frames matches, and 21 command
streams replay exactly. The shared driver fixes row mip placement and pitch;
no texture scaling or visual settings change. Archive 14,434,994 bytes, SHA-256
cef1d81708d06d6fa68b2ac5df6b3f781c0fb59e3026e83e09ee469b112167fa.
Driver gates and 21 targeted host arms, this port's five gates/scan and the
template relink pass. Failed/partial driver captures are retained there.

Relinked identity b3aecd67729ccd91e5ec0fdeb9330e96a9d4e4923ad859aed3a5782fbcd061f1,
PS5 PID 215: presentation succeeds, demo1/the Necropolis reaches map recording,
and the descriptor-array and padded-pitch refusals are absent. The next named
refusal is ps5vk_cmd_draw: "32-bit indices need a runner probe". EndCommandBuffer
returns -13, exit 1 takes the known SIGSYS path. M6 remains open; input/audio
adapters are still stubs. No new visual acceptance is claimed.

Two final FTP trace reads match (SHA-256 167ed71a831d2b6c4ba636f3dd17503a69fa037162d2f19dc425088a9c800de4),
the kernel identifies PID 215, and count=0 is verified. Two deployed executable
reads match all five PT_LOAD segments. The shader cache survives this driver
fix: zero SPIR-V compiles/stores, 532 hits, eight internal NIR compiles. No new
startup timing was measured. Evidence: evidence/m2-r18-map-recording; all 26
captures replay with zero failures. Next: R19, shared 32-bit indexed draws with
correct byte offsets, bounds and index-size packets, then a hardware probe.

## 2026-09-23: concurrent diagnostics need one trace append lock

PID 221 interleaved audio stderr writes and presentation trace appends on the
mounted file. A shared mutex around trace appends and routing periodic audio
through that helper yields 27 complete reports in PID 222. This serializes
trace-helper callers; it does not promise atomicity for arbitrary engine stdio.
Both runs continuously feed nonzero PCM to native AudioOut with zero reported
output errors. See evidence/m5-audio-initial and m5-audio-profile-run.

## 2026-09-23 — allocation growth must respect the large-buffer route

R24's PNG encoder grows a native allocation past the 32 KiB threshold. Routing
only initial malloc/calloc requests left a 2.36 MB native buffer attempting to
grow to 3.54 MB inside the limited private heap. Always-on requested-size ownership
metadata permits safe migration without reading allocator internals. The same
screenshot fixture fails before the change and completes three 4K captures plus
normal exit in diagnostic and normal builds afterward. Foreign or untracked
pointers deliberately retain native realloc behavior. Evidence is under the four
m6-png-* captures; final diagnostic ownership-table drop count is zero.
