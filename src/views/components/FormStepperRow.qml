// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row whose trailing column is a StepperControl. Follows
// Features/Settings/VideoSettingsView.swift Default Seek Interval and
// Fast-forward Speed rows and the Features/Video/VideoInspectorView.swift
// Subtitle Weight row. The value text sits on the label's line; stepped is
// forwarded from the stepper.
import QtQuick
import Niratan

FormRow {
    id: row

    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 1
    property string valueText

    signal stepped(real value)

    accessoryItem: stepper

    StepperControl {
        id: stepper

        value: row.value
        from: row.from
        to: row.to
        stepSize: row.stepSize
        valueText: row.valueText
        compact: row.compact
        Accessible.name: row.label
        onStepped: value => row.stepped(value)
    }
}
