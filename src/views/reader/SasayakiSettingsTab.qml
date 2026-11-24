// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Settings tab of the Sasayaki panel, following
// Features/Sasayaki/SasayakiSheet.swift (settingsTab, SasayakiPlaybackLimits)
// and Core/UserConfig.swift (readerShowSasayakiToggle, sasayakiAutoScroll,
// sasayakiAutoPause and the four colours), in the `.inspector` presentation of
// NativeMac/NativeReuseViews.swift (NativeSettingsForm, NativeSettingsSectionCard,
// NativeSettingsRow, NativeSettingsToggle, NativeSettingsSeparator).
//
// The Playback card shows Delay as "%+.2fs" and Speed as "%.2fx", both
// monospaced semibold, over sliders of -2...2 and 0.5...2.5, step 0.05, with
// 12 horizontal and 10 vertical padding and 8 px spacing. A slider writes when
// the drag ends or on a keyboard or wheel change, then re-binds its value to
// the model so later model changes show.
//
// The Settings card holds the three toggles. Auto-Scroll and Auto-Pause on
// Lookup also re-read the player's cached values. The Light Theme and Dark
// Theme cards show Text Color and Background Color; a pick writes its
// normalizing setter and rebuilds the reader document so cue colours change
// without reopening the book.
//
// The colour control is a swatch that opens a ColorDialog; it keeps the
// #RRGGBBAA order the settings store uses.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: root

    required property SasayakiViewModel player
    required property SasayakiSettingsViewModel settings

    // NativeSettingsSeparator renders nothing in the inspector presentation
    // (`if presentation != .inspector`), so no in-card separator is drawn; the
    // section card's bottom hairline is the only divider.

    // A toggle row that writes on change and re-binds to the model.
    component ToggleRow: InspectorFormRow {
        id: toggleRow

        property string title: ""
        // The settings property this row writes.
        property string propertyName: ""
        // Invoked after the value is written.
        signal written

        StyledLabel {
            Layout.fillWidth: true
            text: toggleRow.title
            textStyle: Theme.calloutFont
        }

        ToggleSwitch {
            id: toggle

            checked: root.settings[toggleRow.propertyName]

            onToggled: {
                root.settings[toggleRow.propertyName] = checked;
                checked = Qt.binding(function() {
                    return root.settings[toggleRow.propertyName];
                });
                toggleRow.written();
            }
        }
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 0

            InspectorFormSection {
                title: qsTr("Playback")

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingMd
                    Layout.rightMargin: Theme.spacingMd
                    Layout.topMargin: Theme.sliderRowPaddingVertical
                    Layout.bottomMargin: Theme.sliderRowPaddingVertical
                    spacing: Theme.sliderRowSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        StyledLabel {
                            text: qsTr("Delay")
                            textStyle: Theme.calloutFont
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        StyledLabel {
                            text: (root.player.delay >= 0 ? "+" : "") + root.player.delay.toFixed(2) + "s"
                            textStyle: Theme.calloutFont
                            weight: Theme.weightSemibold
                            monospacedDigits: true
                        }
                    }

                    Slider {
                        id: delaySlider

                        Layout.fillWidth: true
                        from: -2
                        to: 2
                        stepSize: 0.05
                        value: root.player.delay
                        Accessible.name: qsTr("Delay")

                        function commit() {
                            root.player.setDelay(value);
                            value = Qt.binding(function() { return root.player.delay });
                        }

                        onMoved: if (!pressed) commit()
                        onPressedChanged: if (!pressed && value !== root.player.delay) commit()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingMd
                    Layout.rightMargin: Theme.spacingMd
                    Layout.topMargin: Theme.sliderRowPaddingVertical
                    Layout.bottomMargin: Theme.sliderRowPaddingVertical
                    spacing: Theme.sliderRowSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        StyledLabel {
                            text: qsTr("Speed")
                            textStyle: Theme.calloutFont
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        StyledLabel {
                            text: root.player.rate.toFixed(2) + "x"
                            textStyle: Theme.calloutFont
                            weight: Theme.weightSemibold
                            monospacedDigits: true
                        }
                    }

                    Slider {
                        id: rateSlider

                        Layout.fillWidth: true
                        from: 0.5
                        to: 2.5
                        stepSize: 0.05
                        value: root.player.rate
                        Accessible.name: qsTr("Speed")

                        function commit() {
                            root.player.setRate(value);
                            value = Qt.binding(function() { return root.player.rate });
                        }

                        onMoved: if (!pressed) commit()
                        onPressedChanged: if (!pressed && value !== root.player.rate) commit()
                    }
                }
            }

            InspectorFormSection {
                title: qsTr("Settings")

                ToggleRow {
                    title: qsTr("Show Sasayaki Toggle")
                    propertyName: "showSasayakiToggle"
                }

                ToggleRow {
                    title: qsTr("Auto-Scroll")
                    propertyName: "autoScroll"
                    onWritten: root.player.reloadSasayakiSettings()
                }

                ToggleRow {
                    title: qsTr("Auto-Pause on Lookup")
                    propertyName: "autoPause"
                    onWritten: root.player.reloadSasayakiSettings()
                }
            }

            InspectorFormSection {
                title: qsTr("Light Theme")

                ColorRow {
                    title: qsTr("Text Color")
                    hex: root.settings.textColor
                    onPicked: function(color) {
                        root.settings.textColor = color;
                        ReaderViewModel.refreshSasayakiColors();
                    }
                }

                ColorRow {
                    title: qsTr("Background Color")
                    hex: root.settings.backgroundColor
                    onPicked: function(color) {
                        root.settings.backgroundColor = color;
                        ReaderViewModel.refreshSasayakiColors();
                    }
                }
            }

            InspectorFormSection {
                title: qsTr("Dark Theme")

                ColorRow {
                    title: qsTr("Text Color")
                    hex: root.settings.darkTextColor
                    onPicked: function(color) {
                        root.settings.darkTextColor = color;
                        ReaderViewModel.refreshSasayakiColors();
                    }
                }

                ColorRow {
                    title: qsTr("Background Color")
                    hex: root.settings.darkBackgroundColor
                    onPicked: function(color) {
                        root.settings.darkBackgroundColor = color;
                        ReaderViewModel.refreshSasayakiColors();
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.readerInspectorFormInset
            }
        }
    }

    // A label row whose accessory is the colour swatch, as a hidden-label
    // ColorPicker shows it. The store keeps #RRGGBBAA; the swatch needs
    // #AARRGGBB.
    component ColorRow: InspectorFormRow {
        id: colorRow

        property string title: ""
        property string hex: ""
        signal picked(string color)

        readonly property color swatch: colorRow.hex.length === 9
                                        ? "#" + colorRow.hex.slice(7) + colorRow.hex.slice(1, 7)
                                        : colorRow.hex

        StyledLabel {
            Layout.fillWidth: true
            text: colorRow.title
            textStyle: Theme.calloutFont
        }

        ColorWell {
            Layout.preferredWidth: Theme.colorWellHeight
            Layout.rightMargin: Theme.spacingXs
            Accessible.name: colorRow.title
            dialogTitle: colorRow.title
            color: colorRow.swatch
            onPicked: selectedColor => {
                const rgba = [selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a];
                colorRow.picked("#" + rgba.map(function(channel) {
                    return Math.round(channel * 255).toString(16).padStart(2, "0");
                }).join(""));
            }
        }
    }
}
