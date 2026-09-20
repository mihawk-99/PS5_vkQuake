# What is happening right now

Volatile. Rewritten in place; what this repository *is* — the build, the identity,
the code map — is in `docs/PORT.md` and does not belong here. The runs are in
`docs/PHASE_LOG.md`, the plan in `docs/PLAN.md`, the requests to the driver in
`docs/PS5_VULKAN_REQUESTS.md`.

## Where the port is

**The title runs on the console and its Vulkan instance and device come up.** Each
console run has moved the failure point further along: `getcwd` in `Sys_Init`,
which the SDK declares and the runtime does not provide; then `W_LoadWadFile`,
because the game data had never been deployed; then `vkEnumeratePhysicalDevices`,
where the forwarders had called `vkGetInstanceProcAddr(NULL, …)`; and now device
initialisation, which the depth-stencil gap stops.

So M2's instance and device halves are done, and M2's render-pass step is stopped
by a gap in `../PS5_Vulkan` rather than by anything here — see **Blockers**.

The latest run is committed as evidence rather than transcribed
(`evidence/m2-device/`, replayed by `bash tools/verify.sh evidence`):

```
Vendor: AMD
Device: PS5 AGC GPU (ps5vk)
vkCreateDevice -> 0
Device extensions:
 VK_KHR_swapchain

QUAKE ERROR: Cannot find VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT depth buffer format
```

A signed title exists and every gate is green — `format unit build integration
evidence`, with the unit gate at 18 tests.

The evidence gate reads two kinds of console record. `tools/fetch-trace.py` pulls
the title's own `/app0/trace.txt` into the ignored `klog/` tree and
`tools/evidence.py distil` turns it into a committed record with the assertions it
is replayed against — with `--tail`, because that trace is append-mode so the run
being recorded is at the end of the file rather than the start; that is
`evidence/m2-device/`. `evidence/exit-sigsys/` is the kernel's own record from a
klog listener capture, and it records a bug of this port's.
`tests/test_evidence.py` pins the distiller's run selection and the gate's
verdicts, because a gate whose needles never match is decoration.

## Next

One step: **settle the rest of M2 without the engine.** The driver's surface,
swapchain and present path is implemented and console-proven on its own (its C1),
so the 3840x2160 display-plane surface, the swapchain, and a presented cleared
frame can all be proven through *this* port's platform layer — the SDL shim,
`ps5_window.c` and the `vk_globals.c` loader — without waiting for the engine's
render passes, which the depth gap stops.

## Open questions

- **The title takes SIGSYS on its exit path, every run.** The kernel records the
  process leaving through `exit()`, then a user thread receiving signal 12 at
  `rip 0x8000003ac` — in libkernel's stubs, not in the image. Three runs, three
  records, the same instruction, with exit values 1, 0 and 0, so it is the exit
  path and not `Sys_Error`. It is why an ordinary run's klog reads as a crash
  (`SCE_SHELL_UTIL_ERROR_APPLICATION_CRASH`, a coredump and a `gpudump.elf` run),
  and it is this port's bug rather than the driver's. The frames are unsymbolized
  on purpose: `build/title.map` is from a later build than the binary that
  crashed, so the fix waits on a re-run against a freshly built title. Recorded in
  `evidence/exit-sigsys/` and `docs/FINDINGS.md`.
- **The driver's compute coverage.** vkQuake's lightmap update is a compute pass
  writing a storage image and `../PS5_Vulkan` has none; the default texture path is
  a staging copy and is fine. The plan is to close the lightmap gap engine-side.
- **The ACO abort.** `../PS5_Vulkan` records a `aco::schedule_program` SIGFPE on
  the second compile of a signed-integer pixel shader in one process. Quake
  compiles many pipelines, so this may surface during bring-up.

## Blockers

**The driver reports no combined depth-stencil format, and vkQuake requires one.**
`../PS5_Vulkan` reports `DEPTH_STENCIL_ATTACHMENT_BIT` for neither
`VK_FORMAT_D24_UNORM_S8_UINT` nor `VK_FORMAT_D32_SFLOAT_S8_UINT` — the
specification's footnote requires at least one — while vkQuake accepts nothing else
and aborts device initialisation. There is no engine-side fix worth having: the
stencil is functionally used for the sky occlusion trick, and the driver has no
stencil path at all.

Its own audit knows the row and cannot gate it: the two formats are `{sym2}`,
`tools/format_audit.py` files every `{sym2}` cell as conditional without checking
whether it is met, and its output lists `VK_FORMAT_D32_SFLOAT` in that bucket while
the driver carries the bit — so the requirement reads as "conditional" rather than
unmet. `../PS5_Vulkan` is maintained separately and is read-only from here, so this
is reported in `docs/PS5_VULKAN_REQUESTS.md` R1 rather than patched. It blocks the
engine reaching its render passes; it does not block the surface and swapchain work
above, nor the SIGSYS fix.
