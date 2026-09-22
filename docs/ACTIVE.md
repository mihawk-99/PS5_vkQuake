# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**Both gates this port was waiting on are closed in `../PS5_Vulkan`, and the port has
not been run since they closed.** This tree's last commit is 2026-09-20 (build
identity `c0677c1f`) and its last console run is `evidence/m2-device/`, which stops
in device initialisation on the depth format. Since then the driver (now `ad500a0`)
has:

- given `VK_FORMAT_D24_UNORM_S8_UINT` and `VK_FORMAT_D32_SFLOAT_S8_UINT` their
  `DEPTH_STENCIL_ATTACHMENT` feature — the audit's second `must:` clause, which this
  port's R1 reported as violated — and proved the stencil plane on the console
  (`v0-stencil-clear`). `tools/format_audit.py` now evaluates that clause instead of
  filing it conditional, so the hole R1 named is closed at both ends;
- accepted `anisotropyEnable` at the reported maximum as the no-op it is
  (`v0-sampler-anisotropy`, pid 138, **0 mismatched texels**), which is what
  `R_InitSamplers` was refused on.

So two sentences this file used to carry are **false today**: that the depth gap
blocks device initialisation, and that the `aco::schedule_program` abort may surface
during bring-up. The second was never an ACO fault at all — the driver's own runner
divided a sampled-format row by its zero texel size, and closed it with a console run
(`../PS5_Vulkan/docs/BLOCKERS.md`). `docs/FINDINGS.md` carries both corrections.

**The port has now been run against the driver as it stands, and it stops earlier than
before.** Build identity `10b17473`, evidence `evidence/m2-loader/`:

```
Vulkan Initialization
vkCreateInstance -> 0
QUAKE ERROR: vkGetInstanceProcAddr failed to find vkGetPhysicalDeviceProperties2
```

**Cause, and it is this port's own missing-loader surface.** `../PS5_Vulkan` now honestly
reports a Vulkan 1.0 instance (`PS5VK_INSTANCE_API_VERSION`, corrected 2026-09-21 by the
CTS round that found the instance claiming 1.3), so its `vkGetInstanceProcAddr` answers
the extension spelling of a promoted entry point and not the core one. vkQuake enables
`VK_KHR_get_physical_device_properties2` and then loads the **core** names; on a desktop
a loader aliases that pair, and this console has no loader. Fixed in
`platform/ps5/vk_loader.c` (two named pairs, `tests/vk_loader_test.c` on the host), with
`SDL_Vulkan_GetVkGetInstanceProcAddr` now handing the engine the aliasing lookup.

## Next

**One more console run — the fix is deployed** (build identity `9ad1d1f36bae987c`) — and
what it reaches is genuinely open, because the depth-format check it never got to is now
untested rather than passed. Two outcomes, both informative: past the depth formats into
`R_InitSamplers` and the render passes, or a new stop to read.

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
