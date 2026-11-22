// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dialog buttons in the Niratan control style: push buttons at the trailing
// edge, the accepting one drawn as the default button with the accent fill.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.DialogButtonBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentWidth: (contentItem as ListView)?.contentWidth
    spacing: 8
    padding: 20
    topPadding: 12
    alignment: Qt.AlignRight

    delegate: Button {
        prominent: T.DialogButtonBox.buttonRole === T.DialogButtonBox.AcceptRole
                   || T.DialogButtonBox.buttonRole === T.DialogButtonBox.YesRole
    }

    contentItem: ListView {
        implicitWidth: contentWidth
        model: control.contentModel
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }

    background: Item {
        implicitHeight: Theme.controlHeight
    }
}
