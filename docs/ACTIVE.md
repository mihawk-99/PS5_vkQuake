## Where the port is

**M2 met:** PID 197 presented a frame, confirmed by the human as the Quake menu
or console. Evidence: `evidence/m2-first-frame/`. M3–M6 remain unaccepted.

**R13 passed the earlier staging-memory stop.** Driver cb1fa76 records one copy
per region. Its explicit build, eleven gates, fifteen targeted check-driver
arms and PS5 PID 198 probes passed (486 PASS, zero FAIL, twelve exact replays).
The port passed all five gates and the shader scan before launch.

PPSA99010 PID 199, identity
`28581900784f866f50da7cab0eedba4a10d6a03d4816e8959833ad6e9b19f945`,
compiled 540 stages and presented successfully, then allocated map lightmap,
indirect-draw and visibility data without the prior OOM. It stopped on two
interleaved named refusals: tiled-chain blit and one set with two dynamic
offsets, then asserted on an indirect draw's stride. Upstream uses count=1,
stride=0 in r_brush.c. Two full FTP reads match (SHA-256 193e4095…); kernel
records PID 199 abort and termination; console count=0. Capture harness still
finishing its 900-second window. Evidence: `evidence/m2-r13-map-recording/`.

## Next

**User priority: persistent shader caching and measured faster warm launch.**
The driver pipeline-cache API is currently an empty stub; every stage compiles.
Implement persistence at the shared compiler boundary, with content-based
invalidation, corruption fallback and cold/warm witnesses before hardware.
Record the rendering failures above for later cycles; caching does not fix them.

## Open questions

- M3 needs stable menu acceptance; M4 input and M5 audio adapters are stubs;
  M6 needs a textured/lightmapped world without refusals.
- R10 subpass read matched only the first quarter-width on hardware.
- `v0-lines` still fails; line guards remain.
- Menu ImageQuery warnings need measured output.
- Earlier exit SIGSYS and ignored staging EndCommandBuffer errors remain.
- Step 0 trace capture and reproducible build identity are closed.
