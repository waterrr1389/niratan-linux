import QtQuick
import QtQuick.Controls
import Niratan
ApplicationWindow {
    visible: true
    width: 960
    height: 640
    StackView { anchors.fill: parent; initialItem: LibraryPage {} }
}
