## Where the port is

**R10 landed in the driver, and the port is relinked against it.** Driver `60041d8`, three commits:
`f9131e5` (a shader the compiler cannot lower is refused, not fatal), `3ee2f96` (the subpass read
through the input attachment's descriptor), `5f7e910` (the console case and its own measurements).
The archive this port now links is `build/driver/ps5/libps5vk.ps5.a`, 14,415,958 bytes,
`sha256 6e12550b…`, built 18:02 — and the relink is proved by content, not by clock: the deployed
`eboot.bin` carries the new refusal sentences (`strings` finds three of them, including
`AddressingModelPhysicalStorageBuffer64 not supported`).

```
port build identity   71b0989185c3a96cb69e54a3c5ffe8bb773b046260e0a8b4fb84f1ce988bc79a
dist/PPSA99010/eboot.bin   23,870,146 bytes   sha256 52b28b1a12c473b2…
deployed                   verified: the console reads this build's identity marker back
```

**What was wrong before this step.** The tree's artifacts were from 16:47 — the engine archive, the
eboot and the identity header all — while R10's first commit landed at 17:05 and the archive it
produced at 18:02. So the build that was standing had been relinked against the *R9* archive
`f3d749d6…`, not against R10. The claim "rebuilt against the latest driver" did not match the
workspace; the relink is this step's work.

**The next run should reach the first frame.** Of the port's 67 deployed shaders, 20 declare
capabilities beyond `Shader`; the driver's audit compiles 15 and refuses 6, and **all six belong to
vkQuake's ray-tracing paths**: `update_lightmap_{8,10}bit_rt_comp` and `ray_debug_comp` are created
only `if (vulkan_globals.ray_query)`, and `mesh_interpolate_comp`, `skinning_comp` and
`skinning_8_comp` return early when it is false. This driver advertises no ray query, so
`vulkan_globals.ray_query` is false and none of the six is ever created — pipeline creation should
now finish, start-up should complete, and the engine should present.

**The driver's own open defect is what to look for on the screen.** The subpass read works but is
correct only for `x < 960` of the 3840-wide display: past that the fetch returns band 0, because the
row-stored attachment's width word is `(extent.width - 1) >> 2`. So the first frame is expected to be
a picture whose right three quarters are wrong, and that is a driver item, not a port one
(`docs/PS5_VULKAN_REQUESTS.md`, R10's status note).

## Next

**One run: `bash tools/run-title.sh --no-build --no-deploy --watch 600`** (or a longer window — the
engine spends eight to ten minutes compiling shaders before it draws). What it must show: no refusal
line, no `ACO ERROR`, and a presented frame. Then `evidence/m2-first-frame/` with the trace's tail and
the console owner's word for what is on the screen.

## Open questions

- **The identity is not reproducible across rebuilds of identical inputs.** Two builds of the same
  sources and the same driver archive produced `31a85d8b…` and `71b09891…`, and the identity hashes
  the engine archive's own bytes. Either an input really moved or the build is not deterministic;
  unmeasured, and it matters because the identity is the only tie between a run and its sources.
- **The title takes SIGSYS on its exit path, every run.** This port's bug, unchanged
  (`evidence/exit-sigsys/`); `tools/symbolize-crash.py` needs `build/title.map` from the same build.
- **The line guards stay until `v0-lines` passes** — the driver's own open item; they cost two
  pipelines start-up does not create.
- **`OpImageQuerySize` in the menu upscaler** — `draw_pic_xbr_frag` and its alphatest twin declare
  `ImageQuery`; the fork warns and compiles them anyway, fourteen times a run. If the lowering is
  missing, the menu's scaling is wrong rather than absent — unmeasured, because no run has reached a
  menu yet.

## Blockers

**None on the port's side.** R10's read is in, the refusal is in, and the remaining driver defect
(the read past `x = 960`) degrades the picture rather than stopping the frame.
