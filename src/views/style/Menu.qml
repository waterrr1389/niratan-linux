// SPDX-License-Identifier: GPL-3.0-or-later
//
// Menu in the Niratan control style, after macOS menus: a rounded floating
// surface with a little inner padding around compact rows.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 0
    padding: 5
    overlap: 5

    delegate: MenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex

        T.ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: FloatingBackground {
        implicitWidth: 180
        implicitHeight: 24
    }

    T.Overlay.modal: Rectangle {
        color: Theme.shadow
    }
}
