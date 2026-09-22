## Where the port is

**Start-up is finished. The port now reaches a frame, and stops inside the first frame's command
recording.** One run, `bash tools/run-title.sh --no-build --no-deploy --watch 900`, build identity
`71b0989185c3a96cb69e54a3c5ffe8bb773b046260e0a8b4fb84f1ce988bc79a` (`evidence/m2-end-command-buffer/`):

```
Creating pipelines
[ps5vk] compile done: result=0        538 times — no refusal, no ACO abort
   … the three input-attachment shaders R10 unblocked compile here, their
     SpvCapabilityInputAttachment warnings ending in result=0
vkEndCommandBuffer -> -13

ERROR-OUT BEGIN
                        <- empty: nothing names the refused command
QUAKE ERROR: vkEndCommandBuffer failed with code -13
```

Every earlier wall is behind it: the whole world family and the three specialization-constant
families after it (R9), the input-attachment read (R10), and the compute kernels created last
(screen effects, lightmaps, indirect). 269 pipelines now exist where the previous best run died at
138. No frame was presented, because the refusal is in *recording*, before the submit.

**The stop is R11, and it is the shape R4 exists to prevent:** a refusal that names nothing. The
driver's own recording refusals carry a message (`ps5vk_cmd_buffer_refuse` takes one), so either an
unnamed path set this error or the message is printed where the title's trace cannot see it. The port
cannot act on it — it cannot dodge a refusal it cannot read — so the next step is the driver naming
the command, which its host runner can do without a console.

**R10 works and its residual defect is the driver's.** The read compiles; on the console the driver
measured subpass 0's attachment correct (16 of 16) and subpass 1's fetch correct for the first
quarter-width only (past `x = 960` of 3840 it returns band 0 — `SQ_RSRC_IMG_WORD2`'s
`(extent.width - 1) >> 2`). So when a frame does present, expect it right on the left quarter and
wrong across the rest until that descriptor is fixed.

**The relink is in and proved by content**, not by clock: `libps5vk.ps5.a` 14,415,958 bytes,
`sha256 6e12550b…` (driver `60041d8`), `dist/PPSA99010/eboot.bin` 23,870,146 bytes,
`sha256 52b28b1a…`, deployed and verified by the console reading the identity marker back; `strings`
finds three of R10's new sentences in it, including
`AddressingModelPhysicalStorageBuffer64 not supported`.

## Next

**Waiting on `../PS5_Vulkan` for R11** — name the command the first frame's recording refuses. The
cheap route is the driver's own host runner replaying a recording; the console is not needed to find
it. Nothing for the port to change meanwhile.

## Open questions

- **The identity is not reproducible across rebuilds of identical inputs.** Two builds of the same
  sources and the same driver archive produced `31a85d8b…` and `71b09891…`, and the identity hashes
  the engine archive's own bytes. Unmeasured; it matters because the identity is the only tie between
  a run and its sources.
- **`tools/run-title.sh` cannot finish cleanly for this title**: it requires `retroarch.log`, which
  this port does not write (it writes `trace.txt`), so a good run still exits 1 with "Required
  development log retroarch.log was not captured". Inherited from the RetroArch tree; worth one edit.
- **The title takes SIGSYS on its exit path, every run** (`evidence/exit-sigsys/`);
  `tools/symbolize-crash.py` needs `build/title.map` from the same build.
- **The line guards stay until `v0-lines` passes** — the driver's own open item.
- **`OpImageQuerySize` in the menu upscaler** — warned and compiled anyway, fourteen times a run;
  the menu has not been drawn yet, so its effect is unmeasured.

## Blockers

**R11 blocks the frame.** The first frame is recorded, refused and never submitted, and the refusal
names nothing — so nothing can be fixed from this side until the driver says what it refused.
