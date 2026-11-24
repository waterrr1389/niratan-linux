// SPDX-License-Identifier: GPL-3.0-or-later
//
// The small heading of a reader side panel group, following
// NativeMac/NativeReaderInspector.swift `NativeReaderInspectorSectionTitle`:
// caption semibold in the secondary colour, leading-aligned.

import QtQuick
import Niratan

StyledLabel {
    textStyle: Theme.captionFont
    weight: Theme.weightSemibold
    secondary: true
    elide: Text.ElideRight
}
