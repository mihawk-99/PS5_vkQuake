# What is happening right now

Volatile. Rewritten in place; the runs are in `docs/PHASE_LOG.md`, the plan is in
`docs/PLAN.md`, the requests to the driver are in `docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**The title runs on the console and its Vulkan instance and device come up.**
Three console runs have happened: the first died in `Sys_Init` on `getcwd`, which
the SDK declares and the runtime does not provide; the second got through `main`,
`Sys_Init` and `Host_Init` and died in `W_LoadWadFile` because the game data had
never been deployed; the third reached the driver:

```
Vendor: AMD
Device: PS5 AGC GPU (ps5vk)
vkCreateDevice -> 0
Device extensions: VK_KHR_swapchain

QUAKE ERROR: Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format
```

So M2's instance and device halves are done, and M2's render-pass step is stopped
by a gap in `../PS5_Vulkan` rather than by anything here — see **Blockers**.

**A signed title exists and every gate is green.** `dist/PPSA99010/eboot.bin`,
23.8 MB. vkQuake's own sources are unmodified: its 5036-line Vulkan backend
compiles and links as upstream wrote it, because the port describes the console
to the renderer rather than editing the renderer. The frontend is gone: `src/`
holds only PS5 platform code, and the RetroArch documentation is preserved
unchanged under `docs/inherited/`.

## The title's identity, and its data

`sce_sys/param.json` is the single source for all of it — `build.sh` validates it,
`build-title.sh` reads `titleId` from it for the `dist/` path, and
`deploy-title.py` reads the signed copy on the console's side. Applied with the
boilerplate's own initializer, `make init TITLE_ID=PPSA99010 APP_NAME=vkQuake`.

| Field | Value |
| --- | --- |
| `titleId` | `PPSA99010` |
| `conceptId` | `99010` |
| `contentId` | `UP9000-PPSA99010_00-VKQUAKE000000000` |
| `titleName` | `vkQuake` |

The artwork is vkQuake's own `Misc/vkQuake_512.png`, which is already the
512x512 launcher icon. `pic0.dds`, `pic1.dds` and `snd0.at9` are absent rather
than replaced: conforming ones need a BC7 and an ATRAC9 encoder this host does
not have, and `tools/validate-assets.sh` requires both or neither.

The owner supplies `pak0.pak`; this repository never fetches, commits or stages
it. It lives at `id1/pak0.pak` — the relative path vkQuake's filesystem layer
looks for under the base directory — and becomes `/app0/id1/pak0.pak` on the
console. `.gitignore` covers it anywhere in the tree, and
`tests/test_game_data_ignored.py` checks that it does.

## What is built, and what proves it

`tools/build-vkquake-engine.sh` compiles 74 sources — 72 of vkQuake's engine from
upstream's own `meson.build`, plus the two platform files — into
`build/vkquake/libvkquake_engine.ps5.a`, defining `Host_Init`,
`Cvar_RegisterVariable`, `VID_Init` and `GL_EndRendering` among 1900-odd others.

`tools/verify.sh` is green on all five gates — `format unit build integration
evidence` — and the unit gate is 12 tests. The shaders and the embedded pak are
generated, not committed, because upstream generates them,
`tools/build-vkquake-shaders.sh` refusing to finish unless the symbols it
produced are exactly the set `Shaders/shaders.h` declares.

## src/, after the strip

| File | What it is |
| --- | --- |
| `display.cpp`, `.hpp` | VideoOut, direct memory, two buffers, present-and-wait |
| `trace.cpp`, `.hpp` | The startup trace, readable from the title folder |
| `ps5_directory.cpp`, `.h` | Directory enumeration without libc's denied `opendir` |
| `locale_shims.c` | The `_l` locale functions glslang and SPIRV-Cross use |
| `memory_ps5.cpp`, `memory_diagnostics.*` | Allocator routing past the libc heap |
| `audio_ps5.cpp` | AudioOut ring and worker thread; the `audio_driver_t` table is gone |
| `input_ps5.cpp`, `.h` | The pad, behind a small C surface in the console's numbering |

## The two device interfaces

`in_sdl.c`, `in_sdl2.c` and `snd_sdl.c` are the only upstream platform files still
excluded; `platform/ps5/ps5_input.c` and `ps5_audio.c` stand in for them, the
engine's `IN_*` and `SNDDMA_*` interfaces **deliberately doing nothing**. The
interesting half of both already exists — `src/input_ps5.cpp` reads the pad,
`src/audio_ps5.cpp` drives AudioOut — so what is missing is the key mapping and
the `dma_t` adapter, which are M4 and M5. They report absence rather than
inventing input or accepting samples a device would never drain.

## Next

One step: **settle the rest of M2 without the engine.** The driver's surface,
swapchain and present path is implemented and console-proven on its own (its C1),
so the 3840x2160 display-plane surface, the swapchain, and a presented cleared
frame can all be proven through *this* port's platform layer — the SDL shim,
`ps5_window.c` and the `vk_globals.c` loader — without waiting for the engine's
render passes, which the depth gap stops. A small standalone program that creates
the surface, creates the swapchain, clears and presents N frames, and reports what
each call answered, closes M2's remaining half with evidence this repository owns.

## Open questions

- **The driver's compute coverage.** vkQuake's lightmap update is a compute pass
  writing a storage image and `../PS5_Vulkan` has none; the default texture path
  is a staging copy and is fine. The plan is to close the lightmap gap on the
  engine side.
- **The ACO abort.** `../PS5_Vulkan` records a `aco::schedule_program` SIGFPE on
  the second compile of a signed-integer pixel shader in one process. Quake
  compiles many pipelines, so this may surface during bring-up.
- **The console trace is not a committed artifact.** The startup trace goes to
  `/app0/trace.txt` on the console and is read back over FTP; the failure quoted
  above was read that way. `tools/deploy-title.py` does not fetch it, so until it
  does, a run's trace is a claim rather than the evidence the gates expect.

## Blockers

**The driver reports no combined depth-stencil format, and vkQuake requires one.**
`../PS5_Vulkan` reports `DEPTH_STENCIL_ATTACHMENT_BIT` for neither
`VK_FORMAT_D24_UNORM_S8_UINT` nor `VK_FORMAT_D32_SFLOAT_S8_UINT` — the
specification requires at least one — while vkQuake accepts nothing else and
aborts device initialisation. There is no engine-side fix worth having: the
stencil is functionally used for the sky occlusion trick, and the driver has no
stencil path at all. `../PS5_Vulkan` is maintained separately and is read-only
from here, so this is reported in `docs/PS5_VULKAN_REQUESTS.md` R1 rather than
patched. It blocks the engine reaching its render passes; it does not block the
surface, swapchain and present work above.
