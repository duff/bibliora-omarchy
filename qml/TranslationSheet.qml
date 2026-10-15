import QtQuick

// The translations, 1–5 or ↑ ↓ and Enter. The page stays on the verse you
// were reading.
Sheet {
    id: sheet

    property int selected: 0

    function show() {
        const list = Reader.translations
        selected = 0
        for (let i = 0; i < list.length; ++i)
            if (list[i].key === Reader.translationKey)
                selected = i
        open = true
        keys.forceActiveFocus()
    }

    function choose(i) {
        Reader.setTranslation(Reader.translations[i].key)
        close()
    }

    panelWidth: Theme.sp(520)
    panelHeight: column.implicitHeight + Theme.sp(32)

    Item {
        id: keys
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            const n = Reader.translations.length
            const t = event.text
            if (event.key === Qt.Key_Escape || t === "t") sheet.close()
            else if (event.key === Qt.Key_Down || t === "j") sheet.selected = (sheet.selected + 1) % n
            else if (event.key === Qt.Key_Up || t === "k") sheet.selected = (sheet.selected + n - 1) % n
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) sheet.choose(sheet.selected)
            else if (t >= "1" && t <= String(n)) sheet.choose(Number(t) - 1)
            else return
            event.accepted = true
        }
    }

    Column {
        id: column
        y: Theme.sp(16)
        width: parent.width

        Caption { text: "Translation" }

        Repeater {
            model: Reader.translations

            Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool selected: index === sheet.selected
                readonly property bool current: modelData.key === Reader.translationKey

                width: column.width
                height: Theme.sp(36)
                color: selected ? Theme.selectedFill : hover.hovered ? Theme.hoverFill : "transparent"

                HoverHandler { id: hover }
                TapHandler { onTapped: sheet.choose(row.index) }
                Rectangle { visible: row.selected; width: 2; height: parent.height; color: Theme.accent }

                KeyCap {
                    x: Theme.sp(20)
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.index + 1
                }
                Txt {
                    id: abbrev
                    x: Theme.sp(56)
                    width: Theme.sp(90)
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.abbrev
                    tone: row.current ? "accent" : row.selected ? "bright" : "normal"
                    strong: row.current
                }
                Txt {
                    anchors.left: abbrev.right
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.sp(20)
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.name
                    tone: "dim"
                }
            }
        }
    }

    component Caption: Txt {
        x: Theme.sp(20)
        height: Theme.sp(30)
        size: "caption"
        tone: "faint"
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        verticalAlignment: Text.AlignTop
    }
}
