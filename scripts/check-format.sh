#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Format gate: C++ lines changed since HEAD must match .clang-format.
#
# Only changed lines are checked, so the existing tree need not be reformatted
# at once; a new edit is. Compressing code onto one line to fit the 500-line
# limit changes lines and is therefore rejected. Staged, unstaged and untracked
# files under src/ and tests/ are all covered; third_party/ is left alone.

set -e
cd "$(dirname "$0")/.."
fail=0

# Tracked files with staged or unstaged changes. git clang-format compares
# them with HEAD and prints the hunks it would rewrite; --quiet keeps the
# "did not modify any files" message out of the output when nothing changes.
changed=$(git diff --name-only --diff-filter=ACMR HEAD -- src tests |
    grep -E '\.(cpp|h)$' || true)
if [ -n "$changed" ]; then
    diff=$(git clang-format --quiet --diff HEAD -- $changed || true)
    if [ -n "$diff" ]; then
        echo "$diff" >&2
        fail=1
    fi
fi

# git clang-format only sees tracked files, so an untracked file is checked
# whole; every line of it is new.
untracked=$(git ls-files --others --exclude-standard -- src tests |
    grep -E '\.(cpp|h)$' || true)
for file in $untracked; do
    if ! clang-format --style=file "$file" | diff -u "$file" - >/dev/null; then
        clang-format --style=file "$file" | diff -u "$file" - >&2
        fail=1
    fi
done

exit "$fail"
