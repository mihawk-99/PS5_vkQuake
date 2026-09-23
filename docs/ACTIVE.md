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

**R14–R16 accepted in the driver and exercised by vkQuake.** Single-draw
stride, independent dynamic UBO offsets and tiled water mip blits pass their
PS5 probes. Driver R16 `c7f6f95`, PID 207: 277 PASS, zero FAIL; all four mip
frames match all 8,294,400 pixels, all 87,040 lower texels match independently,
and ten streams replay exactly. Earlier failed PID 206 evidence is retained.

**Requested relink/launch completed; M6 still open.** Port PID 208, identity
`b6a1e9540a03996b94a42346de7e0868fb339b883ea0ddab6e84d1407c6f126f`,
presented a frame and reached demo1/the Necropolis map recording. The three
previous map failures are absent. Two new named refusals then stop recording:
set 0 binding 2 has three descriptors; set 0 binding 0 needs a padded pitch of
256 texels outside the measured custom-pitch descriptor coverage. Full image
shape/format is not yet measured; do not assume mip count or array layer count
from that sentence alone. EndCommandBuffer returns -13; exit 1 takes the known
SIGSYS exit path. The game is not yet playable.

Two final trace reads match and kernel PID 208 agrees. The two-minute harness
ended with count=0; no title remains running. First-present on-screen check
was requested and is pending. Evidence: `evidence/m2-r16-map-recording/`.
Next driver witnesses: R17 sampled-image descriptor array with distinct entries,
then R18 reproduce and measure the refused padded image shape; see
`docs/PS5_VULKAN_REQUESTS.md`. Input/audio adapters still need implementation.

## Verification

R16 explicit driver build zero warnings; fifteen targeted check-driver arms,
cache checks and all eleven gates PASS. Archive 14,428,778 bytes, SHA-256
8d5206d5d4fc1535c342916b71c81e57d62ae4086d14fcd993074bc4c2fc8c67.
Port five gates, per-run shader scans and template relink PASS. Two deployed
ELF reads match all five PT_LOAD segments. Evidence replay: 24 captures,
zero failed. Changed driver header invalidated prior shader-cache entries as
designed: PID 208 had 99 SPIR-V compiles/stores, 433 hits and eight NIR compiles.
No new warm-start timing was measured.

## Other open work

- M3 stable menu; M4 input and M5 audio engine adapters remain stubs;
  M6 textured/lightmapped world without refusals.
- R10 quarter-width subpass read, failing v0-lines and ImageQuery output.
- Earlier exit SIGSYS; staging path ignores EndCommandBuffer failure.
- Step 0 trace capture and reproducible build identity are closed.
