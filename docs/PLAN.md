# PS5 vkQuake: the plan

vkQuake 1.36.0 as a native PlayStation 5 homebrew title. vkQuake is a Quake
source port whose renderer is Vulkan, and the console has no Vulkan of its own:
the implementation comes from `../PS5_Vulkan`, a Mesa-derived Vulkan 1.0 driver
built on AGC and VideoOut. This project is the application that sits on top of
it — the engine, and the platform layer that connects an engine written for SDL
to a console that has none.

This file is the top-level plan: the gates that decide whether a milestone is
done, the milestone map, and the invariants that constrain the code. It is read
every session, so it stays short and never records progress — the state is in
`docs/ACTIVE.md` and the runs are in `docs/PHASE_LOG.md`.

## The gates

A milestone is done when all four hold for the step that closes it. They are
ordered: a later gate never excuses an earlier one.

1. **It runs.** `tools/verify.sh` is green on this host, in order: the shell and
   the C and C++ this project owns are linted and formatted, the host unit tests
   pass, and the PS5 target builds from a clean tree.
2. **It is correct.** The cross-built artifact is structurally what the console's
   loader requires. `tools/build-vkquake-engine.sh` checks the one thing that can
   be checked about target objects — that the toolchain is the target toolchain —
   because x86_64-sie-ps5 shares the host's ELF header and no per-object test can
   tell a host object from a target one. See `docs/FINDINGS.md`.
3. **It works on the target.** The staged tree is uploaded to the console,
   launched from the console's own homebrew launcher, and the run reaches the
   step's definition of working — the title boots, the engine initialises, the
   first frames are drawn and presented — with the console's `klog` captured for
   the whole run.
4. **It is provable.** The console capture is distilled into a committed artifact
   under `evidence/<step>/`, and `tools/verify.sh evidence` replays it against the
   committed expectation on this host and exits non-zero on a difference. A claim
   with no artifact is not a pass.

## Milestone map

| Milestone | What it delivers | Detail |
| --- | --- | --- |
| M0 | The contract: the pin, the port layer's shape, a green gate runner, and vkQuake's engine cross-compiling into an archive | `docs/PHASE_LOG.md` |
| M1 | The console shell: an `eboot.bin` that loads, takes over the screen, reports its identity and exits cleanly | `docs/PHASE_LOG.md` |
| M2 | Vulkan and the surface: vkQuake's own instance and device initialise on the PS5 driver, and a swapchain built on a `VkDisplayPlaneSurfaceKHR` presents a cleared frame | `docs/PHASE_LOG.md` |
| M3 | The engine runs: the console, the config and the filesystem come up, pak data loads, and Quake's menu draws | `docs/PHASE_LOG.md` |
| M4 | Input: the PS5 pad drives the console, the menu and in-game movement | `docs/PHASE_LOG.md` |
| M5 | Audio: the sound mixer feeds the console's audio out | `docs/PHASE_LOG.md` |
| M6 | The world: a map renders with textures and lightmaps — the point at which the driver's missing storage images are met, and closed by the engine-side lightmap path | `docs/PHASE_LOG.md` |
| M7 | Release: performance, config and save persistence, and a release ZIP with an evidence-backed acceptance run | `docs/PHASE_LOG.md` |

Each milestone is a sequence of steps, one commit each, in `docs/PHASE_LOG.md`.
A step that turns out to be two steps is split before it is started, not after it
is half-built.

`docs/REFERENCE.md` — the step ladder and the environment — does not exist yet. It
is written when M1's steps are known well enough to describe, which is after the
first console run rather than before it. The three documents below that were
carried over from the RetroArch project describe the console-side procedure
truthfully and name RetroArch where they do; they are inherited, not adapted.

## Invariants

These constrain every change. Breaking one is a design decision, and a design
decision is written down in `docs/REFERENCE.md` and `docs/FINDINGS.md` before the
code that depends on it.

- **Upstream stays upstream.** vkQuake is fetched at a pinned revision by
  `tools/fetch-vkquake.sh` into `vendor/`, which is never committed and never
  hand-edited, and which keeps no history so it cannot be committed into. Every
  change this port makes to upstream's behaviour is either a file in `platform/`
  that is compiled *instead of* an upstream file, or an edit applied to a copy by
  `tools/apply-port-patches.py`. A change that cannot be expressed as one of those
  two is a change to the strategy, not to a file.
- **The toolchain is named everywhere.** Every compilation of PS5 code goes
  through `$PS5_PAYLOAD_SDK/bin/prospero-*` or `tooling/prospero-clang18`, in the
  environment *and* as variables, so that a build which hardcodes `cc` cannot
  silently produce a host object. That failure is not hypothetical and it is not
  detectable per object: the target and the host share an ELF header, so the
  pipeline checks the toolchain instead (`docs/FINDINGS.md`).
- **The SDL compatibility layer is a boundary, not a port.** `platform/ps5/SDL.h`
  declares only what a file this project compiles actually calls, and
  `platform/ps5/sdl_ps5.c` implements it. Windowing, events, gamepads and audio
  devices do not belong there — they are the port layer's own files, written
  against the console's APIs. A symbol added to the shim needs a caller, and the
  reason it is in the shim rather than in a port file.
- **No direct hardware access.** The engine talks to the GPU only through the
  Vulkan implementation in `../PS5_Vulkan`, and to the console through the public
  payload SDK. No AGC register writes, no `sceGnm*` command submission, no
  firmware offsets, and no private GPU structures in this repository.

## Where the truth lives

| Question | Document |
| --- | --- |
| What is being built, and why | this file |
| What is happening right now | `docs/ACTIVE.md` |
| What a step is, and how it is accepted | `docs/PHASE_LOG.md` |
| What was measured, and what it forces | `docs/FINDINGS.md` |
| What happened, run by run | `docs/PHASE_LOG.md` |
| How to verify, ship and debug | `docs/TESTING.md`, `docs/DEPLOYMENT.md`, `docs/TROUBLESHOOTING.md` |

If a question has no home, add it to the table and create the document; do not
answer it in two places.

## What is deliberately not planned

- The exploit, the kernel payload and the console's configuration. This project
  builds an application; it does not jailbreak a console, and it never writes to
  a console it was not pointed at.
- The Vulkan driver and the shader compiler. Graphics on this console are
  `../PS5_Vulkan`'s work; this project consumes its released archives and reports
  what vkQuake needs from them by name.
- The game data. Quake's `pak0.pak` is the console owner's to supply; it is never
  fetched, never committed and never staged into this repository.
- Retail-style packaging: signing keys, `sce_sys` beyond a homebrew title's own
  metadata, and store submission.
- A software-rendered fallback as the product. `src/display.cpp` drives VideoOut
  directly and stays useful for bringing the title up and for proving the shell
  before the renderer works, but the shipped application renders through vkQuake's
  own Vulkan renderer.

Scope grows by changing this file, in its own commit, with the reason in the
message. It never grows by a step quietly doing more than its milestone says.
