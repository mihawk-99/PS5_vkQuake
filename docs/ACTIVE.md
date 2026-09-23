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

Driver rounds R14-R29 each keep their proof in the driver's jobs/r*-*/ and are
closed: vertex stride, dynamic UBO offsets, mip tails, descriptor arrays, padded
pitches, UINT32 indices (R14-R19); the retired quarter-width probe allegation
(R20); dedicated flush dedup (R22); swapchain TRANSFER_SRC (R23); depth state
leaking into colour-only UI (R25, PID 246 verifies every tested HUD style);
sampler LOD bias (R26, PID 251); blend control (R27, PID 259); copy/wait/signal
timing (R28, PID 261); the common tile address (R29). Scale 2 and tasks 0 do not
improve FPS; defaults stay scale 1, tasks 1. The normal source retains fifteen
exact upstream edits.

R28 driver 9f9f395 isolates the slow work: PID 261 start CPU copies average
24.095 ms, versus 0.041 ms at E1M1 spawn, which points at water mip generation
without pretending aggregate timing identifies one operation. R29's integer
filter lowered that to 22.657 ms with no FPS gain and was rolled back. Both keep
their evidence: m6-copy-profile and m6-mip-integer-benchmark.

## Manual deployment done; the console went away again

Driver c3e51f6 evaluates the existing common tile equation directly. PID 267
passes four full 4K mip frames and all generated lower texels; four strict
replays. PID 268 passes 3,501 checks across mip/upload/copy/format regressions.
Host checks cover 2,441,216 addresses and 144 random-colour blits. Explicit
rebuild, full host/cache, eleven driver gates, port five gates/scan and template
relink pass. Controlled R29 game performance remains unmeasured.

The already-built game identity is
e3525e30191b2b1ac4260f9fd476cbc6c6e34cf15a30ebf9f2af72b75ea88bc1.

**Manual deployment COMPLETE.** The saved R29 identity above is installed.
Two served ELF reads and all five PT_LOAD segments match; shader scan and
manifest pass. Game data/cache are present; no args, autoexec or
diagnostic/profile flags. The user subsequently launched it and confirmed the
game works. Their 15-30 FPS estimate is not an instrumented benchmark.
Evidence: manual-r29-deployment; 48 captures replay.

At 2026-09-23 12:36 UTC the console stopped answering again: control on 9111
returns no route to host. Just before that it held a live application --
`procs` reported `title=PPSA99010 count=1`, from live kernel process
enumeration, not a cached record -- so the owner had the game up and nothing
was staged, uploaded, launched or closed. This is the same class of event as
the earlier outage and is not evidence of a game crash.

## The next console session, in order

Driver R31 (0f52d0a) adds default-off instrumentation for what R28's means
cannot separate: the application's own CPU time before and after a submission,
the submission call apart from the marker poll loop, unsuccessful marker checks
per step, flip-status calls and vblank waits per present with their durations,
the interval between confirmed presents with its histogram, and a
sceVideoOutWaitVblank cadence probe. Host build clean, eleven gates and
check-driver/shader-cache pass; jobs/r31-frame-profile holds it. It changes no
packet, wait, flip or copy when profiling is off, and it is not an optimization:
the R29 baseline is still the first thing to measure.

1. `python3 build/r29b-preserve.py snapshot`, then `bash build/r29-run.sh
   baseline 250`. The scripts in build/ are ignored working files; preserve.py
   now owns the user's state, because r29b-stage.py refuses to stage unless a
   snapshot exists and r24-collect.py used to restore the configs by DELETING
   them. Do not run r24-collect.py against the current state: `vkQuake.cfg`
   (1020 bytes) and `id1/vkQuake.cfg` (2510 bytes) both exist now and are the
   owner's settings.
2. That run measures the deployed R29 with the existing counters, so its
   numbers compare directly with R28's. Read them with
   `python3 build/profile-metrics.py <capture>`; it reproduces
   evidence/m6-copy-profile/metrics.txt exactly from that capture.
3. Then relink the port against the R31 driver and run `build/r29-run.sh
   profile 250` for the decomposition. Relink only after step 2: run-title.sh
   validates the deployed binary against build/title_build_identity.h, so a
   relink before the baseline run makes the baseline unrunnable.
4. The fixture already samples `host_speeds`, which prints `tot/server/gfx/snd`
   per frame and has never been captured; Con_Printf output does reach the log,
   so step 2 returns the engine's own split too. Those samples run at r_tasks 0
   while the timed phases run at r_tasks 1 -- do not average them together.
5. Stage build/r30-stage.py only when all three new save paths are absent.
   Its fixture moves/fires, saves, loads, and visits all eight shareware maps.
   build/r30-saves.py retains saves twice and compares position/ammo/restoration;
   collect screenshots and remove only these test files. Then extend the soak.

The console's own config sets host_maxfps 200, r_scale 1, vid_vsync 0 and
r_waterwarp 1, so the engine's frame cap is not what limits the frame rate.
r_scale is not a resolution knob on this port -- every target is created at
vid.width x vid.height, the drawable is the one 3840x2160 mode, and r_scale 2
would not lower it either. Do not read the recorded "scale 2 gives no FPS gain"
as evidence about fill rate.

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
