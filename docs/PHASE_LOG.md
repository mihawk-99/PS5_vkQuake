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

### The crash, and what it was

The first run died with SIGSEGV. `tools/console-run.sh` was not used, so the
capture came from listening to the console's log while the title was launched, and
the console's own crash report is what it holds:

```
# signal: 11 (SIGSEGV)
# reason: page fault (user read instruction, page not present)
# fault address: 00000010002627e6
# rip: 00000010002627e6
# backtrace:
# 0000000000b8c6b1
# 0000000000a6ba5e
# 0000000000400190
```

Symbolized against `build/title.map`:

```
$ python3 tools/symbolize-crash.py klog/vkquake-klog-104144.log
  0x000000b8c6b1  Sys_Init +0x31
  0x000000a6ba5e  main +0xae
  0x000000400190  .llvm_jump_table_sizes +0x0
```

`Sys_Init +0x31`, and disassembling that address names the instruction exactly:

```
78c6a3:  mov    $0x3ff,%esi
78c6a8:  mov    %rbx,%rdi
78c6ab:  call   *0xead41f(%rip)        # 1639ad0 <getcwd>
78c6b1:  test   %rax,%rax              <-- the return address in the backtrace
```

**The port called `getcwd`, and `getcwd` was not there.** The fault is an
instruction fetch at an address outside the title's text, which is what an
indirect call through an unfilled GOT slot does (`-fno-plt` is one of the target
wrapper's flags).

Why the link accepted it, which is the part worth keeping:

- `getcwd` is **not** in the payload SDK's `libc.a` - 0 definitions, measured.
- It **is** in `$PS5_PAYLOAD_SDK/target/lib/libc_stub_weak.so` - 1 definition.
  The name is exact: that library exists so a link succeeds, and it is not a
  runtime implementation.
- The linked ELF carried `U getcwd`, so the call was resolved at load time, and
  the console's `libc.prx` does not export it either.

So a function the SDK declares, the link accepts, and the runtime does not have
becomes a crash on first call with nothing in the build to warn of it. This is the
same shape as the seven libc functions already shimmed, except those were caught by
the linker and this one was not.

### The fix, and the six beside it

`getcwd` now returns `/app0`, which is the true answer for a title - it is the
folder the console mounts the application at, and where `id1/pak0.pak`, the config
and the trace all live. Upstream uses it to set `host_parms->basedir`, so this is
what makes the game data resolve without patching upstream. The previous project on
this console met the same absence and answered it by patching
`fill_pathname_application_path` to a constant; a shim is the option available
here, because vkQuake is not ours to patch.

Six more went in with it, chosen because they are the same kind of function - the
ones a console libc omits because a console has no such thing - and because
vkQuake calls them: `getenv`, `setenv`, `putenv` and `unsetenv` as a small
in-process environment (`VID_Init` calls `putenv("SDL_VIDEO_CENTERED=center")` on
the way to the Vulkan path), `getprogname` as `eboot.bin`, and `gethostname` as
`ps5`.

Verified in the linked image: all twelve shims are defined (`t`) rather than
imported (`U`).

```
$ llvm-nm build/llvm-pie.elf | grep -E ' (getcwd|getenv|putenv|getline)$'
0000000000793700 t getcwd
...
$ bash tools/verify.sh
verify: PASS (format unit build integration evidence)
```

### The second console run: main, Host_Init, and then the game data

Captured on the klog listener while the title was launched. A different failure,
and much further in:

```
# signal: 12 (SIGSYS)
# rip: 00000008000003ac          <-- inside libkernel.sprx
# backtrace: 0xb8c4bb 0xb900a3 0xa6e6da 0xa6baaa 0x400190

$ python3 tools/symbolize-crash.py klog/vkquake-listen-104649.log
  0x000000b8c4bb  Sys_Error +0x24b
  0x000000b900a3  W_LoadWadFile +0x333
  0x000000a6e6da  Host_Init +0x6a
  0x000000a6baaa  main +0xfa
```

So the getcwd fix worked and the title went a long way past it: `main` ran,
`Sys_Init` completed, `Host_Init` ran, and the engine got as far as loading `gfx.wad`
out of the game data. It failed, `W_LoadWadFile` called `Sys_Error`, and the SIGSYS
is the console's answer to `exit()` - the same thing the RetroArch project recorded
("kernel exit(0) raises SIGSYS for this application"). The signal is the exit, not
the fault; the fault is the missing data.

**The game data had never been deployed.** `/data/homebrew/PPSA99010/` held the
five title files and no `id1/`. `id1/pak0.pak` is now there, 18,689,235 bytes
uploaded and its size read back.

Two things went in with it, because the run that found this could not say why:

- `src/trace.cpp` now points stdout and stderr at `/app0/trace.txt` from a
  constructor, before `main`. `Sys_Error` prints its message to stdout, and on this
  console that stream reaches nothing - so the console's report named the function
  that called `Sys_Error` and nothing about the reason. Appending and unbuffered,
  because a stream that prints and then exits loses a buffered line.
- `platform/ps5/vk_globals.c` now traces. The file is generated by this project, so
  it is a legitimate place to report what the driver answered: every Vulkan global
  that returns `VkResult` logs a failure, and the three that build the graphics
  stack - `vkCreateInstance`, `vkCreateDevice`, `vkEnumeratePhysicalDevices` - log
  both ways. "Reached it, and it worked" is what M2 has to show, and a run that
  only reports failures cannot show it.

### What the runtime does and does not provide, measured

`getcwd` was not a one-off, so the class was worth pinning down.
`tooling/native/libc_builder.cpp` builds the clean-room runtime, and it derives each
export's NID from its name:

    NID = base64(reverse(sha1(name + kNidSuffix)[0:8]))[:11], '/' -> '-'

`tooling/native/runtime/api-surface.txt` is that runtime's export set, so a name can
be turned into a NID and asked whether the runtime has it.
`tools/check-runtime-surface.py` does that, and self-checks against the console's
own verdicts: `malloc`, `printf`, `fopen` and `strlen` come back provided, and
`getcwd`, `getenv` and `putenv` do not.

The measurement that matters is what it says about the SDK's `.so` files. `getcwd`
is in `libkernel.so`, and it is in the imported set of this title *and* of the
RetroArch title that ran here for months - because importing a symbol and calling
one are different things, and that project patched its `getcwd` call site away. So
the SDK's modules are a statement about what links and the runtime manifest is a
statement about what runs, and only the second predicts a fault:

```
$ python3 tools/check-runtime-surface.py
build/llvm-pie.elf imports 2502 symbols
  clean-room runtime: 2566 exports, which is a statement about what runs
  libkernel and Sce stubs: 7358 names, which is a statement about what links
  123 import(s) only a LINK STUB provides. getcwd was one of these: ...
```

Those 123 are candidates, not faults - `mmap`, `open` and `pthread_create` are
among them and plainly work, since the trace file was written. Eleven of them are
imported by this port and by no title that has run here (`closedir`, `execvp`,
`fork`, `freeaddrinfo`, `getaddrinfo`, `gethostbyname`, `getuid`, `opendir`,
`pthread_attr_getstacksize`, `raise`, `readdir`), which is a narrower list and not
a safer one: `getcwd` is not on it.

### The probe answered the opposite of the hypothesis, which is the useful answer

The run with `/app0/probe.txt` present wrote this to `/app0/trace.txt`:

```
probe: static pointer tables, read before main
probe: net_drivers at 1aa1ff0, count 2
probe:   [0].Init = b212c0
probe:   [1].Init = b23690
probe: net_landrivers at 1aa20f0, count 2
probe:   [0].Init = b26ae0
probe:   [1].Init = b27700
probe: end
```

**Every pointer is valid before main.** The relocations are applied, the module
writer carries them, and the two adjacent tables the static analysis could not
reconcile are both correct at that moment. The probe was built to distinguish
"the loader did not apply it" from "something zeroed it", and it settled on the
second.

It settled on it because of the crash that follows in the same run: `r12` is
`1aa20f0`, the address the probe read, and `r13` is 0 - the same slot - and the
call goes to zero. Net_landrivers[0].Init is correct before main and zero at
NET_Init.

So the question narrowed from "is the image relocated" to "what writes eight zero
bytes at 0x1aa2100 during Host_Init", and Host_Init's order is known:

  Mem_Init, Tasks_Init, Cbuf_Init, Cmd_Init, LOG_Init, Cvar_Init, COM_Init,
  COM_InitFilesystem, Host_InitLocal, W_LoadWadFile, Key_Init, Con_Init, PR_Init,
  Mod_Init, NET_Init

with the trace showing the run reaches Con_Init's "Console initialized." and dies
at NET_Init, so the writer is one of those.

`src/probe.c` now also exports `ps5_probe_watch`, which logs the same pointer, and
`platform/ps5/sdl_ps5.c` calls it on every `SDL_CreateMutex`. Upstream's host.c is
not this project's to edit, but the engine calls SDL throughout that sequence and
SDL is this project's shim, so the trace gets a sample per mutex between main and
the fault. The last sample before the crash names the step that did it.

### The pointer is never zeroed, so the call is not reading it

The run with the change-watching probe settled it, and settled it against the
reading of the crash that two rounds of analysis had been built on:

```
probe: net_landrivers at 1aa60f0, count 2
probe:   [0].Init = b26b80
probe: end
Command line:
Using SDL version 2.0.0
probe: watch net_landrivers[0].Init = b26b80
Detected 16 CPUs.
Initializing vkQuake 1.36.0
Host_Init
probe: watch net_landrivers[0].Init = b26b80     (x200, to the end)
getpwuid: Operation not permitted
Steam library not found.
Playing shareware version.
probe: watch net_landrivers[0].Init = b26b80
Console initialized.
```

**The value never changes.** It is correct before main and still correct at the last
sample, which is after `Con_Init` printed "Console initialized." Nothing zeroes it.

So the call is not reading that slot, and the disassembly must be being read
wrongly. For this build, whose map matches the crash this time:

```
$ python3 tools/symbolize-crash.py klog/vkquake-listen-110526.log
  0x000000b237d1  Datagram_Init +0xa1
  0x000000b20b47  NET_Init +0x207
  0x000000a6ea34  Host_Init +0x94
  0x000000a6bdda  main +0xfa
```

and the register dump pairs with it - `r12 = 1aa60f0`, which is exactly this
build's `net_landrivers`, and `r13 = 0`:

```
7237b1: lea 0xf82938(%rip),%r12   # 16a60f0 <net_landrivers>
7237cc: call *0x10(%r12,%r13,1)
7237d1: cmp $0xffffffff,%eax      <-- the return address in the backtrace
```

The probe reads `net_landrivers + 0x10` and gets `b26b80`. The call reads
`0x10(%r12,%r13,1)` with `r12 = net_landrivers` and `r13 = 0` and goes to zero. The
same address, in the same run, two answers. One of the two readings is wrong and
the disassembly is the more likely of them: the probe's arithmetic is three lines
of C, and the instruction's meaning depends on what `r12` holds *at the call*, which
the dump only shows after the fault.

The next instrument narrows it without needing to resolve that. `ps5_probe_watch`
now logs only when the value *changes*, and `src/memory_ps5.cpp` calls it from
`__wrap_malloc`. PR_Init and Mod_Init run between the last mutex and the crash and
create none, but they allocate constantly, so the allocator puts a sample inside
them - and because the probe is change-triggered, a trace that would otherwise be
thousands of identical lines stays readable whichever way the answer goes.

### Both readings are right, so the memory changes

The probe and the call read the same address in the same run and disagree, and every
step of both was re-checked against this build's own map rather than an older one.
The frame is `Datagram_Init +0xa1` and `Datagram_Init` is at 0x723760 in both
`build/title.map` and `llvm-nm`:

```
7237e1: lea 0xf82908(%rip),%r12   # 16a60f0 <net_landrivers>
7237fc: call *0x10(%r12,%r13,1)
723801: cmp $0xffffffff,%eax      <-- the return address in the backtrace
```

with `r12 = 1aa60f0` and `r13 = 0` in the register dump, so the call reads
`[1aa6100]` - the byte the probe reads and reports as `b26bb0`. `NET_Init` was read
too, in case it were the writer: it allocates sockets, allocates the message buffer,
registers three cvars and adds four commands, and never touches the table.

So the memory changes between the last sample and the call, and the remaining
question is which step does it. The repair is the instrument: the constructor now
records the correct value, and `ps5_probe_watch` compares against it on every
allocation, writes a line naming the change, and puts the value back. Nothing in the
engine writes that field - `Datagram_Init` writes `initialized` at +8 and
`controlSock` at +0xc - so a change is damage, and restoring it is what any caller
would have to do anyway.

One thing that broke in the process and is worth keeping: the two hooks are weak.
`src/memory_ps5.cpp` and `platform/ps5/sdl_ps5.c` are each compiled on their own by a
host test, and a strong reference to the probe made both untestable - the link
failed with the probe absent. A missing diagnostic should mean no diagnostic, not a
build error.

### The probe cannot tell "no change" from "not called"

The repairing watch ran and logged once - the value `b26d00`, correct, at Sys_Init's
mutex - and then nothing, and the run stopped in the same place. But that is the
same output the probe would produce if it were never called again: change-triggered
logging makes "the value never changed" and "the hook never fired" identical, and
they are the two answers that would send this in opposite directions.

So the watch now also logs every 256th call with a counter. If the samples continue
up to the crash, then the hook is firing that late and no change ever happened -
which would mean the call is not reading the word the probe reads, and the
disassembly is wrong in a way three rounds of checking have not found. If the
samples stop early, the allocator hook is not firing and the probe is looking at
nothing between the last mutex and the crash.

Either answer is the end of this ambiguity, which is what the last round cost.

### The backtrace was lying, because this port dropped a flag upstream insists on

The liveness counter settled what the probe could not: the hook fires, and the value
never changes.

```
probe: watch #1   net_landrivers[0].Init = b26d30
probe: watch #256 net_landrivers[0].Init = b26d30
probe: watch #512 net_landrivers[0].Init = b26d30
probe: watch #768 net_landrivers[0].Init = b26d30
Console initialized.
```

Seven hundred and sixty-eight samples through `Host_Init`, every one the same, and
the run still dies on a call to zero. So the call is not reading that word, the
probe is not wrong, and the frame - `Datagram_Init +0xa1` - is.

`UDP4_Init` is the function that call reaches, and it contains no tail calls, so the
walk should have found a frame inside it and did not. The reason is this project's
own build:

```
meson.build:9  # Always build keeping frame pointers to get better backtraces
meson.build:10 add_project_arguments(cc.get_supported_arguments('-fno-omit-frame-pointer', ...
```

`tools/build-vkquake-engine.sh` compiled the engine at `-O2` without it. A console
crash report is a frame-pointer walk, so with the frames omitted the walk lands
wherever the stack looks plausible - and it landed on the caller. Four rounds of
this port were spent proving a table was fine when the table was never the subject.

The flag is restored, with upstream's reason and this port's evidence for it in the
comment. A backtrace that names the wrong function is worse than no backtrace,
because it is believable.

### Frame pointers did not change the frame, so the frame was not the problem

Restoring `-fno-omit-frame-pointer` and re-running gave the identical backtrace:

```
  0x000000b23981  Datagram_Init +0xa1
  0x000000b20cf7  NET_Init +0x207
```

and the same registers - `r12 = 1aa60f0`, `r13 = 0` - against this build's own
disassembly, which still puts the call at `Datagram_Init +0xa1` reading
`0x10(%r12,%r13,1)`. The flag was worth restoring on its own merits and is upstream's
own choice, but it did not explain this.

So the reading stands and the probe stands, and the only way both hold is a write
with no allocation after it - the sampled watch logged every 256th call and the last
one was 256 calls before the fault. The watch now logs every call up to a bound, so
the last line before the fault is the value the call itself would have read. That
closes the last gap in the instrumentation: there is no longer a window in which a
write can hide.

What the addresses do confirm, from this run's own probe:

```
probe: net_drivers at 1aa5ff0, count 2
probe:   [1].Init = b238e0        == Datagram_Init, which is where the backtrace says we are
probe: net_landrivers at 1aa60f0, count 2
probe:   [0].Init = b26d30        == UDP4_Init, the call's target
```

The table, the code and the fault address all name the same thing. Only the value
disagrees.

### The window after Con_Init has no samples in it at all

Full-resolution logging settled the shape of the problem and pointed at the
instrument rather than the code. 846 samples, every one `b26d10`, the last at
`Console initialized.` - and then the call goes to zero.

Every address is confirmed against this build: `b26d10` is `UDP4_Init`, `b238c0` is
`Datagram_Init`, and the crash is that call at `Datagram_Init +0xa1`. The table, the
code and the fault address all name the same thing.

What the 846 samples do *not* cover is the window that matters. Every one came from
an allocation, and from `Con_Init` through `PR_Init`, `Mod_Init` and `NET_Init` to
the driver loop, the engine allocates through Quake's zone rather than through
malloc - `Mem_Alloc` takes from a pool `Mem_Init` allocated once. So there are no
samples in the window at all, and the last one is on the wrong side of it.

That is the fourth instrument in a row with a blind spot the size of the thing being
looked for: a change-triggered watch cannot tell no-change from not-called, a
256-call sample cannot see a write in the last 255 calls, an allocation-driven watch
cannot sample a window that does not allocate, and a backtrace without frame pointers
names the wrong function. Each was built to remove the previous one's blind spot and
introduced its own.

So the sampler is now independent of allocation: a detached thread, one millisecond
apart, logging only changes. If the value is ever wrong it says when, and if it is
never wrong then the call is not reading this word - which would need a different
instrument than any of these, not a better-tuned one.

### The sampler came back clean, which re-reads the whole backtrace

One line, `b26da0`, and never another. A millisecond-resolution sampler ran for the
whole life of the process and the slot held `UDP4_Init` from before `main` until the
crash. With the 846 allocation samples, there is no moment at which that word was
wrong.

So the call is not reading it, and the frame walker's answer finally reads correctly
instead of contradictorily: **a null `call` pushes no frame.** The walker follows rbp
chains, so the faulting instruction - a call to zero inside `UDP4_Init` - has no
frame of its own on the stack, and the innermost frame it can find is `UDP4_Init`'s
return address, which is into `Datagram_Init` at `+0xa1`. The symbolizer was reporting
the *caller* of the faulting function, exactly as it should, and the port spent four
rounds proving a table correct that was never the subject.

`UDP4_Init` reaches its socket calls through GOT slots - `__error`, `strerror`,
`getsockname`, `gethostbyname`, `socket`, `ioctl`, `bind` and the rest - and a weak
reference reads the same slot the call does. A symbol the runtime does not provide
reads back as NULL here for precisely the reason the call went to zero, so the probe
now checks all sixteen and names the missing ones.

### The null call was `gethostbyname`, and the port had shimmed its neighbours

The weak-symbol check from the probe named it in one line:

```
probe: MISSING gethostbyname
probe: socket symbol check done (16 calls)
```

`UDP4_Init` calls it through a GOT slot nothing fills:

```
Datagram_Init -> UDP4_Init -> call *GOT(gethostbyname) -> 0
```

This is the ninth function of the `getcwd` class - declared by the SDK, accepted by
the linker, absent at run time - and the port had shimmed eight of its neighbours
(`getcwd`, `getenv`, `setenv`, `putenv`, `unsetenv`, `getline`, `getpwuid`,
`backtrace`, `gethostbyaddr`, `gethostname`) and missed this one. The instrument that
found it took one line, and the four rounds before it went into a table that was
never wrong, because a null call pushes no frame and the frame walk correctly
reported the caller of the function that faulted.

`gethostbyname` now returns NULL with `HOST_NOT_FOUND`, which is what upstream
expects: `UDP4_Init` prints "gethostbyname failed" with the resolver's error and
carries on with the loopback address - which is what a console wants anyway.

The probe switch and the trace file were cleared afterwards, so the next run records
the engine's own output and nothing else.

### M2: vkQuake's Vulkan instance initialises on the console

The run after the `gethostbyname` shim is the one the whole port has been aimed at.

```
UDP4_Init: gethostbyname failed (host not found)
UDP4_OpenSocket: Permission denied
UDP4_Init: Unable to open control socket, UDP disabled
Server using protocol 999+ (FTE-RMQ)
Exe: 11:30:16 Sep 20 2026
SDL Video Driver: PS5 VideoOut

Vulkan Initialization
Using Vulkan 1.1
vkCreateInstance -> 0
Instance extensions:
 VK_KHR_surface
 VK_KHR_display
 VK_KHR_get_physical_device_properties2
```

Networking initialised and reported its failures gracefully - the shim working, and the
console refusing sockets as it will. `SV_Init` ran, the engine printed its banner,
our SDL shim answered the video-driver query, `VID_Init` was reached, and vkQuake's
own Vulkan instance came up against ../PS5_Vulkan with **result 0**. The extension list
is the one platform/ps5/SDL_vulkan.h hands it - surface, display, and the Mesa
runtime's properties2 - which is what the driver declares.

The crash that follows is in this project's own generated code, and it is the same
class of mistake as the last four rounds in one respect only: a call went to zero and
the console named the caller.

```
6cca56: mov  vulkan_instance,%rdi
6cca63: call vkEnumeratePhysicalDevices     <-- through the generated forwarder
6cca69: test %eax,%eax                      <-- VID_Init +0xbf9
```

The forwarder resolved the command with `vkGetInstanceProcAddr(NULL, ...)`, for all
seventy-nine of them. That is right for the four true globals and wrong for the rest,
and the driver says so in one line:

```c
ps5vk_GetInstanceProcAddr(VkInstance _instance, const char *pName)
{
   VK_FROM_HANDLE(ps5vk_instance, instance, _instance);
   return vk_instance_get_proc_addr(instance ? &instance->vk : NULL, ...);
}
```

With a null instance it answers only `vkCreateInstance`,
`vkEnumerateInstanceExtensionProperties`, `vkEnumerateInstanceLayerProperties` and
`vkEnumerateInstanceVersion`. `vkEnumeratePhysicalDevices` is instance-level, so it
resolved to NULL and was called.

`tools/gen-vk-globals.py` now emits the part of a loader a statically linked single-ICD
title actually needs: it remembers the instance from `vkCreateInstance` and the device
from `vkCreateDevice`, keeps the null-instance path for the four globals, and resolves
everything else through the instance first and the device second.

### M2's device half, and the first blocker that is a real one

The run with the loader fix is the furthest this title has been:

```
vkEnumeratePhysicalDevices -> 0
Vendor: AMD
Device: PS5 AGC GPU (ps5vk)
vkCreateDevice -> 0
Device extensions:
 VK_KHR_swapchain

ERROR-OUT BEGIN
QUAKE ERROR: Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format
```

The physical device enumerated, the device was identified as `PS5 AGC GPU (ps5vk)`,
`vkCreateDevice` returned 0, and the swapchain extension is enabled. The instance and
the device halves of M2 are now **proven on hardware**.

And the failure is legible, which is the stderr redirect earning its place: the engine
names its own problem instead of dying silently with `exit_value=0`.

### The depth-stencil gap, measured

vkQuake requires one of exactly two formats, each with
`VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` on optimal tiling:

```c
vkGetPhysicalDeviceFormatProperties (vulkan_physical_device, VK_FORMAT_D24_UNORM_S8_UINT, &format_properties);
qboolean x8_d24_support = (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
vkGetPhysicalDeviceFormatProperties (vulkan_physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &format_properties);
qboolean d32_support = ...;
...
// This cannot happen with a compliant Vulkan driver. The spec requires support for one of the formats.
Sys_Error ("Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format");
```

`../PS5_Vulkan` reports **zero** combined depth-stencil formats - `grep -cE
'D24_UNORM_S8|D32_SFLOAT_S8|D16_UNORM_S8' driver/ps5vk_image.c` is 0 - and two
depth-only ones:

```
driver/ps5vk_image.c:428  {VK_FORMAT_D16_UNORM,  ... DEPTH_STENCIL_ATTACHMENT_BIT | SAMPLED | BLIT_SRC | TRANSFER_SRC | TRANSFER_DST, 0, 7 /*16_UNORM*/ ...}
driver/ps5vk_image.c:442  {VK_FORMAT_D32_SFLOAT, ... DEPTH_STENCIL_ATTACHMENT_BIT | TRANSFER_SRC | TRANSFER_DST | SAMPLED | BLIT_SRC, 0, 22 /*32_FLOAT*/ ...}
```

and its draw path accepts only those two:

```
driver/ps5vk_draw.c:534  if ((depth_view->format != VK_FORMAT_D32_SFLOAT &&
driver/ps5vk_draw.c:535       depth_view->format != VK_FORMAT_D16_UNORM) || ...
```

**This is a driver conformance gap, not an application bug.** The Vulkan specification
requires an implementation to support at least one of `D24_UNORM_S8_UINT` or
`D32_SFLOAT_S8_UINT` for depth-stencil attachment, which is what vkQuake's comment
says when it calls the case impossible. Every conformant Vulkan application that wants
a depth buffer meets this, so it is the driver's gap to close rather than something
this port should work around - and it is the first blocker found here that is not
this project's own code.

It is also small: an entry mapping the combined format onto the same `32_FLOAT`
hardware word the depth-only one already uses, with the stencil aspect ignored, plus
the draw path's two-format check widened to three. Quake uses no stencil, so ignoring
it costs nothing that has been measured.

### Correction: Quake does use stencil, and the driver has no stencil path

The entry above ends by calling the fix small and by saying "Quake uses no
stencil, so ignoring it costs nothing that has been measured". **That is wrong,
and this entry supersedes it.** It was written from the format table and the
draw path without reading what the engine does with a stencil attachment. It
does a great deal:

```
gl_rmisc.c:3384-3395   sky stencil write: colorWriteMask = 0, compareOp = ALWAYS,
                       passOp = REPLACE, reference = 0x1 -- stencil only, no colour
gl_rmisc.c:3424-3436   skybox consume: depthTestEnable = VK_FALSE,
                       compareOp = EQUAL, writeMask = 0x0, reference = 0x1
```

That pair is the sky occlusion trick, and the engine additionally builds a
parallel render-pass set keyed on `MAIN_RENDER_PASS_STENCIL_CLEAR` whose variants
differ only in stencil semantics. The stencil aspect is load-bearing across the
whole pass set.

The earlier entry was also wrong about the size. `../PS5_Vulkan` has no stencil
path at all, not merely no combined format: `DB_STENCIL_INFO` is the constant
"stencil disabled" word `0x20000180`, the stencil read and write bases and
`DB_STENCIL_CLEAR` are written zero, `stencilTestEnable` is refused at pipeline
creation, and a stencil clear is refused by name. The format entry is the visible
end of it, not the work.

**Consequence for the port.** There is no engine-side fix worth having. Accepting
depth-only `D32_SFLOAT` would leave the sky pass writing and testing a stencil
aspect that does not exist, and emulating the trick by editing the renderer would
break the invariant that upstream stays unmodified. M2's render-pass step is
genuinely stopped, and the fix belongs in `../PS5_Vulkan`.

**Also recorded.** `../PS5_Vulkan` is maintained separately and is read-only from
this repository. The gap is therefore reported rather than patched:
`docs/PS5_VULKAN_REQUESTS.md` R1 carries the failure, the specification
requirement, the engine code paths that reach it, the two places the driver's own
roadmap already tracks it (`unknowns-depth-words`, and `vk_b3_image_test.c`'s
assertion that the format reports no features), and the acceptance test.

What this does **not** stop is the rest of M2. The driver's swapchain and present
path is implemented and console-proven independently of depth, so the surface,
swapchain and presented-frame half of M2 can still be settled here without the
engine — which is the next step.

### Reading the driver before asking it: the gap is real, and the audit hides it

The step before this one wrote a request to `../PS5_Vulkan` asking it to support
a combined depth-stencil format. Before sending it, this step read that repository
— its git state, its progress documents and its own tooling — to check the request
was right. It was right about the gap and wrong about how to present it, and the
reading changed the request's whole framing.

**Its state.** Local `main` is one commit ahead of the published `PS5Vulkan/main`
— `23bcea1` ("round 7: the storage image, 16 rows closed") is not pushed, so the
request's original citation named a commit the maintainer cannot see. Round 8's
work is uncommitted in the tree. Two remotes share one URL, one of them 244
commits stale, and a `master` branch sits 308 behind. The rung is closed: 179
required formats, 54 reported, "0 compiler-blocked and 0 probe-reachable", with
four blockers named — the descriptor type, the hardware's sRGB fetch order, ACO
stability, and nothing else.

**What the reading found.** The two depth-stencil formats are not among the four
the audit reports as missing. They are among the 55 it reports as *conditional*,
because the specification marks them `{sym2}` — and `tools/format_audit.py` files
every `{sym2}` cell without ever checking whether the requirement is met, with
`--check` failing only on `{sym1}`. This footnote is the case that breaks the
abstraction: it carries **two `must` clauses under one marker**, one for the
depth-only pair and one for the combined pair. The driver satisfies the first and
violates the second.

The proof it is the classification rather than the driver's table is in the tool's
own output, which lists `VK_FORMAT_D32_SFLOAT` as conditional for
`DEPTH_STENCIL_ATTACHMENT` while the driver carries that bit. The bucket holds a
met row and a violated row and cannot tell them apart.

**So the request was rewritten** to lead with the tooling finding rather than the
driver gap. That is not a softer ask — it is a more accurate one, and it is
verifiable by the maintainer in one command. The row is documented and deliberate:
`docs/V0_FORMATS_AUDIT.md:238` gives its reason and closing path, and `:24` says
conditional rows are listed that way "so a conditional row is never mistaken for a
closed one". The classification is working as designed; it simply cannot
distinguish a caveat from a disjunction.

**Also corrected.** The first draft offered register addresses as if they were the
work. They are not: the driver writes AGC register packets in ps5-opengl's
compacted numbering, where `DB_Z_INFO` is offset `0x010` against Mesa's absolute
`0x028040`, so Mesa's `amdgfxregs.h` gives field positions and op enumerations but
neither the offset mapping nor the measured enable word. The request now says that
plainly rather than implying the encoding is already in hand. ps5-opengl, checked
for the same reason, has no stencil path either — its `src/` is only `platform/` —
so the audit's "nothing has recorded" stands.

No code changed in this step and nothing was pushed. `../PS5_Vulkan` was read
only, as it must be: it is maintained separately.

### The console trace becomes an artifact, and the request stops quoting one

Every console run so far was read over FTP and then written about. The request to
`../PS5_Vulkan` quoted one of those transcripts, and the evidence gate reported
"0 captures" -- a gate that had nothing to replay and a request whose central fact
was a claim. This step closes both.

`tools/fetch-trace.py` pulls the title's own `/app0/trace.txt` into the ignored
`klog/` tree. It reuses `tools/deploy-title.py`'s `load_settings` and `title_id`
rather than repeating them, so there is one definition of where the console is and
one of which title is being talked to -- the same import `tools/run-title.sh`
already uses. Run against the console, it returned 2,027 bytes over 74 lines.

Two things about that file decided how it gets recorded. The title opens its trace
for append (`src/trace.cpp`, `fopen(path, "a")`) and `tools/deploy-title.py` never
deletes it, so the file holds **every run since the folder was deployed** and the
run being evidenced is at the end. `tools/evidence.py distil` kept only the first
`--head` lines, which would have recorded the *oldest* run in the file -- in this
capture, a run that died at `vkEnumeratePhysicalDevices` before the device was
ever created. So `distil` gained `--tail N`, which selects the newest run instead,
and `tests/test_evidence.py` pins it: tail records the newest run, the `--head`
default still records the oldest for captures that do not accumulate, and `--tail`
wins if both are given.

The second thing the trace corrected is this file's own history. The runs were
narrated as three; the append-mode file shows a further one between the pak fix
and the device fix, which reached the instance and stopped at
`vkEnumeratePhysicalDevices` -- the forwarder fault. `docs/ACTIVE.md` now says
what the artifact shows rather than what the narration said.

`tests/test_evidence.py` is nine cases over the gate itself, because this is the
one gate a rebuild cannot falsify. `compare` is tested to fail: a missing needle,
a present `must_not_contain` needle, a capture that distilled to nothing, a step
holding only half its two files. `distil` is tested to record the right run. The
evidence directory was decoration the moment those verdicts stopped being
exercised, and nothing else in the suite would have noticed.

The record is `evidence/m2-device/`: the newest run, distilled with `--tail 46`,
asserting the six facts that are M2's instance and device halves -- VideoOut as
the video driver, `vkCreateInstance -> 0`, `Vendor: AMD`, `Device: PS5 AGC GPU
(ps5vk)`, `vkCreateDevice -> 0` and `VK_KHR_swapchain` -- plus the depth-stencil
error the run stops on, and `W_LoadWadFile` as a `must_not_contain` so a title
that lost its game data again cannot read as a pass.

That the error is asserted rather than hidden is deliberate and is stated in the
capture's own `human_check`: the gate is green for the state the port is actually
in, and the record must be replaced when `../PS5_Vulkan` reports a combined
depth-stencil format, because the run should then continue into its render passes.
A record that asserted only the successes would have to be rewritten silently.

Verify:
  $ python3 tools/fetch-trace.py
  $ python3 tools/evidence.py distil klog/trace-20260920T155302Z.txt --step m2-device --tail 46 ...
  $ bash tools/verify.sh
  verify: PASS (format unit build integration evidence)
  ==> [evidence] m2-device: OK (raw klog/trace-20260920T155302Z.txt, 74 lines)
  ==> [evidence] 1 capture(s) replayed, 0 failed

### The klog listener came back with a crash the trace file could not show

The listener started in an earlier step finished, and its capture
(`klog/vkquake-listen-113523.log`, 688 KB over 8,066 lines) holds something the
title's own trace cannot: **the kernel's record of what happens after the title
prints its last line.**

Three runs of this title are in it, and all three end the same way. The kernel
first records the process leaving through `exit()`, then reports a fatal signal on
a user thread:

```
# process pid=182, eboot.bin calls exit() exit_value=1.
# A user thread receives a fatal signal
# signal: 12 (SIGSYS)
# rip: 00000008000003ac
# backtrace:
# 0000000000b8caab  # 0000000000ace520  # 0000000000a6ed7f
# 0000000000a6c09a  # 0000000000400190
```

The three records carry the **same rip**, in libkernel's syscall stubs. The three
exit values are 1, 0 and 0 — so this is not the error path and not `Sys_Error`: a
run that left through `exit(0)` took the identical signal at the identical
instruction. It sits on the exit path itself.

**This is this port's bug, not the driver's**, and it is the reason the klog of an
ordinary run reads like a crash: the console answers a fatal signal with
`SCE_SHELL_UTIL_ERROR_APPLICATION_CRASH`, a coredump and a `gpudump.elf` run. No
GPU fault appears anywhere in the capture; the GPU dump is routine report tooling.

**It also corrects the record.** The `W_LoadWadFile` run was written up as dying of
SIGSYS because the game data was missing, and deploying `id1/pak0.pak` was recorded
as the fix. It did make that run stop failing — but the signal was never on that
path. Both runs carry `rip: 00000008000003ac`, and the earlier entry attributed a
symptom of the exit path to the reason for the exit. `docs/FINDINGS.md` carries the
correction.

The record is `evidence/exit-sigsys/`: one complete kernel record, distilled from a
verbatim 34-line slice of the capture held in the ignored `klog/` tree, asserting
the exit line, the signal, the rip and the backtrace. The other two records are the
same shape and the same rip, which the capture's own note states so a reader knows
this is three-for-three rather than a single sample.

**The five frames are recorded raw, not symbolized.** `build/title.map` is from a
later build than the binary that crashed — the identity moved from `440199d7` to
`c0677c1f`, and the identity covers `src/`, `platform/ps5/`, three build scripts and
the linked archives, so the map describes a different image. Symbolizing with it
would produce exactly the confident wrong names `tools/symbolize-crash.py` warns
about in its header. The next step for this bug is a re-run against a freshly built
title, so the crash and the map are the same build.

Worth stating plainly: none of this blocks the request to `../PS5_Vulkan`. The
depth-stencil finding stands on its own, and the console calling every run a crash
does not change what the run said before it stopped.

Verify:
  $ python3 tools/evidence.py compare evidence/
  exit-sigsys: OK (raw klog/vkquake-exit-sigsys-113523.txt, 37 lines)
  m2-device: OK (raw klog/trace-20260920T155302Z.txt, 74 lines)
  2 capture(s) replayed, 0 failed

---

## 2026-09-22: the two driver gates closed, and what replaced them

**No console run in this entry.** It records a read of `../PS5_Vulkan` at `ad500a0`
against this tree at `91b9e7c`, and it exists because three statements in
`docs/ACTIVE.md` had gone false while nothing here changed: the depth gap is closed, the
ACO abort was never an ACO fault, and the gate that replaced them is not the one the
notes assumed.

### What the driver closed

- **The depth-stencil clause — `docs/PS5_VULKAN_REQUESTS.md` R1.** Both combined
  formats now carry `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT`
  (`driver/ps5vk_image.c`, the audit's second `must:` clause), the stencil plane is
  proved on the console (`v0-stencil-clear`), and `tools/format_audit.py` reads the
  footnote's clauses whole and fails `--check` on an unmet one. R1's status is updated
  in place; its body is kept as the report it was.
- **Anisotropy.** `v0-sampler-anisotropy` (pid 138) draws the address probe's frame with
  the flag off and on at the reported maximum and compares the two texel for texel:
  **0 mismatched texels**, and a request past the reported maximum refused by name.
  `R_InitSamplers` was the refusal this port would meet first after the depth format. It
  is not a refusal any more.
- **The ACO abort this tree recorded as a risk was never an ACO fault.** The driver's
  own runner divided a sampled-format row by its zero texel size; the three console
  attributions came from symbolizing addresses without subtracting the `0x400000` load
  base, and the case is closed with a console run (`jobs/aco-min`, pid 287,
  `../PS5_Vulkan/docs/BLOCKERS.md` row 9).

### What that leaves as the first refusal, read rather than measured

The driver checks the **pipeline layout**'s bindings, not the shader's used ones:
`ps5vk_descriptor_options` (`../PS5_Vulkan/driver/ps5vk_pipeline.c:195-220`) walks
`layout->set_layouts[set]` and refuses a binding whose `stride` is 0, and it is called
for the fragment stage at `:1265`. vkQuake's first pipeline (`R_CreateBasicPipelines`,
`basic_alphatest` on the main pass) uses `basic_pipeline_layout`, which vkQuake defines
as `{single_texture, mboit_input_attachment}` (`vendor/vkQuake/Quake/gl_rmisc.c`), and
that second layout is three `INPUT_ATTACHMENT` bindings at `FRAGMENT` stage — a type
`ps5vk_descriptor_stride` has no entry for.

**Prediction, for the next run to falsify:** the first `vkCreateGraphicsPipelines` fails
with `set 1 binding 0: descriptor type 10 has no proven table entry`, and vkQuake raises
`QUAKE ERROR: vkCreateGraphicsPipelines failed (basic_alphatest) with code -13`.

### The reading this corrects

`attachmentCount = resolve ? 3 : 2` in vkQuake's main render pass
(`vendor/vkQuake/Quake/gl_vidsdl.c`) is the **render pass's** attachment count — colour
plus depth — and subpass 0's `colorAttachmentCount` is 1. With `vid_fsaa` at its default
`0` there is no resolve attachment, so the plain pass needs one colour attachment; the
two-colour shapes belong to the OIT variants, which `r_oit`'s default of `1` (WBOIT)
selects. **`colorAttachmentCount > 1` is therefore not this port's next gate**: MRT is
what restores OIT, which the port is dropping as a registered accommodation.
`docs/PS5_VULKAN_REQUESTS.md` R2 carries the driver-side request that falls out of the
read: three core 1.0 descriptor types (`SAMPLER`, `SAMPLED_IMAGE`, `INPUT_ATTACHMENT`)
the table lacks while the per-stage limits advertise them.

### What is left

One console run of the current build, no code change, turns all of the above into a
measurement — and the same run produces the freshly built title `evidence/exit-sigsys/`
is waiting for, since `build/title.map` came from a later build than the binary that
crashed.

Verify:
  $ git -C ../PS5_Vulkan log --oneline -1
  ad500a0 Step 1b's two reads: the clear is general, the consumer counts, and the offsets are the device's
  $ sed -n '195,220p' ../PS5_Vulkan/driver/ps5vk_pipeline.c
  (the layout walk; `binding->stride == 0` is the refusal the prediction names)
  $ python3 tools/evidence.py compare evidence/
  2 capture(s) replayed, 0 failed

---

## 2026-09-22: the first run against the current driver, and the loader the console is missing

**A console run, build identity `10b174731bf50195`** — the first since 2026-09-20, and the
first against a driver that has closed both gates this port was waiting on. It stops
earlier than the run before it, which is not a regression but a different gate: the
driver became honest about its API version on 2026-09-21 (`PS5VK_INSTANCE_API_VERSION`,
1.3 -> 1.0, the CTS round that found the instance claiming a version it does not
implement), and that changed what `vkGetInstanceProcAddr` answers.

```
Vulkan Initialization
vkCreateInstance -> 0

QUAKE ERROR: vkGetInstanceProcAddr failed to find vkGetPhysicalDeviceProperties2
STACK TRACE:
(null)
```

Recorded as `evidence/m2-loader/` (the newest run only; that trace is append-mode and
holds four runs), which supersedes `evidence/m2-device/` for the reason that record's
own note named: it asserted the depth-format stop and said it must be replaced once the
driver reports a combined depth-stencil format.

### What the message means, and why the fix is this port's

vkQuake enables `VK_KHR_get_physical_device_properties2` — which the driver advertises
and its own host test exercises by the extension's spelling
(`driver/tests/vk_b2_device_test.c:96`, `VK_FUNCTION(instance, GetPhysicalDeviceProperties2KHR)`)
— and then loads the **core** names, `vkGetPhysicalDeviceProperties2` and
`vkGetPhysicalDeviceFeatures2` (`Quake/gl_vidsdl.c:889-890`). Vulkan's application-facing
names for a promoted extension are an alias pair and **the loader** keeps that pair; a
driver is not required to. This console has no loader: the driver is linked into the
title and `platform/ps5/vk_globals.c` supplies only what a loader's global trampolines
would (`tools/gen-vk-globals.py` says so in its own header). So the missing piece was
the port's, not the driver's.

### The fix

`platform/ps5/vk_loader.c`: the driver's lookup with the aliasing applied when it has
none, for exactly the two promoted pairs, with `SDL_Vulkan_GetVkGetInstanceProcAddr`
handing the engine that lookup instead of the raw symbol. The table is named rather than
a blanket "append KHR", and the file's retirement trigger is the driver claiming Vulkan
1.1. Checked on the host by `tests/vk_loader_test.c` (through `tests/test_vk_loader.py`)
against a driver of the console's shape: a Vulkan 1.0 instance that answers the
extension's spelling and not the core one — six checks, including that a name the driver
answers is not aliased and that a name with no twin stays missing.

Verify:
  $ bash tools/verify.sh
  verify: PASS (format unit build integration evidence)
  $ bash tools/verify.sh --list  # evidence replay
  m2-loader: OK (raw klog/trace-20260922T144954Z.txt, 153 lines)
  3 capture(s) replayed, 0 failed

**Still unmeasured after this run:** the depth-format check the previous run stopped on
(now behind the loader gate), and the pipeline-creation refusal the reading in
`docs/ACTIVE.md` predicts. The title is deployed with the fix as build identity
`9ad1d1f36bae987c`, so the next run answers both.
