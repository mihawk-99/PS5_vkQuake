# Requests to `../PS5_Vulkan`

Cross-project requests. `../PS5_Vulkan` is maintained separately and is
**read-only from this repository** — nothing here edits it. Each section below is
a self-contained report that can be handed to that project's maintainer as-is: it
carries its own evidence, its own reproduction and its own acceptance criteria,
because the maintainer does not have this repository's context.

One section per request, newest last.

---

## R1 — `format_audit.py` cannot see this footnote's second must-clause

**Status. Closed by the driver** (`../PS5_Vulkan` `ad500a0`; the tree this was reported
against, `23bcea1`, no longer exists). Both halves landed: `VK_FORMAT_D24_UNORM_S8_UINT`
and `VK_FORMAT_D32_SFLOAT_S8_UINT` now carry
`VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` (`driver/ps5vk_image.c`), the stencil
plane is proved on the console (`v0-stencil-clear`), and `tools/format_audit.py` reads
the footnote's clauses whole and fails `--check` on an unmet one (`required_clauses`;
`../PS5_Vulkan/docs/BLOCKERS.md`, row 11). The body below is the report as it was made
and is kept unedited — what it asked for is done, so it is a record rather than a
request.

**Reported against.** `../PS5_Vulkan` local `main` at `23bcea1`, which is **one
commit ahead of the published `PS5Vulkan/main` (`2b494d2`)** and is not pushed;
the round-8 work in the tree is uncommitted. Line numbers below are from that
tree, and the `tools/format_audit.py` and `docs/V0_FORMATS_AUDIT.md` content they
name is unchanged from `2b494d2`.

### The short version

`VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT`'s footnote in the specification
contains **two `must` clauses that share one `{sym2}` marker**. The driver
satisfies the first and violates the second. `tools/format_audit.py` files every
`{sym2}` row as conditional **without ever checking whether the requirement is
met**, and `--check` fails only on `sym1` rows — so the violated clause is
invisible to the gate. A real application reached it.

This is reported as a tooling finding first, because the tooling half is
verifiable in one command and stands on its own whatever is decided about the
driver half.

### The evidence, in four steps

**1. The footnote requires two things, and one marker covers both.** From the
vendored specification, `.deps/native/vulkan-docs/formats-v1.4.354.adoc`, the
footnote under the "Mandatory Format Support" table's
`DEPTH_STENCIL_ATTACHMENT` column:

> `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` feature must: be supported for
> at least one of `VK_FORMAT_X8_D24_UNORM_PACK32` and `VK_FORMAT_D32_SFLOAT`, and
> must: be supported for at least one of `VK_FORMAT_D24_UNORM_S8_UINT` and
> `VK_FORMAT_D32_SFLOAT_S8_UINT`.

Both clauses are `must`. The table marks all four formats `{sym2}`
(`formats-v1.4.354.adoc:3737-3738`), and `{sym2}` means "must: be supported on at
least some of the named formats, with more information in the table where the
symbol appears" (`:3282`).

**2. The driver satisfies clause one and violates clause two.** It reports
`DEPTH_STENCIL_ATTACHMENT` for `VK_FORMAT_D32_SFLOAT`
(`driver/ps5vk_image.c:442-446`), which satisfies clause one. It reports it for
neither `VK_FORMAT_D24_UNORM_S8_UINT` nor `VK_FORMAT_D32_SFLOAT_S8_UINT` — its
only other depth entry is `D16_UNORM` (`:428-432`) — so clause two is unmet.

**3. The audit tool never checks.** `tools/format_audit.py:140-151`:

```python
for name, features in sorted(required.items()):
    for feature, marker in features.items():
        carried = reported.get(name, set())
        if marker == "sym1":
            if feature not in carried:
                missing.setdefault(name, []).append(feature)
            elif feature == "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT":
                ...
        else:
            conditional.setdefault(name, []).append(f"{feature} ({marker})")
```

The `else` arm appends to `conditional` and **never consults `carried`**. And
`main` ends `return 1 if args.check and missing else 0` (`:171`), so a conditional
row cannot fail the gate.

**4. The bucket holds satisfied and unsatisfied rows alike, which is the proof.**
The tool's own output, `python3 tools/format_audit.py`, prints under
"conditional (at least some formats, or with caveats)":

```
  VK_FORMAT_D24_UNORM_S8_UINT                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT (sym2)
  VK_FORMAT_D32_SFLOAT                           VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT (sym2)
  VK_FORMAT_D32_SFLOAT_S8_UINT                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT (sym2)
  VK_FORMAT_X8_D24_UNORM_PACK32                  VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT (sym2)
```

`VK_FORMAT_D32_SFLOAT` is in that list and the driver **does** carry the bit. So
the list is marker-driven, not state-driven: a row whose requirement is met and a
row whose requirement is violated are indistinguishable inside it. The summary
line "4 formats miss a required feature" is therefore an undercount for this
footnote — the count of four is the two sRGB rows and the two storage-image
atomics rows, and this clause is in neither.

### It is known, and deliberately ungated

This is not a claim that the row was overlooked. It is recorded:

- `docs/V0_FORMATS_AUDIT.md:238`, the `DEPTH_STENCIL_ATTACHMENT` row: "the two
  stencil formats need the stencil registers and path, which nothing has
  recorded", closing path "a runner probe of the DB format words and the stencil
  registers, then the C5 readback per format".
- `docs/V0_FORMATS_AUDIT.md:24`: "`{sym2}` and `{sym3}` are required on at least
  some of the named formats or with caveats, and are listed as conditional **so a
  conditional row is never mistaken for a closed one**."

That last line is the point. The row is not closed, and the classification exists
to say so — but it also removes the row from the gate, and for a footnote with two
independent clauses that is where it stops being visible. The audit's closing
summary names three things standing in the way of the remaining rows (the
descriptor type, the fetch order, the compiler); this row is quoted against a
fourth, "nothing has recorded", which that sentence does not enumerate.

### How an application reached it

This port is vkQuake 1.36.0 on the console (PPSA99010), and it is the first real application
on the driver. It gets through instance creation, physical-device enumeration and
`vkCreateDevice`, and then stops in device initialisation with its own error.

The capture is committed rather than transcribed: **`evidence/m2-device/`** holds
the console's own trace, distilled from the title's `/app0/trace.txt`, with the
expectations it is replayed against. `python3 tools/evidence.py compare evidence/`
reproduces the verdict offline. Verbatim, the end of that run:

```
vkEnumeratePhysicalDevices -> 0
vkEnumeratePhysicalDevices -> 0
Vendor: AMD
Device: PS5 AGC GPU (ps5vk)
vkCreateDevice -> 0
Device extensions:
 VK_KHR_swapchain

ERROR-OUT BEGIN


QUAKE ERROR: Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format
STACK TRACE:
(null)
```

`QUAKE/gl_vidsdl.c:1416-1436` queries exactly the two formats clause two names,
requires `DEPTH_STENCIL_ATTACHMENT_BIT` on one of them, and calls the failing
branch impossible for a compliant driver. It is right to: clause two says `must`.

### Why "claim the format and ignore the stencil" is not a fix

Worth stating, because it is the cheap-looking option and it does not work for
this application. vkQuake uses the stencil functionally, for the sky occlusion
trick (`QUAKE/gl_rmisc.c`): one pipeline rasterises sky geometry with
`colorWriteMask = 0`, `stencilTestEnable = VK_TRUE`, `compareOp = ALWAYS`,
`passOp = REPLACE` and `reference = 0x1`, writing stencil and no colour
(`:3384-3395`); a second draws the skybox with `depthTestEnable = VK_FALSE`,
`compareOp = EQUAL`, `writeMask = 0x0` and `reference = 0x1`, so it survives only
where sky was rasterised (`:3424-3436`). The engine also builds a parallel
render-pass set keyed on `MAIN_RENDER_PASS_STENCIL_CLEAR` whose variants differ
only in stencil load and clear semantics.

So a format entry mapped onto the existing `32_FLOAT` word with the stencil aspect
ignored would leave that pass writing and testing an aspect that does not exist.

For completeness: vkQuake uses `pDepthStencilAttachment` only and never a separate
`pStencilAttachment`, so the driver's refusal of *separate* stencil attachments
(`driver/ps5vk_draw.c:506`) is not the path being hit. The format table is.

### The two halves of a fix, and they are separable

**The tooling half, which is small and independently worth doing.** A `{sym2}`
marker is not always a disjunction — sometimes it is a caveat on a single named
format — so the general fix is to teach the audit which footnote disjunctions
exist and to evaluate each clause separately, rather than to make all `{sym2}`
rows strict. For this footnote that means: parse the two `at least one of` clauses
and check each against the reported table, so the row reports as satisfied or not
instead of merely conditional. Whatever the mechanism, the invariant worth
asserting is the one the current output cannot express: **a conditional row whose
requirement is satisfiable is either met or listed as unmet.** A test that fails
if `VK_FORMAT_D32_SFLOAT` and `VK_FORMAT_D32_SFLOAT_S8_UINT` sit in the same
unqualified bucket would have caught this.

**The driver half, which is the stencil path.** `../PS5_Vulkan` has none:
`DB_STENCIL_INFO` is the constant "stencil disabled" word `0x20000180`
(`driver/ps5vk_draw.c:264`), the stencil read and write bases and
`DB_STENCIL_CLEAR` are written zero (`:266-278`), `stencilTestEnable` is refused
at pipeline creation (`driver/ps5vk_pipeline.c:867`) and a stencil clear is
refused by name (`driver/ps5vk_image.c:2516`, `:2575`).

**A partial lead, offered as a lead and not as the answer.** I looked for a public
source for the encoding, since that is the first step of the method in
`docs/BLOCKERS.md`. Two readings:

- **ps5-opengl has no stencil path either** — its `src/` contains only
  `platform/` — so the driver's "nothing has recorded" holds for it too.
- **Mesa's register database does carry the field layout and the enumerations**,
  in the `amdgfxregs.h` already in this tree at
  `.deps/native/opengl-sdk/third_party/opengnm-psbc/src/amd/common/amdgfxregs.h`:
  `DB_DEPTH_CONTROL` `0x028800` (`STENCIL_ENABLE` bit 0, `STENCILFUNC` bits 8-10,
  `STENCILFUNC_BF` bits 20-22, the `V_028800_FRAG_*` compare enumeration,
  `:12877`), `DB_STENCIL_CONTROL` `0x02842C` (`STENCILFAIL`, `STENCILZPASS`,
  `STENCILZFAIL` and their `_BF` twins, four bits each, with the
  `V_02842C_STENCIL_*` op enumeration where KEEP is 0 and REPLACE is 3 or 4,
  `:11487`), `DB_STENCILREFMASK`
  `0x028430` and `_BF` `0x028434` (`:11523`, `:11537`), `DB_STENCIL_INFO`
  `0x028044` (`:10121`).

That is genuinely less than it looks, and I would rather say so than overstate it:
the driver does not address registers absolutely. It writes AGC register packets
(`struct ps5vk_agc_register { uint16_t offset; ... }`) in ps5-opengl's compacted
numbering — its `DB_Z_INFO` is offset `0x010`, where Mesa puts the register at
`0x028040` — so Mesa supplies the field positions and the op and compare
enumerations, **not** the offset mapping and **not** the measured enable word. The
analogue of the measured `DB_Z_INFO` word `0x80000183` is exactly what is missing,
which is what `docs/V0_FORMATS_AUDIT.md:238` already says.

### What would prove it

The tooling half is provable here and now: `python3 tools/format_audit.py` should
report this clause as unmet, and the existing `tests/test_tools.py` recount is
where a regression belongs.

The driver half needs a console measurement, in the shape the audit already names:
a runner probe of the DB format words and the stencil registers — a frame that
writes stencil = 1 over part of a target with colour writes masked off, then a
pass that tests equality against 1 with depth testing disabled and a
distinguishable colour, read back so the colour appears **only** where the first
pass rasterised, with the stencil plane read back too; then the same at one and
four samples, since Quake's sample count is configurable.

Then the application-level check, which this port can supply: build PS5 vkQuake
against the driver and let it reach its render passes. Today it cannot get that
far. I can run that on the console and report the result, including a symbolized
crash or a validation failure, if that is useful.

### What is not being asked

- No API surface beyond what the specification already requires.
- No change to the fixed 3840x2160@60 display mode, the swapchain path or WSI —
  those are working here and are not implicated.
- No schedule or priority claim from this repository.
- Not a claim that the row was missed. It is documented and classified; the
  request is that the classification stop hiding the clause, and that the clause
  get a closing measurement.

---

## R2 — three core 1.0 descriptor types are advertised and absent from the table

**Status.** **Read** from vkQuake's source and the driver's table, not yet measured on
the console. One prediction follows from it, and a single console run of this port
settles it — see "what would prove it".

**Reported against.** `../PS5_Vulkan` `ad500a0`.

### The short version

`ps5vk_descriptor_stride` (`driver/ps5vk_descriptor_set_layout.c`) answers
`UNIFORM_BUFFER`, `UNIFORM_BUFFER_DYNAMIC`, `COMBINED_IMAGE_SAMPLER`, `STORAGE_BUFFER`,
`UNIFORM_TEXEL_BUFFER`, `STORAGE_TEXEL_BUFFER` and `STORAGE_IMAGE`, and **0 for
everything else**. `ps5vk_descriptor_options` (`driver/ps5vk_pipeline.c:195-220`)
refuses a layout binding whose stride is 0:

```
set %u binding %u: descriptor type %d has no proven table entry
```

so a pipeline layout naming a type without a table entry cannot be used by any pipeline
at all. The device meanwhile reports `maxPerStageDescriptorSamplers = 16`,
`maxPerStageDescriptorSampledImages = 16` and `maxPerStageDescriptorInputAttachments = 4`
(`driver/ps5vk_physical_device.c:86-91`), and those three families have no entry. This
is R1's kind of gap — a limit advertised and not honoured — and all three are core
Vulkan 1.0, not extensions.

### Why it is a class rather than an instance

The demo programme that produced R1–R9 uses only combined image samplers, uniform and
storage buffers and texel buffers — the forms the table does have — so **nothing has
ever exercised any of the three**. The separated form (`SAMPLER` + `SAMPLED_IMAGE`) is
what the Vulkan guidance recommends and what a renderer reaching for one independent
sampler will use; a pipeline whose layout declares it is the case that was missing.

### What vkQuake declares, and what each one blocks

Read from `vendor/vkQuake/Quake/gl_rmisc.c` and the shader source, not run:

| vkQuake layout | binding | type | stage | what it blocks |
| --- | --- | --- | --- | --- |
| `basic_pipeline_layout`, set 1 = `mboit_input_attachment_set_layout` | 0-2 | `INPUT_ATTACHMENT` | `FRAGMENT` | **the first pipeline the engine creates** — `basic_alphatest`, every render-pass variant |
| `gui_pipeline_layout`, set 1 = `gui_sampler_set_layout` | 0 | `SAMPLER` | `FRAGMENT` | the menu and every 2D draw: `draw_pic.frag` reads a separate `texture2D` and `sampler` |
| `lightmap_compute_layout` | 1, 2 | `SAMPLED_IMAGE` | `COMPUTE` | the lightmap update compute pass |
| `world`, `alias`, `md5` layouts | — | `INPUT_ATTACHMENT` at set 3 or 4 | `FRAGMENT` | OIT (WBOIT/MBOIT) only, once the first row is handled |

### Why the first row lands where it does

It is not visible from either tree alone, so it is worth stating: the check walks the
**pipeline layout**, not the shader's used bindings. `basic_pipeline_layout` is
`{single_texture, mboit_input_attachment}` and the plain opaque pipelines use it too —
the input attachment is what the MBOIT *composite* variant reads, but every pipeline
built against that layout is refused, including one whose SPIR-V never mentions it. An
application therefore pays for a descriptor type it does not use, and this port cannot
get past `R_CreatePipelines` by leaving OIT switched off.

### What the port is doing meanwhile, so none of this is urgent for its sake

- OIT is dropped as a **registered accommodation**, with this request's
  `INPUT_ATTACHMENT` item as its retirement trigger: `r_oit` off, the OIT/MBOIT pipeline
  variants not created, the input-attachment set dropped from every layout that
  survives, and the bmodel set renumbered from 4 to 3 so the world layout fits inside
  the advertised four sets.
- That leaves `SAMPLER` (the menu) and `SAMPLED_IMAGE` (the lightmap) as the two this
  port would otherwise re-express as combined image samplers in upstream's shaders. It
  can do that; it would rather report the type than patch around it, and the type is
  what every application after it will want.

### What would prove it, and in what order

1. **`SAMPLER` and `SAMPLED_IMAGE` first, together** — one mechanism, the separated
   form. The probe shape this port's GUI pipeline has: set 0 binding 0 `SAMPLED_IMAGE`,
   set 1 binding 0 `SAMPLER`, fragment stage, one texture through a nearest sampler,
   read back. **Acceptance: the frame is texel-for-texel the frame the same texture
   draws through one `COMBINED_IMAGE_SAMPLER`** — the separated form must not be a
   different picture, which is exactly what a "it drew something" check would miss.
2. **`INPUT_ATTACHMENT` second**, and it is a design conversation rather than a table
   entry, as the driver's own response says: it needs a subpass to read from. This
   port's shape when it arrives: three input attachments in one set, `FRAGMENT` stage,
   holding what a previous subpass wrote.
3. **MRT (`colorAttachmentCount > 1`) is not on this port's critical path**, so it needs
   no re-prioritising for this port's sake: with OIT off and `vid_fsaa 0`, vkQuake's
   plain main render pass is one colour attachment plus depth. MRT is what restores OIT
   later, and the template's `MRT` demo is its own reproduction.

### What is not being asked

- No change to `maxBoundDescriptorSets = 4`. This port renumbers to fit inside it; the
  advertised limit is not the driver's to widen.
- No request to check a shader's *used* bindings instead of the layout's. That check is
  defensible and its refusal names the type; the request is the missing table entry,
  not a weaker check.
- Nothing about `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR`, which vkQuake declares
  behind a ray-query check this device does not advertise.

---

## R3 — `vkCreateBufferView` refuses `range = VK_WHOLE_SIZE`

**Status.** **Measured** on the console: the run stops with `vkCreateBufferView failed with code -13`
(build identity `9ad1d1f36bae987c`, recorded as `evidence/m2-renderer/`). The cause below is read from both
trees and is one line.

**Reported against.** `../PS5_Vulkan` at the tree whose `libps5vk.ps5.a` was built 2026-09-22 10:45.

### The call, from vkQuake's source

`Quake/gl_vidsdl.c`, `R_CreatePaletteOctreeBuffers`, which runs in `VID_Init` — before the first frame:

```c
ZEROED_STRUCT (VkBufferViewCreateInfo, buffer_view_create_info);
buffer_view_create_info.buffer = palette_colors_buffer;   /* UNIFORM_TEXEL_BUFFER | TRANSFER_DST */
buffer_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
buffer_view_create_info.range  = VK_WHOLE_SIZE;           /* offset stays 0 */
vkCreateBufferView (vulkan_globals.device, &buffer_view_create_info, NULL, &palette_buffer_view);
```

### Why the driver refuses it

`ps5vk_CreateBufferView` (`driver/ps5vk_buffer.c:189`) checks the format first and the range second:

```c
if (pCreateInfo->range == 0 || pCreateInfo->offset >= buffer->vk.size ||
    pCreateInfo->range > buffer->vk.size - pCreateInfo->offset)
```

`VK_WHOLE_SIZE` is `~0ULL`, so the third clause is true for every buffer and the call is refused with a
sentence naming 18446744073709551615 bytes. Vulkan defines the value as "from `offset` to the end of the
buffer", and it is what an application writes when it wants the whole buffer — vkQuake writes it in **19**
places.

The format is not implicated: `VK_FORMAT_R8G8B8A8_UNORM` carries
`VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT` in the driver's own table (`driver/ps5vk_image.c:72`), and
that check passes.

### It is the one site that does not resolve the value

Three other places in the same driver handle the idiom, which is what makes this read as an omission
rather than a decision:

| site | what it resolves |
| --- | --- |
| `ps5vk_cmd_buffer.c:357` | `vkCmdUpdateBuffer`'s size |
| `ps5vk_descriptor_set.c:352` | a descriptor's buffer range |
| `ps5vk_draw.c:2310` | the draw's buffer read |

### What would close it

Resolve the value once, before the bounds check, and store the resolved range on the view:

```c
const VkDeviceSize range = pCreateInfo->range == VK_WHOLE_SIZE
                              ? buffer->vk.size - pCreateInfo->offset
                              : pCreateInfo->range;
```

then keep rejecting `range == 0` and `range > buffer->vk.size - offset` as before, and set
`view->range = range` so whatever reads the view — the texel-buffer descriptor — gets a byte count rather
than a sentinel.

**Acceptance.** vkQuake's own call shape is created and works: a view of the whole of a buffer holding
`NUM_PALETTE_OCTREE_COLORS` `uint32_t`s, format `R8G8B8A8_UNORM`, usage
`UNIFORM_TEXEL_BUFFER | TRANSFER_DST`, with the `R32_SFLOAT`/`R32_UINT` texel-buffer probes' own shape as
the console case. The port can then measure it end to end: the palette octree is what turns an 8-bit
texture into colour.

### What is not being asked

- No change to the format check or to the "no probe has proved a texel buffer for it" refusal; that
  reporting is right and this port depends on it.
- No audit of the other `VK_WHOLE_SIZE` sites beyond the three named, which already handle it.
- Nothing about the palette octree path itself. The alternative — patching upstream's `VK_WHOLE_SIZE` into
  an explicit size — is declined: it would hide the class behind an application-specific change.

### A diagnostic option, the driver's to take or leave

An application receives the driver's refusal sentences only through a `VK_EXT_debug_utils` messenger.
vkQuake enables that extension only in its own `_DEBUG` builds, and this port's traces capture stderr, so a
console run shows `-13` and nothing else — this request took a code read to pin down, where the driver's
own sentence (range, offset, buffer size) would have named it in the trace. If a refusal also wrote its
sentence to stderr when no messenger is installed, every console run of every application would name its
own cause.

---

## R4 — `vkCreateSwapchainKHR` asserts on application input instead of refusing

**Status.** **Measured** on the console: the run that reached a swapchain ends in the driver's
own assertion, verbatim in the title's trace (`evidence/m2-swapchain/`, build identity
`f11c4ca7d0db5680`):

```
Using FIFO present mode
assertion failed: info->imageFormat == VK_FORMAT_B8G8R8A8_UNORM && info->imageColorSpace ==
VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && info->imageExtent.width == PS5VK_DISPLAY_WIDTH && … &&
(info->imageUsage & ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0 && … &&
info->minImageCount <= PS5VK_SWAPCHAIN_IMAGES
(driver/ps5vk_wsi.c:444, ps5vk_CreateSwapchainKHR)
```

**Reported against.** `../PS5_Vulkan` `dda292c` and the archives built 2026-09-22 11:06.

### The check is right; the form is not

The assertion enforces the valid-usage rule the comment above it names — the surface's
capabilities, formats and present modes must allow the parameters. That is the driver's job
and this port is not asking for it to be dropped. What is wrong is that it is an `assert`: a
shipped driver aborts the title, with no `VkResult` the application can see and no field it
can act on, instead of refusing.

Any application that asks for something the surface does not report meets it, and they are
ordinary requests: another present mode, three images, a 1280x720 swapchain, or — the case
here — a usage the surface does not advertise.

### What this port's application asked, and what it has done about it

vkQuake sets `imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT`
and never consults `VkSurfaceCapabilitiesKHR::supportedUsageFlags`, while this driver reports
`COLOR_ATTACHMENT` alone (`driver/ps5vk_wsi.c:262`, "Rendering is the only use swapchain
images have been proven for"). So the assertion is firing on a real violation by the
application, and the driver is consistent: it advertises exactly what it honours.

**That half is fixed on this side and is not part of the request.** The port now intersects
the request with what the surface reports (`platform/ps5/vkquake-edits.py`), which is what
the valid-usage rule requires, and it is an intersection rather than a removal — when this
driver proves `TRANSFER_SRC` for swapchain images and advertises it, the application picks it
up with no change.

### What is being asked

Return an error with a sentence that names the field which is out of range, in the driver's
own style — the same file already does it for the one other create-time rule it enforces
(`"VideoOut belongs to another swapchain"`, `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR`). One
sentence per field, or one sentence naming the first field that disagrees, either is fine;
`VK_ERROR_UNKNOWN` is what the rest of this driver uses for a refusal the API cannot express.

**Acceptance.** A `VkSwapchainCreateInfoKHR` the surface does not allow — `imageUsage` with a
bit outside `supportedUsageFlags` is the measured one — returns an error rather than
aborting the title, and the sentence names `imageUsage` (or `imageExtent`, or `presentMode`,
matching whichever field was changed). The existing correct requests must keep succeeding
unchanged, which the port's own next run is the check for.

### What is not being asked

- No change to `supportedUsageFlags`. `COLOR_ATTACHMENT` alone is honest and the port now
  asks for exactly that; advertising a use nothing has proved would be the R1-class gap this
  project keeps reporting.
- No `TRANSFER_SRC` support. It is the one thing that would turn vkQuake's screenshots back
  on — its screenshot path copies from the presented image (`gl_vidsdl.c`, `vkCmdCopyImageToBuffer`)
  — but it is a capability claim that needs its own probe, so it belongs in a request of its
  own if it is wanted at all, not folded into this one.
- No change to the assertion's *conditions*, which are the valid-usage rule.

---

## R5 — `VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT` is refused although the format must support it

**Status.** **Measured** on the console: the run stops with `vkCreateImage failed with code -11`
(`VK_ERROR_FORMAT_NOT_SUPPORTED`), immediately after creating the swapchain
(`evidence/m2-color-buffer/`, build identity `055c10f384f02852`).

**Reported against.** `../PS5_Vulkan` `dda292c` and the archives built 2026-09-22 11:06.

### The call

vkQuake's `GL_CreateColorBuffer` (`Quake/gl_vidsdl.c`), the offscreen target the main render
pass renders into — 2D, `VK_FORMAT_R8G8B8A8_UNORM`, 3840x2160, 1 mip level, 1 array layer, one
sample, optimal tiling, and:

```c
image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
```

### Why the driver refuses it

`ps5vk_format_usage` (`driver/ps5vk_image.c:693-713`) maps a format's features to usages —
`TRANSFER_SRC`, `TRANSFER_DST`, `SAMPLED`, `COLOR_ATTACHMENT`, `DEPTH_STENCIL_ATTACHMENT`,
`STORAGE` — and `ps5vk_image_supported` (`:735`) refuses any usage bit the mapping does not
carry. There is no `INPUT_ATTACHMENT` clause, so an image whose usage names it is refused, and
`vkGetPhysicalDeviceImageFormatProperties2` answers `VK_ERROR_FORMAT_NOT_SUPPORTED` for the same
combination.

**That combination is one the driver has to support.** The specification's Format Feature
Dependent Image Usage Flags table (`formats-v1.4.354.adoc:4242`, the copy vendored in
`../PS5_Vulkan/.deps/native/vulkan-docs/`) requires:

| image usage flag | required format feature flag |
| --- | --- |
| `VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT` | `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT` **or** `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` |

`VK_FORMAT_R8G8B8A8_UNORM` carries `COLOR_ATTACHMENT` in the driver's own table
(`driver/ps5vk_image.c:68`), so the usage is mandatory for it. This is the last gap of its kind
here: vkQuake writes exactly seven image usage flags and the mapping covers the other six.

### What would close it

One clause beside the others in `ps5vk_format_usage`:

```c
/* The specification requires this usage for any format carrying either of the two
 * attachment features (formats.adoc, Format Feature Dependent Image Usage Flags). */
if (features & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
   usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
```

**Acceptance.** vkQuake's colour-buffer combination is created —
`R8G8B8A8_UNORM`, 2D, optimal, one sample, `COLOR_ATTACHMENT | INPUT_ATTACHMENT | SAMPLED |
STORAGE` — and `vkGetPhysicalDeviceImageFormatProperties2` returns success for it. The B3 image
test's format/usage matrix is where that belongs, and the format audit's row for the usage can
claim the bit once it exists.

### What is not being asked, and this matters

- **Not input attachments.** The descriptor type, its stride, its write path and subpass reads
  are R2 and stay there. This request is about *image creation*: the driver currently refuses
  an image the specification says it must allow, and reports that refusal to applications that
  ask first, which is the R1-class gap — advertised support that is not honoured.
- No change to the other six mappings, and none to the format table's feature bits.
- No subpass rendering, and no `VK_KHR_create_renderpass2`-shaped work.

---

## R6 — `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP` is refused, and it is core 1.0

**Status.** **Measured** on the console: the run that compiles every pipeline stops at
`QUAKE ERROR: vkCreateGraphicsPipelines failed (warp) with code -13`
(`evidence/m2-warp-strip/`, build identity `4dc9c645c017f8e9`).

**Reported against.** `../PS5_Vulkan` `d23eebb` and the archives built 2026-09-22 12:01.

### The refusal

`ps5vk_pipeline.c:1213-1216`:

```c
if (!info->pInputAssemblyState ||
    (info->pInputAssemblyState->topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST &&
     info->pInputAssemblyState->topology != VK_PRIMITIVE_TOPOLOGY_META_RECT_LIST_MESA) ||
    info->pInputAssemblyState->primitiveRestartEnable)
   return vk_errorf(device, VK_ERROR_UNKNOWN,
                    "only triangle lists without primitive restart are supported");
```

The refusal names itself, which is the driver's own style and is why this took one read. What it
refuses is a **core Vulkan 1.0 topology with no feature bit gating it**: `TRIANGLE_STRIP` is in
the specification's required set, and every AMD primitive enumeration the register database
carries (and AGC's own) includes strips.

### Why this port cannot work around it

vkQuake's warp pipeline sets it (`Quake/gl_rmisc.c:3094`) for `R_RasterWarpTexture`
(`Quake/gl_warp.c`), which builds a **warp-tessellated strip mesh**: two vertices per row, with
`num_verts` growing as the tesselation tightens, drawn as one strip so consecutive rows share
their edge. It is not a three-vertex full-screen triangle that could be re-expressed as a list:
re-expressing it would mean changing how the mesh is generated, which is a change to upstream's
renderer for a driver limitation — exactly what this port declines to do.

### What would close it

Map the topology where the triangle list is mapped. Nothing else about the pipeline changes: the
draws are arrays (`vkCmdDraw`), so no index buffer is involved, and the strip's alternating
winding for the second and later triangles is the hardware's own behaviour — the front-face and
cull bits the driver already programs stay as they are.

**Acceptance**, in the driver's own shape: a probe that draws the same geometry twice, once as a
`TRIANGLE_STRIP` and once as the equivalent `TRIANGLE_LIST` (the strip's triangles written out),
and reads the target back — **the frames must be identical**, which is what distinguishes a mapped
topology from one that draws something. A second frame with the winding reversed, culled, would
also show the strip's alternating winding is being honoured.

### What is not being asked

- No other topology. `TRIANGLE_FAN`, the line and point topologies and
  `primitiveRestartEnable` stay refused if they are not proved — this port uses none of them, and
  a claim per topology is the driver's own rule.
- No change to the refusal's wording or to `META_RECT_LIST_MESA`, which Mesa's own meta draws use.

---

## R7 — the compute path binds one storage buffer, and a real kernel needs a texture and a storage image

**Status.** **Measured** on the console: the texture-warp kernel's pipeline is refused,
`QUAKE ERROR: vkCreateComputePipelines failed (cs_tex_warp) with code -13`
(`evidence/m2-compute-bindings/`, build identity `900d2d14`), after its compile succeeded.

**Reported against.** `../PS5_Vulkan` working tree of 2026-09-22 12:32 (`ps5vk_compute.c`).

### The refusal

`ps5vk_compute.c`, after the compile:

```c
if (metadata.user_sgpr_count > PS5VK_COMPUTE_MAX_USER_DATA ||
    metadata.descriptor_binding_count != 1)
   return vk_errorf(device, VK_ERROR_UNKNOWN,
                    "a dispatch needs one declared binding and at most %u user-data dwords; the "
                    "compiler reported %u binding(s) and %u dwords", …);
const PsbcDescriptorBinding *const binding = &metadata.descriptor_bindings[0];
if (binding->type != PSBC_DESCRIPTOR_STORAGE_BUFFER || …)
   return vk_errorf(device, VK_ERROR_UNKNOWN,
                    "the dispatch's binding is not a storage buffer whose table entry fits a "
                    "chunk (type %d, offset %u, stride %u)", …);
```

So a dispatch may declare **one binding, and it must be a storage buffer**. vkQuake's
`cs_tex_warp` declares two — a combined image sampler to read and a storage image to write
(`Quake/gl_rmisc.c`, `tex_warp_descriptor_set_layouts`; bound at `Quake/gl_warp.c:128` as
`{texture, storage_image}`) — and its **lightmap update** declares three (a storage image and two
sampled images, `lightmap_compute_layout_bindings`), which is the one on this port's M6 path.

### Why it matters even though this port is not asking for it now

The water warp has a second implementation upstream — the raster path through the strip pipeline
(`R_RasterWarpTexture`) — so the port is defaulting `r_waterwarpcompute` to 0 as a registered
accommodation and does not need the compute path for its first frame. The lightmap update has no
such alternative, and the driver's own compute path was written when the only consumer was its own
probe. Any application that dispatches a kernel reading a texture — which is what compute is
usually for — meets this refusal.

### What would close it

Generalise the dispatch path the way the graphics path already works: N declared bindings, each of
a type the descriptor table has (the table now carries combined image samplers, storage images,
storage buffers and the texel buffers), one table per set the shader reads, and the user-data
budget checked against the same `PS5VK_MAX_USER_DATA` the graphics stages use. The compiler
already reports the bindings and their offsets; what is missing is the dispatch's table writes for
anything but a single storage buffer, and the reader in `ps5vk_compute.c` that assumes index 0.

**Acceptance**, in the driver's own shape: a compute probe whose shader reads one texture and
writes one storage image, dispatched once, with the written image read back and compared against
the texels the kernel was told to produce — and a second probe with a single storage buffer, so the
existing shape is not lost. The port's own test is the lightmap pass once M6 reaches it.

### What is not being asked

- No change to the graphics path's descriptor handling, which already takes N bindings.
- No new descriptor types: this is about the dispatch reading the table the driver already writes.
- No work on the port's behalf *now*: the port's water warp takes the raster path, and this is
  recorded so the lightmap update is not a surprise when it arrives.
