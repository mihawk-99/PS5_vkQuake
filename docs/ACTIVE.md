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

**Same map-recording stop remains:** after presenting, PIDs 199, 202 and 203
reach lightmap/indirect/visibility allocations, then named tiled-chain blit
and multiple-dynamic-offset refusals and an indirect-stride assertion.
The cold cache survived that crash and was reused by the warm process.
R13 fixed the earlier staging upload OOM; caching does not repair rendering.

## Current work

The user explicitly resumed M6: continue until the game is stable and playable.
Driver R14 `2925ff6` fixes the valid single-draw count=1/stride=0 assertion.
Host indexed/non-indexed checks pass; PS5 probe PID 204 passes pixel readback
and exact stream replay (121 PASS, zero FAIL). Driver eleven gates, port five
gates and template relink pass. No port retry yet. R15 is now being verified
in the driver for independent dynamic uniform offsets; the named water-texture
mip blit remains after it. Input/audio adapters still need implementation.

## Verification

Explicit driver build zero warnings, 167 check-driver arms and eleven gates
PASS; persistent-key/corruption/fresh-process package tests PASS. Probe PIDs
200/201 each passed 241 checks, zero FAIL; twelve submissions replay exactly.
Template relink PASS. Port five gates and scan passed before launch, with scan
repeated before each console run; evidence replay now 23 captures, zero failed.
Archive: 14,425,130 bytes, SHA-256 e089e060… . No title is left running.

## Other open work

- M3 stable menu; M4 input and M5 audio engine adapters remain stubs;
  M6 textured/lightmapped world without refusals.
- R10 quarter-width subpass read, failing v0-lines and ImageQuery output.
- Earlier exit SIGSYS; staging path ignores EndCommandBuffer failure.
- Step 0 trace capture and reproducible build identity are closed.
