import QtQuick

// Search the translation you're reading, as you type: every word must be
// there, the last one also as the start of a word, "quotes" for a phrase.
// ↑ ↓ (or ctrl j/k) move, Enter goes to the verse, Esc closes and keeps the
// search for next time. With the field empty, your past searches.
Sheet {
    id: sheet

    function show(text) {
        if (text !== undefined && text !== "")
            Search.query = text
        input.text = Search.query
        input.selectAll()
        open = true
        input.forceActiveFocus()
    }

    // Enter before the results are in waits for them.
    property bool goWhenReady: false

    function go() {
        if (Search.busy) {
            goWhenReady = true
            return
        }
        goWhenReady = false
        if (Search.activate())
            sheet.close()
        else {
            input.text = Search.query
            input.cursorPosition = input.text.length
        }
    }

    Connections {
        target: Search
        function onResultsChanged() {
            if (sheet.goWhenReady && !Search.busy)
                sheet.go()
        }
        function onQueryChanged() {
            sheet.goWhenReady = false
        }
    }

    panelWidth: Theme.sp(880)
    panelHeight: 100000

    onClosed: Search.remember()

    Row {
        id: field
        x: Theme.sp(20)
        y: Theme.sp(16)
        width: parent.width - Theme.sp(40)
        spacing: Theme.sp(10)

        Txt { text: "/"; size: "title"; tone: "accent"; strong: true; anchors.verticalCenter: parent.verticalCenter }

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
            onTextEdited: Search.query = text

            Keys.onPressed: (event) => {
                const ctrl = event.modifiers & Qt.ControlModifier
                if (event.key === Qt.Key_Escape) sheet.close()
                else if (event.key === Qt.Key_Down || (ctrl && (event.key === Qt.Key_J || event.key === Qt.Key_N)))
                    Search.selected = Search.selected + 1
                else if (event.key === Qt.Key_Up || (ctrl && (event.key === Qt.Key_K || event.key === Qt.Key_P)))
                    Search.selected = Search.selected - 1
                else if (event.key === Qt.Key_PageDown) Search.selected = Search.selected + 10
                else if (event.key === Qt.Key_PageUp) Search.selected = Search.selected - 10
                else if (ctrl && event.key === Qt.Key_Delete && Search.showingHistory) Search.clearHistory()
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) sheet.go()
                else return
                event.accepted = true
            }

            Txt {
                visible: input.text.length === 0
                text: "Search the " + Reader.translation
                size: "title"
                tone: "faint"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Row {
        id: summary
        x: Theme.sp(20)
        anchors.top: field.bottom
        anchors.topMargin: Theme.sp(8)
        spacing: Theme.sp(8)

        Txt { text: Search.summary; size: "small"; tone: "dim" }
        Txt {
            visible: Search.showingHistory && !Search.historyIsExamples
            text: "· ctrl delete forgets them"
            size: "small"
            tone: "faint"
        }
    }

    Rectangle {
        id: rule
        anchors.top: summary.bottom
        anchors.topMargin: Theme.sp(12)
        width: parent.width
        height: 1
        color: Theme.hairline
    }

    ListView {
        id: list
        anchors.top: rule.bottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.sp(8)
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        keyNavigationEnabled: false
        model: Search
        currentIndex: Search.selected
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Rectangle {
            id: row
            required property int index
            required property string kind
            required property string reference
            required property string text
            readonly property bool selected: index === Search.selected

            width: ListView.view.width
            height: kind === "verse" ? body.implicitHeight + ref.implicitHeight + Theme.sp(20) : Theme.sp(34)
            color: selected ? Theme.selectedFill : hover.hovered ? Theme.hoverFill : "transparent"

            HoverHandler { id: hover }
            Rectangle { visible: row.selected; width: 2; height: parent.height; color: Theme.accent }
            TapHandler {
                onTapped: {
                    Search.selected = row.index
                    sheet.go()
                }
            }

            Txt {
                id: ref
                visible: row.kind === "verse"
                x: Theme.sp(20)
                y: Theme.sp(10)
                text: row.reference
                size: "small"
                tone: row.selected ? "accent" : "dim"
            }
            Text {
                id: body
                visible: row.kind === "verse"
                x: Theme.sp(20)
                anchors.top: ref.bottom
                anchors.topMargin: Theme.sp(3)
                width: parent.width - Theme.sp(40)
                text: row.text
                textFormat: Text.StyledText
                wrapMode: Text.Wrap
                color: row.selected ? Theme.foregroundBright : Theme.foreground
                font.family: Reader.fontFamily
                font.pixelSize: Math.round(Theme.body * 1.3)
                lineHeight: 1.25
            }
            Txt {
                visible: row.kind !== "verse"
                x: Theme.sp(20)
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - Theme.sp(40)
                text: row.kind === "goto" ? "→ " + row.text : row.text
                textFormat: Text.StyledText
                tone: row.kind === "goto" ? "accent" : row.selected ? "bright" : "normal"
            }
        }
    }
}
