# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**The engine walks the whole of start-up and reaches the swapchain.** Build identity
`f11c4ca7d0db5680`, recorded as `evidence/m2-swapchain/` — the run in order:

```
vkCreateInstance -> 0 … vkCreateDevice -> 0 / VK_KHR_swapchain
Using D32_S8 depth buffer format
Creating command buffers / Initializing staging / Creating descriptor set layouts
Reallocating dynamic vertex, index and uniform buffers / Initializing samplers
Texture lod bias: 0.000000 / Creating pipeline layouts
Allocating lightstyles buffer (0 KB) / lights (12 KB) / submodel transforms (768 KB)
Allocating bmodel instances buffer (1024 KB)
Sound Initialization
Using FIFO present mode
assertion failed: … (driver/ps5vk_wsi.c:444, ps5vk_CreateSwapchainKHR)
```

- **Every gate that stood in front of this is passed, measured**: the depth-stencil format
  (D32_S8), `R_InitSamplers` (the anisotropy no-op), the pipeline layouts (five sets, because
  the driver's set limit is a draw-time check), the palette octree's whole-buffer view —
  the driver's R3 fix, end to end — and the engine's world buffers and sound initialisation.
- The stop is the swapchain, and it splits in two. The driver advertises
  `supportedUsageFlags = COLOR_ATTACHMENT` and honours exactly that; vkQuake hardcodes
  `COLOR_ATTACHMENT | TRANSFER_SRC` and never consults the capabilities, which is a
  valid-usage violation (`VUID-VkSwapchainCreateInfoKHR-imageUsage-01276`). **The port's half
  is fixed** (`platform/ps5/vkquake-edits.py`, the first use of the PLAN's second shape: an
  edit applied to a copy), by intersecting the request with what the surface reports. The
  driver's half is **R4**: an assert on application input aborts a title with nothing the
  application can act on, where its own style is a refusal that names the field.

**The one feature the intersection costs** is the screenshot: vkQuake copies from the
presented image and needs `TRANSFER_SRC`. It comes back by itself if the driver ever proves
and advertises that use for swapchain images — the intersection picks it up with no change —
so it is a named limitation rather than a workaround to retire.

The three earlier runs are still in the record and still replay: `evidence/m2-renderer/`
(the first run inside the renderer), `evidence/m2-loader/` (the loader aliasing this port was
missing, fixed in `platform/ps5/vk_loader.c`) and `evidence/m2-device/` (the 2026-09-20
depth-format stop, marked superseded).

## Next

**One more console run — the swapchain fix is deployed.** Past the swapchain the first frame
begins, and the next stop is *read* rather than measured: the first
`vkCreateGraphicsPipelines` refused because the driver checks the pipeline layout's bindings
rather than the shader's used ones, and vkQuake's `basic_pipeline_layout` names the
three-input-attachment set. That one is this tree's own work — `r_oit` off, the OIT/MBOIT
pipeline variants not created, the input-attachment set dropped, the bmodel set renumbered
4 to 3 — and it needs nothing from `../PS5_Vulkan`.

Behind it, in order: **R2**'s bare `SAMPLER` (the GUI pipelines, which vkQuake creates
unconditionally at start-up and the port cannot dodge cheaply), and **R4**'s refusal shape,
which costs nothing until an application asks for something the surface does not allow.

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
