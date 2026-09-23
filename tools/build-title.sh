#!/usr/bin/env bash
# PS5 vkQuake - build the title.
#
#   tools/build-title.sh            compile and link the title
#   tools/build-title.sh --stage    also copy the result to handoff/<TITLE_ID>/
#
# The build is three steps in one direction:
#
#   1. tools/build-vkquake-engine.sh  compiles vkQuake's engine and the port
#                                     layer into build/vkquake/libvkquake_engine.ps5.a
#   2. make app                       compiles src/, links that archive with the
#                                     Vulkan driver archives and the pipeline's
#                                     CRT, signs the result and assembles the title
#                                     folder under dist/<TITLE_ID>/
#   3. handoff                        a copy of that folder, on request
#
# Step 2 is the project's own Makefile, not a reimplementation of it. What this
# script adds is what the Makefile cannot know, and each of the four was found by
# a failed build:
#
#   PS5_PAYLOAD_SDK      the vendored SDK. The compiler wrapper's default and a
#                        donor checkout's differ, so it is named here.
#   PS5_CLANG            the wrapper defaults to clang-18, which this host does not
#                        have; plain clang is what it builds with.
#   PYTHONPATH           mbedTLS regenerates a source file by running a script that
#                        imports jsonschema; tooling/pystub supplies it.
#   APP_STATIC_ARCHIVES  the engine archive from step 1.
#
# Signing happens inside step 2 and is not optional: the console loads a fake self,
# not an ELF, and an unsigned eboot.bin is a title that fails to start with no
# message of its own.
#
# What this script used to be, and what is missing. It built RetroArch, six
# libretro cores and the import table that let the frontend load them, then linked
# src/ against RetroArch's headers so that a driver table written in src/ matched
# the frontend's view of the same struct. None of that survives: this port links
# vkQuake's engine instead of a frontend, and the driver tables are gone with it.
#
# The one consequence that is not just deletion is that the engine and src/ no
# longer share a header full of #ifdefs, so the ABI-matching problem that block of
# defines existed to solve cannot happen here. The defines below are the port
# layer's own and are few.
#
# NOT YET WIRED: vkQuake's entry point and the ten platform files the port layer
# owes (see docs/ACTIVE.md). Until they exist this script reaches the link and
# fails on the undefined VID_*, Sys_*, IN_* and SNDDMA_* symbols, which is the
# honest state of M0 - the engine compiles, the title does not link.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

# Opt-in diagnostics do not change allocation routing or normal builds.
memory_diagnostics=${PS5_MEMORY_DIAGNOSTICS:-0}
[[ $memory_diagnostics == 0 || $memory_diagnostics == 1 ]] || {
    echo "PS5_MEMORY_DIAGNOSTICS must be 0 or 1" >&2; exit 2;
}
stage=false
case "${1:-}" in
    '') ;;
    --stage) stage=true ;;
    *) echo "usage: ${0##*/} [--stage]" >&2; exit 2 ;;
esac

sdk="$root/.deps/native/ps5-payload-sdk"
[[ -x $sdk/bin/prospero-lld ]] || {
    echo "error: no SDK at $sdk; run 'make deps' first" >&2
    exit 2
}

echo "==> [title] step 1/3: the engine"
bash "$root/tools/build-vkquake-engine.sh"

engine_archive="$root/build/vkquake/libvkquake_engine.ps5.a"
[[ -f $engine_archive ]] || { echo "error: no engine archive at $engine_archive" >&2; exit 2; }

# The Vulkan driver is linked, not loaded. ../PS5_Vulkan measured that a PS5 title
# cannot dlopen a repository-built .so, so the driver arrives as archives that are
# linked whole into the title. This project consumes its released artifacts and
# never builds them (docs/PLAN.md, "What is deliberately not planned").
vulkan_dir="${PS5_VULKAN_DIR:-$root/../PS5_Vulkan}"
vulkan_archives=(
    "$vulkan_dir/build/driver/ps5/libps5vk.ps5.a"
    "$vulkan_dir/.deps/native/vulkan-runtime/lib/libvk_runtime.ps5.a"
    "$vulkan_dir/build/driver/ps5/libpsbc_driver.ps5.a"
    "$vulkan_dir/.deps/native/psbc/lib/libpsbc_support.ps5.a"
)
vulkan_missing=()
for archive in "${vulkan_archives[@]}"; do
    [[ -f $archive ]] || vulkan_missing+=("$archive")
done
if (( ${#vulkan_missing[@]} )); then
    printf 'error: the Vulkan driver archives are missing; the title would link with\n' >&2
    printf '       vkGetInstanceProcAddr unresolved. Build them in ../PS5_Vulkan\n' >&2
    printf '       (tools/build-driver.sh) or set PS5_VULKAN_DIR.\n' >&2
    printf '       missing: %s\n' "${vulkan_missing[@]}" >&2
    exit 2
fi

vulkan_flags="--no-dynamic-linker -z nodynamic-undefined-weak"

# Three Mesa utility sources the archives above reference but do not carry:
# ../PS5_Vulkan's PS5 object list filters u_thread.c, anon_file.c and os_file.c
# out, and its own libvulkan.so.1 only links because a shared object may leave
# symbols undefined. A title may not, so they are compiled here from that
# project's sources with its PS5 configuration and linked as plain objects.
# tools/build-mesa-util.sh says which symbols each one is for. It prints the
# object paths on stdout, so a compile failure has to be caught rather than
# swallowed by a process substitution that would let the link fail later on
# symbols this step exists to supply.
if ! vulkan_object_list=$(PS5_VULKAN_DIR="$vulkan_dir" PS5_PAYLOAD_SDK="$sdk" \
        PS5_CLANG="${PS5_CLANG:-/usr/bin/clang}" bash "$root/tools/build-mesa-util.sh"); then
    echo "error: the driver's Mesa utility objects did not build" >&2
    exit 2
fi
mapfile -t vulkan_objects <<< "$vulkan_object_list"

# Bind the trace and the deployable folder to these exact source and archive
# inputs. The console transforms the SELF container, so its whole-file digest
# differs from anything computed here; what this identity answers is "which
# sources produced the binary that is on the console", which is the question a
# console run's log cannot answer by itself.
python3 - "$root" "$memory_diagnostics" "$engine_archive" "${vulkan_archives[@]}" \
    "${vulkan_objects[@]}" <<'PY'
import hashlib, pathlib, sys
root = pathlib.Path(sys.argv[1])
inputs = sorted(p for p in (root / "src").rglob("*") if p.is_file())
inputs += sorted(p for p in (root / "platform" / "ps5").rglob("*") if p.is_file())
inputs += [root / name for name in ("tools/build-title.sh", "tools/build.sh",
                                    "tools/build-vkquake-engine.sh",
                                    "vendor/vkQuake/.vkquake-revision")]
inputs += [pathlib.Path(name) for name in sys.argv[3:]]
digest = hashlib.sha256()
digest.update(b"memory-diagnostics=" + sys.argv[2].encode() + b"\0")
for path in inputs:
    if not path.is_file():
        continue
    digest.update(path.name.encode() + b"\0")
    digest.update(hashlib.sha256(path.read_bytes()).digest())
identity = digest.hexdigest()
(root / "build/title_build_identity.h").write_text(
    '#define PS5_VKQUAKE_BUILD_ID "build identity: ' + identity + '"\n')
print("==> [title] build identity: " + identity)
PY

echo "==> [title] step 2/3: the title"
# The port layer's defines. PS5_MEMORY_DIAGNOSTICS routes the allocator through
# the wrapping below so that a console run can say where memory went.
port_definitions=""
memory_wrap_flags=""
if [[ $memory_diagnostics == 1 ]]; then
    port_definitions="PS5_MEMORY_DIAGNOSTICS"
    memory_wrap_flags="--wrap=posix_memalign"
fi

PS5_PAYLOAD_SDK="$sdk" \
PS5_CLANG="${PS5_CLANG:-/usr/bin/clang}" \
PYTHONPATH="$root/tooling/pystub${PYTHONPATH:+:$PYTHONPATH}" \
APP_DEFINITIONS="$port_definitions" \
APP_INCLUDE_PATHS="src platform/ps5 vendor/vkQuake/Quake vendor/vkQuake/Windows/misc/include" \
APP_STATIC_ARCHIVES="build/vkquake/libvkquake_engine.ps5.a" \
APP_VULKAN_ARCHIVES="${vulkan_archives[*]}" \
APP_EXTRA_OBJECTS="${vulkan_objects[*]}" \
APP_LINK_FLAGS="$vulkan_flags --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free --wrap=fopen --wrap=fread --wrap=fseek --wrap=fclose $memory_wrap_flags" \
    make app

title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$root/sce_sys/param.json")
dist="$root/dist/$title_id"
[[ -f $dist/eboot.bin ]] || { echo "error: no eboot.bin under $dist" >&2; exit 2; }

# The manifest is recorded here, as part of building, because a folder published
# without one cannot be told apart from the folder published last week: this
# project has already produced a title folder whose eboot.bin was a raw link-stage
# ELF and whose libc.prx was a different build from the one in the tree, with
# every file present and every size plausible. Recording it on every build means
# the digests describe the bytes that exist now, and tools/check-manifest.sh can
# then verify the copy that reaches the console.
# Pre-built shaders for the linked driver build, when tools/shader-cache.py has
# harvested them: the title then ships them and compiles nothing on a fresh
# install. A build with none ships none, which is only slower the first time.
cache_build=$(python3 "$root/tools/shader-cache.py" build 2>/dev/null || true)
rm -rf "$dist/ps5vk-shader-cache"
if [[ -n $cache_build && -d $root/build/shader-cache/$cache_build ]]; then
    mkdir -p "$dist/ps5vk-shader-cache/$cache_build"
    cp -- "$root/build/shader-cache/$cache_build"/*.bin "$dist/ps5vk-shader-cache/$cache_build/"
    printf '==> [title] shipping %s pre-built shaders of driver build %s\n' \
        "$(find "$dist/ps5vk-shader-cache/$cache_build" -name '*.bin' | wc -l)" "$cache_build"
fi

bash "$root/tools/check-manifest.sh" --record

printf '==> [title] built %s (%s files, eboot.bin %s bytes)\n' \
    "$dist" "$(find "$dist" -type f | wc -l)" "$(stat -c %s "$dist/eboot.bin")"

if $stage; then
    out="$root/handoff/$title_id"
    rm -rf -- "$out"
    mkdir -p -- "$root/handoff"
    cp -a -- "$dist" "$out"
    printf '==> [title] step 3/3: staged %s\n' "$out"
else
    echo "==> [title] step 3/3: not staging (pass --stage to copy to handoff/)"
fi
