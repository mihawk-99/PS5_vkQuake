# What happened, run by run

Append-only. New dated entries go at the end. A run is recorded at the same
detail whether it passed or failed, and an entry is never rewritten — a later run
that contradicts an earlier one is a new entry that says so.

A "console run" means an artifact uploaded to the console, launched from the
console's own homebrew launcher, with `klog` captured for the whole run.
**Nothing in this file is a console run yet.** The entries below are host runs:
what was built and what it was built with.

---

## 2026-09-20: M0 — the contract

Four commits, each verified on the host before it was committed.

### The baseline, and the pin (`550b64c`)

The tree arrived as a copy of the PS5 RetroArch project with an empty `.git`; the
copy is missing `patches/`, `build/` and `OLD_PPSSPP/` from the original, none of
which this port needs (`patches/` holds libretro per-core patch sets). The copy
was committed as received, so that the strip in the next commit is a reviewable
diff and so the inherited material stays recoverable.

`tools/fetch-vkquake.sh` pins vkQuake at 1.36.0, verified against the commit the
tag points at (`1b948e29a6e3e412df2e1814615d71fb8040bce5`) rather than the tag's
name. 1.36.0 is the newest release; `master` was 88 commits ahead of it, and a
port wants a fixed target.

```
$ bash tools/fetch-vkquake.sh --check
pin:      1.36.0 (1b948e29a6e3e412df2e1814615d71fb8040bce5)
present:  true
revision: HEAD:1b948e29a6e3e412df2e1814615d71fb8040bce5

$ bash tools/fetch-vkquake.sh
==> [fetch] dropped its history and recorded the revision
$ bash tools/fetch-vkquake.sh --verify
==> [fetch] vendor/vkQuake is at the pinned revision
```

179 MB with history, 39 MB without it.

### The frontend strip (`24d8345`)

Six sources and seventeen test files removed. The unit gate was red before and
green after, because both failures were frontend tests — the direction is
inverted on purpose:

```
before:  Ran 74 tests ... FAILED (errors=2, skipped=8)
after:   Ran 12 tests in 4.950s
         OK
```

`tests/test_platform_paths.py` held three cases: two tested the frontend and one
tested `src/ps5_directory.cpp`. That case survives as `tests/test_ps5_directory.py`.

### The engine cross-compiles (`5017b08`)

`platform/ps5/SDL.h` is the SDL2 surface vkQuake's engine actually uses, and
`tools/build-vkquake-engine.sh` compiles upstream's own `meson.build` source list
minus ten named platform files.

```
$ bash tools/build-vkquake-engine.sh
==> [vkquake] compiled 69 sources for x86_64-sie-ps5
```

The script's first run failed, and usefully: the source list was read from the
base `srcs` array alone, which silently dropped `sys_sdl_unix.c` and `pl_linux.c`
— upstream appends those in the `else` branch of its Windows conditional. The
script now reads both and fails loudly if an excluded file is one upstream does
not name.

The format gate was red on arrival, on `src/thread_probe.cpp`, an inherited file
this port had not touched. It is reformatted there (28 reflowed lines, no
behaviour change) and the gate's scope is extended to `platform/`, which had been
the one directory of this project's own code the policy did not cover.

### The compatibility layer (`943316f`)

```
$ cc -std=gnu11 -O1 -pthread -Wall -Wextra -Werror tests/sdl_ps5_test.c -o build/sdl-ps5-test
$ ./build/sdl-ps5-test
sdl_ps5: the SDL compatibility layer, on the host
  mutex is recursive (three nested locks released)
  mutex excludes: 80000 increments from 4 threads, none lost
  condition: timeout answered non-zero after 25ms, signal answered zero
  threads: 200 joined with a status, 200 detached, no double free
  semaphore: wait, try-wait, post and value agree
  clock: 40ms sleep measured 40ms, counter monotonic, 14 cpus
  error string survives, and a refused call explains itself
  filesystem: /app0 for both path queries, RWops round-trips a file
  cpu features: SSE=1 SSE2=1 AVX=1 AVX2=1, and they match __builtin_cpu_supports
sdl_ps5: all checks passed

$ bash tools/build-vkquake-engine.sh
==> [vkquake] compiled 72 sources for x86_64-sie-ps5
==> [vkquake] toolchain canary: emulated TLS present, so PS5_CLANG is the target compiler
==> [vkquake] 3.6M, 72 objects
```

The CPU-feature line is the one that changed: it first read `AVX2=0` on a CPU that
has AVX2, because `__get_cpuid(7, ...)` returns zeroes on this toolchain. The test
that was supposed to catch it passed, because it checked only that the four answers
agreed with each other. See `docs/FINDINGS.md`.

### The documentation (`HEAD`)

`docs/PLAN.md`, `docs/ACTIVE.md` and `docs/FINDINGS.md` written for this project;
`docs/DEPLOYMENT.md`, `docs/TESTING.md` and `docs/TROUBLESHOOTING.md` moved up
from `docs/inherited/` unchanged; the rest of `docs/inherited/` removed.

Those three were written for the RetroArch title and still are: what carries over
is the console-side procedure — how the title's folder is reached, what `klog`
capture looks like, which failures are the console's rather than the code's. What
does not carry over is anything naming RetroArch, a libretro core, or the RGUI and
XMB menus. They are carried forward rather than rewritten because M1 needs the
console procedure and rewriting it before the first console run would be writing
down something unmeasured.

`tools/build-vkquake-engine.sh` now resolves the payload SDK from
`.deps/native/ps5-payload-sdk` itself, as the boilerplate's `tools/build.sh` does,
rather than requiring the caller to export `PS5_PAYLOAD_SDK`. Verified by running
it in a shell with neither `PS5_PAYLOAD_SDK` nor `PS5_CLANG` set: 72 sources,
3.6M, exit 0.
