# Agent rules

Niratan Linux ports [Niratan](https://github.com/W1ght/Niratan) (SwiftUI) to
Linux with Qt 6.12 Quick and C++23. Niratan's source is the specification;
[Niratan-win](https://github.com/W1ght/Niratan-win) (WinUI 3) is the reference
for how a macOS interaction maps onto a non-Apple desktop.

`PLAN.md` (milestones, scope), `SPEC.md` (layout, quality gate) and `notes/`
are local working files kept out of git through `.git/info/exclude`. Read them
when they exist; the rules below hold either way.

## Layout

- `src/models/`: plain data and pure functions shared by every layer.
- `src/services/<area>/`: logic and I/O, built into the `niratan_core` static
  library. No `QtQuick`, `QtGui` or `QtWidgets`; `QtCore` and `QtNetwork`
  only where needed.
- `src/viewmodels/`: state and commands. Never reads files, opens sockets or
  calls mpv or hoshidicts directly.
- `src/views/`: QML and input or coordinate adaptation only. Colors come from
  the theme module; no color literals outside `src/views/theme/`.
- `src/web/`: scripts injected into WebEngine. Rendering, selection,
  coordinates and events only.
- `tests/<area>/tst_<name>.cpp`: one Qt Test executable per module.
- `third_party/hoshidicts`: the dictionary library as a read-only submodule
  at `885c49f`. Never modify it; wrap its behavior in `services/dictionary`.

Directory names follow Niratan-win (`Services/Dictionary` becomes
`src/services/dictionary`).

## Writing a module

1. Find the Niratan files and symbols that implement the behavior. Every
   value, default and branch order needs a source file and symbol.
2. Write the header first. Its top comment names the Swift files and
   symbols it follows and states the rules a caller needs: decision order,
   edge cases, what passes through unvalidated.
3. Write the tests from the Swift source and Niratan's own
   `script/test_*.swift` cases, which are the most precise specification.
   Each test file names what it follows.
4. Add the source to `niratan_core` in `CMakeLists.txt` and the test to
   `tests/CMakeLists.txt` (copy an existing block), with a stub
   implementation. Confirm the stub fails the tests; a test the stub passes
   tests nothing. Guard `front()` and `->` with `QVERIFY` so a stub fails
   instead of aborting.
5. Implement, then check the implementation against the Swift source, not
   only against the tests. Where tests and Swift disagree, Swift wins and the
   test is fixed.
6. Commit the header, tests and implementation together.

Conventions in existing code: `// SPDX-License-Identifier: GPL-3.0-or-later`
on every file, namespaces `niratan::<area>`, `QTEST_APPLESS_MAIN` unless an
event loop is needed, `operator== = default` on result structs so `QCOMPARE`
works, 4-space indentation. Strings are UTF-8; when Swift counts or trims by
Unicode rules, do the same rather than assuming ASCII.

## Verification

- `scripts/check.sh` configures, builds with `-Werror`, runs `ctest` and
  `all_qmllint`. Every commit must pass it.
- Install the hooks once per clone: `git config core.hooksPath scripts/hooks`.
  `pre-commit` runs the gate and refuses a commit while unstaged or untracked
  changes exist, because the gate builds the working tree. `pre-push` runs the
  gate on every pushed commit in a separate worktree.
- QML views have no unit tests. The user checks visual results on the device.

## Commits

- Conventional Commits 1.0.0 in English: `<type>(<scope>): <description>`,
  imperative, lower case, no trailing period, at most 72 characters. Scope is
  a module path such as `services/dictionary`, or `build`.
- The body explains why when it is not obvious, and names the Niratan files
  and symbols followed. Code copied from Niratan or Niratan-win names its
  source file and commit.
- One intent per commit; every commit builds and passes. Linear history.
- Never commit `build/`, `cache/`, zip fixtures, media or user data.
- No AI tool names and no progress words (Step, Phase, FIXED, AC-x) in code,
  comments, commit messages or PR text. Comments explain intent and
  constraints, not history. Identifiers, comments and commit messages are in
  English; user-visible text goes through `qsTr`.
