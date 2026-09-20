#!/usr/bin/env bash
# PS5 RetroArch - the format gate's second half: the C and C++ this project owns.
#
#   bash tools/lint-format.sh            report and fail on a difference
#   bash tools/lint-format.sh --write    format in place
#
# What is checked, and why the boundary sits where it does. clang-format is run
# over src/, tests/, tooling/native/ and platform/ - the code this project writes -
# and never over vendor/ or build/, which are upstream's and generated:
# reformatting those would make the port's diff against upstream unreadable, and
# the invariant in AGENTS.md is that upstream stays upstream.
#
# platform/ joined the list when the vkQuake port layer did. The port layer is
# this project's own C, it is the code most likely to be read by someone comparing
# it against the SDL source it replaces, and a policy that covers src/ but not the
# directory beside it is an accident of history rather than a decision.
#
# .clang-format at the repository root is the policy. It is read by clang-format
# itself, so this script does not restate any of it.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

write=0
case "${1:-}" in
    '') ;;
    --write) write=1 ;;
    *) echo "usage: ${0##*/} [--write]" >&2; exit 2 ;;
esac

formatter=${CLANG_FORMAT:-}
if [[ -z $formatter ]]; then
    formatter=$(command -v clang-format-18 || command -v clang-format || true)
fi
[[ -n $formatter ]] || { echo "error: clang-format is required for the format gate" >&2; exit 2; }
[[ -f .clang-format ]] || { echo "error: no .clang-format at the repository root" >&2; exit 2; }

# Generated sources are not formatted, they are regenerated. platform/ps5/vk_globals.c
# is emitted by tools/gen-vk-globals.py, and running clang-format over it would
# make the file disagree with its generator - which is the one thing that has to
# stay checkable about it. It is checked the other way instead, below: the
# generator is asked whether the file on disk is what it would write.
generated_sources=(platform/ps5/vk_globals.c)

mapfile -t sources < <(find src tests tooling/native platform -type f \
    \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    2>/dev/null | sort | grep -v -x -F "$(printf '%s\n' "${generated_sources[@]}")" || true)

if (( ${#sources[@]} == 0 )); then
    echo "==> [lint-format] no sources to format"
    echo "lint-format: PASS"
    exit 0
fi

echo "==> [lint-format] $("$formatter" --version) over ${#sources[@]} files"
if (( write )); then
    "$formatter" -i "${sources[@]}"
    echo "==> [lint-format] formatted in place"
    echo "lint-format: PASS"
    exit 0
fi

# --dry-run --Werror makes clang-format a check rather than a filter: it prints
# nothing and exits non-zero on the first file that differs, which is what a gate
# needs. The file list is passed explicitly so the same set is checked whether or
# not this is a git work tree.
if "$formatter" --dry-run --Werror "${sources[@]}" 2>/tmp/lint-format.error; then
    # The generated sources are checked by asking their generator whether the
    # committed copy is what it would write. A generated file that has drifted
    # from its generator is worse than one that is merely unformatted, so this is
    # a failure and not a warning.
    for source in "${generated_sources[@]}"; do
        [[ -f $source ]] || continue
        case $source in
            platform/ps5/vk_globals.c)
                python3 tools/gen-vk-globals.py --check >/dev/null || {
                    echo "$source has drifted from tools/gen-vk-globals.py" >&2
                    echo "run 'python3 tools/gen-vk-globals.py' to regenerate it" >&2
                    echo "lint-format: FAIL" >&2
                    exit 1
                }
                ;;
        esac
    done
    echo "lint-format: PASS"
else
    # clang-format's own message names the file and the line it would change.
    sed -n '1,20p' /tmp/lint-format.error >&2
    echo "run 'bash tools/lint-format.sh --write' to apply the policy" >&2
    echo "lint-format: FAIL" >&2
    exit 1
fi
