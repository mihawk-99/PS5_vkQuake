# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**The engine now gets into the renderer on the console, and two long-standing gates are
measured rather than read.** Build identity `9ad1d1f36bae987c`, recorded as
`evidence/m2-renderer/` — the run in order:

```
vkCreateInstance -> 0
Instance extensions: VK_KHR_surface, VK_KHR_display, VK_KHR_get_physical_device_properties2
Vendor: AMD / Device: PS5 AGC GPU (ps5vk) / vkCreateDevice -> 0
Device extensions: VK_KHR_swapchain
Using D32_S8 depth buffer format
Creating command buffers / Initializing staging / Creating descriptor set layouts
Reallocating dynamic vertex, index and uniform buffers / Initializing samplers
Texture lod bias: 0.000000
Creating pipeline layouts
QUAKE ERROR: vkCreateBufferView failed with code -13
```

- **The depth-stencil gate is passed.** `D32_S8` is chosen and the device comes up: the
  driver's round-12 feature bits work for a real application, which is what R1 asked for
  and the 2026-09-20 run could not reach.
- **`R_InitSamplers` is passed** — the anisotropy flag is accepted as the no-op the driver
  proved it is, so the gate that stood here since the port's first console run is gone.
- **Pipeline layouts are created**, including the five-set world layout: the driver's
  set-count limit is a draw-time check, not a layout-time one.
- It stops in `R_CreatePaletteOctreeBuffers` on `vkCreateBufferView`, which is a driver
  bug with a one-line cause: **R3** in `docs/PS5_VULKAN_REQUESTS.md`.

The two earlier runs are still in the record and still replay: `evidence/m2-loader/` (the
loader aliasing this port was missing, fixed in `platform/ps5/vk_loader.c`) and
`evidence/m2-device/` (the 2026-09-20 depth-format stop, marked superseded).

## Next

**Waiting on `../PS5_Vulkan` for R3** — one line in `ps5vk_CreateBufferView`, and the port
declines to work around it, because patching upstream's `VK_WHOLE_SIZE` into an explicit
size would hide a class that any application hits. Nothing in this tree can move the run
past the palette octree until it lands.

Work that does not wait on it, in the order it becomes useful:

1. **The refusal sentences.** The driver's refusals reach an application only through a
   `VK_EXT_debug_utils` messenger, which vkQuake enables only in its `_DEBUG` builds, so
   every stop so far has been a bare `-13` and each one took a code read to name. The
   port's trace captures stderr, so a messenger installed where the shim first sees the
   instance would print the driver's own sentence into `/app0/trace.txt`. Worth doing
   before the next run, not after.
2. **The descriptor dodge, once the run reaches the first frame**: `r_oit` off, the
   OIT/MBOIT pipeline variants not created, the input-attachment set dropped from every
   surviving layout, and the bmodel set renumbered 4 to 3 — four sets, inside the
   advertised limit, and no new driver capability needed.
3. Then **R2**'s bare `SAMPLER`, which is what the GUI pipelines need and the port cannot
   dodge cheaply.

The stop after that is *read* rather than measured, and is unchanged from the reading
below: the first `vkCreateGraphicsPipelines` refused by name, because the driver checks
the **pipeline layout**'s bindings rather than the shader's used ones
(`ps5vk_descriptor_options`, `../PS5_Vulkan/driver/ps5vk_pipeline.c:195-220`, fragment
call at `:1265`), and vkQuake's `basic_pipeline_layout` is
`{single_texture, mboit_input_attachment}` — three `INPUT_ATTACHMENT` bindings at
`FRAGMENT` stage, a type `ps5vk_descriptor_stride` has no entry for.

**The lazier fact underneath it: the driver owes this port nothing for a first world
draw.** `r_oit` defaults to `1` (WBOIT), and that is the only reason two colour
attachments appear in vkQuake's main pass at all — `attachmentCount = resolve ? 3 : 2`
counts colour **and** depth, subpass 0 has `colorAttachmentCount = 1`, and `vid_fsaa`
defaults to `0`. With OIT off the plain pass is one colour attachment, so **MRT is
what restores OIT later, not what the port waits on now.**

What the driver's descriptor table *is* missing is three core 1.0 types vkQuake
declares — `INPUT_ATTACHMENT`, `SAMPLER`, `SAMPLED_IMAGE` — while advertising
per-stage limits for all three. That is `docs/PS5_VULKAN_REQUESTS.md` R2.

## Open questions

- **The title takes SIGSYS on its exit path, every run.** This port's bug, unchanged
  (`evidence/exit-sigsys/`). Its frames are unsymbolized because `build/title.map`
  came from a later build than the binary that crashed, so it waits on the same thing
  the next step is: a freshly built title and a run of it.
- **The descriptor shapes are the port's own work after that run.** vkQuake declares
  five sets for the world pipeline where the driver advertises four, and three of its
  layouts name types the driver has no entry for. The cheap route: do not create the
  OIT pipeline variants, drop the input-attachment set, renumber the bmodel set from 4
  to 3 — four sets, no merge of the three texture sets, and nothing asked of the
  driver for the world. The menu path is the harder one: vkQuake's GUI shader takes a
  separate `texture2D` and `sampler`, which is R2's second and third type.
- **The lightmap compute path was recorded as blocked on storage images.** The driver
  has since shipped compute dispatch, storage-image descriptors and storage-image
  format bits (`d2-compute`, `v0-storage-image`), but its table still has no
  `SAMPLED_IMAGE` entry, which vkQuake's `lightmap_compute` layout uses twice. Re-check
  this before treating M6's lightmap step as blocked.

## Blockers

**None on the driver's side for this port's next steps.** The driver advertises
`maxBoundDescriptorSets = 4` and honours it, which the shapes above fit inside; the two
gates this port was waiting on are closed. What is left is in this tree.
