// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dialog in the Niratan control style, after macOS alerts and sheets: a
// rounded floating surface with a bold title, the content, and buttons along
// the bottom trailing edge. The page behind is dimmed while modal.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    padding: 20
    topPadding: 8
    spacing: 4

    background: FloatingBackground {
        radius: 16
    }

    header: Label {
        text: control.title
        visible: control.title
        elide: Label.ElideRight
        font.bold: true
        leftPadding: 20
        rightPadding: 20
        topPadding: 20
        bottomPadding: 4
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: Rectangle {
        color: Theme.shadow
    }

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
