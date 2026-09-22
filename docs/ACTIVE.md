## Where the port is

**M2 is still blocked in first-frame recording, now by a named texture pitch
limit.** Build `6b437103029f1702f911ecb5ea75785133a2880388ea3ec74804cf354ef4d705`,
console PPSA99010 PID 195: 540 successful shader compiles, then:

```
[ps5vk] recording refusal in ps5vk_sampled_image: set 0 binding 0 samples a 32-texel-wide image whose rows are padded to 256 bytes; the descriptor's row pitch needs a runner probe (docs/M5_REFERENCE.md, C4)
vkEndCommandBuffer -> -13
QUAKE ERROR: vkEndCommandBuffer failed with code -13
```

No `vkQueuePresentKHR` result occurred. Earlier successful end/submit calls
are startup uploads, not a presented frame. Two FTP reads are byte-identical
and the kernel listener identifies PID 195. Evidence:
`evidence/m2-texture-row-pitch/`.

**R11 made measurable progress.** Driver `0e33761` replaces the optional
inheritance-framebuffer refusal with Mesa's owned secondary command queue,
replayed into the primary's actual attachments. Recording failures print their
command and sentence without a debug messenger. Its runner PID 194 passed
secondary, presentation and render-to-texture probes. The port now reaches
texture descriptor construction. R11's positive first-frame criterion remains
open; the negative trace criterion is met by this boot.

The port now traces the first end/submit/present result and every error.
The host test exercises the real wrappers with successful and failed calls.
All five port gates passed before this run; evidence replay passes afterward.
The linked driver archive is 14,383,172 bytes, SHA-256 `65550cae…`.

## Next

**R12: padded sampled-image rows**, owned in the driver under the mission's
cross-tree authorization. Cheapest source witness: the image upload pads to
256 bytes but descriptor word 4 never carries that pitch. Prove the encoding
with a 32-wide texture readback in the driver's runner before another port boot.
No port texture workaround or visual setting change.

## Open questions

- M2 requires a presented frame plus human screen confirmation; neither has
  occurred. M3 menu, M4 input, M5 audio and M6 world remain unaccepted. Engine
  input/audio adapters are still stubs.
- R10's subpass read matched only the first quarter-width on hardware; the
  cause is not yet proved. `v0-lines` still fails, so line guards remain.
- `OpImageQuerySize` menu upscaler warnings need measured output.
- The exit SIGSYS remains open (`evidence/exit-sigsys/`).
- Step 0 is closed: the harness requires `trace.txt` and checks its newest
  identity. `SOURCE_DATE_EPOCH=0` makes two identical engine builds byte-equal.
  Evidence: `evidence/harness-identity/`.

## Stop condition

The first R12 host check was mistakenly described as validating new code but
used the old archive. The explicit rebuild failed on an unavailable ALIGN
macro. Per the mission's contradiction rule, work stopped and the unverified
candidate is parked in `../PS5_Vulkan/parked/r12-row-pitch/`; the phase log has
a separate correction. This does not alter the measured R11 boot above.
The harness finished with count=0 and capture status 0; no title remains running.
