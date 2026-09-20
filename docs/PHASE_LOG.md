# What happened, run by run

Append-only. New dated entries go at the end. A run is recorded at the same
detail whether it passed or failed, and an entry is never rewritten — a later run
that contradicts an earlier one is a new entry that says so.

A "console run" means an artifact uploaded to the console, launched from the
console's own homebrew launcher, with `klog` captured for the whole run.
**Nothing in this file is a console run yet.** The entries below are host runs:
what was built and what it was built with.

---

## 2026-09-20: M0 — the contract

Four commits, each verified on the host before it was committed.

### The baseline, and the pin (`550b64c`)

The tree arrived as a copy of the PS5 RetroArch project with an empty `.git`; the
copy is missing `patches/`, `build/` and `OLD_PPSSPP/` from the original, none of
which this port needs (`patches/` holds libretro per-core patch sets). The copy
was committed as received, so that the strip in the next commit is a reviewable
diff and so the inherited material stays recoverable.

`tools/fetch-vkquake.sh` pins vkQuake at 1.36.0, verified against the commit the
tag points at (`1b948e29a6e3e412df2e1814615d71fb8040bce5`) rather than the tag's
name. 1.36.0 is the newest release; `master` was 88 commits ahead of it, and a
port wants a fixed target.

```
$ bash tools/fetch-vkquake.sh --check
pin:      1.36.0 (1b948e29a6e3e412df2e1814615d71fb8040bce5)
present:  true
revision: HEAD:1b948e29a6e3e412df2e1814615d71fb8040bce5

$ bash tools/fetch-vkquake.sh
==> [fetch] dropped its history and recorded the revision
$ bash tools/fetch-vkquake.sh --verify
==> [fetch] vendor/vkQuake is at the pinned revision
```

179 MB with history, 39 MB without it.

### The frontend strip (`24d8345`)

Six sources and seventeen test files removed. The unit gate was red before and
green after, because both failures were frontend tests — the direction is
inverted on purpose:

```
before:  Ran 74 tests ... FAILED (errors=2, skipped=8)
after:   Ran 12 tests in 4.950s
         OK
```

`tests/test_platform_paths.py` held three cases: two tested the frontend and one
tested `src/ps5_directory.cpp`. That case survives as `tests/test_ps5_directory.py`.

### The engine cross-compiles (`5017b08`)

`platform/ps5/SDL.h` is the SDL2 surface vkQuake's engine actually uses, and
`tools/build-vkquake-engine.sh` compiles upstream's own `meson.build` source list
minus ten named platform files.

```
$ bash tools/build-vkquake-engine.sh
==> [vkquake] compiled 69 sources for x86_64-sie-ps5
```

The script's first run failed, and usefully: the source list was read from the
base `srcs` array alone, which silently dropped `sys_sdl_unix.c` and `pl_linux.c`
— upstream appends those in the `else` branch of its Windows conditional. The
script now reads both and fails loudly if an excluded file is one upstream does
not name.

The format gate was red on arrival, on `src/thread_probe.cpp`, an inherited file
this port had not touched. It is reformatted there (28 reflowed lines, no
behaviour change) and the gate's scope is extended to `platform/`, which had been
the one directory of this project's own code the policy did not cover.

### The compatibility layer (`943316f`)

```
$ cc -std=gnu11 -O1 -pthread -Wall -Wextra -Werror tests/sdl_ps5_test.c -o build/sdl-ps5-test
$ ./build/sdl-ps5-test
sdl_ps5: the SDL compatibility layer, on the host
  mutex is recursive (three nested locks released)
  mutex excludes: 80000 increments from 4 threads, none lost
  condition: timeout answered non-zero after 25ms, signal answered zero
  threads: 200 joined with a status, 200 detached, no double free
  semaphore: wait, try-wait, post and value agree
  clock: 40ms sleep measured 40ms, counter monotonic, 14 cpus
  error string survives, and a refused call explains itself
  filesystem: /app0 for both path queries, RWops round-trips a file
  cpu features: SSE=1 SSE2=1 AVX=1 AVX2=1, and they match __builtin_cpu_supports
sdl_ps5: all checks passed

$ bash tools/build-vkquake-engine.sh
==> [vkquake] compiled 72 sources for x86_64-sie-ps5
==> [vkquake] toolchain canary: emulated TLS present, so PS5_CLANG is the target compiler
==> [vkquake] 3.6M, 72 objects
```

The CPU-feature line is the one that changed: it first read `AVX2=0` on a CPU that
has AVX2, because `__get_cpuid(7, ...)` returns zeroes on this toolchain. The test
that was supposed to catch it passed, because it checked only that the four answers
agreed with each other. See `docs/FINDINGS.md`.

### The documentation (`HEAD`)

`docs/PLAN.md`, `docs/ACTIVE.md` and `docs/FINDINGS.md` written for this project;
`docs/DEPLOYMENT.md`, `docs/TESTING.md` and `docs/TROUBLESHOOTING.md` moved up
from `docs/inherited/` unchanged; the rest of `docs/inherited/` removed.

Those three were written for the RetroArch title and still are: what carries over
is the console-side procedure — how the title's folder is reached, what `klog`
capture looks like, which failures are the console's rather than the code's. What
does not carry over is anything naming RetroArch, a libretro core, or the RGUI and
XMB menus. They are carried forward rather than rewritten because M1 needs the
console procedure and rewriting it before the first console run would be writing
down something unmeasured.

`tools/build-vkquake-engine.sh` now resolves the payload SDK from
`.deps/native/ps5-payload-sdk` itself, as the boilerplate's `tools/build.sh` does,
rather than requiring the caller to export `PS5_PAYLOAD_SDK`. Verified by running
it in a shell with neither `PS5_PAYLOAD_SDK` nor `PS5_CLANG` set: 72 sources,
3.6M, exit 0.

---

## 2026-09-20: M0 — the RetroArch material is removed

The tree is vkQuake's. What follows is what went, what stayed, and what stayed
only because deleting it would have destroyed work the port needs.

### Removed

Tracked: `assets/` (the XMB and RGUI menus' own artwork), `config/retroarch.cfg`,
`evidence/` (33 captures of RetroArch runs), `parked/`, `title/`, the two PPSSPP
plans at the root, sixteen RetroArch tools (`build-retroarch.sh`,
`retroarch-sources.sh`, `retroarch-flags.sh`, `apply-port-patches.py`,
`apply-runtime-probes.py`, `fetch-retroarch.sh`, the six per-core builders,
`check-core.py`, `core-imports.py`, `probe-xmb-allocations.py`,
`analyze-gpu-profile.py`), `tooling/{fbneo,genesis-plus-gx,mgba,ppsspp,snes9x}`
and `tools/check-memory-diagnostics.py`, which validated a RetroArch build down
to `build/ra/obj/menu_drivers_xmb.c.o` and a hardcoded `dist/PPSA99169`.

Sources: `src/video_ps5.cpp` (a RetroArch `video_driver_t` over `display.cpp`),
`src/main.cpp` (the entry point that built RetroArch's argv), `src/vulkan_trace.cpp`,
`src/menu_memory.h` and `src/memory_xmb.h` (XMB-only), and `src/thread_probe.cpp`,
which loads `ppsspp_libretro.so` and calls `retro_api_version` — frontend
diagnostics, not PS5 plumbing.

Untracked, and therefore permanent: `vendor/retroarch` (247M),
`vendor/retroarch-assets` (15M), `vendor/fceumm`, `work/`, `handoff/` and the
stale `dist/` output. `klog/` is kept: 1.6G of captured console runs that cannot
be regenerated. `handoff/PPSSPP_UPSTREAM_LIBRETRO_RESEARCH.md` went with it, and
the identical file is in `../PS5_RetroArch/handoff/`.

### Kept, and stripped of the frontend instead

The PS5 backends stay, because they are what the port reuses, and each lost its
RetroArch surface:

- `audio_ps5.cpp` — the `audio_driver_t audio_ps5` table and the
  `<audio/audio_driver.h>` include are gone, and its test now compiles with no
  include path beyond the repository root. It also stopped calling
  `ps5_frontend_build_identity()`, which `main.cpp` used to define.
- `input_ps5.cpp` — 536 lines to 311. The `input_driver_t` and
  `input_device_driver_t` tables, the translation onto `RETRO_DEVICE_ID_JOYPAD_*`
  and the `input_autoconfigure_*` announcements are gone; what is left is the pad
  behind `src/input_ps5.h`, which reports the console's own button bits and six
  axes. The mapping onto Quake's keys belongs to whoever drives the engine.
- `memory_ps5.cpp` — the XMB menu slab allocator (`find_menu`, `allocate_menu`,
  `ps5_menu_malloc`) is gone from `__wrap_free`, `__wrap_realloc` and the
  ownership check.
- `memory_diagnostics.cpp` — 439 lines to 335: the `XmbState` ring and the three
  `ps5_memory_xmb_*` observers.
- `display.cpp`, `display.hpp`, `trace.cpp`, `trace.hpp`, `ps5_directory.*`,
  `locale_shims.c` — unchanged except that their headers no longer call
  themselves RetroArch's, and the trace guard is `PS5_VKQUAKE_TRACE_HPP`.

### The documentation

`docs/inherited/` is restored, all fourteen files of it, exactly as the RetroArch
project had them, and the three that had been promoted into `docs/` went back.
`docs/` now holds this project's own four documents and nothing else. `AGENTS.md`
describes this project instead of the last one, and its read order points at the
inherited copies.

### The parts that are build work, not deletion

`tools/build-title.sh` was rewritten rather than trimmed: its step 1 built
RetroArch and six libretro cores and its include paths were RetroArch's headers,
because `src/`'s driver tables had to be laid out exactly as the frontend saw
them. That whole class of problem is gone with the driver tables. What the rewrite
keeps is the part that took failures to get right — the Vulkan archives linked
whole, the Mesa utility objects `tools/build-mesa-util.sh` compiles, the linker
flags, `make app` doing the actual link and signing, and the manifest recorded on
every build.

`Makefile` lost its five libretro core targets and gained `make engine`.
`tools/check-manifest.sh` lost its FCEUmm ABI and metadata checks.

The build gate is red, and expected to be: `tools/build-title.sh` reaches the link
and fails on undefined `VID_*`, `Sys_*`, `IN_*` and `SNDDMA_*`. That is M1.

```
$ bash tools/verify.sh format
lint-format: PASS
verify: PASS (format)
$ bash tools/verify.sh unit
Ran 12 tests in 3.7s
OK
verify: PASS (unit)
$ bash tools/build-vkquake-engine.sh
==> [vkquake] compiled 72 sources for x86_64-sie-ps5
==> [vkquake] toolchain canary: emulated TLS present, so PS5_CLANG is the target compiler
==> [vkquake] 3.6M, 72 objects
```

---

## 2026-09-20: The title's artwork is vkQuake's

`sce_sys/icon0.png` was RetroArch's launcher icon. It is now upstream vkQuake's
own `Misc/vkQuake_512.png`, copied unchanged: the launcher icon has to be a
512x512 PNG (`tools/validate-assets.sh`) and that file already is one, so nothing
was resampled on the way in.

Three assets were removed rather than replaced. `pic0.dds` and `pic1.dds` are the
home screen backgrounds; the validator wants both or neither, and a conforming one
must be a single 3840x2160 BC7_UNORM DX10 DDS with no mipmaps. This host cannot
produce that: ImageMagick 7.1.2 writes DXT5 whatever `dds:compression=bc7` asks
for, and there is no `texconv`, `compressonatorcli`, `nvcompress`, `bc7enc` or
`astcenc` installed. Measured, not assumed - the file it produced carried
`DXT5` at offset 84 with a legacy pixel format where the profile wants `DX10` and
format 98. `snd0.at9` went with them: ATRAC9 needs an encoder this host lacks, and
Quake ships no startup sound to convert.

$ bash tools/validate-assets.sh sce_sys
Presentation assets validated: sce_sys

---

## 2026-09-20: M2 — vkQuake's Vulkan backend compiles for the console, unmodified

The step the port turns on. `gl_vidsdl.c` - vkQuake's entire Vulkan renderer
backend, 5036 lines of instance creation, physical-device selection, feature
negotiation, swapchain building, frame recording and present - now compiles for
`x86_64-sie-ps5` with **no edit to the file**. It is in the engine archive:

```
$ bash tools/build-vkquake-engine.sh
==> [vkquake] compiled 74 sources for x86_64-sie-ps5
==> [vkquake] 3.7M, 74 objects
$ llvm-nm --defined-only build/vkquake/libvkquake_engine.ps5.a | grep -E ' T (VID_Init|GL_EndRendering)$'
0000000000000000 T GL_EndRendering
0000000000000000 T VID_Init
```

How, and why this is a milestone rather than a compile. The measure came first:
compiling the file with the error limit lifted gave **44 undeclared identifiers,
and every one of them a window, a display mode or a cursor**. Not one was Vulkan.
vkQuake asks SDL for five things - the instance extension list, the loader entry
point, a `VkSurfaceKHR`, the drawable size, and a library handle - and does
everything else itself in plain Vulkan. So the port describes the console to the
renderer rather than editing the renderer.

What the console answers, in `platform/ps5/SDL.h`, `SDL_vulkan.h` and
`ps5_window.c`:

- **One display, one mode: 3840x2160 at 60 Hz.** Not a chosen default.
  `../PS5_Vulkan` drives VideoOut at that size and its
  `ps5vk_CreateDisplayPlaneSurfaceKHR` asserts `pCreateInfo->imageExtent` equals
  it, so a surface at any other size is refused rather than scaled. So
  `SDL_Vulkan_GetDrawableSize` reports the screen's mode and not the size a window
  was asked to be - vkQuake compares the two against the surface's `currentExtent`
  in `GL_CreateSwapChain` and would otherwise build a swapchain the driver
  rejects.
- **The surface is a `VkDisplayPlaneSurfaceKHR`.** The sequence is the one
  RetroArch's `khr_display` context driver proved on this hardware - enumerate
  displays, planes and modes, pick the plane that can drive the display, create -
  collapsed to the shape the console has, but with the plural queries kept so a
  driver that grows a second display is described correctly rather than assumed
  away.
- **Everything resolves through the driver's own `vkGetInstanceProcAddr`**, which
  `libps5vk.ps5.a` defines. The driver is linked, not loaded: a PS5 title cannot
  dlopen a repository-built `.so`.

Coverage is checked rather than assumed. `gl_vidsdl.o` needs 28 SDL symbols; the
two shim objects define all 28, and the check is a `comm` of the two symbol lists
so a gap cannot hide:

```
$ comm -23 build/needs.txt build/have.txt     # needed, not provided
                                              # (empty)
```

The window token is not a window and is not pretending to be: it carries the
flags and the size a caller set, because upstream reads them back -
`VID_GetFullscreen` is `SDL_GetWindowFlags` masked with `SDL_WINDOW_FULLSCREEN`,
and the answer decides `modestate`.

### What M2 still needs

The backend compiles and is in the archive. It has not run: the title does not
link, because vkQuake's entry point and the seven remaining platform files are
still missing. How close those are, measured the same way:

| File | Errors | What it wants |
| --- | --- | --- |
| `sys_sdl.c` | **0** | already compiles |
| `sys_sdl_unix.c` | 1 | `SDL_OpenURL` |
| `pl_linux.c` | 1 | the window icon |
| `main_sdl.c` | 8 | `SDL_Init`, `SDL_Quit`, `SDL_GetVersion` |
| `snd_sdl.c` | 43 | the SDL audio-device API |
| `in_sdl.c` | 152 | the SDL gamepad and event API |

So the order is: `main_sdl.c` and the two `sys_sdl` files first, which is M1's
linking title; then the audio and input device APIs, which are M4 and M5 and are
the two real pieces of platform work left.

---

## 2026-09-20: M1 — the title links, and every gate is green

`dist/PPSA99010/eboot.bin`, 23,827,634 bytes, signed, container valid. The build
gate had never passed in this repository; all five do now.

```
$ bash tools/verify.sh
==> [shaders] compiled 67 shaders and packaged vkquake.pak
==> [vkquake] compiled 150 sources for x86_64-sie-ps5
==> [vkquake] 4.4M, 150 objects
==> [title] built dist/PPSA99010 (5 files, eboot.bin 23827634 bytes)
verify: PASS (format unit build integration evidence)
```

The linked binary carries the whole engine and the whole platform layer:

```
$ llvm-nm build/llvm-pie.elf | grep -E ' (main|Host_Init|VID_Init|GL_EndRendering|Sys_Init|IN_Init|SNDDMA_Init|R_CreatePipelines)$'
000000000066b980 t main
000000000066e640 t Host_Init
00000000006cb850 t VID_Init
00000000006ca520 t GL_EndRendering
000000000078c650 t Sys_Init
0000000000793740 t IN_Init
00000000007936d0 t SNDDMA_Init
000000000069f9d0 t R_CreatePipelines
```

They are local rather than global because the link uses
`tooling/native/app-symbols.map`, which is what a title wants: exactly one
exported entry point and no accidental ABI surface.

### The shaders and the pak, which upstream generates and does not commit

Two categories of symbol the link was missing, both produced by upstream's meson
at build time and neither committed: 132 `_spv` symbols and three `vkquake_pak`
ones. `Shaders/Compiled/` ships holding two empty directories.

`tools/build-vkquake-shaders.sh` reproduces the pipeline: `bintoc` and `mkpak`
built from upstream's own sources with the host compiler, then 67 jobs through
`glslangValidator` and `spirv-opt`, then a `.c` per shader and one for the pak.
The job list comes from upstream's `meson.build`, so a shader it adds or a variant
whose defines it changes appears here rather than failing at pipeline creation on
the console.

One real bug found in writing it, and one check that now prevents its return.
`Shaders/shaders.h` is committed and declares every blob the renderer looks for -
`DECLARE_SHADER_SPV(alias_vert)` declares `alias_vert_spv`. The first version of
the generator derived the symbol by stripping the extension and appending `_spv`,
which makes `alias.vert` and `alias.frag` both `alias_spv`; eight pairs of a .vert
and a .frag share a stem, and each pair silently kept only whichever compiled
last. The symbol is now the whole file name with every character outside
`[A-Za-z0-9_]` folded, and the generator compares its output against shaders.h's
own declaration list and fails on any difference in either direction - which is
the check that would have caught it.

### The seven libc functions the payload SDK declares and does not define

`platform/ps5/libc_shims.c`, the same shape as `src/locale_shims.c` and for the
same reason: a clean-room libc that declares a function in its headers and does
not carry it makes the compiler accept the call and the linker refuse it.
`getline` is implemented properly, because upstream uses it; `backtrace`,
`getpwuid`, `gethostbyaddr` and `hstrerror` answer with the absence that is true
on this console, and each says in the file why that answer is the one upstream
already handles.

### The deployed build, and the identity a run needs to be tied to its sources

The first deploy failed, usefully:

```
$ python3 tools/deploy-title.py
ValueError: eboot.bin must contain exactly one build identity; rebuild the title
```

`tools/build-title.sh` hashes the sources, the port layer, the build scripts and
the driver archives into one identity and writes it to `build/title_build_identity.h`
- and nothing referenced it any more. Its only consumers had been `src/main.cpp`,
which the frontend strip deleted, and `audio_ps5.cpp`, whose test report stopped
carrying it. So the string was computed and never compiled in.

`src/build_identity.cpp` puts it back, and the reason it belongs in `src/` rather
than the port layer is the reason it went missing: the identity covers the engine
archive, so the engine cannot also contain the identity without the two depending
on each other. `src/` is compiled afterwards, in step 2, from sources the identity
has already been computed over. It is a constructor, so the line is in
`/app0/trace.txt` before `main` runs, and the reference is what keeps the string
in the image at all.

Deployed:

```
$ python3 tools/deploy-title.py
==> [deploy] 5 files to /data/homebrew/PPSA99010/
    manifest.sha256                     331 bytes  babc8baea33e84ec  ok
    sce_module/libc.prx   the console keeps its own copy (1,335,962 bytes); ours is 1,284,674
    sce_sys/icon0.png                51,125 bytes  945b524f516f9899  ok
    eboot.bin                    24,182,844 bytes stored; all 1 of this build's markers present  ok
    sce_sys/param.json                  843 bytes  342378ed4428ab4a  ok
==> [deploy] published; the console's own runtime library was kept
```

The console keeping its own `libc.prx` is expected and is what the deploy tool
reports rather than a mismatch: the title runs against the console's copy.

### The first console run: the title loads, and says which build it is

After the deploy, `/app0/trace.txt` read back over FTP holds 81 bytes:

```
build identity: d3eadf328898dd4fd1b9b141f1f72125e24d737a2e3eb0d4d5947053279b43a0
```

That is exactly the identity of the deployed `dist/PPSA99010/eboot.bin`, so this
is a run of this build and not a leftover. It is the whole of the trace.

What it proves, and each of these was an open question an hour ago:

- The console's loader accepts the container and starts the title.
- The C runtime comes up and static constructors run - the identity is written by
  one, before `main`.
- `/app0` is writable by the title and the trace facility works.
- The deployed artifact is the one that ran.

What it does not prove, and cannot from this build: whether `main` ran, whether
`Host_Init` reached `VID_Init`, whether the Vulkan instance and device were
created, and whether anything was presented. The port layer traces nothing beyond
the identity - upstream's `main_sdl.c` and `gl_vidsdl.c` are unmodified and write
no trace - so a run that got all the way to a presented frame would leave exactly
this file.

No `klog` was captured for it: the newest capture under `klog/` predates the
deploy, so this run was launched from the console's own launcher rather than
through `tools/console-run.sh`, which is what starts the listener before launching
and attributes the lines after its mark.

Making the next run informative is the obvious next step and does not need
upstream to be patched: the Vulkan global trampolines in `platform/ps5/vk_globals.c`
are generated by this project, so they are a place a run can report
"vkCreateInstance returned 0", "vkCreateDevice returned 0" and so on - which is
precisely what M2 has to show.
