#!/usr/bin/env bash
# PS5 vkQuake - cross-compile vkQuake's engine into an archive for the title.
#
#   tools/build-vkquake-engine.sh            compile and archive
#   tools/build-vkquake-engine.sh --list     print what would be compiled, and why the rest is not
#
# What this produces is build/vkquake/libvkquake_engine.ps5.a: vkQuake's engine
# objects, compiled for x86_64-sie-ps5, ready for tools/build-title.sh to link
# into the title. The engine is C and lives in vendor/, which the title build
# does not compile - it compiles src/ and takes everything else as an archive -
# so this script is the seam between the two.
#
# The list of sources is upstream's own. meson.build enumerates the files the
# desktop build compiles, and this script compiles exactly that list minus the
# ones this port replaces. The check at the bottom is the point of doing it that
# way: a file that upstream adds, or that this port forgets to either compile or
# replace, fails the build instead of going missing quietly.
#
# The ten excluded files are the SDL platform layer. Each is replaced by a port
# file that does the same job on the console - the window and Vulkan surface, the
# event and gamepad layer, the audio device, the file and system layer, and the
# entry point. Until those exist the exclusion is recorded as unported, and
# --list says so rather than implying the port is complete.
#
# What this does NOT need: SDL. Quake/q_stdinc.h includes "SDL.h" from every
# translation unit, and platform/ps5/SDL.h is what that resolves to - the ~30
# functions the engine proper calls, not a windowing system. See that file's
# header for the line between the two.
#
# Where the toolchain comes from. This repository is an application of
# ps5-native-app-boilerplate: the Makefile, tools/build.sh, tooling/prospero-clang18,
# the linker script and the payload SDK bootstrap are that project's, and this
# script follows its convention of resolving the SDK from .deps/ rather than
# asking the caller to export a path. `make deps` fetches it; nothing else has to
# be set up.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

upstream="$root/vendor/vkQuake"
outdir="$root/build/vkquake"
objdir="$outdir/obj"
archive="$outdir/libvkquake_engine.ps5.a"

# Upstream's source list, taken from upstream's build description rather than
# retyped here, so it cannot drift from the revision the pin names.
#
# It comes from two places, because upstream splits it in two: the base `srcs`
# array, and the `else` branch of the Windows conditional that appends the Unix
# network, platform and system sources. Reading only the array would silently
# drop sys_sdl_unix.c and pl_linux.c - which is exactly what the check below
# caught the first time this script ran.
meson_listing=$(
    {
        sed -n '/^srcs = \[/,/^\]/p' "$upstream/meson.build"
        awk "/^if host_machine.system\(\) == 'windows'/{in_if=1}
             in_if && /^else\$/{in_else=1; next}
             in_if && /^endif\$/{in_if=0; in_else=0}
             in_else" "$upstream/meson.build"
    } | grep -o -E 'Quake/[a-z0-9_]+\.c' | sort -u
)

# The SDL platform layer, and what replaces it. The second column is the port
# file; "unported" means the replacement is not written yet, which is a state the
# port is allowed to be in but is not allowed to hide.
#
# Five files are left, and they are the two device APIs. Everything else -
# including gl_vidsdl.c, main_sdl.c, both sys_sdl files and pl_linux.c - compiles
# for this console unmodified, because each needed only SDL functions the port
# answers in platform/ps5/. Nothing upstream is patched; the console is described
# to it.
#
# in_sdl.c and in_sdl2.c want the SDL gamepad and event API; snd_sdl.c wants the
# SDL audio-device API. Those are real interfaces rather than window bookkeeping,
# and they are M4 and M5.
excluded=$(cat <<'EOF'
in_sdl.c	unported	the shared input layer: key mapping, deadzones, joy movement
in_sdl2.c	unported	the SDL2 event loop and gamepad reads
in_sdl3.c	unported	the SDL3 twin of the above; only one of the two is ever built
snd_sdl.c	unported	the SDL2 audio device behind SNDDMA_*
snd_sdl3.c	unported	the SDL3 twin of the above
EOF
)

mode=${1:-}
case "$mode" in
    ''|--list) ;;
    *) echo "usage: ${0##*/} [--list]" >&2; exit 2 ;;
esac

# Every excluded file must be one upstream's list actually names, so a rename
# upstream is a failure here rather than a silent no-op.
excluded_names=$(printf '%s\n' "$excluded" | cut -f1 | grep -v '^$')
for name in $excluded_names; do
    grep -qx "Quake/$name" <<<"$meson_listing" || {
        echo "error: the exclusion list names Quake/$name, which upstream's build does not" >&2
        echo "       upstream moved or removed it; the port layer must be re-checked" >&2
        exit 2
    }
done

to_compile=()
while IFS= read -r source; do
    [[ -n $source ]] || continue
    base=${source#Quake/}
    if grep -qx "$base" <<<"$excluded_names"; then
        continue
    fi
    to_compile+=("$source")
done <<<"$meson_listing"

# The port's own C, compiled with the same flags so it can share the engine's
# headers. Empty until the layers above land, which is why it is a glob.
port_sources=()
while IFS= read -r source; do
    [[ -n $source ]] || continue
    port_sources+=("$source")
done < <(find platform/ps5 -maxdepth 1 -name '*.c' -printf '%p\n' 2>/dev/null | sort)

# The generated C: vkQuake ships GLSL and a build description, not shaders, so the
# SPIR-V arrays and the embedded pak are made here. Both are symbols the renderer
# and the filesystem layer reference by name, so they are not optional and the
# link fails without them - 132 _spv symbols and three pak symbols.
#
# The generators run only when compiling, never for --list: listing what would be
# built should not take a minute and should not need glslang installed.
generated_sources=()
if [[ $mode != --list ]]; then
    bash "$root/tools/build-vkquake-shaders.sh" >&2
    while IFS= read -r source; do
        [[ -n $source ]] || continue
        generated_sources+=("$source")
    done < <(find build/vkquake/generated -maxdepth 1 -name '*.c' -printf '%p\n' 2>/dev/null | sort)
fi

if [[ $mode == --list ]]; then
    printf 'compiled (%d from upstream, %d from the port layer):\n' "${#to_compile[@]}" "${#port_sources[@]}"
    printf '  %s\n' "${to_compile[@]}"
    ((${#port_sources[@]})) && printf '  %s\n' "${port_sources[@]}"
    printf '\nreplaced by the port layer:\n'
    printf '%s\n' "$excluded" | while IFS=$'\t' read -r name state what; do
        [[ -n $name ]] || continue
        printf '  Quake/%-16s %-9s %s\n' "$name" "$state" "$what"
    done
    exit 0
fi

# The SDK is resolved here, not demanded of the caller, because that is what the
# boilerplate this project is built on does: tools/build.sh in
# ps5-native-app-boilerplate sets sdk_root="$root/.deps/native/ps5-payload-sdk"
# and passes it to the compiler wrapper inline, so that a build works from a clean
# checkout without anyone exporting anything. The same path is the same fact here,
# and the environment variable stays supported as an override for a checkout that
# keeps its cache elsewhere.
sdk_root=${PS5_PAYLOAD_SDK:-$root/.deps/native/ps5-payload-sdk}
if [[ ! -d $sdk_root ]]; then
    echo "error: no PS5 payload SDK at $sdk_root" >&2
    echo "       run 'make deps' (tools/setup-native-dependencies.sh) to fetch it" >&2
    exit 2
fi
export PS5_PAYLOAD_SDK=$sdk_root

# The wrapper wants a clang that can target x86_64-sie-ps5; the boilerplate looks
# for clang-18 first and this host has none, so the SDK's own dispatcher is the
# last fallback rather than the first choice. It is a dispatcher and not a second
# toolchain - $sdk_root/bin/clang execs the LLVM its prospero-llvm-config names -
# so the target flags come from the wrapper either way and the selection only
# decides which LLVM runs.
if [[ -z ${PS5_CLANG:-} ]]; then
    PS5_CLANG=$(command -v clang-18 || command -v clang || true)
fi
[[ -n ${PS5_CLANG:-} && -x $PS5_CLANG ]] || {
    echo "error: no clang for the target; set PS5_CLANG to a clang executable" >&2
    exit 2
}
export PS5_CLANG

cc="$root/tooling/prospero-clang18"
# -DTASK_AFFINITY_NOT_AVAILABLE: the CPU pinning path needs _GNU_SOURCE and
#   pthread_setaffinity_np, which the payload SDK's FreeBSD headers do not carry.
#   The engine treats its absence as normal and skips the pinning.
# -D_FILE_OFFSET_BITS=64: upstream's own flag, and sys_sdl_unix.c asserts on it.
# The Vulkan headers are upstream's vendored 1.4.341 set, because the payload SDK
# ships no Vulkan headers and upstream's include guard demands >= 162.
common_flags=(
    -c
    -I"$upstream/Quake"
    -I"$root/platform/ps5"
    -I"$upstream/Windows/misc/include"
    -D_FILE_OFFSET_BITS=64
    -D_GNU_SOURCE
    -DTASK_AFFINITY_NOT_AVAILABLE
    -O2
    -ffunction-sections
    -fdata-sections
    -w
)

rm -rf "$objdir"
mkdir -p "$objdir"

compiled=0
failed=0
objects=()

compile_one() {
    local path=$1 label=$2
    local base
    base=$(basename "${label%.c}")
    local object="$objdir/$base.o"
    if ! sh "$cc" "${common_flags[@]}" "$path" -o "$object" 2>"$objdir/$base.err"; then
        printf 'error: %s did not compile:\n' "$label" >&2
        grep -E 'error:' "$objdir/$base.err" | head -5 | sed 's/^/       /' >&2
        failed=$((failed + 1))
        return
    fi
    objects+=("$object")
    compiled=$((compiled + 1))
}

for source in "${to_compile[@]}"; do
    compile_one "$upstream/$source" "$source"
done

for source in "${port_sources[@]}"; do
    compile_one "$root/$source" "$source"
done

for source in "${generated_sources[@]}"; do
    compile_one "$root/$source" "$source"
done

if ((failed)); then
    echo "error: $failed of $((compiled + failed)) sources did not compile" >&2
    exit 1
fi

# That every object is a *target* object cannot be decided one object at a time,
# and the reason is worth stating because it is the whole hazard: x86_64-sie-ps5
# shares the host's ELF header. Same class, same machine, same OSABI - UNIX -
# System V, same e_flags 0x0. An object compiled for this console and an object
# compiled for the host beside it are indistinguishable by their headers, so a
# stray host object would stage into the title and fail only on the console.
#
# What can be decided is that the toolchain is the target toolchain, and that is
# checked here instead: the wrapper passes -femulated-tls, a host compiler does
# not, and a file with thread-local storage compiled through it must therefore
# define __emutls_v.*. If PS5_CLANG is ever pointed at something that is not the
# target compiler, this canary fails before any engine object is trusted. The
# engine does use TLS - common.c's com_token, com_filesize and the va() buffers
# are all THREAD_LOCAL - so the canary is testing the same mechanism the engine
# depends on, not a synthetic one.
canary_dir="$outdir/canary"
mkdir -p "$canary_dir"
cat >"$canary_dir/tls.c" <<'EOF'
/* The canary: one thread-local definition, so the target's emulated-TLS
 * lowering has something to emit. */
_Thread_local int ps5_vkquake_tls_canary;
int ps5_vkquake_canary_read(void) { return ps5_vkquake_tls_canary; }
EOF
if ! sh "$cc" -c -O2 "$canary_dir/tls.c" -o "$canary_dir/tls.o" 2>"$canary_dir/tls.err"; then
    echo "error: the toolchain could not compile the TLS canary:" >&2
    sed 's/^/       /' "$canary_dir/tls.err" >&2
    exit 2
fi
if ! "$PS5_PAYLOAD_SDK/bin/llvm-nm" --defined-only "$canary_dir/tls.o" 2>/dev/null | grep -q '__emutls_v\.'; then
    echo "error: the toolchain compiled the TLS canary without emulated TLS, so it is" >&2
    echo "       not the x86_64-sie-ps5 compiler this port needs. PS5_CLANG=$PS5_CLANG" >&2
    exit 2
fi

# Each object must at least be a relocatable x86-64 ELF, which rules out a
# truncated or foreign-architecture artifact reaching the archive even though it
# cannot rule out a host one.
for object in "${objects[@]}"; do
    if ! readelf -h "$object" 2>/dev/null | grep -q 'REL (Relocatable file)'; then
        echo "error: $object is not a relocatable ELF object" >&2
        exit 2
    fi
done

rm -f "$archive"
"$PS5_PAYLOAD_SDK/bin/llvm-ar" rcs "$archive" "${objects[@]}"

echo "==> [vkquake] compiled $compiled sources for x86_64-sie-ps5"
echo "==> [vkquake] toolchain canary: emulated TLS present, so PS5_CLANG is the target compiler"
echo "==> [vkquake] $archive"
echo "==> [vkquake] $(du -h "$archive" | cut -f1), ${#objects[@]} objects"
