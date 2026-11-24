// SPDX-License-Identifier: GPL-3.0-or-later
//
// NativeReuseViews.swift NativeSettingsRow in the reader inspector: an
// unpadded row with caller-supplied label and accessory content.
import QtQuick.Layouts
import Niratan

RowLayout {
    Layout.fillWidth: true
    Layout.minimumHeight: Theme.capsuleHeight
    spacing: Theme.compactRowSpacing
}
