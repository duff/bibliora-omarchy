import QtQuick

// A key, drawn as a small cap: KeyCap { text: "space" }
Rectangle {
    property alias text: label.text

    implicitWidth: Math.max(implicitHeight, label.implicitWidth + Theme.sp(10))
    implicitHeight: label.implicitHeight + Theme.sp(4)
    radius: Math.min(Theme.radius, Theme.sp(4))
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    Txt {
        id: label
        anchors.centerIn: parent
        size: "caption"
        tone: "dim"
    }
}
