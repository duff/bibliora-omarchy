import QtQuick

// A word's dictionary entry (the Grok dictionary the iOS app has). Opens on
// the word you double-clicked, or empty with d; type another word to look it
// up. Enter searches for the verses that use it. Esc closes.
Sheet {
    id: sheet

    property var entry: ({})
    readonly property bool found: entry.word !== undefined

    signal searchRequested(string word)

    function show(word) {
        input.text = word
        entry = word.length ? Reader.define(word) : {}
        open = true
        input.forceActiveFocus()
        input.selectAll()
    }

    panelWidth: Theme.sp(720)
    panelHeight: Math.max(Theme.sp(180), content.implicitHeight + field.height + Theme.sp(56))

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
            onTextEdited: sheet.entry = text.trim().length ? Reader.define(text) : {}

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Escape) sheet.close()
                else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && sheet.found)
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

    Column {
        id: content
        anchors.top: rule.bottom
        anchors.topMargin: Theme.sp(16)
        x: Theme.sp(20)
        width: parent.width - Theme.sp(40)
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
