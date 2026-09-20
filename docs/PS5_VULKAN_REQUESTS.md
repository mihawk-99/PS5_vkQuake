# Requests to `../PS5_Vulkan`

Cross-project requests. `../PS5_Vulkan` is maintained separately and is
**read-only from this repository** — nothing here edits it. Each section below is
a self-contained report that can be handed to that project's maintainer as-is: it
carries its own evidence, its own reproduction and its own acceptance criteria,
because the maintainer does not have this repository's context.

One section per request, newest last.

---

## R1 — A depth-stencil format, and the stencil path behind it

**Status.** Open. Blocks M2's render-pass step in this port.

**Reported against.** `../PS5_Vulkan` at `23bcea1`.

### What this is

PS5 vkQuake is a port of vkQuake 1.36.0 to the console, built on this driver as
its Vulkan implementation. It is the first real application on the driver rather
than a probe or a tutorial program, so it is the first thing to exercise the
driver the way a shipped title does.

It now gets through instance creation, physical-device enumeration and
`vkCreateDevice`, and then stops during device initialisation.

### What happens

The console run reaches the device and then aborts:

```
SDL Video Driver: PS5 VideoOut
Using Vulkan 1.1
vkCreateInstance -> 0
Vendor: AMD
Device: PS5 AGC GPU (ps5vk)
vkCreateDevice -> 0
Device extensions: VK_KHR_swapchain

QUAKE ERROR: Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format
```

The message is vkQuake's own, from `QUAKE/gl_vidsdl.c:1416-1436`. Its logic is
exactly this:

```c
vkGetPhysicalDeviceFormatProperties (vulkan_physical_device, VK_FORMAT_D24_UNORM_S8_UINT, &format_properties);
qboolean x8_d24_support = (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
vkGetPhysicalDeviceFormatProperties (vulkan_physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &format_properties);
qboolean d32_support = (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
...
else
{
    // This cannot happen with a compliant Vulkan driver. The spec requires support for one of the formats.
    Sys_Error ("Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format");
}
```

The driver reports `DEPTH_STENCIL_ATTACHMENT_BIT` for neither. Its format table
carries two depth-only entries and no combined one:

```
driver/ps5vk_image.c:428  {VK_FORMAT_D16_UNORM,    ... DEPTH_STENCIL_ATTACHMENT_BIT | ...}
driver/ps5vk_image.c:442  {VK_FORMAT_D32_SFLOAT,   ... DEPTH_STENCIL_ATTACHMENT_BIT | ...}
```

and its draw path accepts only those two:

```
driver/ps5vk_draw.c:534  if ((depth_view->format != VK_FORMAT_D32_SFLOAT &&
driver/ps5vk_draw.c:535       depth_view->format != VK_FORMAT_D16_UNORM) || ...
```

### Why this is worth acting on

The Vulkan specification's required-format-support table requires an
implementation to support **at least one** of `VK_FORMAT_D24_UNORM_S8_UINT` or
`VK_FORMAT_D32_SFLOAT_S8_UINT` for depth-stencil attachment in optimal tiling.
This is the note vkQuake's own comment refers to when it calls the failing branch
impossible for a compliant driver. Any conformant Vulkan application that wants a
depth buffer meets this requirement, so it is not specific to Quake: the driver
cannot report itself complete, and no CTS run can pass, until one of the two is
supported.

### Why the stencil aspect cannot be stubbed

This is the part worth being explicit about, because the obvious cheap fix — add
the format to the table, map it onto the existing `32_FLOAT` hardware word, and
ignore the stencil — **does not work for this application**.

vkQuake uses the stencil buffer functionally, for the sky occlusion trick. It
rasterises sky geometry that writes stencil = 1 and no colour, then draws the
skybox with depth testing disabled and an equality test against that stencil
value, so the skybox appears only where sky was actually rasterised. From
`QUAKE/gl_rmisc.c`:

```c
/* 3384-3395: the sky stencil write -- colour writes masked off, stencil replaced with 1 */
infos.depth_stencil_state.stencilTestEnable = VK_TRUE;
infos.depth_stencil_state.front.compareOp        = VK_COMPARE_OP_ALWAYS;
infos.depth_stencil_state.front.passOp           = VK_STENCIL_OP_REPLACE;
infos.depth_stencil_state.front.compareMask      = 0xFF;
infos.depth_stencil_state.front.writeMask        = 0xFF;
infos.depth_stencil_state.front.reference        = 0x1;
infos.blend_attachment_states[0].colorWriteMask  = 0; // We only want to write stencil

/* 3424-3436: the skybox consume -- no depth test, stencil equality against 1 */
infos.depth_stencil_state.depthTestEnable   = VK_FALSE;
infos.depth_stencil_state.depthWriteEnable  = VK_FALSE;
infos.depth_stencil_state.stencilTestEnable = VK_TRUE;
infos.depth_stencil_state.front.compareOp   = VK_COMPARE_OP_EQUAL;
infos.depth_stencil_state.front.passOp      = VK_STENCIL_OP_KEEP;
infos.depth_stencil_state.front.writeMask   = 0x0;
infos.depth_stencil_state.front.reference   = 0x1;
```

The engine also builds a whole parallel set of render passes keyed on
`MAIN_RENDER_PASS_STENCIL_CLEAR`, whose only difference from the standard passes
is the stencil load/clear semantics — so a stencil aspect is load-bearing
throughout the render-pass set, not in one pipeline.

For completeness: vkQuake uses `pDepthStencilAttachment` only and never a
separate `pStencilAttachment`, so the driver's refusal of *separate* stencil
attachments (`driver/ps5vk_draw.c:506`) is not what is being hit here. What is
hit is the format table, and what is needed behind it is the stencil plane and
the stencil test.

### Where this already sits in your own roadmap

It is not an unknown unknown — your documents already track it:

- `docs/M5_REFERENCE.md:559`, the open unknown named **`unknowns-depth-words`**:
  "the `DB_Z_INFO` format words for `D16_UNORM` and the stencil formats, and the
  stencil registers", with the route "the C5 probe's shape with each format's
  candidate word and the stencil write/read registers", gating the
  `DEPTH_STENCIL_ATTACHMENT` rows.
- `docs/V0_FORMATS_AUDIT.md:238`: "the two stencil formats need the stencil
  registers and path, which nothing has recorded".
- `driver/tests/vk_b3_image_test.c:90` currently asserts that
  `VK_FORMAT_D24_UNORM_S8_UINT` reports **no** features — that assertion is the
  deliberate encoding of the gap, and it will need to change with the fix.

So this request is a request to promote that unknown, not to overturn a decision.

### What closing it involves

Offered as a starting point from the outside, not as a design for your tree — you
know the register path far better than this report does.

1. **The format entry.** `VK_FORMAT_D32_SFLOAT_S8_UINT` with
   `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` in
   `optimalTilingFeatures`, mapping onto the depth word the D32 entry already
   uses (`DB_Z_INFO` `0x80000183`), which is what
   `ps5vk_depth_registers` in `driver/ps5vk_draw.c:248-281` writes. Its
   `DB_STENCIL_INFO` (`0x011`) is currently the constant `0x20000180`, i.e.
   stencil disabled; enabling the plane is the change behind the format.
2. **The stencil plane.** `DB_STENCIL_READ_BASE` (`0x013`),
   `DB_STENCIL_WRITE_BASE` (`0x015`) and their `_HI` halves (`0x01b`, `0x01d`)
   are written zero today, and `DB_STENCIL_CLEAR` (`0x00a`) likewise. Whatever
   the console measures for the stencil plane's tile layout and base addresses
   is the substance of `unknowns-depth-words`.
3. **The pipeline's stencil state.** `driver/ps5vk_pipeline.c:867` currently
   refuses `depth->stencilTestEnable` outright, and
   `ps5vk_depth_control` (`driver/ps5vk_draw.c:287`) derives only the Z half of
   `DB_DEPTH_CONTROL`. A draw needs front/back `compareOp`, `reference`,
   `compareMask`, `writeMask` and the fail/depth-fail/pass ops to reach the
   register.
4. **The clear.** `driver/ps5vk_image.c` refuses a stencil aspect by name in
   `vkCmdClearDepthStencilImage` (`:2516`, `:2575`). Quake clears the combined
   attachment every frame, so the stencil half of that clear will be exercised
   immediately.
5. **Later, not now.** Sampled and transfer access to the stencil aspect, and
   `VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL` from
   `separateDepthStencilLayouts`. Quake needs neither: it never samples the
   depth or stencil image as a texture.

### What would prove it

In your own idiom — a runner probe, in C5's shape:

- Render a frame that writes stencil = 1 over part of the target with colour
  writes masked off, then draws a second pass that tests equality against 1 with
  depth testing disabled and a distinguishable colour. A readback of the colour
  target shows the colour **only** where the first pass rasterised, and the
  stencil plane reads back the written values.
- Run the same at one and four samples, since Quake's `sample_count` is
  configurable and the depth path already carries `NUM_SAMPLES`.
- Then the application-level check, which is the one this port can supply: build
  PS5 vkQuake against the driver and let it reach its render passes. Today it
  cannot get that far.

I can run that application-level check on the console and report the result,
including a symbolized crash or a validation failure, if that is useful to you.

### What is not being asked

- No API surface beyond what the specification already requires.
- No change to the fixed 3840x2160@60 display mode, the swapchain path or WSI —
  those are working here and are not implicated.
- No schedule or priority claim from this repository. It is a report that the
  gap is reachable by an ordinary application, with the code path that reaches
  it.
