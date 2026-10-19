import QtQuick

// One page of the Bible, a running head above it, and nothing else until you
// ask: g goes somewhere, / searches, t changes translation, d looks up a word,
// a is about Bibliora, ? lists the keys.
Window {
    id: window

    width: 1000
    height: 1240
    minimumWidth: 420
    minimumHeight: 420
    visible: true
    title: Reader.ready ? Reader.placeLabel + " · Bibliora" : "Bibliora"
    color: Theme.background

    readonly property real side: Theme.sp(28)
    readonly property real top: Theme.sp(22)
    readonly property real headHeight: Math.round(Theme.body * 1.6)
    readonly property real gap: Theme.sp(18)
    readonly property real footHeight: Math.round(Theme.small * 2.2)

    // The room a page may take. Reader narrows it to a comfortable line and
    // rounds it down to whole lines.
    readonly property real roomWidth: width - 2 * side
    readonly property real roomHeight: height - top - headHeight - gap - gap - footHeight - Theme.sp(12)
    onRoomWidthChanged: Reader.setViewport(roomWidth, roomHeight)
    onRoomHeightChanged: Reader.setViewport(roomWidth, roomHeight)
    Component.onCompleted: {
        Reader.setViewport(roomWidth, roomHeight)
        reading.forceActiveFocus()
    }

    function backToReading() {
        reading.forceActiveFocus()
    }

    function openGoTo(text) {
        goTo.show(text)
    }

    function openSearch(text) {
        search.show(text)
    }

    function define(word) {
        dictionary.show(word)
    }

    // Every key the page answers to. Sheets handle their own.
    function readerKey(event) {
        const ctrl = event.modifiers & Qt.ControlModifier
        const alt = event.modifiers & Qt.AltModifier
        const shift = event.modifiers & Qt.ShiftModifier
        const k = event.key
        const t = event.text
        const selecting = Reader.selecting

        if (alt && k === Qt.Key_Left) Reader.back()
        else if (alt && k === Qt.Key_Right) Reader.forward()
        else if (ctrl && k === Qt.Key_O) Reader.back()
        else if (ctrl && k === Qt.Key_I) Reader.forward()
        else if (ctrl && k === Qt.Key_F) openSearch()
        else if (ctrl && k === Qt.Key_K) openGoTo("")
        else if (event.matches(StandardKey.Copy)) { if (selecting) Reader.copySelection() }
        else if (ctrl || alt) return
        else if (k === Qt.Key_Escape) { if (selecting) Reader.clearSelection(); else return }
        else if (selecting && (t === "j" || k === Qt.Key_Down)) Reader.moveCursor(1)
        else if (selecting && (t === "k" || k === Qt.Key_Up)) Reader.moveCursor(-1)
        else if (selecting && (t === "J")) Reader.moveCursor(5)
        else if (selecting && (t === "K")) Reader.moveCursor(-5)
        else if (selecting && (t === "y" || k === Qt.Key_Return || k === Qt.Key_Enter)) Reader.copySelection()
        else if (k === Qt.Key_Space) { if (shift) Reader.previousPage(); else Reader.nextPage() }
        else if (t === "l" || t === "j" || k === Qt.Key_Right || k === Qt.Key_Down || k === Qt.Key_PageDown)
            Reader.nextPage()
        else if (t === "h" || t === "k" || k === Qt.Key_Left || k === Qt.Key_Up || k === Qt.Key_PageUp
                 || k === Qt.Key_Backspace)
            Reader.previousPage()
        else if (t === "]") Reader.nextChapter()
        else if (t === "[") Reader.previousChapter()
        else if (t === "}") Reader.nextBook()
        else if (t === "{") Reader.previousBook()
        else if (k === Qt.Key_Home) Reader.firstPage()
        else if (k === Qt.Key_End) Reader.lastPage()
        else if (t === "g" || t === ":") openGoTo("")
        else if (t === "c") openGoTo(Reader.bookName + " ")
        else if (t === "/") openSearch()
        else if (t === "t") translations.show()
        else if (t === "n") Reader.toggleVerseNumbers()
        else if (t === "f") Reader.nextFont(1)
        else if (t === "F") Reader.nextFont(-1)
        else if (t === "v") Reader.startSelection()
        else if (t === "d") define("")
        else if (t === "a") about.show()
        else if (t === "?") keys.show()
        else return
        event.accepted = true
    }

    Item {
        id: reading

        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => window.readerKey(event)

        // Clicks beside the page turn it; the mouse's side buttons go back
        // and forward; the wheel turns pages anywhere. Clicks on the page are
        // the page's own (PageItem).
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.BackButton | Qt.ForwardButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.BackButton) Reader.back()
                else if (mouse.button === Qt.ForwardButton) Reader.forward()
                else if (mouse.x < page.x) Reader.previousPage()
                else if (mouse.x > page.x + page.width) Reader.nextPage()
                else if (Reader.selecting) Reader.clearSelection()
                reading.forceActiveFocus()
            }
            onWheel: (wheel) => page.wheel(wheel.angleDelta.y, wheel.angleDelta.x, wheel.pixelDelta.y !== 0 || wheel.pixelDelta.x !== 0)
        }

        // The running head: book, chapter, translation. Each opens its chooser.
        Item {
            id: head

            x: page.x
            y: window.top
            width: page.width
            height: window.headHeight
            visible: Reader.ready

            component HeadLink: Txt {
                signal activated()
                tone: hover.hovered ? "bright" : "dim"
                anchors.verticalCenter: parent.verticalCenter
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: parent.activated() }
            }

            HeadLink {
                anchors.left: parent.left
                text: Reader.bookName
                onActivated: window.openGoTo("")
            }
            HeadLink {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Reader.chapter
                onActivated: window.openGoTo(Reader.bookName + " ")
            }
            HeadLink {
                anchors.right: parent.right
                text: Reader.translation
                onActivated: translations.show()
            }
        }

        PageItem {
            id: page

            x: Math.round((parent.width - width) / 2)
            y: head.y + head.height + window.gap
            width: Reader.pageWidth
            height: Reader.pageHeight
            visible: Reader.ready
            onWordRequested: (word) => window.define(word)

            transform: Translate { id: shift }
            Connections {
                target: Reader
                function onTurned(direction) {
                    turn.stop()
                    shift.x = direction * Theme.sp(14)
                    page.opacity = 0.25
                    turn.start()
                }
            }
            ParallelAnimation {
                id: turn
                NumberAnimation { target: shift; property: "x"; to: 0; duration: 150; easing.type: Easing.OutCubic }
                NumberAnimation { target: page; property: "opacity"; to: 1; duration: 150; easing.type: Easing.OutCubic }
            }
        }

        // Beneath the page, while selecting: what's selected and the keys.
        Item {
            id: foot

            x: page.x
            width: page.width
            y: window.height - window.footHeight - Theme.sp(14)
            height: window.footHeight
            visible: Reader.ready

            Row {
                anchors.centerIn: parent
                visible: Reader.selecting
                spacing: Theme.sp(8)

                Txt { text: Reader.selectionLabel; tone: "accent"; size: "small"; anchors.verticalCenter: parent.verticalCenter }
                Item { width: Theme.sp(6); height: 1 }
                KeyCap { text: "j k"; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: "more or fewer"; tone: "dim"; size: "small"; anchors.verticalCenter: parent.verticalCenter }
                Item { width: Theme.sp(4); height: 1 }
                KeyCap { text: "y"; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: "copy"; tone: "dim"; size: "small"; anchors.verticalCenter: parent.verticalCenter }
                Item { width: Theme.sp(4); height: 1 }
                KeyCap { text: "esc"; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: "let go"; tone: "dim"; size: "small"; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // The package installs the Bible with the app, so this is only for a
        // broken install.
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - Theme.sp(64), Theme.sp(560))
            spacing: Theme.sp(10)
            visible: !Reader.ready

            Txt { text: "Bibliora can't open its Bible"; size: "title"; tone: "bright"; strong: true }
            Txt { width: parent.width; text: Reader.problem; tone: "dim"; wrapMode: Text.Wrap; elide: Text.ElideNone }
            Txt { width: parent.width; text: "Reinstalling Bibliora should bring it back."; tone: "dim"; wrapMode: Text.Wrap; elide: Text.ElideNone }
        }
    }

    // The sheets load just after the first page is up, so it shows sooner.
    component LazySheet: Loader {
        anchors.fill: parent
        z: 10
        asynchronous: true
    }

    LazySheet {
        id: goTo
        function show(text) { if (item) item.show(text) }
        sourceComponent: GoToSheet { onClosed: window.backToReading() }
    }

    LazySheet {
        id: search
        function show(text) { if (item) item.show(text) }
        sourceComponent: SearchSheet { onClosed: window.backToReading() }
    }

    LazySheet {
        id: translations
        function show() { if (item) item.show() }
        sourceComponent: TranslationSheet { onClosed: window.backToReading() }
    }

    LazySheet {
        id: dictionary
        function show(word) { if (item) item.show(word) }
        sourceComponent: DictionarySheet {
            onClosed: window.backToReading()
            onSearchRequested: (word) => {
                close()
                window.openSearch(word)
            }
        }
    }

    LazySheet {
        id: about
        function show() { if (item) item.show() }
        sourceComponent: AboutSheet { onClosed: window.backToReading() }
    }

    LazySheet {
        id: keys
        function show() { if (item) item.open = true }
        sourceComponent: KeysSheet { onClosed: window.backToReading() }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.sp(56)
    }

    Connections {
        target: Reader
        function onToast(message) { toast.show(message) }
    }

    // The Bible's text size, apart from Omarchy's (which the rest follows).
    Shortcut {
        sequences: ["Ctrl+=", StandardKey.ZoomIn]
        onActivated: Reader.changeTextSize(1)
    }
    Shortcut {
        sequences: [StandardKey.ZoomOut]
        onActivated: Reader.changeTextSize(-1)
    }
    Shortcut {
        sequence: "Ctrl+0"
        onActivated: Reader.resetTextSize()
    }
}
