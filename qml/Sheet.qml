import QtQuick

// A panel over a scrim, placed like Omarchy's launcher: centered, toward the
// top. Esc (each sheet's field handles it) or a click outside closes it.
FocusScope {
    id: sheet

    property bool open: false
    property real panelWidth: Theme.sp(640)
    property real panelHeight: Theme.sp(480)
    default property alias content: panel.data

    signal closed()

    function close() {
        if (!open)
            return
        open = false
        closed()
    }

    anchors.fill: parent
    visible: open
    z: 10

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim

        MouseArea {
            anchors.fill: parent
            onClicked: sheet.close()
        }
    }

    Rectangle {
        id: panel

        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(Theme.sp(24), Math.min(parent.height * 0.12, parent.height - height - Theme.sp(24)))
        width: Math.min(sheet.panelWidth, parent.width - Theme.sp(32))
        height: Math.min(sheet.panelHeight, parent.height - Theme.sp(48))
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border
        radius: Theme.radius

        MouseArea { anchors.fill: parent } // clicks inside don't close it
    }
}
