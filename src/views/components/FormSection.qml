// SPDX-License-Identifier: GPL-3.0-or-later
//
// A grouped section: header, a surface holding stacked rows, footer.
// Follows NativeMac/NativeReuseViews.swift NativeSettingsSectionCard.cardsBody
// (cards) and Features/Video/VideoInspectorView.swift inspectorSection with
// VideoInspectorSectionGlassSurface (compact).
//
// Cards: a secondary subheadline semibold header inset 12, the card surface
// at radius 18, a footnote footer inset 12. Compact: a primary subheadline
// semibold header inset 4 with headerAccessory trailing on its line, the
// inspector surface at radius 14, a caption footer inset 4. Header and footer
// take no space when empty. Rows are stacked without spacing and take the
// section's density; a page places FormDivider between them. Rows draw no
// backgrounds of their own, so the surface clips them to its bounds rather
// than to its rounded shape, which needs a shader the software renderer lacks.
import QtQuick
import Niratan

Item {
    id: section

    property string title
    property string footer
    property bool compact: false
    // Items placed trailing on the header's line.
    property alias headerAccessory: accessoryRow.data
    default property alias rows: body.data

    readonly property Item headerLabel: headerText
    readonly property Item footerLabel: footerText
    readonly property Item surface: surfaceRect

    readonly property real inset: compact ? Theme.compactSectionHeaderInset
                                          : Theme.sectionHeaderInset

    width: parent ? parent.width : implicitWidth
    implicitWidth: body.implicitWidth
    implicitHeight: stack.implicitHeight

    Column {
        id: stack

        width: section.width
        spacing: section.compact ? Theme.compactSectionHeaderSpacing : Theme.sectionHeaderSpacing

        // An accessory with text sits on the title's first baseline, as
        // inspectorSection's HStack(alignment: .firstTextBaseline); a glyph
        // without text is centred on the title.
        Item {
            id: header

            readonly property Item firstAccessory: accessoryRow.children.length > 0
                                                   ? accessoryRow.children[0] : null
            readonly property bool baselineAligned: firstAccessory !== null
                                                    && firstAccessory.baselineOffset > 0
            readonly property real accessoryBaseline: baselineAligned
                ? firstAccessory.y + firstAccessory.baselineOffset : 0
            readonly property real titleY: baselineAligned
                ? Math.max(0, accessoryBaseline - headerText.baselineOffset)
                : Math.max(0, (accessoryRow.implicitHeight - headerText.implicitHeight) / 2)
            readonly property real accessoryY: baselineAligned
                ? Math.max(0, headerText.baselineOffset - accessoryBaseline)
                : Math.max(0, (headerText.implicitHeight - accessoryRow.implicitHeight) / 2)

            visible: section.title !== "" || accessoryRow.children.length > 0
            width: stack.width
            implicitHeight: Math.max(titleY + headerText.implicitHeight,
                                     accessoryY + accessoryRow.implicitHeight)

            StyledLabel {
                id: headerText

                x: section.inset
                y: header.titleY
                width: Math.max(0, accessoryRow.x - x - Theme.spacingSm)
                text: section.title
                textStyle: Theme.subheadlineFont
                weight: Theme.weightSemibold
                secondary: !section.compact
                elide: Text.ElideRight
            }

            Row {
                id: accessoryRow

                x: header.width - section.inset - width
                y: header.accessoryY
                spacing: Theme.spacingSm
            }
        }

        Rectangle {
            id: surfaceRect

            width: stack.width
            height: body.implicitHeight
            radius: section.compact ? Theme.compactSectionRadius : Theme.sectionRadius
            color: section.compact ? Theme.compactSectionBackground : Theme.cardBackground
            border.width: Theme.hairlineWidth
            border.color: section.compact ? Theme.compactSectionStroke : Theme.cardStroke
            clip: true

            Column {
                id: body

                width: surfaceRect.width
            }
        }

        StyledLabel {
            id: footerText

            visible: section.footer !== ""
            x: section.inset
            width: stack.width - 2 * section.inset
            text: section.footer
            textStyle: section.compact ? Theme.captionFont : Theme.footnoteFont
            secondary: true
            wrapMode: Text.Wrap
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }
}
