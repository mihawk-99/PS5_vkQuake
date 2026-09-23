## Where the port is

**M2 met: the engine presented a visible frame.** R12 driver `c8658bf`,
port identity `a779b2bde40b52b3ce6ce84dc6a17325b0c5df883a9d61e09ea559ad10c0b342`,
PPSA99010 PID 197: 540 successful compiles, then `vkQueuePresentKHR -> 0`.
The human saw a frame before the crash and identified it as the Quake menu or
console. This is visibility evidence, not stable menu operation or a textured
world. M3–M6 remain unaccepted. Evidence: `evidence/m2-first-frame/`.

**The next failure is map staging memory.** After `the Necropolis` and
`Using protocol 15`, the trace reports:

```
[ScePthread/System] Internal Memory is running out.
[ps5vk] recording refusal in ps5vk_CmdCopyMemoryToImageKHR: no memory to record an image copy
vkEndCommandBuffer -> -1
```

The engine's staging path ignores that end result and submits the invalid
command buffer; Mesa asserts in vk_queue_submit_add_command_buffer. The kernel
records the resulting abort for PID 197 and termination. Two complete FTP reads
match (SHA-256 `f0975b88…`); the console reports count=0. The harness's scheduled
watch/cleanup remains responsible for this run; no second title is launched.

## Next

**R13: bound upload-record memory in the driver.** Source witness: row-layout
uploads append one 272-byte copy record per row; the port's allocator leaves
native reallocations native when they grow beyond its 32 KiB mapping threshold.
The driver already has a one-record region-copy executor. A host reproduction
should establish record-count and byte-correctness before a console probe.
This is a candidate cause of heap pressure, not a measured allocation census.
The staging path's ignored error is a separate port error-handling issue.

## Verified R12

Driver c8658bf explicitly rebuilt: all eleven gates and 167 check-driver arms
PASS. Console probe PID 196: both 64-wide baseline and padded 32-wide texture
matched all 2,073,600 pixels; nearest exact, bilinear maximum error 1. Four
streams replay identically with same-run defaults. Template gates PASS.
Archive: 14,385,036 bytes, SHA-256 `c37afdec…`. Port five gates and shader scan
PASS before PID 197; evidence replay afterward: 20 captures, zero failures.

## Open questions

- M3 needs stable menu acceptance; M4 input and M5 audio adapters are stubs;
  M6 needs a textured/lightmapped world without refusals.
- R10 subpass read matched only the first quarter-width on hardware; the
  cause remains unproved. `v0-lines` still fails; line guards remain.
- Menu `OpImageQuerySize` warnings need measured output.
- The earlier exit SIGSYS remains separate from PID 197's assertion abort.
- Step 0's trace capture and reproducible identity are closed. The earlier
  R12 stale-archive correction remains in the phase log; the user authorized
  resumption and the fresh build/hardware probe now prove the implementation.
