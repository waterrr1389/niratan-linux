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
before a commit. Concurrent runs queue on a lock in the build directory.

```sh
git config core.hooksPath scripts/hooks
```

`pre-commit` runs the check, and refuses the commit while the working tree
holds changes that are not staged, because the check builds the working tree
and would otherwise pass on something the commit does not contain. `pre-push`
checks every commit being pushed, oldest first, in a worktree under the git
directory, so each commit builds on its own. A failing commit prints the tail
of its log and leaves the full log next to that worktree. `--no-verify` skips
either hook. The worktree is reused between pushes and can be removed with
`git worktree remove`.
