#!/bin/sh
# Hard size limit for sources: one file, one job.
#
# A file above LIMIT may not grow. Files already above it when the limit was
# introduced are frozen at their size in scripts/size-baseline.txt; that file
# only shrinks. Splitting one of them is the way to lower its entry.
#
# Physical lines are counted, comments included: the limit is about how much a
# reader has to hold at once, not about statements.

set -e
cd "$(dirname "$0")/.."
LIMIT=500
baseline="scripts/size-baseline.txt"
fail=0

limit_of() {
    awk -v f="$1" '$1 == f { print $2; exit }' "$baseline" 2>/dev/null
}

for file in $(git ls-files 'src/*.cpp' 'src/*.h' 'src/*.qml'; \
              git ls-files --others --exclude-standard 'src/*.cpp' 'src/*.h' 'src/*.qml'); do
    [ -f "$file" ] || continue
    lines=$(wc -l < "$file")
    allowed=$(limit_of "$file")
    if [ -n "$allowed" ]; then
        if [ "$lines" -gt "$allowed" ]; then
            echo "size violation: $file has $lines lines, frozen at $allowed"
            fail=1
        elif [ "$lines" -lt "$allowed" ]; then
            echo "baseline entry can be lowered: $file $lines (was $allowed)"
        fi
    elif [ "$lines" -gt "$LIMIT" ]; then
        echo "size violation: $file has $lines lines, limit is $LIMIT"
        fail=1
    fi
done

if [ -f "$baseline" ]; then
    while read -r file allowed; do
        case "$file" in ''|'#'*) continue ;; esac
        if [ ! -f "$file" ]; then
            echo "stale baseline entry (file gone): $file"
            fail=1
        fi
    done < "$baseline"
fi

exit "$fail"
