// SPDX-License-Identifier: GPL-3.0-or-later
//
// A label with a trailing value on one line, following
// Features/Reader/StatisticsView.swift `remainingRow`: callout text, the
// value in callout tabular digits, at least 8 apart. An empty value shows
// "—" in the secondary colour.

import QtQuick
import Niratan

Item {
    id: row

    property string label
    property string value

    readonly property bool hasValue: value !== ""

    implicitHeight: Math.max(labelText.implicitHeight, valueText.implicitHeight)

    StyledLabel {
        id: labelText

        width: Math.max(0, valueText.x - Theme.spacingSm)
        anchors.verticalCenter: parent.verticalCenter
        text: row.label
        textStyle: Theme.calloutFont
        elide: Text.ElideRight
    }

    StyledLabel {
        id: valueText

        x: row.width - width
        anchors.verticalCenter: parent.verticalCenter
        text: row.hasValue ? row.value : "—"
        textStyle: Theme.calloutFont
        monospacedDigits: true
        secondary: !row.hasValue
    }
}
