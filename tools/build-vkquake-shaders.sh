#!/usr/bin/env bash
# PS5 vkQuake - generate the shaders and the embedded pak upstream's build makes.
#
#   tools/build-vkquake-shaders.sh           generate into build/vkquake/generated
#   tools/build-vkquake-shaders.sh --check   report what is there and stop
#
# Why this exists. vkQuake has no shaders in its repository: it has GLSL and a
# meson.build that compiles it. Every pipeline the renderer builds refers to a
# SPIR-V blob by a C symbol name - `basic_frag_spv`, `world_vert_spv`, and their
# `_size` twins - and those symbols come from .c files that meson generates at
# build time through three host tools:
#
#   glslangValidator   GLSL -> SPIR-V, with the per-variant defines that make one
#                      source into several shaders (basic.frag is nine of them)
#   spirv-opt -Os      strips and canonicalises what glslang emitted
#   bintoc             writes the bytes out as a C array
#
# The same bintoc, given -c, also embeds vkquake.pak - the small pak of stock
# configuration that mkpak builds from Misc/vq_pak - which is where
# `vkquake_pak`, `vkquake_pak_size` and `vkquake_pak_decompressed_size` come
# from.
#
# None of it is committed and none of it is optional, which is why the link fails
# without it: 132 _spv symbols and three pak symbols. Shaders/Compiled/ ships
# holding two empty directories.
#
# The job list is read from upstream's own meson.build rather than retyped, for
# the same reason tools/build-vkquake-engine.sh reads the source list from it: a
# shader upstream adds, or a variant it changes the defines of, has to appear here
# or the renderer will fail at pipeline creation on the console with a null blob
# rather than at build time.
#
# The output is deterministic: glslang and spirv-opt are given no timestamps and
# no build paths, so generating twice gives byte-identical .c files and the build
# does not churn.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

upstream="$root/vendor/vkQuake"
outdir="$root/build/vkquake/generated"
tooldir="$root/build/vkquake/host"

mode=${1:-}
case "$mode" in
    ''|--check) ;;
    *) echo "usage: ${0##*/} [--check]" >&2; exit 2 ;;
esac

[[ -f $upstream/meson.build ]] || {
    echo "error: no vkQuake tree; run tools/fetch-vkquake.sh" >&2
    exit 2
}

for tool in glslangValidator spirv-opt cc; do
    command -v "$tool" >/dev/null || {
        echo "error: $tool is required to build the shaders" >&2
        exit 2
    }
done

# The job list, from upstream's build description. Each line is:
#   <source> <target name> <defines...>
# where the source path is relative to the vkQuake root.
jobs=$(
    python3 - "$upstream/meson.build" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding='utf-8').read()

def block(name):
    """Return the text between `name = [` and its matching `]`."""
    start = text.index(name + ' = [')
    depth = 0
    for index in range(start + len(name) + 4, len(text)):
        if text[index] == '[':
            depth += 1
        elif text[index] == ']':
            if depth == 0:
                return text[start:index + 1]
            depth -= 1
    raise SystemExit(f'{name}: unbalanced brackets')

plain = block('shaders')
for shader in re.findall(r"'(Shaders/[\w.]+)'", plain):
    # A shader whose name mentions sops is compiled for SPIR-V 1.1, which is what
    # lets it use the subgroup operations its source is written around.
    env = ' --target-env vulkan1.1' if 'sops' in shader else ''
    print(f'{shader}\t{shader.split("/")[-1]}\t{env.strip()}')

variants = block('shader_variants')
for source, name, args in re.findall(
        r"\[\s*'(Shaders/[\w.]+)'\s*,\s*'([\w.]+)'\s*,\s*\[([^\]]*)\]\s*\]", variants):
    defines = ' '.join(re.findall(r"'([^']*)'", args))
    print(f'{source}\t{name}\t{defines}')
PY
)

count=$(printf '%s\n' "$jobs" | grep -c . || true)
[[ $count -gt 0 ]] || { echo "error: upstream's meson.build listed no shaders" >&2; exit 2; }

if [[ $mode == --check ]]; then
    printf 'shader jobs: %s\n' "$count"
    printf 'output:      %s\n' "$outdir"
    printf 'generated:   %s .c files present\n' "$(find "$outdir" -name '*.c' 2>/dev/null | wc -l)"
    exit 0
fi

# The two host tools, built from upstream's own sources with the host compiler.
# They are the format converters, not part of the title.
mkdir -p "$tooldir"
build_tool() {
    local name=$1 source=$2
    if [[ ! -x $tooldir/$name || $source -nt $tooldir/$name ]]; then
        cc -O2 -o "$tooldir/$name" "$source"
    fi
}
build_tool bintoc "$upstream/Shaders/bintoc.c"
build_tool mkpak "$upstream/Misc/vq_pak/mkpak.c"

rm -rf "$outdir"
mkdir -p "$outdir/spv"

spirv_opt_args=(-Os --canonicalize-ids --strip-debug)
compiled=0
failed=0

while IFS=$'\t' read -r source name defines; do
    [[ -n $source ]] || continue
    spv="$outdir/spv/$name.spv"

    # shellcheck disable=SC2086 # defines is a list of flags, split on purpose.
    if ! glslangValidator -V --quiet --target-env vulkan1.0 $defines \
            -o "$outdir/spv/$name.raw.spv" "$upstream/$source" >"$outdir/spv/$name.log" 2>&1; then
        printf 'error: %s did not compile:\n' "$source" >&2
        sed 's/^/       /' "$outdir/spv/$name.log" >&2
        failed=$((failed + 1))
        continue
    fi
    if ! spirv-opt "${spirv_opt_args[@]}" "$outdir/spv/$name.raw.spv" -o "$spv" \
            >>"$outdir/spv/$name.log" 2>&1; then
        printf 'error: %s did not optimise:\n' "$name" >&2
        sed 's/^/       /' "$outdir/spv/$name.log" >&2
        failed=$((failed + 1))
        continue
    fi
    rm -f "$outdir/spv/$name.raw.spv" "$outdir/spv/$name.log"

    # The symbol name is the whole file name with every character outside
    # [A-Za-z0-9_] folded to an underscore, and _spv appended - so alias.vert
    # becomes alias_vert_spv and alias.frag becomes alias_frag_spv, which are two
    # symbols and not one. Dropping the extension instead of folding it is the
    # mistake this line was written twice to avoid: it makes eight pairs of a
    # .vert and a .frag that share a stem collide, and each pair silently keeps
    # only whichever compiled last. Shaders/shaders.h declares the names, and the
    # check below compares against it.
    symbol=$(printf '%s_spv' "$name" | tr -c 'A-Za-z0-9_' '_')
    "$tooldir/bintoc" "$spv" "$symbol" "$outdir/${symbol}.c"
    compiled=$((compiled + 1))
done <<<"$jobs"

if ((failed)); then
    echo "error: $failed of $count shaders did not build" >&2
    exit 1
fi

# The contract, checked rather than assumed. Shaders/shaders.h is committed in
# upstream and declares every blob the renderer looks for, so the generated set
# has to be exactly that set: one missing is a null shader at pipeline creation on
# the console, and one extra is a job list that has drifted from the header it is
# supposed to serve.
#
# The header names the symbols without their suffix - DECLARE_SHADER_SPV(basic_vert)
# declares basic_vert_spv - so the suffix is stripped from the generated names
# before the two are compared. Comparing them as they come is how the first
# version of this check reported all sixty-seven as missing.
grep -oE 'DECLARE_SHADER_SPV \([A-Za-z0-9_]+\)' "$upstream/Shaders/shaders.h" |
    grep -oE '\([A-Za-z0-9_]+\)' | tr -d '()' | sort >"$outdir/.declared"
find "$outdir" -maxdepth 1 -name '*_spv.c' -printf '%f\n' |
    sed -e 's/\.c$//' -e 's/_spv$//' | sort >"$outdir/.generated"
missing=$(comm -23 "$outdir/.declared" "$outdir/.generated")
extra=$(comm -13 "$outdir/.declared" "$outdir/.generated")
if [[ -n $missing || -n $extra ]]; then
    if [[ -n $missing ]]; then
        echo "error: shaders.h declares these, which were not generated:" >&2
        printf '       %s\n' $missing >&2
    fi
    if [[ -n $extra ]]; then
        echo "error: these were generated but shaders.h does not declare them:" >&2
        printf '       %s\n' $extra >&2
    fi
    exit 1
fi
rm -f "$outdir/.declared" "$outdir/.generated"

# The embedded pak. mkpak reads a table of contents that names every file it
# packs, so the pak's contents are upstream's list rather than a guess at it.
pak="$outdir/vkquake.pak"
"$tooldir/mkpak" "$pak" "$upstream/Misc/vq_pak" "$upstream/Misc/vq_pak/vq_pak_contents.txt"
"$tooldir/bintoc" -c "$pak" vkquake_pak "$outdir/embedded_pak.c"

# The .spv files are an intermediate: the C arrays are what the title compiles.
rm -rf "$outdir/spv"

echo "==> [shaders] compiled $compiled shaders and packaged $(basename "$pak")"
echo "==> [shaders] $outdir: $(find "$outdir" -name '*.c' | wc -l) generated C files"
