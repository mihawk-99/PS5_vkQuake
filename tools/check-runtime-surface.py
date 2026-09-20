#!/usr/bin/env python3
"""Find the runtime symbols a title imports that the console's runtime lacks.

    python3 tools/check-runtime-surface.py [elf]

Why this exists. The first console run of this title died with SIGSEGV, an
instruction-fetch fault, and the backtrace symbolized to `Sys_Init+0x31` - the
indirect call to `getcwd`. `getcwd` is declared by the payload SDK's headers, is
absent from its static `libc.a`, and is present in
`$PS5_PAYLOAD_SDK/target/lib/libc_stub_weak.so` - a library whose name is exact:
it exists so that a *link* succeeds, not so that a *call* works. The link was
happy, the ELF carried `U getcwd`, and the console's runtime does not export it,
so the call went through an unfilled GOT slot and nowhere else.

Nothing in the build could have said so, and the console could only say it by
crashing. This can say it beforehand.

How. `tooling/native/libc_builder.cpp` builds the clean-room runtime that ships as
`runtime/libc.prx`, and it derives each export's NID from its name:

    NID = base64(reverse(sha1(name + kNidSuffix)[0:8]))[:11], '/' -> '-'

`tooling/native/runtime/api-surface.txt` is the NID set that runtime exports. So a
name can be turned into a NID and asked whether the runtime provides it. The
suffix and the algorithm are quoted from the builder rather than guessed, and the
result is self-checking: `malloc`, `printf` and `fopen` come back provided and
`getcwd` does not, which is what the console demonstrated.

What it does not cover, and this is the important half. Only the clean-room
runtime's manifest (`api-surface.txt`) is a statement about what runs. The SDK's
`libkernel.so` and `libSce*.so` are the *link* set: their symbol tables say the
linker accepted a call, not that anything implements it. `getcwd` is in
`libkernel.so`, is in `libc_stub_weak.so`, was in the imported set of this title
and of the RetroArch title that ran here for months - and it faulted the first
time this port called it, because that is the difference between importing a
symbol and calling one.

So the imports are split in two:

  * nothing in the SDK provides it   - the linker would have failed, so this
                                       should always be empty, and a shim is due
  * only a link stub provides it     - the getcwd class. The build cannot warn,
                                       because the linker is satisfied. Whether
                                       the console implements any given one is
                                       only knowable by calling it, so these are
                                       reported as candidates rather than faults.

Measured against the four titles already built in this workspace, eleven of these
candidates are imported by this port and by no title that has run here
(`closedir`, `execvp`, `fork`, `freeaddrinfo`, `getaddrinfo`, `gethostbyname`,
`getuid`, `opendir`, `pthread_attr_getstacksize`, `raise`, `readdir`). That is a
narrower list but not a safer one: `getcwd` is not on it, because the RetroArch
title imported it too and never called it.

Exit status is 0 unless something in the first group is found, so a gate can use
it.
"""

from __future__ import annotations

import base64
import hashlib
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SURFACE = ROOT / 'tooling/native/runtime/api-surface.txt'
DEFAULT_ELF = ROOT / 'build/llvm-pie.elf'
SDK_LIB = ROOT / '.deps/native/ps5-payload-sdk/target/lib'
NM = ROOT / '.deps/native/ps5-payload-sdk/bin/llvm-nm'

# Quoted from tooling/native/libc_builder.cpp, kNidSuffix. If that constant moves,
# every NID computed here is wrong, and the self-check below is what notices.
NID_SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])

# Names resolved by something other than a runtime module: the driver's own
# archive members, the AGC stubs, and the C++ runtime the title links statically.
NOT_RUNTIME = re.compile(r'^(ps5vk_|vk_|_Z|__cxx|_Unwind|sceAgc)')


def nid(name: str) -> str:
    digest = hashlib.sha1(name.encode() + NID_SUFFIX).digest()
    encoded = base64.b64encode(bytes(reversed(digest[:8]))).decode()
    return encoded[:11].replace('/', '-')


def runtime_exports() -> set[str]:
    """The clean-room runtime's exports, which its manifest lists by NID only."""
    exports = set()
    for line in SURFACE.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if line and not line.startswith('#'):
            exports.add(line.split('|')[0])
    return exports


def module_exports() -> set[str]:
    """The names the SDK's modules export.

    libkernel and the Sce modules are ordinary ELF shared objects with readable
    symbol tables, so these are names rather than NIDs and need no derivation.
    They are what makes the difference between a real gap and a kernel call: mmap,
    open and pthread_create are absent from the clean-room runtime and present
    here, which is why the first version of this reported a hundred and
    twenty-three candidates instead of a handful.
    """
    names = set()
    for library in sorted(SDK_LIB.glob('libkernel*.so')) + sorted(SDK_LIB.glob('libSce*.so')):
        listing = subprocess.run([str(NM), '--defined-only', str(library)],
                                 capture_output=True, text=True).stdout
        names.update(m.group(1) for m in re.finditer(r' [TWiDB] ([A-Za-z_][A-Za-z0-9_]*)$',
                                                     listing, re.M))
    return names


def self_check(exports: set[str]) -> None:
    """The derivation is checked against names the console has already settled.

    malloc, printf and fopen are provided by the runtime and getcwd is not: the
    first three were used by the run that wrote the trace file, and the fourth is
    the one that faulted. If this ever fails, the suffix or the algorithm has
    changed and every answer below is suspect.
    """
    expected = {'malloc': True, 'printf': True, 'fopen': True, 'strlen': True,
                'getcwd': False, 'getenv': False, 'putenv': False}
    for name, provided in expected.items():
        if (nid(name) in exports) != provided:
            raise SystemExit(f'self-check failed: {name} disagrees with the console')


def main() -> int:
    elf = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_ELF
    if not elf.is_file():
        raise SystemExit(f'no ELF at {elf}; build the title first')
    for required in (NM, SURFACE):
        if not required.exists():
            raise SystemExit(f'missing {required}')

    exports = runtime_exports()
    self_check(exports)
    modules = module_exports()

    listing = subprocess.run([str(NM), '--undefined-only', str(elf)],
                             capture_output=True, text=True, check=True).stdout
    imported = sorted({m.group(1) for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)$',
                                                       listing, re.M)})

    # Only the clean-room runtime's manifest is a statement about what runs. The
    # SDK's libkernel.so and libSce*.so are the *link* set, and their symbol
    # tables are not a runtime guarantee - which is the whole lesson of getcwd:
    # getcwd is in libkernel.so, the link accepted it, and the call faulted. So
    # the imports are split in two.
    unprovided = []   # nothing in the SDK's link set either: certainly needs a shim
    stub_only = []    # only a link stub provides it: the getcwd class
    for name in imported:
        if NOT_RUNTIME.match(name) or nid(name) in exports:
            continue
        (stub_only if name in modules else unprovided).append(name)

    print(f'{elf.relative_to(ROOT)} imports {len(imported)} symbols')
    print(f'  clean-room runtime: {len(exports)} exports, which is a statement about'
          f' what runs')
    print(f'  libkernel and Sce stubs: {len(modules)} names, which is a statement'
          f' about what links')

    if unprovided:
        print(f'\n{len(unprovided)} import(s) NOTHING in the SDK provides -'
              f' shim these:')
        for name in unprovided:
            print(f'  {name:<28} nid {nid(name)}')

    if stub_only:
        print(f'\n{len(stub_only)} import(s) only a LINK STUB provides. getcwd was'
              f' one of these:')
        for name in stub_only:
            print(f'  {name:<28} nid {nid(name)}')
        print('\nThese are the getcwd class. The linker accepted them and the SDK'
              '\ndeclares them, so the build cannot warn; whether the console'
              '\nprovides them is only knowable by calling one. A shim in'
              '\nplatform/ps5/libc_shims.c is the fix, and the answer that is true'
              '\non a console is always available for the ones that matter -'
              '\nsee getcwd, which returns /app0 rather than asking anything.')

    if not unprovided and not stub_only:
        print('\nevery import has a provider')
    return 1 if unprovided else 0


if __name__ == '__main__':
    raise SystemExit(main())
