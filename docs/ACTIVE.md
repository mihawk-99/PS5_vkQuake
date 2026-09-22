## Where the port is

**Start-up now compiles everything up to the postprocess pass, and the postprocess pass is where it
dies.** `evidence/m2-subpass-input/` (identity `b41844d2`) — the run in order:

```
… Creating pipelines
[ps5vk] compile done: result=0      (528 times — 267 pipelines, six of them the fragment-less
                                     sky marking passes: basic, warp, particles, FTE particles,
                                     sprites, sky, showtris, the whole world family, alias and
                                     md5 — no refusal anywhere)
SpvCapabilityInputAttachment (40)   28 bytes into the SPIR-V binary
ACO ERROR: aco_select_nir_intrinsics.cpp:5132: Unimplemented intrinsic instr:
           @load_input_attachment_coord
```

**R9 is answered and proven on the console.** The world family — sixteen permutations of one shader
built from five specialization constants — creates, and so do alias, md5 and every family before
them: 267 pipelines, 528 compiles, zero refusals, minutes of compiling at roughly one shader a
second. The run the port was waiting on (`docs/PS5_VULKAN_REQUESTS.md`, R9) is closed.

**The wall is now a subpass input read, and it is a different shape: it is an abort.** The shader is
`postprocess_frag` (`tools/check-shader-capabilities.py`; the five deployed shaders declaring
`OpCapability InputAttachment` are the three `R_CreatePostprocessPipelines` creates plus two MSAA
twins, and it creates `postprocess` first). The driver warns the capability away, hands the module to
ACO, and ACO has no instruction selection for the read, so the process dies instead of the pipeline
being refused. That is R10, and the port cannot dodge it: vkQuake's UI pass is two subpasses over two
colour attachments, subpass 1 reads subpass 0's colour buffer, and that one draw is the only thing
that ever writes the swapchain image.

## Next

**Waiting on `../PS5_Vulkan` for R10** — the read, and the abort turned into a refusal. Nothing for
the port to relink meanwhile; R1–R9 are closed on both sides.

If the driver publishes the read as permanently absent, the port's alternative is a UI pass with one
subpass drawing straight into the swapchain image. Not started: it deletes gamma and contrast over
the whole frame, so it is worth doing only against a published limit.

## Open questions

- **The title takes SIGSYS on its exit path, every run.** This port's bug, unchanged
  (`evidence/exit-sigsys/`). Its frames waited on a title and a map from the same build; every build
  since 2026-09-22 is deployed with its map beside it, so symbolising them is a matter of reading
  `build/title.map` against the run's frames.
- **The line guards stay until `v0-lines` passes.** They cost two pipelines that start-up does not
  create (`debug_lines`, `md5_debug`), and they retire on the driver's probe, not on a run here.
- **`OpImageQuerySize` in the menu upscaler.** `draw_pic_xbr_frag` and its alphatest twin declare
  `SpvCapabilityImageQuery`; the fork warns and compiles them anyway, fourteen times a run. If the
  lowering is missing the menu's scaling is wrong rather than absent — unmeasured, because no run has
  yet reached a menu.
- **The compute kernels are the next unknown.** `screen_effects_*`, `update_lightmap_*`, `skinning_*`,
  `mesh_interpolate`, `ray_debug` are created after the postprocess pass and declare capabilities
  (46, 49, 61, 65, 4472, 5347) this port has never asked the driver for. Named in
  `evidence/m2-shader-capabilities/` so the next stop is read rather than discovered.

## Blockers

**R10 blocks the frame.** The swapchain image is written by the subpass that reads an input
attachment, so until that read compiles there is no picture to present — black and alive, then dead,
is what this build does.
