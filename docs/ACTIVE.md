## Where the port is

**Persistent shader caching is deployed and measured.** Driver `69a5c59`
saves immutable SPIR-V compiler outputs inside each title's
`/app0/ps5vk-shader-cache`. Normal deployment preserves them; shader/compiler
input changes invalidate entries. Failed or corrupt reads compile normally.
No GPU addresses or live resources are persisted.

Same port identity for both runs:
`78bd43a2e575089a96cf8dc561937dd7781c462fbcf051f2fa177ac0c55107b1`.

| Run | PID | First present | SPIR-V compiles | Cache hits | Stores |
| --- | --- | --- | --- | --- | --- |
| Cold | 202 | 30.410 s | 99 | 433 | 99 |
| Warm | 203 | 13.018 s | 0 | 532 | 0 |

Eight internal NIR shaders still compile in each run. Times include launch IPC
and one-second trace polling, not optical scanout. Both final trace reads match
per run, build identities and kernel PIDs match, and the console is idle.
Evidence: `evidence/m2-shader-cache-cold/` and `evidence/m2-shader-cache-warm/`.
Reproduce from here with the driver's jobs/shader-cache/benchmark-vkquake.py,
after this tree's gates, shader scan and verified deployment.

**M2 met:** PID 197 presented a frame, confirmed by the human as the Quake menu
or console. Evidence: `evidence/m2-first-frame/`. M3–M6 remain unaccepted.

## Current work

**R14–R18 accepted in the driver.** R17 `3be25f1` fixes descriptor arrays;
R18 `aafd697` fixes row mip placement and pitched 2D mip descriptors.
R18 PS5 PID 214: 531 PASS, zero FAIL; 19 complete mip frames match every pixel,
21 streams replay exactly. See driver jobs/r18-padded-mips/README.md.

**R19 accepted and game remains alive for 180 seconds.** Driver 8d11392 adds
UINT32 index widths/offsets/bounds and writes size for every indexed draw.
PS5 probe PID 216: 257 PASS, zero FAIL; three complete white pixel frames,
UINT16 before/after and five exact replays. Host 170 arms, eleven gates, port
five gates/scan and template relink PASS.

Port PID 217, identity 7d8aca4169312e1ab9a465c21c7af294b49cbdc493ef33c63788197c373af922,
presents, plays Necropolis demo events and advances to The Door To Chthon.
No refusal or Quake error. Still alive at 180 seconds, then closed by harness;
count=0 verified. Evidence: evidence/m3-r19-demo-run. Both final trace reads
match; kernel PID agrees. 433 cache hits, 99 stores after driver-key change.
No new visual acceptance; user unavailable for eight hours and authorized
autonomous continuation. R20 driver d8080dc corrects that diagnosis: the old probe misread tiled
bytes and never mapped its writer. PID 219 matches every pixel of both
attachments in two frames; no production driver change was necessary.

**Input adapter implemented and native pad opens.** PID 220 ran 300 seconds
without refusal/Quake error, at least 6,613 successful presentations, roughly
20–30 FPS at 4K. Identity 132d1474…; 532 shader-cache hits. Host checks cover
button transitions/release, menu repeat, triggers, deadzones and movement.
The existing DualSense backend opens successfully; physical interactions
await human confirmation. Evidence: evidence/m4-input-demo-run.
Audio adapter now feeds native 48 kHz stereo S16 output. PID 221 and 222 each
ran 300 seconds without refusal/Quake error or reported audio output failure.
PID 222 produced 12,960,000 stereo frames, 49,890 nonzero blocks and 6,335
presents; all 27 periodic audio reports are intact after serializing tracing.
Five gates and 30 evidence captures PASS. Audible quality awaits the user.
Evidence: evidence/m5-audio-initial and evidence/m5-audio-profile-run.
Driver R22 a85010a passes 1,043 console checks and 14 exact replays. Identical
target ranges now flush once per operation. Game PID 225: 300 seconds, 6,557
presents, 532 cache hits and no reported game/audio error. Flush cost falls
7.87 -> 5.79 ms/frame; FPS remains about 20–30 because flip wait grows.
Both trace reads match and PID agrees, harness closed, idle verified.
Evidence: evidence/m5-r22-flush-run. Native quit now hands off to the shell:
PID 226 executes map start, 120 waits and console quit, drains audio without
errors, writes both configuration files, and exits via LoadExec without SIGSYS.
Both final traces/deployed ELF/PID match, idle verified, test files restored.
Five gates and 32 evidence captures PASS. Evidence: evidence/m6-native-quit.
Single-thread rendering tested (PID 228): 6,345 versus 6,357 frames over the
first 27 steady intervals, no useful gain. r_tasks stays at its upstream default.
Evidence: evidence/m6-tasks0-benchmark, 33 capture replays PASS, fixtures removed.
Driver R23 4f8037f proves swapchain TRANSFER_SRC: four full-frame copies and
16 strict replays, 238 console checks pass. Port relink identity b5385c51….
First screenshot fixture (PID 230) wrote no images: queued protocol negotiation
was trapped behind its waits/quit. Retained as an incomplete screenshot test.
Map-only PID 232 then connects through sign-on 1–4 and renders the start map
for 90 seconds, 1,037 presents, steady 14.99 FPS, no game/audio error.
R24 fixes native realloc growth exhausting the private heap during PNG encoding.
PIDs 233/234 reproduce the failure; PID 235 diagnostics and PID 238 normal build
both write three 4K PNGs (start, menu, E1M1) and exit through LoadExec normally.
Diagnostic final report: zero failures/dropped records; native peak 8.40 MB.
Normal identity 9e7cced2…; five gates/scan and 39 evidence captures pass.
Both deployed ELF and final trace reads match, actual PIDs agree, idle verified;
three temporary/generated configuration paths restored absent after each run.
Images show textured/lit worlds, weapon and readable menu. R25 driver 90c28a1
fixes depth state leaking into UI passes. Relink PID 246 writes seven PNGs:
start/E1M1 and classic/transparent/modern HUDs are now visible. Normal LoadExec
exit, 2,550,528 audio frames/zero errors; deployment/final reads/PID agree.
Five gates/scan and 41 captures pass. Menu background remains black and open.
Evidence: evidence/m6-hud-depth-fixed; identity 8eff69e9…; configs restored absent.
Evidence: evidence/m6-png-{heap-exhaustion,heap-diagnosis,growth-diagnostic,growth-normal}.
R26 driver 86b4cc6 fixes the sampler bias 1 refusal exposed by PID 248.
Retest PID 251 completes five 1,200-frame phases, five matching PNGs and normal
exit; 21,079,808 audio frames/zero errors. Scale 1/2 and tasks 0/1 all average
14.84–14.86 FPS. Scale 2 renders correctly but doubles target-flush traffic.
Defaults unchanged; identity fc6ba13a…; configs/profile restored absent.
Evidence: m6-scale-sampler-{refusal,fixed}; 43 capture replays pass.
R27 PID 253 isolates the black menu to its fade overlay: compute-only shows the
world, fade-only and default hide it. Four PNGs and normal exit are verified;
temporary diagnostic removed. Evidence: m6-menu-fade-diagnosis, 44 captures.
Driver d8646ce restores the lost blend-control assignment. PID 259 normal
relink shows the faded world behind both menu openings, a transparent HUD and
start/E1M1 worlds in five verified PNGs. Clean exit, 2,399,744 audio frames/zero
errors; identity 3a6078fa…, all final/deployed reads/PID agree, configs restored.
Evidence: m6-menu-blend-fixed; 45 captures. R28 PID 261 completes two profiling
phases: CPU copies average 24.095 ms on start and 0.041 ms at E1M1 spawn;
sync wait/signal each 0.021 ms. Driver 9f9f395, identity d584176e…, clean exit,
7,082,496 audio frames/zero errors. Two identical PNG reads, deployed/final
reads and PID match; fixture/profile absent. Evidence: m6-copy-profile, 46 captures.
R29 integer filter PID 265: copy 22.657 ms, no FPS gain; candidate rolled back.
Evidence m6-mip-integer-benchmark; 47 captures. Next: shared tile-address
optimization, then gameplay/save/load soak.
M3–M6 remain unaccepted pending the respective evidence. CTS stays out of scope.

## Other open work

- M3 stable menu, M4 physical control and M5 audible confirmation remain open;
  M6 textured/lightmapped world without refusals.
- R10 quarter-width claim retired by corrected R20 readback; v0-lines and ImageQuery output remain.
- Exit SIGSYS fixed by shell handoff; staging path ignores EndCommandBuffer failure.
- Step 0 trace capture and reproducible build identity are closed.
