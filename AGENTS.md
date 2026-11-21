# Agent rules

Niratan Linux ports [Niratan](https://github.com/W1ght/Niratan) (SwiftUI) to
Linux with Qt 6.12 Quick and C++23. Niratan's source is the specification;
[Niratan-win](https://github.com/W1ght/Niratan-win) (WinUI 3) is the reference
for how a macOS interaction maps onto a non-Apple desktop.

`PLAN.md` (milestones, scope), `SPEC.md` (layout, quality gate) and `notes/`
are local working files kept out of git through `.git/info/exclude`. Read them
when they exist; the rules below hold either way. Handover notes in `notes/`
can be stale: check them against `git log` and the tree before acting on them.

## Purpose

The owner uses this application daily on Linux for Japanese immersion
learning. When being usable and matching Niratan in every detail conflict,
usable wins; data that reaches a card, a query or a file still matches Niratan
exactly. Niratan stays pinned at `609d2d7`; upstream changes are not followed.

The first release is done when these flows work end to end:

1. Open an EPUB, look up a word, add an Anki card.
2. Play a local video, look up a subtitle word, add an Anki card.
3. Reading statistics.
4. Sasayaki audiobook playback alongside the reader.
5. Profiles, including an English profile with English lookup.

Work that serves none of these flows waits until they all work.

## Roles

The project is built by agents. The owner is not a GUI developer and does not
review code, QML or diffs. Agents therefore verify their own work: every
change is checked by automated tests and by a review against the Swift
source done by an agent other than the one that wrote it.

Ask the owner only product questions, in plain language with concrete
options: what should happen when they do something. Never ask them to judge
code, layout values or technical trade-offs. When Niratan and Niratan-win
both answer a question, do not ask at all.

A problem the owner reports is reproduced as a failing test before it is
fixed, wherever the layer allows it.

## Workflow

Three agent roles; the `Audit` and `Implement` definitions live in the local
`.opencode/agents/`.

- Coordinator: splits a flow into modules, writes or approves the task file,
  dispatches, reads verdicts and gate output, stages and commits. It does not
  read implementation diffs routinely; that is the reviewer's job.
- `Audit`: judges work against the Swift source only, never against another
  agent's summary. One session per flow, so its context does not drift.
- `Implement`: writes code. One session per module; related modules in a
  batch may reuse it.

Each module has a task file `notes/specs/<area>-<module>.md` from
`notes/specs/TEMPLATE.md`: Swift sources, a behaviour table with tiers, the
files each phase may edit, and a review section. Agents exchange findings
through this file, not through messages. Once the module is committed its
header carries the citations and the task file is deleted.

1. `Audit` drafts the behaviour table from Swift; the coordinator checks it
   is complete and in scope.
2. `Implement` writes the header, tests and a stub that fails them.
3. `Audit` checks every test against its Swift symbol and records `ok` or a
   deviation per behaviour. Deviations go back to step 2.
4. The coordinator stages the approved tests. They are now frozen: after the
   implementation, `git diff --name-only` must show no test file.
5. `Implement` implements until `scripts/check.sh` passes.
6. For tier 1 behaviours, `Audit` checks the implementation against Swift.
   Build and wiring changes skip this; the gates and flow tests cover them.
7. The coordinator commits.

Replies are a few lines: results, deviations, open questions. A delegated
agent that needs a product decision stops and reports it; only the
coordinator asks the owner.

A delegated agent also stops and reports, rather than working around it,
when a frozen test looks wrong, when the files it may edit are not enough
(a needed split, a missing helper), or when Swift and the task file
disagree in a way no Decision covers. The coordinator then widens the file
list, adds a Decision or sends the test back to `Audit`.

The coordinator keeps the working tree to one intent at a time, so each
commit is just `git add` and `git commit`. It does not use `git stash` to
split work into commits: a stash also captures the index and can restore
stale copies of staged files.

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
   test is fixed. Match Swift exactly for anything that reaches a card, a
   query, a file name or a stored value: defaults, branch order, ranges and
   the text produced, including ordinary Unicode input such as U+3000 and
   multibyte characters counted per scalar. Behavior that exists only in
   Swift's runtime and does not occur in normal input (grapheme clusters,
   canonical equivalence, rare line separators, integer traps) need not be
   reproduced; record the difference in the header comment instead.
6. Commit the header, tests and implementation together.

## Writing a view

A QML view is written from the SwiftUI view that implements it, the way a
module is written from Swift logic.

1. The top comment of each QML file names the Swift files and views it
   follows.
2. Carry over structure, sizes, spacing, fonts, colors (through the theme),
   visibility conditions and state bindings from the SwiftUI `body` and its
   modifiers. A value with no Swift source follows Niratan-win; if neither
   has one, ask the owner a product question rather than invent it.
3. The reviewing agent compares the QML against the SwiftUI source item by
   item. Screenshots are not a verification method.

A view is not finished until it is wired: every command it shows reaches a
view model, and every view model command reaches the service that does the
work. A service no view model calls is not a delivered feature.

Conventions in existing code: `// SPDX-License-Identifier: GPL-3.0-or-later`
on every file, namespaces `niratan::<area>`, `QTEST_APPLESS_MAIN` unless an
event loop is needed, `operator== = default` on result structs so `QCOMPARE`
works, 4-space indentation. Strings are UTF-8; when Swift counts or trims by
Unicode rules, do the same rather than assuming ASCII.

## Verification

- `scripts/check.sh` runs the convention gates, configures, builds with
  `-Werror`, runs `ctest` and `all_qmllint`. Every commit must pass it.
- `scripts/check-layers.sh` keeps services free of GUI headers and view
  models and views out of the filesystem; `scripts/layers-baseline.txt` lists today's
  exceptions and may only shrink.
- `scripts/check-size.sh` caps a source file at 500 lines; files above it are
  frozen in `scripts/size-baseline.txt` and may shrink but not grow. Meet the
  limit by splitting along a real seam, not by compressing code: one public
  header may have several `.cpp` files, sharing a module-private header such
  as `<module>_codec.h` that nothing outside the module includes.
- `scripts/check-style.sh` enforces the SPDX line, no TODO markers or debug
  output, no tool or progress words, theme-only colours in QML and the
  include direction (models, then services, then view models, then views).
- `scripts/check-format.sh` runs `git clang-format --diff HEAD` over C++ files
  under `src/` and `tests/`, so changed lines must match `.clang-format`;
  existing files need no reformat, and compressing statements onto one line to
  fit the 500-line limit is rejected.
- `scripts/check-wired.sh` requires every service header to be reached from a
  view model, a view or a flow test, directly or through another wired
  service; `scripts/wired-baseline.txt` lists today's unwired headers and may
  only shrink.
- `scripts/hooks/commit-msg` enforces Conventional Commits, at most 72
  characters and the same banned words.
- Install the hooks once per clone: `git config core.hooksPath scripts/hooks`.
  `pre-commit` runs the gate and refuses a commit while unstaged or untracked
  changes exist, because the gate builds the working tree. `pre-push` runs the
  gate on every pushed commit in a separate worktree.
- Above the services, three layers of tests catch what unit tests cannot,
  without rendering or screenshots:
  - QML load test: each page loads offscreen with its view model; any QML
    warning or binding error fails.
  - View model tests: `tests/viewmodels/`, services replaced by stubs,
    commands invoked and state and signals checked with `QSignalSpy`. A new
    or changed view model comes with its tests.
  - Flow tests: one per first-release flow, driving view models and
    services with fixtures (EPUB, dictionary, AnkiConnect stub) from the
    user action to its result, such as the stub receiving `addNote`. Script
    bridges are exercised by loading the generated page in a headless
    `QWebEnginePage`.
- A flow counts as done when its flow test passes. The owner then uses it on
  the device and reports problems in plain language.

## Commits

- Conventional Commits 1.0.0 in English: `<type>(<scope>): <description>`,
  imperative, lower case, no trailing period, at most 72 characters. Scope is
  a module path such as `services/dictionary`, or `build`.
- The body explains why when it is not obvious, and names the Niratan files
  and symbols followed. Code copied from Niratan or Niratan-win names its
  source file and commit.
- One intent per commit; every commit builds and passes. Linear history.
- Never bypass the hooks (`--no-verify`, `core.hooksPath` changes). A failing
  gate is fixed, not skipped.
- Only the coordinating agent commits, after review. Delegated agents edit
  the files they are given and never run `git commit`, `git add` or
  `git stash`.
- At most two first-release flows are in progress at once, each in its own
  git worktree with its own build directory and its own `Audit` session.
  Within a worktree there is one intent at a time. A flow that shares files
  with another in-progress flow waits.
- Never commit `build/`, `cache/`, zip fixtures, media or user data.
- No AI tool names and no progress words (Step, Phase, FIXED, AC-x) in code,
  comments, commit messages or PR text. Comments explain intent and
  constraints, not history. Identifiers, comments and commit messages are in
  English; user-visible text goes through `qsTr`.
