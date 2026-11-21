#!/bin/sh
# Source conventions from AGENTS.md, checked mechanically.
#
#   spdx       every source and test file names its licence on the first lines
#   markers    no TODO/FIXME left in the tree; the backlog lives in notes/
#   debug      no console.log or qDebug left behind
#   words      no tool names or progress words in code or scripts
#   colour     QML takes colours from the theme module, not from literals
#   direction  models do not reach up into services or view models, services
#              do not reach up into view models or views, view models do not
#              reach into views
#
# Every rule is absolute: there is no baseline here, because a file that
# violates a convention should be fixed rather than grandfathered.

set -e
cd "$(dirname "$0")/.."
fail=0
sources=$(git ls-files 'src/*.h' 'src/*.cpp' 'src/*.qml' 'tests/*.cpp')
scanned=$(git ls-files 'src/*.h' 'src/*.cpp' 'src/*.qml' 'tests/*.cpp' 'scripts/*.sh')

report() {
    echo "$1 $2" >&2
    echo "$3" | head -3 | sed 's/^/    /' >&2
    fail=1
}

for file in $sources; do
    if ! head -3 "$file" | grep -q 'SPDX-License-Identifier:'; then
        report "spdx:" "$file" "no SPDX line in the first three lines"
    fi
done

markers=$(rg -n --no-messages '\b(TODO|FIXME)\b' src tests scripts \
    --glob '!check-style.sh' --glob '!commit-msg' || true)
if [ -n "$markers" ]; then report "markers:" "src tests scripts" "$markers"; fi

debug=$(rg -n --no-messages 'console\.log|qDebug\(' src || true)
if [ -n "$debug" ]; then report "debug:" "src" "$debug"; fi

words=$(rg -n --no-messages 'Codex|Claude|Grok|Gemini' src tests scripts \
    --glob '!check-style.sh' --glob '!commit-msg' || true)
progress=$(rg -n --no-messages '\bFIXED\b|\b(Step|Phase|Section|Week)\s+[0-9]|\bAC-[0-9]+' \
    src tests scripts --glob '!check-style.sh' --glob '!commit-msg' || true)
[ -n "$progress" ] && words="$words
$progress"
if [ -n "$words" ]; then report "words:" "src tests scripts" "$words"; fi

colour=$(rg -n --no-messages '#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b' \
    --glob '!**/theme/**' src/views || true)
if [ -n "$colour" ]; then report "colour:" "src/views" "$colour"; fi

direction=$(rg -n --no-messages '#include "(services|viewmodels|views)/' src/models || true)
if [ -n "$direction" ]; then report "direction:" "src/models" "$direction"; fi
direction=$(rg -n --no-messages '#include "(viewmodels|views)/' src/services || true)
if [ -n "$direction" ]; then report "direction:" "src/services" "$direction"; fi
direction=$(rg -n --no-messages '#include "views/' src/viewmodels || true)
if [ -n "$direction" ]; then report "direction:" "src/viewmodels" "$direction"; fi

exit "$fail"
