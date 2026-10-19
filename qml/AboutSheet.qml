import QtQuick

// About Bibliora, as the iOS app's More Settings has it: donating, feedback,
// the privacy policy and terms of use, and whose each translation is. j k (or
// ↑ ↓) and Enter, or a click, opens one in the browser or in mail. a or Esc
// closes.
Sheet {
    id: sheet

    readonly property var links: [
        { label: "Donate", detail: "bibliora.org", url: "https://bibliora.org/#donate" },
        { label: "Send feedback", detail: "support@bibliora.org",
          url: "mailto:support@bibliora.org?subject=" + encodeURIComponent("Bibliora for Omarchy " + Qt.application.version) },
        { label: "Privacy policy", detail: "bibliora.org/privacy", url: "https://bibliora.org/privacy" },
        { label: "Terms of use", detail: "bibliora.org/terms", url: "https://bibliora.org/terms" },
    ]
    // Everything that opens something, in the order j and k go through them.
    readonly property var targets: links.map(l => l.url)
                                        .concat(Reader.translations.filter(t => t.site.length > 0).map(t => t.site))
    property int selected: 0

    function show() {
        selected = 0
        scroller.contentY = 0
        open = true
        keys.forceActiveFocus()
    }

    function go(url) {
        selected = targets.indexOf(url)
        Qt.openUrlExternally(url)
    }

    panelWidth: Theme.sp(640)
    panelHeight: column.implicitHeight + Theme.sp(40)

    Item {
        id: keys
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            const n = sheet.targets.length
            const t = event.text
            if (event.key === Qt.Key_Escape || t === "a" || t === "q") sheet.close()
            else if (event.key === Qt.Key_Down || t === "j") sheet.selected = (sheet.selected + 1) % n
            else if (event.key === Qt.Key_Up || t === "k") sheet.selected = (sheet.selected + n - 1) % n
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || t === "o")
                Qt.openUrlExternally(sheet.targets[sheet.selected])
            else return
            event.accepted = true
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: column.implicitHeight + Theme.sp(40)

        Column {
            id: column
            y: Theme.sp(20)
            width: scroller.width

            Row {
                x: Theme.sp(20)
                spacing: Theme.sp(10)
                bottomPadding: Theme.sp(14)
                Txt { id: name; text: "Bibliora"; size: "title"; tone: "bright"; strong: true }
                Txt { anchors.baseline: name.baseline; text: Qt.application.version; tone: "faint" }
            }

            Repeater {
                model: sheet.links

                Target {
                    id: link
                    required property var modelData
                    url: modelData.url
                    height: Theme.sp(34)

                    Txt {
                        x: Theme.sp(20)
                        anchors.verticalCenter: parent.verticalCenter
                        text: link.modelData.label
                        tone: link.selected ? "bright" : "normal"
                    }
                    Txt {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.sp(20)
                        anchors.verticalCenter: parent.verticalCenter
                        text: link.modelData.detail
                        tone: "faint"
                        size: "small"
                    }
                }
            }

            Caption { text: "Copyright" }

            Repeater {
                model: Reader.translations

                Target {
                    id: translation
                    required property var modelData
                    url: modelData.site
                    height: notice.implicitHeight + Theme.sp(14)

                    Txt {
                        id: abbrev
                        x: Theme.sp(20)
                        y: Theme.sp(7)
                        width: Theme.sp(96)
                        verticalAlignment: Text.AlignTop
                        text: translation.modelData.abbrev
                        tone: translation.selected ? "bright" : "normal"
                        strong: translation.modelData.key === Reader.translationKey
                    }
                    Txt {
                        id: notice
                        anchors.left: abbrev.right
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.sp(20)
                        y: Theme.sp(7)
                        verticalAlignment: Text.AlignTop
                        text: translation.modelData.notice
                        tone: "dim"
                        size: "small"
                        wrapMode: Text.Wrap
                        elide: Text.ElideNone
                    }
                }
            }

            Target {
                height: grok.implicitHeight + Theme.sp(14)

                Txt {
                    id: dictionary
                    x: Theme.sp(20)
                    y: Theme.sp(7)
                    width: Theme.sp(96)
                    verticalAlignment: Text.AlignTop
                    text: "Dictionary"
                }
                Txt {
                    id: grok
                    anchors.left: dictionary.right
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.sp(20)
                    y: Theme.sp(7)
                    verticalAlignment: Text.AlignTop
                    text: "Its definitions, example sentences, pronunciations and synonyms were all created with Grok."
                    tone: "dim"
                    size: "small"
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                }
            }

            Row {
                x: Theme.sp(20)
                topPadding: Theme.sp(16)
                spacing: Theme.sp(8)
                KeyCap { text: "j k"; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: "choose"; tone: "dim"; size: "small"; anchors.verticalCenter: parent.verticalCenter }
                Item { width: Theme.sp(4); height: 1 }
                KeyCap { text: "enter"; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: "open"; tone: "dim"; size: "small"; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    // A row that opens `url`, if it has one: highlighted when chosen, and
    // kept in view.
    component Target: Rectangle {
        id: target
        property string url
        readonly property bool selected: url.length > 0 && sheet.targets[sheet.selected] === url

        width: column.width
        color: selected ? Theme.selectedFill : hover.hovered && url.length > 0 ? Theme.hoverFill : "transparent"
        onSelectedChanged: {
            if (!selected)
                return
            const top = y + column.y
            if (top < scroller.contentY) scroller.contentY = top
            else if (top + height > scroller.contentY + scroller.height)
                scroller.contentY = top + height - scroller.height
        }

        HoverHandler { id: hover; cursorShape: target.url.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { enabled: target.url.length > 0; onTapped: sheet.go(target.url) }
        Rectangle { visible: target.selected; width: 2; height: parent.height; color: Theme.accent }
    }

    component Caption: Txt {
        x: Theme.sp(20)
        height: Theme.sp(40)
        size: "caption"
        tone: "faint"
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        verticalAlignment: Text.AlignBottom
        bottomPadding: Theme.sp(8)
    }
}
