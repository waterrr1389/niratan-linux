// SPDX-License-Identifier: GPL-3.0-or-later
//
// The per-book statistics editor of the dashboard, following
// `StatisticsBookDetailPanel` in Features/Bookshelf/StatisticsDashboardView.swift:
// the book's cover and title, its recorded days, the selected day's sessions,
// the session editor and the three delete actions. It is a modal card with
// the kit's panel header, list rows, fields and buttons; the delete actions
// confirm through StatisticsDeleteDialog. `host` is the dashboard page, which
// formats the values.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Dialog {
    id: dialog

    property var host
    property var book: ({})
    property string dateKey: ""
    property string sessionId: ""
    readonly property var day: StatisticsViewModel.bookDays.find(
        item => item.dateKey === dateKey)

    function chooseSession(session) {
        sessionId = session ? session.id : ""
        countField.text = session ? String(session.characters) : ""
        hoursField.text = session ? String(session.hours) : ""
        minutesField.text = session ? String(session.minutes) : ""
    }

    function selectDay(item, preferred) {
        dateKey = item ? item.dateKey : ""
        var sessions = item ? item.sessions : []
        chooseSession(sessions.find(session => session.id === preferred)
            ?? (sessions.length === 1 ? sessions[0] : null))
    }

    function restoreSelection(date, session) {
        var days = StatisticsViewModel.bookDays
        selectDay(days.find(item => item.dateKey === date) ?? days[days.length - 1], session)
    }

    function showBook(item) {
        book = item
        StatisticsViewModel.openBookDetails(item.id)
        restoreSelection("", "")
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    // The editor keeps a dashboard margin's worth of the window around it.
    width: parent ? Math.min(parent.width - Theme.dashboardMargin, Theme.bookDetailsWidth)
                  : Theme.bookDetailsWidth
    height: parent ? Math.min(parent.height - Theme.dashboardMargin, Theme.bookDetailsHeight)
                   : Theme.bookDetailsHeight
    modal: true
    padding: 0
    title: qsTr("Statistics")

    background: Rectangle {
        radius: Theme.cardRadius
        color: Theme.cardBackground
        border.width: Theme.hairlineWidth
        border.color: Theme.cardStroke
    }

    header: null
    footer: null

    StatisticsDeleteDialog {
        id: deleteStats

        sessionId: dialog.sessionId
        dateKey: dialog.dateKey
        onAccepted: {
            var id = deleteStats.scope === 0 ? deleteStats.sessionId : ""
            var date = deleteStats.scope === 1 ? deleteStats.dateKey : ""
            if ((deleteStats.scope === 0 && !id) || (deleteStats.scope === 1 && !date))
                return
            if (StatisticsViewModel.deleteBookSessions(id, date))
                dialog.restoreSelection(deleteStats.scope === 0 ? deleteStats.dateKey : "", "")
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        PanelHeader {
            Layout.fillWidth: true
            title: dialog.title
            onCloseRequested: dialog.close()
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.dashboardCardPadding
            Layout.rightMargin: Theme.dashboardCardPadding
            Layout.bottomMargin: Theme.dashboardCardPadding
            spacing: Theme.dashboardCardContentSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.rowSpacing

                Image {
                    Layout.preferredWidth: Theme.rankingCoverWidth
                    Layout.preferredHeight: Theme.rankingCoverHeight
                    source: dialog.book.cover ?? ""
                    fillMode: Image.PreserveAspectFit
                }
                StyledLabel {
                    Layout.fillWidth: true
                    text: dialog.book.title ?? ""
                    textStyle: Theme.title3Font
                    weight: Theme.weightSemibold
                    elide: Text.ElideRight
                }
            }

            StyledLabel {
                Layout.fillWidth: true
                visible: StatisticsViewModel.detailError.length > 0
                text: StatisticsViewModel.detailError
                textStyle: Theme.calloutFont
                color: Theme.popupDanger
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: dialog.width >= Theme.bookDetailsTwoColumnWidth ? 2 : 1
                columnSpacing: Theme.dashboardCardSpacing
                rowSpacing: Theme.dashboardCardContentSpacing

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: Theme.bookDetailsListWidth
                    Layout.preferredHeight: Theme.bookDetailsListHeight
                    clip: true
                    model: StatisticsViewModel.bookDays
                    ScrollBar.vertical: ScrollBar {}
                    delegate: DetailRow {
                        id: detailDay

                        required property var modelData

                        width: ListView.view.width
                        selected: detailDay.modelData.dateKey === dialog.dateKey
                        title: dialog.host.fullDate(detailDay.modelData.dateKey)
                        detail: qsTr("%1 · %2 · %n session(s)", "", detailDay.modelData.sessions.length)
                            .arg(dialog.host.formatCharacters(detailDay.modelData.characters))
                            .arg(dialog.host.formatDuration(detailDay.modelData.duration))
                        onClicked: dialog.selectDay(detailDay.modelData, "")
                    }

                    StyledLabel {
                        anchors.centerIn: parent
                        visible: StatisticsViewModel.bookDays.length === 0
                        text: qsTr("No reading records")
                        textStyle: Theme.calloutFont
                        secondary: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.dashboardTileSpacing

                    StyledLabel {
                        Layout.fillWidth: true
                        text: dialog.day ? dialog.host.fullDate(dialog.dateKey)
                            : qsTr("Select a day to edit its reading sessions.")
                        textStyle: dialog.day ? Theme.headlineFont : Theme.calloutFont
                        secondary: !dialog.day
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: Theme.bookDetailsSessionsHeight
                        clip: true
                        model: dialog.day?.sessions ?? []
                        ScrollBar.vertical: ScrollBar {}
                        delegate: DetailRow {
                            id: detailSession

                            required property var modelData

                            width: ListView.view.width
                            selected: detailSession.modelData.id === dialog.sessionId
                            title: qsTr("%1–%2")
                                .arg(Qt.formatTime(detailSession.modelData.start,
                                                   Qt.locale().timeFormat(Locale.ShortFormat)))
                                .arg(Qt.formatTime(detailSession.modelData.end,
                                                   Qt.locale().timeFormat(Locale.ShortFormat)))
                            detail: qsTr("%1 · %2 · %3")
                                .arg(dialog.host.formatCharacters(detailSession.modelData.characters))
                                .arg(dialog.host.formatDuration(detailSession.modelData.duration))
                                .arg(dialog.host.formatSpeed(detailSession.modelData.speed))
                            onClicked: dialog.chooseSession(detailSession.modelData)
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        visible: dialog.sessionId.length > 0

                        NumberField {
                            id: countField

                            width: Theme.bookDetailsCountFieldWidth
                            placeholderText: qsTr("Characters read")
                            validator: IntValidator { bottom: 0 }
                        }
                        NumberField {
                            id: hoursField

                            width: Theme.bookDetailsTimeFieldWidth
                            placeholderText: qsTr("Hours")
                            validator: IntValidator { bottom: 0 }
                        }
                        NumberField {
                            id: minutesField

                            width: Theme.bookDetailsTimeFieldWidth
                            placeholderText: qsTr("Minutes")
                            validator: IntValidator { bottom: 0; top: 59 }
                        }
                        FormButton {
                            sizeToLabel: true
                            prominent: true
                            text: qsTr("Save")
                            enabled: countField.acceptableInput && hoursField.acceptableInput
                                && minutesField.acceptableInput
                            onClicked: {
                                if (StatisticsViewModel.saveBookSession(dialog.sessionId,
                                    Number(countField.text), Number(hoursField.text),
                                    Number(minutesField.text))) {
                                    dialog.restoreSelection(dialog.dateKey, dialog.sessionId)
                                }
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm

                        FormButton {
                            sizeToLabel: true
                            visible: dialog.sessionId.length > 0
                            text: qsTr("Delete Session")
                            onClicked: deleteStats.ask(0)
                        }
                        FormButton {
                            sizeToLabel: true
                            visible: !!dialog.day
                            text: qsTr("Delete This Day")
                            onClicked: deleteStats.ask(1)
                        }
                        FormButton {
                            sizeToLabel: true
                            visible: StatisticsViewModel.bookDays.length > 0
                            text: qsTr("Delete All Statistics")
                            onClicked: deleteStats.ask(2)
                        }
                    }
                }
            }
        }
    }

    // A selectable list row: a callout title over a caption detail.
    component DetailRow: InspectorListRow {
        id: row

        property string title
        property string detail

        Accessible.name: row.title

        contentItem: Column {
            spacing: Theme.dashboardHeaderSpacing

            StyledLabel {
                width: parent.width
                text: row.title
                textStyle: Theme.calloutFont
                weight: row.selected ? Theme.weightSemibold : Theme.weightRegular
                elide: Text.ElideRight
            }
            StyledLabel {
                width: parent.width
                text: row.detail
                textStyle: Theme.captionFont
                secondary: true
                monospacedDigits: true
                elide: Text.ElideRight
            }
        }
    }

    // A numeric session field at the kit's field height.
    component NumberField: TextField {
        implicitHeight: Theme.fieldHeight
        Accessible.name: placeholderText
        inputMethodHints: Qt.ImhDigitsOnly
    }
}
