// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReaderGoToView.swift highlightsTab and GalleryView.swift sectionHeader:
// an inset caption in a compact list, or a headline with a count and rule.
import QtQuick
import QtQuick.Layouts
import Niratan

Item {
    id: header

    property string title
    property string countText
    property bool compact: false

    implicitHeight: labels.implicitHeight + labels.anchors.topMargin + labels.anchors.bottomMargin

    RowLayout {
        id: labels

        anchors.fill: parent
        anchors.leftMargin: header.compact ? Theme.readerListInset : 0
        anchors.rightMargin: anchors.leftMargin
        anchors.topMargin: header.compact ? Theme.readerListInset : Theme.spacingSm
        anchors.bottomMargin: header.compact ? Theme.readerListSpacing : Theme.spacingSm
        spacing: Theme.compactRowSpacing

        StyledLabel {
            Layout.fillWidth: header.compact
            Layout.preferredWidth: implicitWidth
            Layout.minimumWidth: 0
            Layout.alignment: header.compact ? Qt.AlignBaseline : Qt.AlignVCenter
            text: header.title
            textStyle: header.compact ? Theme.captionFont : Theme.headlineFont
            weight: Theme.weightSemibold
            secondary: header.compact
            elide: Text.ElideRight
        }

        // ReaderGoToView.swift searchResultsContent: a compact header ends
        // in a plain tertiary caption2 count rather than the gallery's pill.
        StyledLabel {
            visible: header.compact && header.countText.length > 0
            Layout.alignment: Qt.AlignBaseline
            text: header.countText
            textStyle: Theme.caption2Font
            tertiary: true
        }

        StyledLabel {
            visible: !header.compact && header.countText.length > 0
            text: header.countText
            textStyle: Theme.captionFont
            weight: Theme.weightSemibold
            secondary: true
            monospacedDigits: true
            horizontalAlignment: Text.AlignHCenter
            leftPadding: Theme.compactSectionHeaderSpacing
            rightPadding: leftPadding
            topPadding: Theme.readerListSpacing
            bottomPadding: topPadding

            background: Rectangle {
                radius: height / 2
                color: Theme.hoverTint
            }
        }

        Rectangle {
            visible: !header.compact
            Layout.fillWidth: true
            implicitHeight: Theme.hairlineWidth
            color: Theme.separator
        }
    }
}
