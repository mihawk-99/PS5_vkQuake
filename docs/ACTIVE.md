# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**The engine reaches its first pipeline.** Build identity `56074ab0e0a278f5`, recorded as
`evidence/m2-pipelines/` — the run in order:

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
Creating color buffer / AA disabled / Creating depth buffer
Creating render passes / Creating frame buffers / Creating pipelines
QUAKE ERROR: vkCreateGraphicsPipelines failed (basic_alphatest) with code -13
```

- **Every gate that stood in front of this is passed, measured**: the depth-stencil format
  (D32_S8), `R_InitSamplers` (the anisotropy no-op), the pipeline layouts (five sets, because
  the driver's set limit is a draw-time check), the palette octree's whole-buffer view (R3,
  end to end), the world buffers, sound, and now the **swapchain** — the intersection with
  `supportedUsageFlags` in `platform/ps5/vkquake-edits.py` was what it needed.
- The colour buffer creates (R5), the depth buffer, the render passes and the framebuffers
  all follow, and the stop is **the prediction this file carried for three driver rounds**:
  the first `vkCreateGraphicsPipelines` refused, naming `basic_alphatest`. The driver walks
  the *pipeline layout*'s bindings rather than the shader's used ones, `basic_pipeline_layout`
  is `{single_texture, mboit_input_attachment}`, and the second set is three
  `INPUT_ATTACHMENT` bindings at `FRAGMENT` stage — a type `ps5vk_descriptor_stride` has no
  entry for. That is **R2**, and it is now the only thing between this port and its first
  frame: the basic pipelines need `INPUT_ATTACHMENT`, the GUI pipelines need the bare
  `SAMPLER`, and the lightmap compute layout needs `SAMPLED_IMAGE`.
- The port's own half of that is **built, gated and deliberately not deployed**: see the
  four-set accommodation below, which a run without R2 would spend a console cycle failing to
  notice.

**The one feature the intersection costs** is the screenshot: vkQuake copies from the
presented image and needs `TRANSFER_SRC`. It comes back by itself if the driver ever proves
and advertises that use for swapchain images — the intersection picks it up with no change —
so it is a named limitation rather than a workaround to retire.

The three earlier runs are still in the record and still replay: `evidence/m2-renderer/`
(the first run inside the renderer), `evidence/m2-loader/` (the loader aliasing this port was
missing, fixed in `platform/ps5/vk_loader.c`) and `evidence/m2-device/` (the 2026-09-20
depth-format stop, marked superseded).

## Next

**Waiting on `../PS5_Vulkan` for R2** — the three core 1.0 descriptor types vkQuake declares
and the driver advertises per-stage limits for. Nothing in this tree gets past
`R_CreatePipelines` without it, and the port has already done the half that is its own:

**The four-set accommodation** (`platform/ps5/vkquake-edits.py`, ten edits, applied by the
engine build and pinned by `tests/test_vkquake_edits.py`): OIT's input-attachment set leaves
the world and md5 layouts — which is what fits them inside the four sets the driver binds —
the bmodel instance block moves from set 4 to set 3 in the layout, in `Shaders/world.vert` and
at all four bind sites, `r_oit` defaults to 0 so a frame never selects the dropped family, and
`tools/build-vkquake-shaders.sh` compiles the oit/mboit *variants* as their base shader because
a shader declaring a set its layout does not hold is a mismatch rather than a warning. The
retirement trigger is R2 plus the driver's MRT item: with input attachments and multi-colour
renderings, OIT comes back and these edits are reverted.

Build identity `1d7bd836df05a745`, `bash tools/verify.sh` PASS. It deploys the moment R2 lands —
and not before, because the run would stop at this same line.

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
- **Whether the menu needs its own accommodation.** vkQuake's GUI shader reads a separate
  `texture2D` and `sampler` in two sets; R2 gives it the types, and if a run still refuses
  there the choice is a second edit (a combined sampler in the GUI path) or waiting for the
  driver. Not decided until a run says which.

## Blockers

**R2 is the one thing in the way of the first frame** — the input-attachment type for the
basic pipelines, the bare sampler for the GUI ones, the sampled image for the lightmap
compute pass. Everything else that stopped a run so far is closed, and the port's own half of
this one is built and waiting on a gate that is not its own.
