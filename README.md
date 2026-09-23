# vkQuake for PlayStation 5

A native PS5 homebrew port of [vkQuake](https://github.com/Novum/vkQuake),
using the companion [PS5 Vulkan driver](https://github.com/mihawk-99/PS5_Vulkan).
It runs Quake's Vulkan renderer through AGC and VideoOut, with native DualSense
input, AudioOut sound and a persistent compiled-shader cache.

**The console owner confirmed that the game works on September 23, 2026.**
This is a working development port. Performance optimization and systematic
save/load, all-map and long-duration acceptance remain in progress. The owner's
current estimate is **15–30 FPS**, not a controlled benchmark of the latest build.

[Installation](#installation-and-updates) · [Controls](#controls) ·
[Building](#building-from-source) · [Performance](#performance-and-the-4k120-target) ·
[Testing](#testing-and-reporting-problems) · [Project layout](#project-layout)

## Current capabilities

| Area | Implemented and verified | Remaining acceptance |
| --- | --- | --- |
| Rendering | Textured start/E1M1 worlds, menu fades and transparent HUD; console screenshots read back correctly | All eight shareware maps and extended play |
| Display | 3840×2160 output through the driver's 60 Hz FIFO mode | Other display modes, including 120 Hz |
| Input | Native DualSense adapter; host checks for buttons, releases, triggers, menu repeat and analog movement | Detailed physical controller acceptance |
| Audio | 48 kHz stereo S16; repeated five-minute runs with nonzero samples and zero native-output errors | Listening checks across gameplay |
| Shader cache | Compiled SPIR-V outputs persist across title restarts and crashes | Console validation of the separate internal-NIR cache candidate |
| Screenshots and exit | 4K PNG capture and normal return to the shell | Longer gameplay/save/load soak |

The driver fixes behind this include vertex stride, dynamic uniform offsets,
descriptor arrays, padded texture pitches and mip tails, 32-bit indices,
depth-state transitions, sampler LOD bias and menu blending. The latest deployed
R29 build also simplifies common tiled-image address calculations, with
console pixel comparisons covering mip generation, uploads, copies and formats.

The latest working state and exact deployment identity live in
[docs/ACTIVE.md](docs/ACTIVE.md). Historical runs and their limitations are in
[docs/PHASE_LOG.md](docs/PHASE_LOG.md). A successful manual test does not imply
that every map, mod or Vulkan feature has passed acceptance.

## Requirements

- A PS5 with an already working homebrew environment. Current console validation
  uses firmware 10.01; compatibility with other versions is not established here.
- A directory-title loader. The tested setup uses
  [etaHEN](https://github.com/etaHEN/etaHEN) and
  [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus).
- FTP access for installation; the tested service is
  [ftpsrv](https://github.com/ps5-payload-dev/ftpsrv), port 2121.
- A DualSense controller and a display supported by the current 4K60 output path.
- Your own Quake game data. The tested data set is the shareware `id1/pak0.pak`.
  Game data is not downloaded, committed or included by this project.

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
`ps5vk-shader-cache/` when updating. The filesystem and game directory determine
which configuration is active; do not replace user configurations with benchmark
fixtures. Screenshots are written into the game directory.

Compiled shader packages are stored at `/app0/ps5vk-shader-cache`, visible over
FTP under the title folder. Successful entries are reused after restarting the
game and after application crashes. Keys account for shader/compiler inputs;
changed or invalid entries are recompiled. Do not clear the cache during normal
updates. Startup can be slower after a compiler or shader change.

A measured same-binary cold/warm pair reached its first present in
**30.410 / 13.018 seconds**, with **99 / 0 SPIR-V compilations** and
**433 / 532 cache hits**. These are historical controlled runs, not a startup-time
promise for every build or map. Eight internal NIR stages still compile at launch;
the candidate to cache those is parked in the driver and is not in the deployed
R29 executable. See the committed
[cold](evidence/m2-shader-cache-cold/) and
[warm](evidence/m2-shader-cache-warm/) evidence.

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

## Performance and the 4K120 target

**Both repositories need performance work, with the driver the first priority
indicated by the current measurements.** Quake's modest scene complexity does
not remove the cost of CPU image processing, large cache flushes and serialized
submission in this developing driver.

The R28 baseline, before the latest R29 address optimization, measured:

| Mean per frame | Start map | E1M1 spawn |
| --- | ---: | ---: |
| CPU image copies | 24.095 ms | 0.041 ms |
| Cache flush | 5.954 ms | 3.974 ms |
| Whole queue work | 33.222 ms | 6.905 ms |
| Present/flip path | 14.415 ms | 7.431 ms |
| Flushed target bytes | 384.09 MiB | 256.06 MiB |

Queue time includes other categories; these rows must not all be added together.
The profiler's `gpu_ms` includes submission/completion waiting and is not an
isolated measure of GPU execution. The start-map copy cost points toward water
mip generation, but per-operation profiling is needed to attribute it precisely.
See [the captured baseline](evidence/m6-copy-profile/) and the driver's
[metrics](https://github.com/mihawk-99/PS5_Vulkan/blob/main/jobs/r28-copy-profile/metrics.txt).
The latest R29 build has pixel-correctness proof; its controlled game performance
comparison is still pending.

120 FPS gives the entire frame **8.33 ms**, versus 16.67 ms at 60 FPS. Moving
from an estimated 15–30 FPS to 120 requires roughly **4–8× the throughput**.
The practical order of work is:

1. Measure repeatable scenes, frame-time distribution and individual image
   operations on the deployed build.
2. Optimize the driver's CPU image/mip paths, investigate GPU blits, reduce
   unnecessary cache work and improve submission overlap while retaining
   resource-ownership correctness and exact pixel comparisons.
3. Profile engine-side water passes, uploads and scheduling; reduce avoidable
   work where measurements support it. The tested `r_scale 2` and `tasks 0`
   settings did not improve FPS; defaults remain `r_scale 1`, `tasks 1`.
4. Establish stable 4K60, then validate a real 120 Hz VideoOut/WSI mode and frame
   pacing. The driver currently exposes only 3840×2160 at 60 Hz with FIFO.

The console supports 4K120 output with an appropriate display and HDMI setup,
but the homebrew driver must implement and prove that path
([Sony's display guide](https://www.playstation.com/en-ca/support/hardware/ps5-4k-resolution-guide/)).
A frame-rate cvar alone cannot enable it. 4K120 is a target, not an achieved or
guaranteed result. Shader caching improves startup rather than steady gameplay FPS.

## Testing and reporting problems

The application gate runner is the command reference:

```bash
bash tools/verify.sh --list
bash tools/verify.sh                 # format, unit, build, integration, evidence
bash tools/verify.sh format evidence # bounded documentation/evidence check
```

Hardware changes additionally require executable readback, a listener running
before launch, the actual process ID, final trace collection and pixel evidence.
Finish one watch harness before launching another title. Preserve original
configuration backups and remove only fixtures created for the test.

Useful bug reports include the build identity from `trace.txt`, driver and port
commits, firmware, map and reproduction steps, relevant trace/klog lines, and a
screenshot where applicable. State whether the shader cache was warm or cold.
Remove console addresses and credentials before sharing logs. Raw captures stay
ignored; distilled results and expected outputs are committed under `evidence/`.

If the game cannot find its data, check the exact `id1/pak0.pak` location. If a
launch exits, inspect the title's `trace.txt` before rebuilding. A slow first
launch and consistently slow gameplay are separate issues; retain the cache
and report both timings. For further developer procedures see
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
