# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**The engine reaches its first pipeline, and the compile inside it runs out of the console's
internal memory.** The last two runs, in order — `evidence/m2-pipelines/` (identity
`56074ab0`) and `evidence/m2-internal-heap/` (identity `5cdf664e`, the allocation report):

```
vkCreateInstance -> 0 … vkCreateDevice -> 0 / VK_KHR_swapchain
Using D32_S8 depth buffer format
Creating command buffers / staging / descriptor set layouts / samplers / pipeline layouts
Allocating lightstyles, lights, submodel transforms, bmodel instances buffers
Sound Initialization / Using FIFO present mode
Creating color buffer / Creating depth buffer / Creating render passes / frame buffers
Creating pipelines
[ps5vk] compile start: nir=0 words=880bd4d48
[ScePthread/System] Internal Memory is running out.
```

Every gate before that is passed and measured: the depth-stencil format, `R_InitSamplers`, the
pipeline layouts, the palette octree's whole-buffer view (R3), the colour buffer (R5), the
swapchain (the port's first edit to upstream), and — with the driver's R2 — **pipeline creation
itself**. The last stop is this tree's own: the engine's start-up had filled the console's small
internal heap before the compiler asked for anything, and `evidence/m2-internal-heap/` has the
numbers and the callers (see below). The threshold that decides what leaves that heap is 32 KiB
now instead of 1 MiB; the run after this one says whether the compile fits.

## The reading behind the next stop

The first `vkCreateGraphicsPipelines` is refused by name, because the driver checks the
**pipeline layout**'s bindings rather than the shader's used ones
(`ps5vk_descriptor_options`, `../PS5_Vulkan/driver/ps5vk_pipeline.c:195-220`, the fragment
call at `:1265`), and vkQuake's `basic_pipeline_layout` is
`{single_texture, mboit_input_attachment}` — three `INPUT_ATTACHMENT` bindings at `FRAGMENT`
stage, a type `ps5vk_descriptor_stride` has no entry for.

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

## Open questions

- **The title takes SIGSYS on its exit path, every run.** This port's bug, unchanged
  (`evidence/exit-sigsys/`). Its frames are unsymbolized because `build/title.map`
  came from a later build than the binary that crashed, so it waits on the same thing
  the next step is: a freshly built title and a run of it.
- **Whether the menu needs its own accommodation.** vkQuake's GUI shader reads a separate
  `texture2D` and `sampler` in two sets; R2 gives it the types, and if a run still refuses
  there the choice is a second edit (a combined sampler in the GUI path) or waiting for the
  driver. Not decided until a run says which.

## Blockers

**R2 is the one thing in the way of the first frame** — the input-attachment type for the
basic pipelines, the bare sampler for the GUI ones, the sampled image for the lightmap
compute pass. Everything else that stopped a run so far is closed, and the port's own half of
this one is built and waiting on a gate that is not its own.
