#!/usr/bin/env python3
"""Harvest the driver's compiled shaders from the console, to ship them pre-built.

    python3 tools/shader-cache.py build        print the linked driver's cache build (16 hex)
    python3 tools/shader-cache.py harvest      read this build's entries back from the console

../PS5_Vulkan keeps one cache directory per driver build under the title's folder,
/app0/ps5vk-shader-cache/<build>/, one <blake3 key>.bin per compiled shader. After
one launch of a new driver build has compiled everything, `harvest` reads every
entry of that build back over FTP -- twice, because this console's FTP has served
a file's previous bytes -- checks that each entry's header names the key its file
is called by, and keeps them in build/shader-cache/<build>/. tools/build-title.sh
copies that directory into dist/, so the next deployment ships it and a fresh
install or update of the title compiles nothing.

The entries are the console's own compiler output for that build, byte for byte,
so shipping them is the same as the console having compiled them itself; a key
that does not match what the driver would compute is simply never looked up.
"""
from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from ps5_ftp import connect  # noqa: E402

HEADER = ROOT.parent / "PS5_Vulkan" / "build" / "driver" / "generated" / "ps5vk_cache_build.h"


def build() -> str:
    match = re.search(r'PS5VK_CACHE_PS5_BUILD "([0-9a-f]{16})', HEADER.read_text())
    if not match:
        raise SystemExit(f"no PS5 cache build in {HEADER}; build ../PS5_Vulkan first")
    return match.group(1)


def deploy_settings() -> dict:
    spec = importlib.util.spec_from_file_location("deploy", ROOT / "tools" / "deploy-title.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module, module.load_settings()


def read(ftp, path: str) -> bytes:
    chunks: list[bytes] = []
    ftp.retrbinary(f"RETR {path}", chunks.append)
    return b"".join(chunks)


def harvest() -> int:
    tag = build()
    module, settings = deploy_settings()
    remote = f"/data/homebrew/{module.title_id()}/ps5vk-shader-cache/{tag}"
    local = ROOT / "build" / "shader-cache" / tag
    local.mkdir(parents=True, exist_ok=True)
    with connect(**settings) as ftp:
        ftp.cwd(remote)
        lines: list[str] = []
        ftp.retrlines("LIST", lines.append)
        names = [line.split()[-1] for line in lines if line.split()[-1].endswith(".bin")]
        if not names:
            raise SystemExit(f"{remote} holds no entries: launch this build once first")
        for name in names:
            first, second = read(ftp, f"{remote}/{name}"), read(ftp, f"{remote}/{name}")
            if first != second:
                raise SystemExit(f"{name}: two reads differ")
            if first[:32].hex() != name[:-4]:
                raise SystemExit(f"{name}: header key {first[:32].hex()} does not name the file")
            (local / name).write_bytes(first)
    print(f"harvested {len(names)} entries of build {tag} into {local.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "build":
        print(build())
    elif command == "harvest":
        sys.exit(harvest())
    else:
        raise SystemExit(__doc__)
