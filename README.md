# Niratan Linux

A Linux port of [Niratan](https://github.com/W1ght/Niratan), an EPUB reader and
local video player with dictionary lookup. Licensed under GPL-3.0-or-later.

## Build

Requires Qt 6.12 (Quick, QuickControls2), CMake 3.28 or newer and Ninja.

```sh
git submodule update --init --recursive
cmake --preset debug
cmake --build build
./build/niratan
```

## Check

```sh
scripts/check.sh
```

Runs configuration, the build, the test suite and QML lint. All four must pass
before a commit.
