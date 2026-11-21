#!/bin/sh
# Structural gate: the layering from AGENTS.md, checked mechanically.
#
# R1  src/services must not include a GUI header.
# R2  src/viewmodels must not touch the filesystem or spawn processes.
# R3  src/views must not either; views reach files through services.
#
# scripts/layers-baseline.txt holds the pairs that violate a rule today. The
# list only shrinks: a violation that is not listed fails the gate, and an
# entry that no longer violates anything is reported so it can be removed.

set -e
cd "$(dirname "$0")/.."
baseline="scripts/layers-baseline.txt"
fail=0

listed() {
    grep -qx "$1 $2" "$baseline" 2>/dev/null
}

check() {
    rule="$1"
    pattern="$2"
    paths="$3"
    hits=$(rg -l --no-messages -e "$pattern" "$paths" || true)
    for file in $hits; do
        if ! listed "$rule" "$file"; then
            echo "layer violation [$rule]: $file"
            rg -n --no-messages -e "$pattern" "$file" | head -3 | sed 's/^/    /'
            fail=1
        fi
    done
}

check services-gui '#include <Q(Quick|Gui|Widgets)' src/services
check viewmodel-io \
    '#include <Q(File|Dir|FileSystemWatcher|Process)>|json_file\.h|read_json_file|write_json_file|std::filesystem' \
    src/viewmodels
check view-io \
    '#include <Q(File|Dir|FileSystemWatcher|Process|TemporaryFile|SaveFile)>|json_file\.h|read_json_file|write_json_file|std::filesystem' \
    src/views

# Stale baseline entries: keep the list honest.
if [ -f "$baseline" ]; then
    while read -r rule file; do
        case "$rule" in ''|'#'*) continue ;; esac
        case "$rule" in
        services-gui) pattern='#include <Q(Quick|Gui|Widgets)' ;;
        viewmodel-io) pattern='#include <Q(File|Dir|FileSystemWatcher|Process)>|json_file\.h|read_json_file|write_json_file|std::filesystem' ;;
        view-io) pattern='#include <Q(File|Dir|FileSystemWatcher|Process|TemporaryFile|SaveFile)>|json_file\.h|read_json_file|write_json_file|std::filesystem' ;;
        *) echo "unknown rule in baseline: $rule"; fail=1; continue ;;
        esac
        if [ ! -f "$file" ]; then
            echo "stale baseline entry (file gone): $rule $file"
            fail=1
        elif ! rg -q --no-messages -e "$pattern" "$file"; then
            echo "baseline entry can be removed: $rule $file"
        fi
    done < "$baseline"
fi

exit "$fail"
