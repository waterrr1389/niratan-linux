// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import Niratan

// Placeholder shell until the bookshelf and navigation land. The minimum
// size follows HoshiNativeMacApp's .frame(minWidth: 900, minHeight: 620)
// with .windowResizability(.contentMinSize).
ApplicationWindow {
    visible: true
    width: 960
    height: 640
    minimumWidth: 900
    minimumHeight: 620
    title: "Niratan"
    color: Theme.windowBackground

    DictionaryPage {
        anchors.fill: parent
    }
}
