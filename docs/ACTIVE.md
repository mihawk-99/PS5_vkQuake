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
Opt-in driver profiling measures 509.54 MiB of target cache eviction per frame,
7.87 ms/frame; next is duplicate-flush removal with console pixel regressions.
M3–M6 remain unaccepted pending the respective evidence. CTS stays out of scope.

## Other open work

- M3 stable menu, M4 physical control and M5 audible confirmation remain open;
  M6 textured/lightmapped world without refusals.
- R10 quarter-width claim retired by corrected R20 readback; v0-lines and ImageQuery output remain.
- Earlier exit SIGSYS; staging path ignores EndCommandBuffer failure.
- Step 0 trace capture and reproducible build identity are closed.
