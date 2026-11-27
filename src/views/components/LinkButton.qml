// SPDX-License-Identifier: GPL-3.0-or-later
//
// Accent link text that opens `url` externally, or, without a url, a bare
// action such as VideoInspectorView's VideoInspectorLinkButtonStyle Reset. Follows SwiftUI Link as in
// Features/Settings/VideoSettingsView.swift (the Jimaku link in a row's label
// position): the row label's font in the accent colour, underlined on hover,
// with a pointing-hand cursor; secondary while disabled. The density follows the enclosing FormSection
// unless set.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.AbstractButton {
    id: control

    // A click leaves the keyboard where it was, as on macOS (see the
    // style's Button.qml).
    focusPolicy: Qt.TabFocus

    property url url
    // Caption semibold, as VideoInspectorLinkButtonStyle draws a header action.
    property bool small: false
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

    onClicked: {
        if (url.toString() !== "")
            Qt.openUrlExternally(url)
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    contentItem: StyledLabel {
        text: control.text
        textStyle: control.small ? Theme.captionFont : control.compact ? Theme.calloutFont : Theme.bodyFont
        weight: control.small ? Theme.weightSemibold : control.compact ? Theme.weightRegular : Theme.weightMedium
        color: control.enabled ? Theme.accent : Theme.textSecondary
        font.underline: control.hovered || control.visualFocus
        elide: Text.ElideRight
    }
}
