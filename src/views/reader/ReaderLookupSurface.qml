// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtWebEngine
import Niratan

Item {
    id: surface

    property rect selectionAnchor
    signal dismissRequested()

    // PopupModels.swift: PopupLayout positions the surface in reader coordinates.
    readonly property int gap: 4
    readonly property int borderInset: 6
    readonly property real leftSpace: selectionAnchor.x - gap
    readonly property real anchorRight: selectionAnchor.x + selectionAnchor.width
    readonly property real anchorBottom: selectionAnchor.y + selectionAnchor.height
    readonly property real rightSpace: parent.width - anchorRight - gap
    readonly property real aboveSpace: selectionAnchor.y - gap
    readonly property real belowSpace: parent.height - anchorBottom - gap
    readonly property bool placeRight: rightSpace >= leftSpace || rightSpace >= ReaderViewModel.popupWidth
    readonly property bool placeBelow: belowSpace >= aboveSpace || belowSpace >= ReaderViewModel.popupHeight
    readonly property bool fullWidth: ReaderViewModel.popupFullWidth
    readonly property bool vertical: ReaderViewModel.verticalWriting
    readonly property real insetWidth: parent.width - 2 * borderInset
    readonly property real insetHeight: parent.height - 2 * borderInset
    property bool swipeDismissed: false

    function showResults(html: string): void {
        results.loadHtml(html, "about:blank")
    }

    width: Math.max(1, fullWidth ? insetWidth : Math.min(ReaderViewModel.popupWidth, vertical
                    ? Math.min(parent.width, Math.max(leftSpace, rightSpace) - borderInset) : insetWidth))
    height: Math.max(1, Math.min(ReaderViewModel.popupHeight, insetHeight,
                     vertical || fullWidth ? insetHeight : (placeBelow ? belowSpace : aboveSpace) - borderInset))
    x: fullWidth ? Math.max(0, (parent.width - width) / 2)
       : vertical ? Math.max(0, Math.min(parent.width - width, placeRight
                             ? anchorRight + gap : selectionAnchor.x - gap - width))
       : Math.max(0, Math.min(parent.width - width, Math.max(borderInset,
                             Math.min(parent.width - width - borderInset, selectionAnchor.x))))
    y: Math.max(0, Math.min(parent.height - height, Math.max(borderInset,
                Math.min(parent.height - height - borderInset, fullWidth
                         ? parent.height : vertical ? selectionAnchor.y : placeBelow
                         ? anchorBottom + gap : selectionAnchor.y - gap - height))))
    opacity: ReaderViewModel.popupDisableTransparency ? 1.0 : 0.95

    // PopupView.swift places the action bar outside the scaled results.
    Rectangle {
        id: actionBar
        anchors.top: parent.top
        width: parent.width
        height: visible ? Math.min(parent.height, closeButton.implicitHeight) : 0
        visible: ReaderViewModel.popupActionBar
        color: Theme.popupBackground
        clip: true

        ToolButton {
            id: closeButton
            anchors.right: parent.right
            height: parent.height
            text: "\u00d7"
            Accessible.name: qsTr("Close lookup")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: surface.dismissRequested()
        }
    }

    WebEngineView {
        id: results
        anchors.top: actionBar.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        backgroundColor: Theme.popupBackground
        property real popupScale: ReaderViewModel.popupScale
        onPopupScaleChanged: runJavaScript("document.documentElement.style.zoom = " + popupScale)

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus && visible)
                runJavaScript("document.documentElement.style.zoom = " + popupScale + "; window.renderPopup();")
        }

        DragHandler {
            id: swipe
            enabled: surface.visible && ReaderViewModel.popupSwipeToDismiss
            target: null
            xAxis.enabled: false
            onActiveChanged: {
                if (!active) {
                    surface.swipeDismissed = false
                    return
                }
                if (!surface.swipeDismissed &&
                    Math.abs(persistentTranslation.y) > ReaderViewModel.popupSwipeThreshold) {
                    surface.swipeDismissed = true
                    surface.dismissRequested()
                }
            }
        }
    }
}
