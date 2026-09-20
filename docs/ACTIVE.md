# What is happening right now

Volatile. Rewritten in place; the runs are in `docs/PHASE_LOG.md`, the plan is in
`docs/PLAN.md`.

## Where the port is

Milestone **M0**. The engine compiles for `x86_64-sie-ps5` and the SDL
compatibility layer that makes that possible is written and tested. Nothing has
run on the console, and the title does not link yet: vkQuake's entry point and the
ten platform files the port layer owes do not exist.

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

`tools/build-vkquake-engine.sh` compiles 72 sources — the 71 of vkQuake's
non-platform engine that upstream's `meson.build` names, plus
`platform/ps5/sdl_ps5.c` — into `build/vkquake/libvkquake_engine.ps5.a` (3.6M,
1914 defined symbols including `Host_Init` and `Cvar_RegisterVariable`). It needs
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

## The ten files the port layer owes

Excluded from the archive by name, each recorded with what replaces it. All ten
are still `unported`; `tools/build-vkquake-engine.sh --list` prints them.

`gl_vidsdl.c` (window, display modes, the `VkSurfaceKHR`), `main_sdl.c` (entry
point and the client loop's delay), `sys_sdl.c` + `sys_sdl_unix.c` (file handle
table, base directory, performance counter, directory scan), `in_sdl.c` +
`in_sdl2.c` (event loop, gamepad, key mapping), `snd_sdl.c` (the seven `SNDDMA_*`
functions), `pl_linux.c` (window icon, clipboard, message box).

## Next

1. The entry point and `sys_ps5.c`, so the title links and boots — that is M1, and
   the first thing a console run can prove.
2. `vid_ps5.c`: vkQuake's `GL_InitInstance`/`GL_InitDevice` reached with a PS5
   surface. Three SDL calls carry all of it — the instance extension list,
   `SDL_Vulkan_GetVkGetInstanceProcAddr`, and surface creation.

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
