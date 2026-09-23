## Current objective

Make vkQuake stable, playable and faster on the PS5: 4K60 first, then 4K120 on
the owner's 120 Hz TV. The loop: measure work per frame, make one focused
change, verify it on the console, commit it with evidence. Never close a running
session for a benchmark (`procs` first; the runner's restart refuses another
title). CTS is out of scope. Never claim flawless gameplay from a short run.

## How to measure now (read before any run)

- **Display: VRR "Apply to Unsupported Games" is ON** (owner, 2026-09-23 ~13:30).
  Since then a bare vblank measures 20.872 ms (VRR floor, no flips), the present
  does not wait, and **FPS is the work** (m6-r38-vrr-baseline). Every run before
  m6-r38-vrr-baseline had it OFF: E1M1 was then held at a constant 29.25 ms
  period (34.19 FPS) by a present wait whose cause was never established.
- **Work, not FPS.** The present line carries `work_ms` (present return to next
  present entry, submission included), `present_ms`, a period histogram in 60 Hz
  vblanks and the SDL shim's kernel-entry counts per frame. Always on; two TSC
  reads a frame; one write per ten seconds.
- **Run-to-run variation is ~2 ms of work** at E1M1 (m6-r38-marker-spin 17.74 ms
  vs m6-r40-execute 19.3-19.8 ms on nearly the same code). A change smaller than
  that needs repeated A/B runs before it is claimed.
- **The driver profile is nearly free** (driver 44337ef: TSC timestamps, one
  write(2)). Profiled runs before 44337ef paid ~20 us a timestamp and a 1.6-5.8 s
  summary write once a window (the old "unattributed stall"); their window means
  are inflated, their counts and per-call ratios are not.
- **A system call costs ~20 us on this console** (getpid 20.1, clock_gettime 20.3)
  against 12 ns for a TSC read (m6-r34-cost-probe). The engine clock is the TSC
  now (e0c8c25); released 64 KiB+ mappings are reused (b89315f).
- Fixture `build/r32-autoexec.cfg` (FIXTURE= for build/r29b-stage.py, `noprofile`
  for no driver profile); preserve/stage/run/collect/restore as in the brief.
  Slice a trace from its last `build identity:` line.

## Measured progress (steady windows, trimmed fixture)

| step | E1M1 | start map |
| --- | --- | --- |
| R29 baseline, VRR off (m6-r29-baseline) | 29.58 FPS | 19.72 FPS |
| profile cheap, VRR off (m6-r36-work-metric) | 34.19, work 23.67 | 26.68, work 37.27 |
| mapped-only flush, VRR off (m6-r37-mapped-flush) | 34.19-39.28, work 20.8-21.6 | 31.25, work 31.8 |
| same code, VRR on (m6-r38-vrr-baseline) | 50.65, work 19.52 | 31.11, work 31.92 |
| marker spin (m6-r38-marker-spin) | **55.76**, work 17.74 | **33.08**, work 30.00 |

Driver share of an E1M1 frame is now ~0.9 ms (queue 0.71, flip 0.19); the
application's own time is ~17-19 ms and scene-independent; the start map adds
~11.5 ms of CPU water-warp mip blits (gl_warp.c: 512x512, 5 mips, linear).
Ruled out this session: begin/reset cost (0.33 ms, was clock reads), draw
encoding (0.08 ms), vkCmdExecuteCommands replay (0.14 ms), the SDL semaphore
(parked/sdl-semaphore, no gain: the 81 blocking waits a frame are idle workers).

## Next

1. Split the application's ~17 ms: engine-side timing of the render path at
   r_tasks 0 and 1 (one thread makes attribution exact), and whatever driver
   calls the chain still lumps together (descriptor updates, maps).
2. Start map: the 11.5 ms of CPU mip blits -- a 2:1 box fast path or a GPU blit.
3. 120 Hz mode at swapchain creation in the driver (attribute3 0x80040 is set),
   then gameplay/stability acceptance (build/r30-*), then the parked NIR cache.

Owner questions open: which jailbreak payloads/firmware (the 20 us system call
looks environmental), and whether a debugger/sampling payload is available.
