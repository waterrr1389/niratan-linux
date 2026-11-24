// SPDX-License-Identifier: GPL-3.0-or-later
//
// NativeReuseViews.swift NativeSettingsSectionCard.inspectorBody: grouped
// rows without a card surface, under an optional caption and above a divider.
import QtQuick
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: section

    property string title
    default property alias rows: body.data

    Layout.fillWidth: true
    Layout.leftMargin: Theme.readerInspectorFormInset
    Layout.rightMargin: Theme.readerInspectorFormInset
    Layout.topMargin: Theme.inspectorHeaderPadding
    Layout.bottomMargin: Theme.inspectorHeaderPadding
    spacing: Theme.spacingXs

    StyledLabel {
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.rowSubtitleSpacing
        visible: section.title.length > 0
        text: section.title
        textStyle: Theme.captionFont
        weight: Theme.weightSemibold
        secondary: true
    }

    ColumnLayout {
        id: body

        Layout.fillWidth: true
        spacing: 0
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Theme.hairlineWidth
        color: Theme.popupDivider
        opacity: 0.6
    }
}
