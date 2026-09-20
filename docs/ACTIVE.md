# What is happening right now

Volatile. Rewritten in place; the runs are in `docs/PHASE_LOG.md`, the plan is in
`docs/PLAN.md`.

## Where the port is

Milestone **M2**, on the compile half of it. vkQuake's engine compiles for
`x86_64-sie-ps5`, and so does its **entire Vulkan backend** — `gl_vidsdl.c`, 5036
lines, unmodified — because the port describes the console to the renderer instead
of editing it. Nothing has run on the console, and the title does not link yet:
vkQuake's entry point and seven platform files are still missing.

The frontend is gone. `src/` holds only PS5 platform code, the RetroArch tools,
assets, vendor trees, evidence captures and PPSSPP plans are deleted, and the
RetroArch documentation is preserved unchanged under `docs/inherited/`.

## The title's identity

`sce_sys/param.json` is the single source for all of it — `build.sh` validates it,
`build-title.sh` reads `titleId` from it for the `dist/` path, and
`deploy-title.py` reads the signed copy on the console's side.

| Field | Value |
| --- | --- |
| `titleId` | `PPSA99010` |
| `conceptId` | `99010` |
| `contentId` | `UP9000-PPSA99010_00-VKQUAKE000000000` |
| `titleName` | `vkQuake` |

Applied with the boilerplate's own initializer: `make init TITLE_ID=PPSA99010
APP_NAME=vkQuake`.

The artwork is vkQuake's. `sce_sys/icon0.png` is upstream's own
`vendor/vkQuake/Misc/vkQuake_512.png`, which is already the 512x512 PNG the
launcher icon has to be — no conversion, so nothing was resampled on the way in.
The RetroArch title's `pic0.dds`, `pic1.dds` and `snd0.at9` are gone.

`pic0.dds` and `pic1.dds` are the home screen backgrounds, and they are optional:
`tools/validate-assets.sh` requires both or neither. They are absent rather than
replaced because a conforming one must be a single 3840x2160 BC7_UNORM DX10 DDS
without mipmaps, and this host has no BC7 encoder — ImageMagick 7.1.2 silently
writes DXT5 whatever `dds:compression=bc7` asks for, and no `texconv`,
`compressonatorcli` or `nvcompress` is installed. Shipping RetroArch's artwork in
a vkQuake title would be worse than shipping none, and a DXT5 file would fail the
validator on the next build rather than on the console. `snd0.at9` went the same
way: it needs an ATRAC9 encoder this host does not have, and Quake has no shipped
startup sound to convert.

## The game data

The owner supplies `pak0.pak`; this repository never fetches, commits or stages
it. It lives at `id1/pak0.pak` — the relative path vkQuake's filesystem layer
looks for under the base directory, `COM_AddGameDirectory(GAMENAME)` with
`GAMENAME` `"id1"` — and becomes `/app0/id1/pak0.pak` on the console.

`.gitignore` covers `pak0.pak`, `pak*.pak` and `id1/` anywhere in the tree, and
`tests/test_game_data_ignored.py` checks that it does: `git check-ignore` on the
paths data can occupy, a scan of the index for anything pak-shaped already
tracked, and a requirement that the file on disk be absent from `git status`
entirely. The guard was verified by removing the rules and watching it go red.

## What is built, and what proves it

`tools/build-vkquake-engine.sh` compiles 74 sources — 72 of vkQuake's engine
from upstream's own `meson.build`, including its whole Vulkan backend, plus
`platform/ps5/sdl_ps5.c` and `platform/ps5/ps5_window.c` — into
`build/vkquake/libvkquake_engine.ps5.a` (3.7M), defining `Host_Init`,
`Cvar_RegisterVariable`, `VID_Init` and `GL_EndRendering` among 1900-odd others. It needs
no environment set up: like the boilerplate's `tools/build.sh`, it resolves the
payload SDK from `.deps/native/ps5-payload-sdk` itself.

`tools/verify.sh format` and `tools/verify.sh unit` are green. The unit gate is 12
tests, including `tests/test_sdl_ps5.py` (the compatibility layer against the
host's real pthreads), `tests/test_game_data_ignored.py` and
`tests/test_audio_ps5.py` (the AudioOut backend, now with no include path beyond
the repository root — if it ever needs one again, frontend coupling has returned).

`tools/verify.sh build` is **red, and expected to be**: `tools/build-title.sh`
reaches the link and fails on the undefined `VID_*`, `Sys_*`, `IN_*` and
`SNDDMA_*` symbols. That is M1's work, not a regression.

## src/, after the strip

| File | What it is |
| --- | --- |
| `display.cpp`, `display.hpp` | VideoOut, direct memory, two buffers, present-and-wait |
| `trace.cpp`, `trace.hpp` | The startup trace, readable from the title folder |
| `ps5_directory.cpp`, `.h` | Directory enumeration without libc's denied `opendir` |
| `locale_shims.c` | The `_l` locale functions glslang and SPIRV-Cross are written against |
| `memory_ps5.cpp`, `memory_diagnostics.*` | Allocator routing past the libc heap, and its diagnostics |
| `audio_ps5.cpp` | AudioOut ring and worker thread; the `audio_driver_t` table is gone |
| `input_ps5.cpp`, `input_ps5.h` | The pad, behind a small C surface in the console's own numbering |

## The platform files the port layer still owes

Seven, excluded from the archive by name and printed by
`tools/build-vkquake-engine.sh --list`. `gl_vidsdl.c` is no longer among them:
it compiles, and the port answered it with a display model and a surface rather
than a rewrite.

How far each is, measured by compiling it with the error limit lifted:

| File | Errors | What it wants |
| --- | --- | --- |
| `sys_sdl.c` | **0** | already compiles |
| `sys_sdl_unix.c` | 1 | `SDL_OpenURL` |
| `pl_linux.c` | 1 | the window icon |
| `main_sdl.c` | 8 | `SDL_Init`, `SDL_Quit`, `SDL_GetVersion` |
| `snd_sdl.c` | 43 | the SDL audio-device API |
| `in_sdl.c` | 152 | the SDL gamepad and event API |

Those counts are the work list, and they say the remaining platform work is the
audio device and the pad — M5 and M4 — not the renderer.

## Next

1. The entry point and the two `sys_sdl` files, so the title links and boots —
   that is M1, and the first thing a console run can prove. `sys_sdl.c` already
   compiles and `sys_sdl_unix.c` needs one function.
2. Then the console run that closes M2: vkQuake's own `GL_InitInstance` and
   `GL_InitDevice` against the linked driver, a swapchain on the display-plane
   surface, and a cleared frame presented to VideoOut. Everything up to the
   console is in place; what is left is the link and the run.

## Open questions

- **The driver's compute coverage.** vkQuake's lightmap update is a compute pass
  writing a storage image and `../PS5_Vulkan` has none; the default texture path
  is a staging copy and is fine. The plan is to close the lightmap gap on the
  engine side. Whether anything else breaks is answered by a frame.
- **The ACO abort.** `../PS5_Vulkan` records a `aco::schedule_program` SIGFPE on
  the second compile of a signed-integer pixel shader in one process. Quake
  compiles many pipelines, so this may surface as an abort during bring-up.

## Blockers

None. `dist/` and `handoff/` no longer hold other titles, so `deploy-title.py`
will not refuse on ambiguity.
