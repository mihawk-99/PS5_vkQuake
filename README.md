# vkQuake for PlayStation 5

A native PS5 homebrew port of [vkQuake](https://github.com/Novum/vkQuake),
using the companion [PS5 Vulkan driver](https://github.com/mihawk-99/PS5_Vulkan).
It runs Quake's Vulkan renderer through AGC and VideoOut, with native DualSense
input, AudioOut sound and a persistent compiled-shader cache.

**The game works and runs at up to 120 FPS at 4K.** Walking the start map, the
measured frame period is a steady 8.34 ms (119.88 FPS) on a 4K120 VRR display with
kstuff paused at launch (see [Performance](#performance)). Systematic all-map,
save/load and long-duration acceptance is still in progress.

[Installation](#installation-and-updates) · [Controls](#controls) ·
[Building](#building-from-source) · [Performance](#performance) ·
[Testing](#testing-and-reporting-problems) · [Project layout](#project-layout)

## Current capabilities

| Area | Implemented and verified | Remaining acceptance |
| --- | --- | --- |
| Rendering | Textured worlds, water/lava/teleporter warps, particles, models, menu fades and transparent HUD; 4K screenshots read back correctly; the three shareware demos (E1M3, E1M4, E1M6) render | All eight shareware maps in play; an intermittent texture glitch I have seen but not yet reproduced |
| Display | 3840×2160; presents at the display's own pace, which on a VRR TV means anywhere from 48 to 120 Hz | The driver still reports 60 Hz to the engine; a fixed 120 Hz mode without VRR |
| Performance | 119.88 FPS walking the start map (kstuff paused); 52–55 FPS with kstuff active | E1M1 and the other maps measured at 120 Hz |
| Input | Native DualSense adapter; host checks for buttons, releases, triggers, menu repeat and analog movement | Detailed controller feel |
| Audio | 48 kHz stereo S16; repeated five-minute runs with nonzero samples and zero native-output errors | Listening checks across gameplay |
| Startup | First frame 0.55–0.77 s after the process starts; every shader comes from the cache, including the driver's internal ones | — |
| Shader cache | One directory per driver build; the title ships the compiled set, so a fresh install compiles nothing | — |
| Diagnostics | Any frame over 40 ms writes one trace line saying where its time went | — |
| Screenshots and exit | 4K PNG capture and normal return to the shell | Longer gameplay/save/load soak |

The driver work behind this includes vertex stride, dynamic uniform offsets,
descriptor arrays, padded pitches and mip tails, 32-bit indices, depth-state
transitions, sampler LOD bias and menu blending; and for speed, flushing only
memory the application maps, a bounded spin on the GPU's completion marker,
blits resampled on five threads and a persistent cache for internal shaders.

The current state and build identity live in [docs/ACTIVE.md](docs/ACTIVE.md);
every run and its limits are in [docs/PHASE_LOG.md](docs/PHASE_LOG.md).

## Requirements

- A PS5 with a working homebrew environment. I test on firmware 10.01;
  other versions are untested.
- A directory-title loader. I use [etaHEN](https://github.com/etaHEN/etaHEN) and
  [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus).
- FTP access for installation; I use
  [ftpsrv](https://github.com/ps5-payload-dev/ftpsrv) on port 2121.
- A DualSense controller and a 4K display.
- Your own Quake game data. I test with the shareware `id1/pak0.pak`; game data
  is never downloaded, committed or included by this project.

For full speed, two console settings matter (see [Performance](#performance)):

- **etaHEN: pause kstuff on game launch.** With kstuff active, every system call
  costs about 20 µs instead of under 1 µs, which alone limits the game to ~50 FPS.
- **Screen and Video → VRR: Apply to Unsupported Games**, with 120 Hz output on
  automatic, on a VRR-capable TV. Without VRR the game still runs, but presents
  on a fixed 60 Hz grid.

This repository does not configure the console's homebrew environment. Registered
Quake, expansion packs, mods and multiplayer are not covered by the current
acceptance results.

## Installation and updates

The title ID is **PPSA99010**. Use the complete title folder produced by the build,
not an isolated executable. Close vkQuake before replacing its files.

For a built checkout, create a local `.env` from `.env.example` if you do not
already have one, and set `PS5_HOST` and `FTP_PORT` for your console. Keep this
file private. From the repository root:

```bash
# Upload the already-built dist/PPSA99010 title; does not launch it.
python3 tools/deploy-title.py

# Read back and check the deployed executable identity.
python3 tools/deploy-title.py --check
```

Alternatively, `make deploy` builds the title before uploading it. Neither form
rebuilds the sibling Vulkan driver: rebuild that explicitly when changing it.

Supply your game data separately by FTP. The console folder should contain:

```text
/data/homebrew/PPSA99010/
├── eboot.bin
├── sce_sys/                 # title metadata and icon
├── ps5vk-shader-cache/       # shipped compiled shaders, one directory per driver build
├── ...                      # other files from the complete built title
└── id1/
    └── pak0.pak             # supplied by you
```

The mounted title sees that directory as `/app0`. Launch vkQuake from your
homebrew launcher after deployment completes. Normal updates preserve files
outside the staged build, including game data, saves and shader caches. Keep a
backup of your data before manually replacing or removing a title directory.

**For manual play, do not use `tools/run-title.sh`.** It is a developer watch
harness that closes the title after its observation window. Automated fixtures
can also change maps, move the player and exit the game.

## Controls

The native adapter translates DualSense input into vkQuake's controller keys.
These are the stock bindings; saved user bindings take precedence.

| DualSense control | Default action |
| --- | --- |
| Left stick | Move and strafe; navigate menus |
| Right stick | Look |
| R2 | Fire |
| L2 | Jump |
| L1 / R1 | Previous / next weapon |
| Cross / Circle | Confirm / back in menus |
| Options | Open or close the menu |
| Touchpad press | Toggle the Quake console |
| D-pad | Arrow-key navigation |

Square, Triangle and stick clicks are exposed as controller keys for binding;
the stock configuration does not assign them gameplay actions. The touchpad
opens the console but is not an on-screen keyboard.

Archived controller settings include `joy_enable`, `joy_deadzone_move`,
`joy_deadzone_look`, `joy_deadzone_trigger`, `joy_sensitivity_yaw`,
`joy_sensitivity_pitch` and `joy_invert`. Their implementation is in
[platform/ps5/ps5_input.c](platform/ps5/ps5_input.c). Stock bindings come from
upstream's `Misc/vq_pak/default.cfg`, fetched with the engine.

## Configuration, saves and shader cache

Preserve `vkQuake.cfg`, `id1/vkQuake.cfg`, saves under `id1/`, and
`ps5vk-shader-cache/` when updating. Screenshots are written into the game
directory.

Compiled shader packages live at `/app0/ps5vk-shader-cache/<driver build>/`, one
directory per driver build (keys include the build, so entries from another build
would never be used). Entries survive restarts and crashes; changed or invalid
entries are recompiled. The build ships the set for the linked driver when it has
been harvested:

```bash
# After the first launch of a new driver build has compiled everything:
python3 tools/shader-cache.py harvest   # read this build's entries back from the console
bash tools/build-title.sh               # dist/ now carries them; the next deploy ships them
```

A launch whose cache held only the shipped entries compiled nothing
([evidence](evidence/m6-r47-shipped-cache/)). A normal launch reaches its first
frame 0.55–0.77 s after the process starts ([evidence](evidence/m6-r45-startup/));
the console's launcher takes about 2.6 s before that. A first launch after a
driver change, with nothing shipped, spends about 2 s compiling.

## Building from source

Use a Linux/WSL development host with Bash, Git, Python 3, Make, a C/C++ toolchain,
Clang/LLVM/LLD, `glslangValidator` and `spirv-opt`. The native dependency scripts
fetch the pinned public payload SDK. `make doctor` reports host requirements.
The driver's Mesa-derived builds additionally require its documented generators
and compiler SDK setup.

Keep these repositories side by side, or set `PS5_VULKAN_DIR` to the driver tree:

```text
PS5_Homebrews/
├── PS5_Vulkan/
└── PS5_vkQuake/
```

First follow the driver's [build instructions](https://github.com/mihawk-99/PS5_Vulkan#3-build)
to prepare its SDK/compiler dependencies. Its explicit archive build sequence is:

```bash
cd ../PS5_Vulkan
bash tools/fetch-mesa.sh
bash tools/build-psbc-ps5.sh
bash tools/build-vulkan-runtime.sh
bash tools/build-driver.sh
```

Then build the application:

```bash
cd ../PS5_vkQuake
make doctor
make deps
bash tools/fetch-vkquake.sh
bash tools/build-title.sh
```

The engine revision is pinned in [tools/fetch-vkquake.sh](tools/fetch-vkquake.sh).
The title script generates the shaders and embedded configuration pak, compiles
the engine and platform layer, links the Vulkan archives and signs/assembles
`dist/PPSA99010/`. It does not include `pak0.pak`.

The linked driver artifacts are `libps5vk.ps5.a`, `libvk_runtime.ps5.a`,
`libpsbc_driver.ps5.a` and `libpsbc_support.ps5.a`. A missing or stale driver
archive must be addressed in the driver repository before relinking the game.
The driver is statically linked: this is not a runtime Vulkan `.so` installation.

Do not hand-edit the ignored `vendor/vkQuake` tree. Reproducible upstream changes
belong in [platform/ps5/vkquake-edits.py](platform/ps5/vkquake-edits.py), while
native platform code belongs in `platform/ps5/` and `src/`.

## Performance

Measured on the console at 3840×2160, walking the start map (steady windows):

| Setup | FPS | Work per frame | Evidence |
| --- | ---: | ---: | --- |
| Starting point (R29, no VRR) | 19.7 | — | [m6-r29-baseline](evidence/m6-r29-baseline/) |
| kstuff active, VRR on | 52.4–55.4 | ~18 ms | [m6-r42-parallel-blit](evidence/m6-r42-parallel-blit/) |
| **kstuff paused, VRR on** | **119.88** | 4.0–4.4 ms | [m6-r49-kstuff-paused](evidence/m6-r49-kstuff-paused/) |

At 119.88 FPS every frame takes 8.29–8.40 ms: the game is waiting on the display's
120 Hz ceiling, not on work. Of the ~4 ms of work, about 2.4 ms is the engine,
1.4 ms the CPU mip generation for water and teleporter textures, and the rest
submission.

Two console-side factors decide most of this:

- **kstuff.** It traps system calls, and each one then costs ~20 µs (measured
  20.1 µs for `getpid`) instead of 0.73 µs. The engine and driver make hundreds a
  frame, so pausing kstuff at game launch (an etaHEN setting) takes the frame from
  ~18 ms of work to ~4 ms.
- **VRR.** With VRR the TV shows a frame as soon as it is ready, between 48 and
  120 Hz. A frame that misses the 48 Hz window (20.8 ms) is held to 29.2 ms, so a
  frame with more work than that swings between ~50 and ~34 FPS. Without VRR the
  output is a fixed 60 Hz grid.

Remaining performance work: the driver still reports 60 Hz to the engine and has
no fixed 120 Hz mode for displays without VRR, and the other maps have not been
measured at 120 Hz. The measurements, rounds and what was ruled out are in
[docs/ACTIVE.md](docs/ACTIVE.md) and the driver's `jobs/`.

## Testing and reporting problems

The application gate runner is the command reference:

```bash
bash tools/verify.sh --list
bash tools/verify.sh                 # format, unit, build, integration, evidence
bash tools/verify.sh format evidence # bounded documentation/evidence check
```

Hardware changes additionally require executable readback, a listener running
before launch, final trace collection and pixel evidence. `build/r29b-preserve.py`
snapshots and restores my configuration around a fixture run, and the harness
refuses to start while another title is running.

A useful bug report includes the build identity from `trace.txt`, driver and port
commits, firmware, map and reproduction steps, relevant trace/klog lines, and a
screenshot where applicable. State whether the shader cache was warm or cold.
Remove console addresses and credentials before sharing logs. Raw captures stay
ignored; distilled results and expected outputs are committed under `evidence/`.

If the game cannot find its data, check the exact `id1/pak0.pak` location. If a
launch exits, inspect the title's `trace.txt` before rebuilding. A slow first
launch and slow gameplay are separate issues; keep the cache and report both. A
slow frame (over 40 ms) leaves a `PS5 hitch:` or `PS5 slow host frame:` line in
`trace.txt` that says where its time went. For further developer procedures see
[docs/PORT.md](docs/PORT.md) and [docs/PLAN.md](docs/PLAN.md).

## Project layout

| Path | Purpose |
| --- | --- |
| `platform/ps5/` | SDL compatibility surface, native engine adapters and reproducible upstream edits |
| `src/` | PS5 title entry/support, memory, trace, display, input and audio backends |
| `tools/`, `tooling/` | Fetch, build, sign, deploy, verify and evidence tools |
| `sce_sys/` | Title identity and launcher assets |
| `tests/`, `evidence/` | Host regression checks and committed console evidence |
| `docs/` | Port architecture, active state and append-only run history |
| `docs/inherited/` | Frozen reference documentation from the original RetroArch foundation |
| `vendor/`, `.deps/`, `build/`, `dist/` | Ignored dependencies and generated outputs |

## Credits and licensing

This port builds on [vkQuake](https://github.com/Novum/vkQuake),
[id Software's Quake](https://github.com/id-Software/Quake), the
[PS5 Vulkan driver](https://github.com/mihawk-99/PS5_Vulkan),
[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl),
[Mesa](https://gitlab.freedesktop.org/mesa/mesa), and the
[public PS5 payload SDK](https://github.com/ps5-payload-dev/sdk).
The native foundation and reused platform code originate in
BlackBearReloaded's PS5 homebrew work and the preceding RetroArch port.

Keep each source file's copyright and SPDX license notices; fetched engine,
compiler and library dependencies retain their upstream licenses. Quake game
data is separate from the source code and is not distributed here. The project
ships no proprietary console runtime extracted from a console. It is an
independent homebrew project, unaffiliated with Sony or id Software.
