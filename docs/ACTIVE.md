## Current objective

Make vkQuake stable, playable and faster on the PS5. The user authorized
uninterrupted work while unavailable for eight hours; do not wait for questions
or physical confirmation. CTS is out of scope. M2 was human-confirmed earlier
as a visible Quake menu/console. Never claim flawless gameplay from a short run.

## Verified application state

Persistent shader cache driver 69a5c59 survives title restarts and application
crashes. Same binary, PIDs 202/203: cold/warm first present 30.410/13.018 seconds;
99/0 SPIR-V compiles, 433/532 cache hits. Eight internal NIR stages still compile
on each launch. Exact evidence: m2-shader-cache-cold and m2-shader-cache-warm.
Normal deployments preserve the cache; shader/compiler changes invalidate keys.

Driver R14–R19 fixes cover vertex stride, dynamic UBO offsets, mip tails,
descriptor arrays, padded pitches and UINT32 indices. R20 retires the earlier
quarter-width allegation: the old probe read tiled memory incorrectly. Its
corrected writer and reader match every pixel. Driver jobs retain the proofs.

Native input adapter 79e9b6b opens DualSense. Host checks cover transitions,
release, menu repeat, triggers, deadzones and movement. Physical pad interactions
remain unconfirmed. Native audio d537cbc feeds 48 kHz stereo S16 output and
repeated 300-second runs report nonzero PCM with zero native-output errors.
Audible quality remains unconfirmed. These are remaining acceptance limits,
not reasons to stop autonomous testing.

Native quit a9ee9d2 uses shell LoadExec and no longer produces SIGSYS. R24
7f9deae migrates growing native allocations to mmap, fixing PNG heap exhaustion.
PID 238 writes three 4K PNGs and exits normally. Driver R25 90c28a1 fixes depth
state leaking into colour-only UI: PID 246 verifies all tested HUD styles.
Driver R26 86b4cc6 fixes sampler LOD bias. PID 251 completes five 1,200-frame
scale/tasks phases and five correct PNGs, with 21,079,808 audio frames/zero errors.
Scale 2 and tasks 0 do not improve FPS; defaults stay scale 1, tasks 1.

R27 driver d8646ce restores pipeline blend control. Normal game PID 259 shows
a correctly faded world behind both menu openings, a transparent HUD, and
textured start/E1M1 worlds in five readbacks. Clean exit, 2,399,744 audio frames,
zero errors; deployment/final trace/PNG reads and kernel PID agree. Temporary
menu diagnostic was removed; normal source retains fifteen exact upstream edits.
Evidence: m6-menu-blend-fixed. Configurations restored absent.

R28 driver 9f9f395 isolates the slow work: PID 261 start CPU copies average
24.095 ms, versus 0.041 ms at E1M1 spawn. Queue sync wait/signal are each
0.021 ms. Both 1,200-frame phases, two PNGs and native exit pass. This points
to water mip generation, without pretending aggregate timing identifies one
individual operation. Evidence: m6-copy-profile.

R29 first integer filter PID 265 lowers copy cost to 22.657 ms, without an FPS
gain (start about 14.81, E1M1 29.58). It was rolled back. Two PNGs, normal exit,
6,891,008 audio frames/zero errors; all final/deployed reads/PID agree. Fixture
and profile absent. Evidence: m6-mip-integer-benchmark; 47 captures replay.

## Next runs and current interruption

Driver c3e51f6 evaluates the existing common tile equation directly. PID 267
passes four full 4K mip frames and all generated lower texels; four strict
replays. PID 268 passes 3,501 checks across mip/upload/copy/format regressions.
Host checks cover 2,441,216 addresses and 144 random-colour blits. Explicit
rebuild, full host/cache, eleven driver gates, port five gates/scan and template
relink pass. Game performance remains unmeasured.

The already-built game identity is
e3525e30191b2b1ac4260f9fd476cbc6c6e34cf15a30ebf9f2af72b75ea88bc1.
At about 09:24 UTC the console became unreachable: FTP/control/klog all return
no route, local route exists, neighbor failed, discovery has no response.
Deployment failed before connecting. No new executable or fixture was uploaded.
Last game PID 265 and probe PID 268 exited/closed normally; the cause of network
loss is unknown. Do not describe this as a proven game crash.

1. Retry control/FTP. Once reachable, require count=0, deploy the saved R29
   game, verify two served ELF reads/all load segments, then stage
   build/r29b-stage.py. It checks configs/profile absent before uploading.
2. Scan shaders immediately before launch; run the listener first using
   tools/run-title.sh --no-build --no-deploy --watch 250. The harness waits its
   whole watch even after exit; finish it before another title launch.
3. Collect via build/r24-collect.py r29b, remove the profiling flag separately,
   verify actual PID/final trace/readback twice and compare steady timing with
   R28. The fixture also samples built-in host_speeds after each timed phase.
4. Stage build/r30-stage.py only when all three new save paths are absent.
   Its fixture moves/fires, saves, loads, and visits all eight shareware maps.
   build/r30-saves.py retains saves twice and compares position/ammo/restoration;
   collect screenshots and remove only these test files. Then extend the soak.

While hardware is offline, a separate driver NIR-cache candidate passes host
fresh-process cold/warm/disabled output equality and eight strict mip replays.
Warm compilation count is zero; full driver/cache and eleven gates pass.
The patch is parked/nir-shader-cache in the driver; production source/archive
are restored to R29. Console/startup checks remain pending. Finish the saved
R29 benchmark before applying it.

## Persistence and acceptance

The original three configs were absent; build/r23-fixture-backup/manifest.json
is the original backup and must not be overwritten. Preserve user saves and
shader caches. Raw console data, downloaded images and network settings stay
ignored. Committed distilled evidence is under evidence/; detailed historical
steps are append-only in docs/PHASE_LOG.md and driver jobs/.

Still needed: gameplay/save/load/all-map soak, better frame rate, physical pad
and audible checks when the user returns, and the final release acceptance.
