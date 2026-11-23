// SPDX-License-Identifier: GPL-3.0-or-later
//
// Accent link text that opens `url` externally. Follows SwiftUI Link as in
// Features/Settings/VideoSettingsView.swift (the Jimaku link in a row's label
// position): the row label's font in the accent colour, underlined on hover,
// with a pointing-hand cursor. The density follows the enclosing FormSection
// unless set.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.AbstractButton {
    id: control

    property url url
    property bool compact: section ? (section as FormSection).compact : false

    readonly property Item section: {
        for (let item = parent; item; item = item.parent) {
            if (item instanceof FormSection)
                return item
        }
        return null
    }

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: 0

    Accessible.role: Accessible.Link

    onClicked: Qt.openUrlExternally(url)

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    contentItem: StyledLabel {
        text: control.text
        textStyle: control.compact ? Theme.calloutFont : Theme.bodyFont
        weight: control.compact ? Theme.weightRegular : Theme.weightMedium
        color: Theme.accent
        font.underline: control.hovered || control.visualFocus
        elide: Text.ElideRight
    }
}
