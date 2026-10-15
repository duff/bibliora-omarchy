import QtQuick

// Go to a book, chapter or verse, as Omarchy's launcher would: type, and the
// list narrows. Enter goes; on a book it opens the book's chapters. Tab
// writes the row into the field. ↑ ↓ (or ctrl j/k) move. Esc closes.
Sheet {
    id: sheet

    function show(text) {
        GoTo.reset(text)
        input.text = text
        input.cursorPosition = text.length
        open = true
        input.forceActiveFocus()
    }

    function sync() {
        if (input.text !== GoTo.text) {
            input.text = GoTo.text
            input.cursorPosition = input.text.length
        }
    }

    panelWidth: Theme.sp(680)
    panelHeight: Theme.sp(620)

    Row {
        id: field
        x: Theme.sp(20)
        y: Theme.sp(16)
        width: parent.width - Theme.sp(40)
        spacing: Theme.sp(10)

        Txt { text: "›"; size: "title"; tone: "accent"; strong: true; anchors.verticalCenter: parent.verticalCenter }

        TextInput {
            id: input
            width: parent.width - Theme.sp(30)
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.foregroundBright
            selectionColor: Theme.alpha(Theme.accent, 0.35)
            selectedTextColor: Theme.foregroundBright
            font.family: Theme.fontFamily
            font.pixelSize: Theme.title
            clip: true
            onTextEdited: GoTo.text = text

            Keys.onPressed: (event) => {
                const ctrl = event.modifiers & Qt.ControlModifier
                if (event.key === Qt.Key_Escape) sheet.close()
                else if (event.key === Qt.Key_Down || (ctrl && (event.key === Qt.Key_J || event.key === Qt.Key_N)))
                    GoTo.selected = GoTo.selected + 1
                else if (event.key === Qt.Key_Up || (ctrl && (event.key === Qt.Key_K || event.key === Qt.Key_P)))
                    GoTo.selected = GoTo.selected - 1
                else if (event.key === Qt.Key_PageDown) GoTo.selected = GoTo.selected + 10
                else if (event.key === Qt.Key_PageUp) GoTo.selected = GoTo.selected - 10
                else if (event.key === Qt.Key_Tab) { GoTo.complete(); sheet.sync() }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    if (GoTo.activate()) sheet.close()
                    else sheet.sync()
                }
                else return
                event.accepted = true
            }

            Txt {
                visible: input.text.length === 0
                text: "A book, chapter or verse: john 3:16"
                size: "title"
                tone: "faint"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Rectangle {
        id: rule
        anchors.top: field.bottom
        anchors.topMargin: Theme.sp(14)
        width: parent.width
        height: 1
        color: Theme.hairline
    }

    Txt {
        anchors.top: rule.bottom
        anchors.topMargin: Theme.sp(14)
        x: Theme.sp(20)
        visible: GoTo.note.length > 0
        text: GoTo.note
        tone: "dim"
    }

    ListView {
        id: list
        anchors.top: rule.bottom
        anchors.topMargin: Theme.sp(6)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.sp(8)
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        model: GoTo.rows
        currentIndex: GoTo.selected
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Rectangle {
            id: row
            required property var modelData
            required property int index
            readonly property bool selected: index === GoTo.selected

            width: ListView.view.width
            height: Theme.sp(32)
            color: selected ? Theme.selectedFill : hover.hovered ? Theme.hoverFill : "transparent"

            HoverHandler { id: hover }
            Rectangle { visible: row.selected; width: 2; height: parent.height; color: Theme.accent }
            Rectangle {
                visible: row.modelData.groupStart === true
                x: Theme.sp(20)
                width: parent.width - Theme.sp(40)
                height: 1
                color: Theme.hairline
            }
            TapHandler {
                onTapped: {
                    GoTo.selected = row.index
                    if (GoTo.activate(row.index)) sheet.close()
                    else { sheet.sync(); input.forceActiveFocus() }
                }
            }

            Txt {
                id: label
                x: Theme.sp(20)
                width: Math.min(implicitWidth, parent.width * 0.4)
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData.label
                tone: row.selected ? "bright" : "normal"
            }
            Txt {
                anchors.left: label.right
                anchors.leftMargin: Theme.sp(14)
                anchors.right: parent.right
                anchors.rightMargin: Theme.sp(20)
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData.kind === "recent" ? (row.modelData.detail === "ahead" ? "ahead" : "")
                                                      : row.modelData.detail
                tone: "faint"
                size: "small"
                horizontalAlignment: row.modelData.kind === "book" || row.modelData.kind === "recent"
                                     ? Text.AlignRight : Text.AlignLeft
            }
        }
    }
}
