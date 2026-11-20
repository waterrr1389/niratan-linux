// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import Niratan

// Placeholder window. Size is not a spec; M1 sets it from Niratan's window
// spec. Colors come from the theme module, never literals.
ApplicationWindow {
    visible: true
    width: 960
    height: 640
    title: "Niratan"
    color: Theme.windowBackground
}
