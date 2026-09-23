# The port itself

Stable. What this repository builds, what it is made of, and the identity the
console sees. State, next steps and blockers are in `docs/ACTIVE.md`; the runs are
in `docs/PHASE_LOG.md`. Nothing here changes from one session to the next, which is
why it is not in `docs/ACTIVE.md` — that file is paid for on every session.

## The title's identity, and its data

`sce_sys/param.json` is the single source for all of it — `build.sh` validates it,
`build-title.sh` reads `titleId` from it for the `dist/` path, and
`deploy-title.py` reads the signed copy on the console's side. Applied with the
boilerplate's own initializer, `make init TITLE_ID=PPSA99010 APP_NAME=vkQuake`.

| Field | Value |
| --- | --- |
| `titleId` | `PPSA99010` |
| `conceptId` | `99010` |
| `contentId` | `UP9000-PPSA99010_00-VKQUAKE000000000` |
| `titleName` | `vkQuake` |

The artwork is vkQuake's own `Misc/vkQuake_512.png`, already the 512x512 launcher
icon; `pic0.dds`, `pic1.dds` and `snd0.at9` are absent rather than replaced,
because conforming ones need a BC7 and an ATRAC9 encoder this host does not have,
and `tools/validate-assets.sh` requires both or neither.

The owner supplies `pak0.pak`; this repository never fetches, commits or stages it.
It lives at `id1/pak0.pak` — the relative path vkQuake's filesystem layer looks for
under the base directory — and becomes `/app0/id1/pak0.pak` on the console.
`.gitignore` covers it anywhere in the tree, and `tests/test_game_data_ignored.py`
checks that it does.

The build identity is a hash of `src/`, `platform/ps5/`, three build scripts, the
pinned vkQuake revision and the linked archives, written to
`build/title_build_identity.h` and printed into the trace by a constructor. The
console transforms the SELF container so its whole-file digest differs from
anything computed here; what the identity answers is *which sources produced the
binary on the console*, which is the question a run's log cannot answer by itself.
`tools/deploy-title.py` refuses to publish a title whose identity it cannot read
back out of the deployed `eboot.bin`. Note that a rebuild can move it, so a crash
and the `build/title.map` used to symbolize it must come from the same build.

## What is built

`tools/build-vkquake-engine.sh` reads upstream's source list and compiles the
engine with the PS5 platform files into `build/vkquake/libvkquake_engine.ps5.a`.
It generates the renderer's shaders and embedded configuration pak as needed.
The upstream tree is fetched, not hand-edited: fifteen exact compatibility edits
are applied reproducibly by `platform/ps5/vkquake-edits.py`. Native SDL-compatible
services and the input/audio adapters live under `platform/ps5/`.

`tools/build-title.sh` links that archive with the Vulkan driver archives and Mesa
objects into `dist/PPSA99010/`. The shaders and the embedded pak are generated, not
committed, because upstream generates them: `tools/build-vkquake-shaders.sh` runs
67 `glslangValidator` jobs and a `mkpak`, and refuses to finish unless the symbols
it produced are exactly the set `Shaders/shaders.h` declares.

## src/, after the strip

The frontend is gone. `src/` holds only PS5 platform code, and the RetroArch
documentation is preserved unchanged under `docs/inherited/`.

| File | What it is |
| --- | --- |
| `display.cpp`, `.hpp` | VideoOut, direct memory, two buffers, present-and-wait |
| `trace.cpp`, `.hpp` | The startup trace, readable from the title folder |
| `ps5_directory.cpp`, `.h` | Directory enumeration without libc's denied `opendir` |
| `locale_shims.c` | The `_l` locale functions glslang and SPIRV-Cross use |
| `memory_ps5.cpp`, `memory_diagnostics.*` | Allocator routing past the libc heap |
| `audio_ps5.cpp` | AudioOut ring and worker thread; the `audio_driver_t` table is gone |
| `input_ps5.cpp`, `.h` | The pad, behind a small C surface in the console's numbering |

Upstream SDL input/audio implementations (including their SDL3 variants) are
excluded from the engine build. `platform/ps5/ps5_input.c` implements `IN_*`
through the native pad backend, including buttons, menu repeat, triggers,
deadzones and movement. `platform/ps5/ps5_audio.c` implements `SNDDMA_*` through
the AudioOut backend at 48 kHz stereo S16. Automated coverage and remaining
physical/audible acceptance are recorded in `docs/ACTIVE.md`.
