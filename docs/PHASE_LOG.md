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
