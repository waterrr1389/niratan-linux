// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import Niratan

// Placeholder window. The initial size is not a spec; the minimum follows
// HoshiNativeMacApp's .frame(minWidth: 900, minHeight: 620) with
// .windowResizability(.contentMinSize). Colors come from the theme module,
// never literals.
ApplicationWindow {
    visible: true
    width: 960
    height: 640
    minimumWidth: 900
    minimumHeight: 620
    title: "Niratan"
    color: Theme.windowBackground
}
