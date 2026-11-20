#!/bin/sh
# The gate that must pass before every commit. Any failure stops the rest.
set -e
cd "$(dirname "$0")/.."
cmake --preset debug
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target all_qmllint
