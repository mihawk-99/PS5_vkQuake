## Current objective

Make vkQuake stable, playable and faster on the PS5: stable 4K60 first, then
4K120 when a 120 Hz display is attached. The immediate assignment is an
implementation and measurement round, not another plan: measure the deployed R29
binary, attribute the largest cost, make one focused change, verify it on the
console, repeat. The owner confirms the deployed R29 game works and estimates
15-30 FPS; that estimate is not an instrumented benchmark. The heartbeat is
PAUSED: do not run automated fixtures or a harness that closes their game, and
never close a running session to obtain a benchmark. CTS is out of scope. M2 was
human-confirmed as a visible Quake menu/console. Never claim flawless gameplay
from a short run.

## Verified application state

Persistent shader cache driver 69a5c59 survives title restarts and application
crashes. Same binary, PIDs 202/203: cold/warm first present 30.410/13.018 seconds;
99/0 SPIR-V compiles, 433/532 cache hits. Eight internal NIR stages still compile
on each launch. Exact evidence: m2-shader-cache-cold and m2-shader-cache-warm.
Normal deployments preserve the cache; shader/compiler changes invalidate keys.

Native input adapter 79e9b6b opens DualSense. Host checks cover transitions,
release, menu repeat, triggers, deadzones and movement. Native audio d537cbc
feeds 48 kHz stereo S16 output and repeated 300-second runs report nonzero PCM
with zero native-output errors. Native quit a9ee9d2 uses shell LoadExec and no
longer produces SIGSYS. R24 7f9deae migrates growing native allocations to mmap,
fixing PNG heap exhaustion. Physical pad interactions and audible quality remain
unconfirmed: acceptance limits, not reasons to stop autonomous testing.

Driver rounds R14-R29 are closed and each keeps its proof in the driver's
jobs/r*-*/: vertex stride, dynamic UBO offsets, mip tails, descriptor arrays,
padded pitches and UINT32 indices (R14-R19); the retired quarter-width probe
allegation (R20); flush dedup (R22); swapchain TRANSFER_SRC (R23); depth state
leaking into colour-only UI (R25, PID 246 verifies every tested HUD style);
sampler LOD bias (R26, PID 251); blend control (R27, PID 259); copy/wait/signal
timing (R28, PID 261); the common tile address (R29). The normal source retains
fifteen exact upstream edits. Scale 2 and tasks 0 do not improve FPS; defaults
stay scale 1, tasks 1.

R28 driver 9f9f395 isolated the slow work: PID 261 start CPU copies averaged
24.095 ms against 0.041 ms at E1M1 spawn, which pointed at water mip generation
without identifying an operation. R29's integer filter cut that to 22.657 ms
with no FPS gain and was rolled back; R29's tile-address change then cut it to
11.619 ms and did move the frame rate. Evidence: m6-copy-profile,
m6-mip-integer-benchmark, m6-r29-baseline.

## The measured baseline, and what it says

**R29 is a real speedup and the controlled baseline now exists.** On the
deployed binary -- verified before the run by two served-ELF reads and all five
PT_LOAD segments against evidence/manual-r29-deployment -- the start map went
14.81 -> 19.72 FPS with CPU copies 24.095 -> 11.619 ms, and E1M1 was unchanged
at 29.58. Evidence: m6-r29-baseline.

The console went unreachable at 12:36 UTC holding a live application (`procs`
reported `title=PPSA99010 count=1`, from live kernel enumeration) and came back
idle at 12:40; nothing was staged, uploaded, launched or closed. Four runs then
followed, each with the owner's state snapshotted and restored exactly.

Two facts change how everything else should be read:

- **The frame period is quantised to whole 60 Hz vblanks.** frame_min_ms was
  49.976 ms at the start map and 33.292 ms at E1M1, against 16.6831 ms measured
  a vblank. A partial saving cannot raise FPS until a whole interval is crossed.
  That is what R22's and R29's "no FPS gain" results were really showing, and it
  means the work a frame does -- not FPS -- is what to measure.
- **The largest remaining cost is the application's own CPU: 21.7 ms a frame,
  identical at both maps**, in a run where the driver's queue cost differs
  threefold between them. host_speeds agrees from the other side -- of E1M1's
  35.4 ms frame, gfx is 33.9 and server 1.6. Evidence: m6-r31-frame-profile.

The application's calls are timed as well as the stretches between them now, and
`call_begin_ms` alone is 2.4 ms a frame: the `vkBeginCommandBuffer` calls are a
real per-frame cost no earlier counter could see, because a long call and a long
stretch looked the same. The rest of the 21.7 ms is therefore not all engine
code, and the next instrument is to time the recording entry points themselves.

Also measured: `gpu_ms` is not GPU execution (`submit_ms` is 0.169 ms a step and
the rest is the marker poll's 1 ms sleeps); a present waits exactly one vblank
and never zero; and the display's real cadence is 16.6831 ms over 60 intervals,
59.941 Hz, spread 0.108 ms.

**Do not log a line per frame on this console.** src/trace.cpp reopens stderr
unbuffered onto /app0/trace.txt, so every stdio write is its own write to that
filesystem. One summary line written as one fprintf per field cost 12% of the
frame budget and the run measuring it came out 10-13% slow; enabling
host_speeds, a line per frame, collapsed the game to 0.04 FPS. Both driver
summary lines are now a single write each for exactly this reason.

R31 does not regress the game: with no profiling flag and no probe, E1M1 runs
29.97 FPS at 33.37 ms -- the two-vblank floor -- and the start map 21.12, against
the baseline's 29.58 and 19.72 with profiling on. Evidence:
m6-r31-no-regression. That build (`dc19779c`) is what the console holds.

## The 120 Hz output path is established, by measurement

The console was moved to the owner's 4K120 Hz TV (120 Hz Output: Automatic), so
the display path was measured rather than assumed.

**It negotiated 4K60 and the mode was refused, because the title did not declare
it.** `sceVideoOutIsOutputSupported(handle, 15, ...)` returned 1 but
`sceVideoOutConfigureOutput(handle, 15, ...)` returned 0x80290016, identically
before the framebuffers were registered, after them, and on a handle that had
been opened and nothing more -- so neither the port's lifecycle nor the flip
rate is the variable.

**The gate is the title's own metadata.** `sce_sys/param.json` carried
`attribute3: 0`; the publicly released ps5-opengl SDK's folder builder sets
`attribute3 = 0x80040` when its profile is above 60 Hz, and records that a
title's metadata can reject the 120 Hz request. Setting that bit makes the same
call return 0.

**And the measurement confirms it is real, rather than a flag being flipped:**
the vblank period halves, from 16.6831 ms (59.941 Hz) to **8.3416 ms
(119.881 Hz)**, and the restore returns 0 and puts the period back to 16.6834 ms
(59.940 Hz). Confirmed at all three points in the port's life. Evidence:
m6-output-mode-120hz.

**What is proven and what is not.** The 120 Hz *output path* is proven on the
hardware: the console accepts the mode and scans out at 119.88 Hz, with a working
60 Hz fallback. The *frame rate* is not: that run presented about 29 distinct
frames a second, so the output repeats each frame roughly four times. A 120 Hz
panel showing a 29 FPS game is not 120 FPS, and the driver must not be described
as rendering at 120 Hz until distinct presented frames reach it.

The driver still reports 60000 millihertz. That is now a decision rather than an
unknown -- the mode is not engaged outside the opt-in probe, and reporting
120000 before the driver actually selects the mode would be a claim it has not
earned.

Next for this thread: configure the output mode for real at swapchain creation
when the metadata declares it, restore it at close, keep the 60 Hz fallback when
the mode is refused, and only then report the refresh truthfully -- which means
enumerating the modes rather than asserting one. Then the throughput work below
becomes the binding constraint, because 120 distinct frames a second needs the
whole frame under 8.333 ms.

## Next
## Next

1. **Find the 21.7 ms.** R31 times application stretches between Vulkan calls
   but not the calls themselves, and `app_pre_ms` exceeds the sum of its gaps by
   the duration of `vkGetQueryPoolResults`, every `vkBeginCommandBuffer`, every
   `vkEndCommandBuffer` and the acquire. Time those calls. Also make the gap
   chain thread-local, or decompose at `r_tasks 0`: the engine records on the
   main thread and presents on a worker, and two threads closing one shared
   chain is why the per-slot split is not yet quoted as a finding.
2. Then, in order of measured size at E1M1: the cache flush (3.96 ms, 256 MiB a
   frame over render targets the application never maps), and the marker poll's
   ~2.4 ms of 1 ms sleeps for a GPU that finishes in under a millisecond.
   Neither alone reaches 16.667 ms; the 21.7 ms has to come down too.
3. Stage build/r30-stage.py only when all three new save paths are absent. Its
   fixture moves/fires, saves, loads and visits all eight shareware maps;
   build/r30-saves.py retains saves twice and compares position/ammo/restoration.
   Then extend the soak. None of this is done yet.
4. The parked NIR cache in the driver (parked/nir-shader-cache) is unstarted
   console work: eight internal NIR stages still compile per launch, and its
   host cold/warm/disabled equality and eight strict replays pass.

Working notes. The scripts under build/ are ignored working files:
r29b-preserve.py owns the owner's state, r29b-stage.py takes `noprofile` to run
the same fixture with the instrumentation compiled in and never armed, and
r24-collect.py restores the configs by DELETING them so it must not be run
against the current state. A relink or a `tools/verify.sh` rewrites
build/title_build_identity.h, and run-title.sh validates the deployed binary
against it, so a rebuild between staging and a `--no-deploy` run makes that run
fail its identity check -- that is a local harness mismatch, not a deployment
failure; read the served ELF and the trace's own identity instead.

The console's config sets host_maxfps 200, r_scale 1, vid_vsync 0 and
r_waterwarp 1, so the engine's cap is not what limits the frame rate. r_scale is
not a resolution knob on this port -- every target is created at vid.width x
vid.height and the drawable is the one 3840x2160 mode -- so "scale 2 gives no
FPS gain" says nothing about fill rate.

While hardware is offline, a separate driver NIR-cache candidate passes host
fresh-process cold/warm/disabled output equality and eight strict mip replays.
Warm compilation count is zero; full driver/cache and eleven gates pass.
The patch is parked/nir-shader-cache in the driver; production source/archive
are restored to R29. Console/startup checks remain pending. Finish the saved
R29 benchmark before applying it.

## Persistence and acceptance

The original three configs were absent; build/r23-fixture-backup/manifest.json
is the original backup and must not be overwritten. The owner has since played,
so build/r29b-preserve/manifest.json is now the current-state snapshot: it
records presence or absence and the exact bytes of each configuration path, and
build/r29b-preserve.py restore puts that back. Never overwrite either manifest.
Preserve user saves and shader caches. Raw console data, downloaded images and
network settings stay ignored. Committed distilled evidence is under evidence/;
detailed historical steps are append-only in docs/PHASE_LOG.md and driver jobs/.

Still needed: gameplay/save/load/all-map soak, better frame rate, physical pad
and audible checks when the user returns, and the final release acceptance.
