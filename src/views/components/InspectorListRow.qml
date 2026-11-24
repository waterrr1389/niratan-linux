// SPDX-License-Identifier: GPL-3.0-or-later
//
// NativeReaderInspector.swift NativeReaderInspectorRowButtonStyle, used by
// ReaderGoToView.swift: inset content,
// a rounded hover/press surface and an accent tint for the current row.
import QtQuick
import QtQuick.Controls
import Niratan

Button {
    id: row

    property bool selected: false
    property bool hasContextMenu: false
    signal contextMenuRequested()

    leftPadding: Theme.readerListInset
    rightPadding: Theme.readerListInset
    topPadding: Theme.readerListRowPadding
    bottomPadding: Theme.readerListRowPadding
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.readerListRadius
        color: row.selected ? (row.down ? Theme.readerRowSelectedPress : Theme.readerRowSelected)
                            : row.down ? Theme.pressTint
                                       : row.hovered || row.visualFocus ? Theme.hoverTint : "transparent"
    }

    TapHandler {
        enabled: row.hasContextMenu
        acceptedButtons: Qt.RightButton
        onTapped: row.contextMenuRequested()
    }
}
