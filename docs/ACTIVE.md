# What is happening right now

Volatile. Rewritten in place; the runs are in `docs/PHASE_LOG.md`, the plan is in
`docs/PLAN.md`.

## Where the port is

Milestone **M0**, and inside it the engine is up: vkQuake's own C compiles for
`x86_64-sie-ps5` and the SDL compatibility layer that makes that possible is
written and tested. Nothing has run on the console yet, and nothing links into a
title yet.

The three commits that got here are `550b64c` (the baseline imported and vkQuake
pinned at 1.36.0), `24d8345` (the RetroArch frontend stripped out) and `5017b08`
plus `943316f` (the engine archive, and the compatibility layer).

## What is built, and what proves it

`tools/build-vkquake-engine.sh` compiles 72 sources — the 71 of vkQuake's
non-platform engine that upstream's own `meson.build` names, plus
`platform/ps5/sdl_ps5.c` — into `build/vkquake/libvkquake_engine.ps5.a` (3.6M,
1914 defined symbols including `Host_Init` and `Cvar_RegisterVariable`). It needs
no environment set up: like the boilerplate's `tools/build.sh`, it resolves the
payload SDK from `.deps/native/ps5-payload-sdk` itself.

`tools/verify.sh format` and `tools/verify.sh unit` are green. The unit gate is 13
tests, including `tests/test_sdl_ps5.py`, which runs the compatibility layer
against the host's real pthreads.

## The ten files the port layer owes

Upstream's platform layer is excluded from the archive by name, and each exclusion
is recorded with what replaces it. All ten are still `unported`:

| Upstream file | What the port layer must provide |
| --- | --- |
| `gl_vidsdl.c` | The window, display modes, and the `VkSurfaceKHR` — on this console a `VkDisplayPlaneSurfaceKHR` through `VK_KHR_display` |
| `main_sdl.c` | The entry point and the client loop's delay |
| `sys_sdl.c`, `sys_sdl_unix.c` | The file handle table, the base directory, the performance counter, `Sys_FindFirst` over `ps5_directory` |
| `in_sdl.c`, `in_sdl2.c` (or `in_sdl3.c`) | The event loop, the gamepad, and the shared key-mapping and movement layer |
| `snd_sdl.c` (or `snd_sdl3.c`) | The seven `SNDDMA_*` functions, fed by `src/audio_ps5.cpp` |
| `pl_linux.c` | The window icon, the clipboard and the message box |

## Next

1. `platform/ps5/sys_ps5.c` and the entry point, so the title links and boots —
   that is M1, and it is the first thing a console run can prove.
2. `platform/ps5/vid_ps5.c`: vkQuake's `GL_InitInstance`/`GL_InitDevice` reached
   with a PS5 surface instead of an SDL one. Three call sites carry all of it —
   `SDL_Vulkan_GetInstanceExtensions`, `SDL_Vulkan_GetVkGetInstanceProcAddr` and
   `SDL_Vulkan_CreateSurface`.
3. `tools/build-title.sh` wired to consume the engine archive through
   `APP_STATIC_ARCHIVES`, so `make app` produces an `eboot.bin`.

## Open questions, and what would answer them

- **The driver's compute coverage.** vkQuake's texture upload, lightmap update,
  SSAO and screen effects are compute passes writing storage images, and
  `../PS5_Vulkan` does not implement storage images yet. The default texture path
  is a staging copy and is fine; the lightmap path has no CPU fallback and the
  plan is to give it one. Whether anything else breaks is answered by a frame,
  not by reading headers.
- **The ACO abort.** `../PS5_Vulkan` records a `aco::schedule_program` SIGFPE on
  the second compile of a signed-integer pixel shader in one process. Quake
  compiles many pipelines, so this may surface as an abort during bring-up rather
  than as a clean error.
- **`AGENTS.md` still describes the RetroArch project.** It is this repository's
  operating manual and its read order now points at `docs/PLAN.md` and this file,
  both of which exist; its description of what the project *is* does not.

## Blockers

None. Nothing above is waiting on anything but work.
