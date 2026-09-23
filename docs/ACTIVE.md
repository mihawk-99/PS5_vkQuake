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

**Relink/launch completed; M6 remains open.** Port PID 215, identity
`b3aecd67729ccd91e5ec0fdeb9330e96a9d4e4923ad859aed3a5782fbcd061f1`,
presents and reaches demo1/the Necropolis map recording. R17/R18 refusals are
absent. The next named refusal is "32-bit indices need a runner probe" in
ps5vk_cmd_draw; EndCommandBuffer -13, exit 1, known SIGSYS exit path.
Next: R19 in the shared driver, including byte offsets/bounds and index type,
then PS5 pixel verification and another game launch. Input/audio remain stubs.
The user prioritizes a running, stable, optimized vkQuake; console CTS is out
of scope. No new visual acceptance is claimed for PID 215.

## Verification

R18 archive 14,434,994 bytes, SHA-256 cef1d817…; explicit driver rebuild,
21 targeted host arms, eleven gates, port five gates/scan and template pass.
Two deployed ELF reads match all five PT_LOAD segments. Two final trace reads
match; kernel PID 215 agrees; count=0 verified. Evidence:
`evidence/m2-r18-map-recording/`; 26 captures replay with zero failures.
This launch reused 532 cache hits, zero SPIR-V compiles/stores and eight NIR
compiles. No new startup timing measurement. Driver failed/partial captures
remain preserved alongside complete PID 214 evidence.

## Other open work

- M3 stable menu; M4 input and M5 audio engine adapters remain stubs;
  M6 textured/lightmapped world without refusals.
- R10 quarter-width subpass read, failing v0-lines and ImageQuery output.
- Earlier exit SIGSYS; staging path ignores EndCommandBuffer failure.
- Step 0 trace capture and reproducible build identity are closed.
