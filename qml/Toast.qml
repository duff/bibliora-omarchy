import QtQuick

// A word for a moment after something happens: "Copied John 3:16, NKJV".
Rectangle {
    id: toast

    function show(message) {
        label.text = message
        opacity = 1
        hide.restart()
    }

    width: label.implicitWidth + Theme.sp(32)
    height: label.implicitHeight + Theme.sp(16)
    radius: Theme.radius
    color: Theme.surfaceRaised
    border.width: 1
    border.color: Theme.border
    opacity: 0
    visible: opacity > 0
    z: 20
    Behavior on opacity { NumberAnimation { duration: 160 } }

    Txt { id: label; anchors.centerIn: parent; tone: "bright" }
    Timer { id: hide; interval: 1600; onTriggered: toast.opacity = 0 }
}
