## Where the port is

**The world family is the wall, and it is a capability rather than a bug.** `evidence/m2-world-pipelines/`
(identity `1bd8b91f`) — the run in order:

```
vkCreateInstance -> 0 … vkCreateDevice -> 0 / VK_KHR_swapchain / D32_S8 depth buffer
staging, descriptor set layouts, samplers, pipeline layouts, world buffers, sound, swapchain
Creating pipelines
[ps5vk] compile start: nir=0 … [ps5vk] compile done: result=0      (276 times)
vkCreateGraphicsPipelines -> -13
QUAKE ERROR: vkCreateGraphicsPipelines failed (world 0) with code -13
```

Everything the port has met so far is closed and measured: the depth-stencil format,
`R_InitSamplers`, the pipeline layouts, the palette octree (R3), the colour buffer (R5), the
swapchain (its own edit), the descriptor types (R2), the dispatch path (R7), the fragment-less
pipelines (the `sky_stencil` marking pass creates), the two line-topology debug pipelines (the
port's own guards), and the world *family* is reached for the first time.

**The refusal is R9: specialization constants.** The world family's fragment stage carries a
five-entry `VkSpecializationInfo` (`gl_rmisc.c:3618-3625`), the driver refuses specialization
constants outright (`ps5vk_pipeline.c:621`), and no family called before it attaches one — so
`world 0` is the first pipeline in the engine to meet it. The port cannot dodge it: vkQuake's
sixteen world permutations *are* specialization constants, and the alias, md5 and postprocess
families use them too, so without R9 no world can be drawn.

**A correction to this file's earlier entry.** It said the world and alias families created in the
`m2-md5-debug` run. They did not: `md5_debug` is created inside `R_CreateShowTrisPipelines`, which
is called *before* the world family, so that run never reached it. `docs/PHASE_LOG.md` carries the
correction as its own entry, and the run above is the first to reach the world family.

## Next

**Waiting on `../PS5_Vulkan` for R9**, and on the driver's rebuild: the archive this port links
(`libps5vk.ps5.a`, 13:35) predates the five commits the last round landed (R8, the `DI_PT`
strip/fan correction, the fragment-less case, R4), so when the rebuilt archive arrives the port
relinks once and picks all of them up — and retires its two line guards at the same time.

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
