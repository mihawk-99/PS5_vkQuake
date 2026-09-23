## Current objective

vkQuake at 4K is working, smooth and running at up to 120 FPS on my 4K120 VRR TV.
What remains: the driver reporting its refresh truthfully (it still says 60 Hz),
a fixed 120 Hz mode for displays without VRR, the other maps measured at 120 Hz,
and the gameplay/stability acceptance run. An intermittent texture glitch I have
seen in play is set aside until I can capture it. The loop stays the same: measure
work per frame, make one focused change, verify it on the console, commit it with
evidence. Never close a running session for a benchmark (`procs` first; the
harness and the runner's restart refuse another title). CTS is out of scope.

## Console setup that the numbers depend on

- **etaHEN "pause kstuff on game launch" is ON.** With kstuff active a system call
  costs ~20 us (getpid 20.1 us, m6-r34-cost-probe); paused, 0.73 us
  (m6-r49-kstuff-paused). That alone is the difference between ~18 ms and ~4 ms of
  work a frame. It also applies to launches from the harness.
- **VRR "Apply to Unsupported Games" is ON**, 120 Hz output automatic. A bare
  vblank then measures 20.872 ms (VRR floor, no flips pending) and frames present
  as soon as they are ready, up to 119.88 Hz. A frame whose work exceeds ~20.8 ms
  is held to ~29.2 ms. Every run before m6-r38-vrr-baseline had VRR off.

## How to measure

- The present line (`PS5 present:`) carries `work_ms`, `present_ms`, a period
  histogram in 60 Hz vblanks and the SDL shim's kernel-entry counts per frame.
  Always on; one write per ten seconds.
- Slow frames report themselves: `PS5 hitch:` (a period over 40 ms: work, present,
  shim/allocator/file counts) and `PS5 slow host frame:` (the engine's phases and
  its costliest server commands). The driver adds `[ps5vk] hitch` when profiled.
- The driver profile is nearly free (TSC timestamps, one write). Fixtures:
  `build/r32-autoexec.cfg` (two standing phases), the walk in
  evidence/m6-r41-walk, the New Game run in evidence/m6-r43-newgame-svc, and
  `build/r45-startup-autoexec.cfg`; stage with FIXTURE= for build/r29b-stage.py
  (`noprofile` for no driver profile). Slice a trace from its last
  `build identity:` line.

## Where it stands (steady windows, walking the start map unless noted)

| step | FPS | work | evidence |
| --- | --- | --- | --- |
| R29 baseline, VRR off (standing) | 19.72 | — | m6-r29-baseline |
| driver fixes through R42, kstuff active, VRR on | 52.4-55.4 | ~18 ms | m6-r42-parallel-blit |
| kstuff paused, VRR on | **119.88** | 4.0-4.4 ms | m6-r49-kstuff-paused |

At 119.88 FPS every frame is 8.29-8.40 ms: the display's ceiling, not the work.
Driver split: engine 2.36 ms, queue 2.08 ms (CPU warp-mip blits 1.39), one vblank
wait. Startup: first present 0.55-0.77 s into the process (m6-r45-startup); the
console's launcher adds ~2.6 s. Nothing compiles: the internal NIR cache landed
and the title ships each driver build's compiled set (tools/shader-cache.py;
after a driver change: launch once, harvest, rebuild; m6-r47-shipped-cache).
The New Game stutters were centre-print logging through the unbuffered stdout,
fixed by buffered console streams (m6-r43-newgame-svc, m6-r44-buffered).

Known limits: `r_waterwarpcompute 0` ends the game with vkEndCommandBuffer -13,
because the driver renders only into 3840x2160 colour targets and the raster
warp path renders 512x512 images; the default compute path works. The demo tour
(klog/r50-*) found no glitch; E1M4's and E1M6's pure-black regions did not change
with mips off or fast sky, and look like unlit geometry.

## Next

1. The driver reports the real refresh: enumerate VideoOut's modes, select 120 Hz
   at swapchain creation when the metadata allows it (attribute3 0x80040 is set),
   restore it at close, keep the 60 Hz fallback, and report what was measured.
2. Measure E1M1 and the other maps at 120 Hz.
3. Gameplay/stability acceptance: all eight maps, save/load with position and
   ammunition compared, menus and HUD, repeated transitions, exit and relaunch,
   and a soak with its duration reported (build/r30-* covers part of it; its
   stager refuses existing configs and must be adapted to r29b-preserve first).

## Persistence

build/r29b-preserve.py snapshots and restores my configuration files (contents
and absence) around every fixture run; build/r23-fixture-backup/manifest.json is
the original pre-play backup and is never overwritten. Saves and the shader cache
are preserved. The stale pre-6ff265f cache entries were deleted (1,983 files,
11.1 MB); only the current build's directory remains. Raw console data stays in
the ignored klog/; distilled evidence is under evidence/.
