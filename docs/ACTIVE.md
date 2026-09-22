## Where the port is

**Start-up is one debug pipeline from done: 275 pipelines compiled, one refusal left.**
`evidence/m2-debug-lines/` (identity `c899d93e`) — the run in order:

```
vkCreateInstance -> 0 … vkCreateDevice -> 0 / VK_KHR_swapchain
Using D32_S8 depth buffer format
staging, descriptor set layouts, samplers, pipeline layouts, world buffers, sound, swapchain
Creating pipelines
[ps5vk] compile start: nir=0 … [ps5vk] compile done: result=0      (275 times)
vkCreateGraphicsPipelines -> -13
QUAKE ERROR: vkCreateGraphicsPipelines failed (debug_lines) with code -13
```

Every gate this port has met is passed and measured: the depth-stencil format, `R_InitSamplers`, the
pipeline layouts, the palette octree's whole-buffer view (R3), the colour buffer (R5), the swapchain
(its first edit to upstream), the three descriptor types (R2), the compute path's shared tables
(R7 — the kernels that stopped two runs now compile), and **the fragment-less pipelines**:
vkQuake's `sky_stencil` marking pass, `stageCount = 1` with colour mask 0, is created.

The last refusal is `debug_lines`, the bounding-box debug draw, for
`VK_PRIMITIVE_TOPOLOGY_LINE_LIST` — the only line topology vkQuake asks for, since the FTE particle
family's line variants are gated on `non_solid_fill`, which this device does not claim. That is
**R8**, and the port is not waiting for it: the pipeline is created only when
`r_showbboxes`/`r_showfields` asks for the feature, mirroring upstream's own conditional creation in
the FTE family (the eleventh edit, retiring with R8).

## Next

**One more run.** With that edit, nothing known stands in front of the remaining families — world,
alias, md5, postprocess, screen effects, update-lightmap, indirect and the animation kernels all
compiled in this run's stretch — and then the first frame, whose postprocess pass binds an input
attachment: the one thing R2 implemented and has not driven.

If it stops, the trace names it and it becomes the next request. If it reaches a frame, the
artifact is that frame's readback or the trace line that says one was presented.

## Open questions

- **The title takes SIGSYS on its exit path, every run.** This port's bug, unchanged
  (`evidence/exit-sigsys/`). Its frames waited on a title and a map from the same build; every
  build since 2026-09-22 is deployed with its map beside it, so symbolising them is now a
  matter of reading `build/title.map` against the run's frames.
- **The menu path needs no accommodation after all.** vkQuake's GUI shader reads a separate
  `texture2D` and `sampler`, and the run that reached `R_CreateBasicPipelines` created the GUI
  pipelines in the same loop as `basic_alphatest` — so the driver's R2 published the separated
  pair and this port changed nothing for it.

## Blockers

**R8 is the last known item, and it is not in the way** (the port's edit covers it). Everything
before it is closed on both sides — the two driver gates this port
began with, its own loader aliasing, the swapchain's usage, the buffer view's sentinel, the image
usage, the descriptor types, the allocator threshold, and the strip topology — and what remains is
the dispatch path's binding rule, which start-up meets through kernels ordinary frames use.
