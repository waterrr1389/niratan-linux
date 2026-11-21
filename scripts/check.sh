#!/bin/sh
# The gate that must pass before every commit. Any failure stops the rest.
# Runs hold a lock on the build directory, so concurrent runs queue instead of
# building over each other.
set -e
self="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
cd "$(dirname "$self")/.."
if [ -z "$NIRATAN_CHECK_LOCKED" ]; then
    mkdir -p build
    NIRATAN_CHECK_LOCKED=1 exec flock build/.check.lock "$self" "$@"
fi
scripts/check-layers.sh
scripts/check-size.sh
scripts/check-style.sh
scripts/check-format.sh
scripts/check-wired.sh
cmake --preset debug
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target all_qmllint
