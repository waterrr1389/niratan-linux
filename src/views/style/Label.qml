// SPDX-License-Identifier: GPL-3.0-or-later
//
// Label in the Niratan control style: macOS label colour on the interface
// font. Pages set their own size and colour where the Swift view does.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Label {
    color: Theme.textPrimary
    linkColor: Theme.accent
}
