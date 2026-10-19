import QtQuick

// Every key Bibliora answers to. ? or Esc closes it.
Sheet {
    id: sheet

    readonly property var groups: [
        ["Reading", [
            [["l", "j", "space"], "Next page (arrows and page down too)"],
            [["h", "k"], "Previous page"],
            [["]", "["], "Next or previous chapter"],
            [["}", "{"], "Next or previous book"],
            [["home", "end"], "First or last page of the chapter"],
            [["alt ←", "alt →"], "Back and forward through where you've been"],
        ]],
        ["Finding", [
            [["g"], "Go to a book, chapter or verse (also :)"],
            [["c"], "This book's chapters"],
            [["/"], "Search the translation you're reading"],
            [["d"], "Look up a word on the page (or right-click it)"],
        ]],
        ["Copying", [
            [["v"], "Pick verses, from the top of the page (or ctrl-click one)"],
            [["j", "k"], "More or fewer verses"],
            [["y"], "Copy them, with where they're from"],
        ]],
        ["The page", [
            [["t"], "Translation"],
            [["n"], "Verse numbers on or off"],
            [["f"], "Next reading font"],
            [["ctrl +", "ctrl −"], "Bigger or smaller text (ctrl 0: back to the start)"],
            [["?"], "These keys"],
        ]],
    ]

    panelWidth: Theme.sp(640)
    panelHeight: column.implicitHeight + Theme.sp(40)

    onOpenChanged: if (open) catcher.forceActiveFocus()

    Item {
        id: catcher
        anchors.fill: parent
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape || event.text === "?" || event.text === "q") {
                sheet.close()
                event.accepted = true
            }
        }
    }

    Column {
        id: column
        x: Theme.sp(24)
        y: Theme.sp(20)
        width: parent.width - Theme.sp(48)
        spacing: Theme.sp(6)

        Repeater {
            model: sheet.groups

            Column {
                id: group
                required property var modelData
                required property int index
                width: column.width
                spacing: Theme.sp(6)
                topPadding: index > 0 ? Theme.sp(10) : 0

                Txt {
                    text: group.modelData[0]
                    size: "caption"
                    tone: "faint"
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 1
                    bottomPadding: Theme.sp(2)
                }

                Repeater {
                    model: group.modelData[1]

                    Row {
                        required property var modelData
                        spacing: Theme.sp(14)

                        Row {
                            width: Theme.sp(140)
                            spacing: Theme.sp(4)
                            Repeater {
                                model: modelData[0]
                                KeyCap { required property string modelData; text: modelData }
                            }
                        }
                        Txt {
                            text: modelData[1]
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}
