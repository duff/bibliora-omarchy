import QtQuick

// A word's dictionary entry (the Grok dictionary the iOS app has). d opens it
// on the words of the page you're reading, the ones the Bible uses least
// first; a right click or double click on a word opens it on that word. Type
// to look up another, ↑ ↓ (or tab, ctrl j/k) to move through the words, and
// Enter searches for the verses that use the one showing. Esc closes.
Sheet {
    id: sheet

    property var words: []
    property int selected: -1
    property var entry: ({})
    readonly property bool found: entry.word !== undefined
    readonly property bool listed: width - Theme.sp(32) >= Theme.sp(560) // the words beside the entry

    signal searchRequested(string word)

    function show(word) {
        input.text = word
        refresh()
        open = true
        input.forceActiveFocus()
        input.selectAll()
    }

    function refresh() {
        words = Reader.dictionaryWords(input.text)
        pick(0)
    }

    function pick(i) {
        if (words.length === 0) {
            selected = -1
            entry = {}
            return
        }
        selected = Math.max(0, Math.min(words.length - 1, i))
        entry = Reader.define(words[selected])
        entryView.contentY = 0
    }

    panelWidth: Theme.sp(760)
    panelHeight: Math.max(Theme.sp(460), content.implicitHeight + field.height + Theme.sp(64))

    Row {
        id: field
        x: Theme.sp(20)
        y: Theme.sp(16)
        width: parent.width - Theme.sp(40)
        spacing: Theme.sp(10)

        Txt { text: "?"; size: "title"; tone: "accent"; strong: true; anchors.verticalCenter: parent.verticalCenter }

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
            onTextEdited: sheet.refresh()

            Keys.onPressed: (event) => {
                const ctrl = event.modifiers & Qt.ControlModifier
                const k = event.key
                if (k === Qt.Key_Escape) sheet.close()
                else if (k === Qt.Key_Down || k === Qt.Key_Tab || (ctrl && (k === Qt.Key_J || k === Qt.Key_N)))
                    sheet.pick(sheet.selected + 1)
                else if (k === Qt.Key_Up || k === Qt.Key_Backtab || (ctrl && (k === Qt.Key_K || k === Qt.Key_P)))
                    sheet.pick(sheet.selected - 1)
                else if (k === Qt.Key_PageDown) sheet.pick(sheet.selected + 10)
                else if (k === Qt.Key_PageUp) sheet.pick(sheet.selected - 10)
                else if ((k === Qt.Key_Return || k === Qt.Key_Enter) && sheet.found)
                    sheet.searchRequested(sheet.entry.word)
                else return
                event.accepted = true
            }

            Txt {
                visible: input.text.length === 0
                text: "A word to look up"
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
        id: caption
        readonly property bool shown: sheet.listed && input.text.length === 0 && sheet.words.length > 0
        anchors.top: rule.bottom
        x: Theme.sp(20)
        height: shown ? Theme.sp(34) : 0
        visible: shown
        text: "On this page"
        size: "caption"
        tone: "faint"
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        verticalAlignment: Text.AlignBottom
        bottomPadding: Theme.sp(4)
    }

    ListView {
        id: list
        visible: sheet.listed
        anchors.top: caption.bottom
        anchors.topMargin: Theme.sp(6)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.sp(8)
        width: Theme.sp(200)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        model: sheet.words
        currentIndex: sheet.selected
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Rectangle {
            id: row
            required property string modelData
            required property int index
            readonly property bool selected: index === sheet.selected

            width: ListView.view.width
            height: Theme.sp(30)
            color: selected ? Theme.selectedFill : hover.hovered ? Theme.hoverFill : "transparent"

            HoverHandler { id: hover }
            TapHandler {
                onTapped: {
                    sheet.pick(row.index)
                    input.forceActiveFocus()
                }
            }
            Rectangle { visible: row.selected; width: 2; height: parent.height; color: Theme.accent }

            Txt {
                x: Theme.sp(20)
                width: parent.width - Theme.sp(32)
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData
                tone: row.selected ? "bright" : "normal"
            }
        }
    }

    Rectangle {
        visible: sheet.listed
        anchors.top: rule.bottom
        anchors.bottom: parent.bottom
        anchors.left: list.right
        width: 1
        color: Theme.hairline
    }

    Flickable {
        id: entryView
        anchors.top: rule.bottom
        anchors.bottom: parent.bottom
        anchors.left: sheet.listed ? list.right : parent.left
        anchors.right: parent.right
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: content.implicitHeight + Theme.sp(32)

        Column {
            id: content
            x: Theme.sp(24)
            y: Theme.sp(16)
            width: entryView.width - Theme.sp(48)
            spacing: Theme.sp(12)

            Txt {
                visible: !sheet.found && input.text.trim().length > 0
                text: "No entry for “" + input.text.trim() + "”"
                tone: "dim"
            }

            Row {
                visible: sheet.found
                spacing: Theme.sp(12)
                Text {
                    text: sheet.entry.word || ""
                    color: Theme.foregroundBright
                    font.family: Reader.fontFamily
                    font.pixelSize: Math.round(Theme.heading * 1.5)
                }
                Txt {
                    anchors.baseline: parent.children[0].baseline
                    text: sheet.entry.pronunciation ? "[" + sheet.entry.pronunciation + "]" : ""
                    tone: "dim"
                }
            }

            Repeater {
                model: sheet.found ? sheet.entry.senses : []

                Row {
                    id: sense
                    required property var modelData
                    required property int index
                    width: content.width
                    spacing: Theme.sp(10)

                    Txt { text: (sense.index + 1) + "."; tone: "faint"; width: Theme.sp(22); verticalAlignment: Text.AlignTop }
                    Column {
                        width: parent.width - Theme.sp(32)
                        spacing: Theme.sp(3)
                        Text {
                            width: parent.width
                            text: sense.modelData.definition
                            wrapMode: Text.Wrap
                            color: Theme.foreground
                            font.family: Reader.fontFamily
                            font.pixelSize: Math.round(Theme.body * 1.3)
                        }
                        Text {
                            visible: sense.modelData.example.length > 0
                            width: parent.width
                            text: "“" + sense.modelData.example + "”"
                            wrapMode: Text.Wrap
                            color: Theme.textDim
                            font.family: Reader.fontFamily
                            font.pixelSize: Math.round(Theme.body * 1.2)
                            font.italic: true
                        }
                    }
                }
            }

            Txt {
                visible: sheet.found && (sheet.entry.synonyms || "").length > 0
                width: content.width
                text: "Synonyms: " + (sheet.entry.synonyms || "")
                tone: "dim"
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }

            Row {
                visible: sheet.found
                spacing: Theme.sp(8)
                topPadding: Theme.sp(4)
                KeyCap { text: "enter"; anchors.verticalCenter: parent.verticalCenter }
                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        const n = sheet.entry.verses || 0
                        return "search the " + n + (n === 1 ? " verse" : " verses") + " in the " + (sheet.entry.translation || "")
                               + " that use it"
                    }
                    tone: "dim"
                    size: "small"
                }
            }
        }
    }
}
