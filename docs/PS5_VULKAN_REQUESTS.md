# Requests to `../PS5_Vulkan`

Cross-project requests. `../PS5_Vulkan` is maintained separately and is
**read-only from this repository** — nothing here edits it. Each section below is
a self-contained report that can be handed to that project's maintainer as-is: it
carries its own evidence, its own reproduction and its own acceptance criteria,
because the maintainer does not have this repository's context.

One section per request, newest last.

---

## R1 — `format_audit.py` cannot see this footnote's second must-clause

**Status.** Open. Blocks M2's render-pass step in this port.

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

This port is vkQuake 1.36.0 on the console, and it is the first real application
on the driver. It gets through instance creation, physical-device enumeration and
`vkCreateDevice`, and then stops in device initialisation with its own error:

```
Device: PS5 AGC GPU (ps5vk)
vkCreateDevice -> 0
Device extensions: VK_KHR_swapchain

QUAKE ERROR: Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format
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
