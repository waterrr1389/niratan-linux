// SPDX-License-Identifier: GPL-3.0-or-later
//
// Text in one of the kit's text styles, following SwiftUI Text.font(_:),
// .fontWeight, .foregroundStyle(.secondary) and .monospacedDigit(). Only the
// pixel size and weight come from the style token, so the family stays the
// application font (Inter with Noto Sans CJK JP). Monospaced digits turn on
// Inter's tabular figures rather than switching to a monospace family.
// Every text the kit draws is a StyledLabel.
import QtQuick
import QtQuick.Controls
import Niratan

Label {
    // One of the Theme text style tokens (Theme.bodyFont and its siblings).
    property font textStyle: Theme.bodyFont
    // Defaults to the style's own weight; one of the Theme weight tokens.
    property int weight: textStyle.weight
    property bool secondary: false
    property bool tertiary: false
    property bool monospacedDigits: false

    font.pixelSize: textStyle.pixelSize
    font.weight: weight
    font.features: monospacedDigits ? { "tnum": 1 } : {}
    color: tertiary ? Theme.textTertiary : secondary ? Theme.textSecondary : Theme.textPrimary
}
