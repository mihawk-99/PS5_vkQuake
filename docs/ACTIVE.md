# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**Every pipeline the engine creates now compiles; the stop is one driver topology.**
`evidence/m2-warp-strip/` (identity `4dc9c645`) — and behind it, in order, `m2-pipelines/`
(identity `56074ab0`), `m2-internal-heap/` (identity `5cdf664e`, the allocation report), and the
runs before those:

```
vkCreateInstance -> 0 … vkCreateDevice -> 0 / VK_KHR_swapchain
Using D32_S8 depth buffer format
Creating command buffers / staging / descriptor set layouts / samplers / pipeline layouts
Allocating lightstyles, lights, submodel transforms, bmodel instances buffers
Sound Initialization / Using FIFO present mode
Creating color buffer / Creating depth buffer / Creating render passes / frame buffers
Creating pipelines
[ps5vk] compile start: nir=0 … [ps5vk] compile done: result=0   (for every pipeline created)
vkCreateGraphicsPipelines -> -13
QUAKE ERROR: vkCreateGraphicsPipelines failed (warp) with code -13
```

Every gate before that is passed and measured: the depth-stencil format, `R_InitSamplers`, the
pipeline layouts, the palette octree's whole-buffer view (R3), the colour buffer (R5), the
swapchain (the port's first edit to upstream), pipeline creation with the driver's R2, and — after
this tree's own fix — **every pipeline compile**, which is what the `result=0` lines say. The
internal-heap failure and its numbers are in `evidence/m2-internal-heap/`. The last stop is a
driver limitation: `TRIANGLE_STRIP`, which the warp pipeline's tessellated mesh needs and
`../PS5_Vulkan` maps only triangle lists for. That is **R6**.

## The reading behind the next stop

Superseded by measurement: the pipeline-creation stop below is what the run reached *before* the
driver's R2, and it is kept because the reading is what the port predicted three driver rounds
early. The stop now is R6.

The first `vkCreateGraphicsPipelines` was refused by name, because the driver checks the
**pipeline layout**'s bindings rather than the shader's used ones
(`ps5vk_descriptor_options`, `../PS5_Vulkan/driver/ps5vk_pipeline.c:195-220`, the fragment
call at `:1265`), and vkQuake's `basic_pipeline_layout` is
`{single_texture, mboit_input_attachment}` — three `INPUT_ATTACHMENT` bindings at `FRAGMENT`
stage, a type `ps5vk_descriptor_stride` had no entry for.

Two facts make the dodge cheap, and both are reads rather than measurements: `r_oit` defaults
to `1` (WBOIT), and that is the only reason two colour attachments appear in vkQuake's main
pass at all — `attachmentCount = resolve ? 3 : 2` counts colour **and** depth, subpass 0 has
`colorAttachmentCount = 1`, and `vid_fsaa` defaults to `0`. With OIT off the plain pass is one
colour attachment, so **MRT is what restores OIT later, not what the port waits on now**.

Behind that, `../PS5_Vulkan` owes the two things this port cannot do for itself: **R2** (the
three core 1.0 descriptor types vkQuake declares while the driver advertises per-stage limits
for all three) and **R4** (the refusal shape, which costs nothing until an application asks
for something the surface does not allow).

The refusal *sentences* are deliberately not on that list: vkQuake enables
`VK_EXT_debug_utils` only in its own `_DEBUG` builds, so a port-side messenger would have to
inject the extension into the engine's instance creation — a hack in the application to hear a
sentence that belongs to the driver's logging path, which the driver has recorded as its own
open item with that scope.

## Next

**Waiting on `../PS5_Vulkan` for R6** — one topology mapped where the triangle list is. The port
cannot dodge it: the warp mesh is tessellated (two vertices per row, drawn as one strip), so a
list would mean changing how upstream generates it, which is the class of workaround this tree
declines.

Nothing else is pending on this side. The run after R6 should reach the first frame, whose
postprocess pass binds an input attachment — the one thing R2 implemented and has not driven.

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

**R6 is the one thing in the way of the first frame.** Everything else that stopped a run so far
is closed: the two driver gates this port began with, its own loader aliasing, the swapchain's
usage, the buffer view's sentinel, the image usage, the descriptor types, and the port's own
allocator threshold. What remains is one topology the driver has never mapped, and a request.
