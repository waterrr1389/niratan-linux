#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Wiring gate: a service no view model reaches is not a delivered feature.
#
# A header under src/services is wired when a root includes it, or when the
# header or source of a wired service includes it. Roots are src/main.cpp,
# src/viewmodels, src/views and tests/flows. Unit tests under tests/<area> are
# not roots: a service only they reach is still unwired.
#
# scripts/wired-baseline.txt lists the headers unwired today. The list only
# shrinks: an unlisted unwired header fails the gate, and a listed header that
# is now wired is reported so it can be removed.

set -e
cd "$(dirname "$0")/.."
baseline="scripts/wired-baseline.txt"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Service headers named by the files given on stdin, as src-relative paths.
includes_of() {
    while read -r file; do
        [ -f "$file" ] || continue
        dir=$(dirname "$file" | sed 's|^src/||')
        rg -o --no-filename '#include "[^"]+\.h"' "$file" | sed 's/#include "//; s/"$//' |
            while read -r inc; do
                case "$inc" in
                    services/*) echo "$inc" ;;
                    */*) ;;
                    *) case "$dir" in services/*) echo "$dir/$inc" ;; esac ;;
                esac
            done
    done
}

fd -e h . src/services | sed 's|^src/||' | sort -u >"$work/all"

roots=$(fd -e cpp -e h . src/viewmodels src/views; echo src/main.cpp)
[ -d tests/flows ] && roots="$roots
$(fd -e cpp -e h . tests/flows)"
echo "$roots" | includes_of | sort -u >"$work/wired"

while :; do
    sed 's|^|src/|' "$work/wired" | while read -r header; do
        echo "$header"
        echo "${header%.h}.cpp"
    done | includes_of | sort -u | comm -13 "$work/wired" - >"$work/new"
    [ -s "$work/new" ] || break
    sort -u "$work/wired" "$work/new" -o "$work/wired"
done

comm -23 "$work/all" "$work/wired" >"$work/unwired"
grep -v '^#' "$baseline" 2>/dev/null | sed '/^$/d' | sort -u >"$work/baseline" || true

fail=0
for header in $(comm -23 "$work/unwired" "$work/baseline"); do
    echo "unwired service: src/$header (no view model, view or flow test reaches it)"
    fail=1
done
for header in $(comm -13 "$work/unwired" "$work/baseline"); do
    echo "wired now, remove from $baseline: $header"
    fail=1
done
exit $fail
