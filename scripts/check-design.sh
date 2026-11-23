#!/bin/sh
# Ratchet on literal type, spacing and size values in QML.
#
# Pages take text sizes, weights and spacing from the control kit
# (src/views/components/) or the theme (src/views/theme/) and do not spell
# them as bare numbers (AGENTS.md, "Writing a view", Design language). Those
# two directories are exempt; every other src/**/*.qml file, src/views/style/
# included, is counted in four categories:
#
#   fonts    lines assigning pixelSize or pointSize (font.pixelSize:,
#            font.pointSize: or inside a font { } group) whose right-hand side
#            holds a numeric literal: `font.pixelSize: 13`,
#            `font.pixelSize: Theme.fontBody + 2`
#   weights  each Font.Thin ... Font.Black, each `bold: true` and each
#            `weight: <number>`
#   spacing  lines assigning spacing, rowSpacing, columnSpacing, margins,
#            *Margin, padding, *Padding (optionally after `Layout.` or
#            `anchors.`) whose right-hand side holds a numeric literal other
#            than a bare 0: `spacing: 6`, `Layout.leftMargin: Theme.spacingSm + 4`
#            count; `spacing: 0` and `cond ? Theme.spacingSm : 0` do not
#   sizes    lines assigning width, height, implicitWidth, implicitHeight,
#            radius, size, iconSize, diameter or a Layout preferred, minimum
#            or maximum width or height, with the same literal rule:
#            `width: 64`, `radius: 8` count; `width: parent.width` does not
#
# An assignment, or a property declaration with a value, starts a line or
# follows `{` or `;`, and its right-hand side ends at the next `;` or closing
# `}`. Strings, // comments and lines starting with // are ignored, and digits
# inside identifiers (Theme.title1Font) are not literals. The script reads lines, it does not parse QML: a false
# positive from unusual formatting is fixed by rewriting the line.
#
# Files that had literals when the gate was introduced are frozen at their
# counts in scripts/design-baseline.txt; that file only shrinks, and a file
# without an entry is allowed none. When a page moves to the kit, lower its
# entry to the counts `scripts/check-design.sh --print` reports for it (or
# delete the entry at zero); the gate says when an entry can be lowered.

set -e
cd "$(dirname "$0")/.."
baseline="scripts/design-baseline.txt"
list=$(mktemp)
trap 'rm -f "$list"' EXIT

{ git ls-files 'src/*.qml'; git ls-files --others --exclude-standard 'src/*.qml'; } |
    grep -v -e '^src/views/theme/' -e '^src/views/components/' | sort -u >"$list" || true

awk -v mode="${1:-check}" -v baseline="$baseline" '
# The line without the contents of strings and without a // comment.
function clean(s,    out, i, c, quote) {
    out = ""
    quote = ""
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (quote != "") {
            if (c == "\\") {
                i++
            } else if (c == quote) {
                quote = ""
                out = out c
            }
            continue
        }
        if (c == "\"" || c == "\047") {
            quote = c
            out = out c
            continue
        }
        if (c == "/" && substr(s, i + 1, 1) == "/")
            break
        out = out c
    }
    return out
}

# The right-hand side starting at s: up to the next ; or the } closing the
# enclosing group.
function rhs(s,    i, c, depth) {
    depth = 0
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (c == "(" || c == "[" || c == "{")
            depth++
        else if (c == ")" || c == "]")
            depth--
        else if (c == "}") {
            if (depth == 0)
                break
            depth--
        } else if (c == ";" && depth == 0)
            break
    }
    return substr(s, 1, i - 1)
}

# True when s holds a numeric literal (10, 1.5, .5, 0x10); a bare 0 counts
# only when zero_counts.
function has_literal(s, zero_counts,    token) {
    s = " " s
    while (match(s, /[^A-Za-z0-9_.]\.?[0-9][0-9A-Za-z_.]*/)) {
        token = substr(s, RSTART + 1, RLENGTH - 1)
        if (zero_counts || token != "0")
            return 1
        s = substr(s, RSTART + RLENGTH)
    }
    return 0
}

# True when one assignment matching name in line has a counted right-hand side.
function assigns_literal(line, name, zero_counts,    rest, value) {
    rest = line
    while (match(rest, "(^|[{;])[ \t]*((readonly[ \t]+)?property[ \t]+[A-Za-z_][A-Za-z0-9_<>.]*[ \t]+)?" name "[ \t]*:")) {
        rest = substr(rest, RSTART + RLENGTH)
        value = rhs(rest)
        if (has_literal(value, zero_counts))
            return 1
    }
    return 0
}

function occurrences(line, pattern,    n) {
    n = 0
    while (match(line, pattern)) {
        n++
        line = substr(line, RSTART + RLENGTH)
    }
    return n
}

function count(file,    line, text, fonts, weights, spacing, sizes) {
    fonts = weights = spacing = sizes = 0
    while ((getline line < file) > 0) {
        if (line ~ /^[ \t]*\/\//)
            continue
        text = clean(line)
        if (assigns_literal(text, "([A-Za-z_][A-Za-z0-9_]*\\.)*(pixelSize|pointSize)", 1))
            fonts++
        weights += occurrences(" " text " ", "[^A-Za-z0-9_.]Font\\.(Thin|ExtraLight|Light|Normal|Medium|DemiBold|Bold|ExtraBold|Black)[^A-Za-z0-9_]")
        weights += occurrences(" " text " ", "[^A-Za-z0-9_]bold[ \t]*:[ \t]*true[^A-Za-z0-9_]")
        weights += occurrences(" " text, "[^A-Za-z0-9_]weight[ \t]*:[ \t]*[-+]?[0-9]")
        if (assigns_literal(text, "((Layout|anchors)\\.)?(spacing|rowSpacing|columnSpacing|margins|leftMargin|rightMargin|topMargin|bottomMargin|padding|leftPadding|rightPadding|topPadding|bottomPadding|horizontalPadding|verticalPadding)", 0))
            spacing++
        if (assigns_literal(text, "(Layout\\.)?(width|height|implicitWidth|implicitHeight|radius|size|iconSize|diameter|preferredWidth|preferredHeight|minimumWidth|minimumHeight|maximumWidth|maximumHeight)", 0))
            sizes++
    }
    close(file)
    return fonts " " weights " " spacing " " sizes
}

# A baseline line is "<path> <fonts> <weights> <spacing> <sizes>"; the path may hold
# spaces.
function path_of(line) {
    sub(/[ \t]+[0-9]+[ \t]+[0-9]+[ \t]+[0-9]+[ \t]+[0-9]+[ \t]*$/, "", line)
    return line
}

BEGIN {
    fail = 0
    while ((getline line < baseline) > 0) {
        if (line ~ /^[ \t]*(#|$)/)
            continue
        n = split(line, fields, /[ \t]+/)
        path = path_of(line)
        frozen[path] = fields[n - 3] " " fields[n - 2] " " fields[n - 1] " " fields[n]
        order[++entries] = path
    }
    close(baseline)
    split("fonts weights spacing sizes", names, " ")
}

{
    file = $0
    if ((getline probe < file) < 0)
        next
    close(file)
    counts = count(file)
    if (mode == "--print") {
        if (counts != "0 0 0 0")
            print file " " counts
        next
    }
    split(counts, now, " ")
    if (file in frozen) {
        split(frozen[file], allowed, " ")
        lowered = 0
        for (i = 1; i <= 4; i++) {
            if (now[i] + 0 > allowed[i] + 0) {
                print "design violation: " file " " names[i] " " now[i] ", frozen at " allowed[i]
                fail = 1
            } else if (now[i] + 0 < allowed[i] + 0) {
                lowered = 1
            }
        }
        if (lowered)
            print "baseline entry can be lowered: " file " " counts
    } else {
        for (i = 1; i <= 4; i++) {
            if (now[i] + 0 > 0) {
                print "design violation: " file " " names[i] " " now[i] ", limit is 0"
                fail = 1
            }
        }
    }
}

END {
    if (mode == "--print")
        exit 0
    for (i = 1; i <= entries; i++) {
        if ((getline probe < order[i]) < 0) {
            print "stale baseline entry (file gone): " order[i]
            fail = 1
        } else {
            close(order[i])
        }
    }
    exit fail
}
' "$list"
