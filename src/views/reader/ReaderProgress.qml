// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

// NativeReaderView.progressString places book and chapter progress side by
// side at the top and on separate lines at the bottom.
Rectangle {
    implicitHeight: progressLines.implicitHeight
    color: ReaderViewModel.pageBackground
    border.width: 1
    border.color: Theme.cardStroke
    visible: ReaderViewModel.hasBook && (ReaderViewModel.showProgress || ReaderViewModel.showChapterProgress)

    GridLayout {
        id: progressLines
        anchors.fill: parent
        columns: ReaderViewModel.showProgressTop ? 2 : 1
        rowSpacing: 0
        columnSpacing: 0

        Repeater {
            model: [false, true]
            delegate: ProgressBar {
                id: progressLine
                required property bool modelData
                visible: modelData ? ReaderViewModel.showChapterProgress : ReaderViewModel.showProgress
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                padding: 6
                value: modelData ? ReaderViewModel.logicalChapterProgress : ReaderViewModel.progress

                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        const count = !progressLine.modelData && ReaderViewModel.showsPageCount && ReaderViewModel.pageCount > 0
                            ? qsTr("%1 / %2").arg(ReaderViewModel.pageIndex).arg(ReaderViewModel.chapterPageCount) : ""
                        const percent = ReaderViewModel.showPercentage ? (progressLine.value * 100).toFixed(2) + "%" : ""
                        return progressLine.modelData && percent ? qsTr("(%1)").arg(percent) : (count + " " + percent).trim()
                    }
                    color: ReaderViewModel.infoColor
                    font.pixelSize: 11
                }
            }
        }
    }
}
