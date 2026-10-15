import QtQuick

// Themed text. tone: normal | bright | dim | faint | accent | positive | warning | negative
// size: caption | small | body | title | heading
Text {
    property string tone: "normal"
    property string size: "body"
    property bool strong: false

    color: {
        switch (tone) {
        case "bright": return Theme.foregroundBright
        case "dim": return Theme.textDim
        case "faint": return Theme.textFaint
        case "accent": return Theme.accent
        case "positive": return Theme.positive
        case "warning": return Theme.warning
        case "negative": return Theme.negative
        default: return Theme.foreground
        }
    }
    font.family: Theme.fontFamily
    font.pixelSize: {
        switch (size) {
        case "caption": return Theme.caption
        case "small": return Theme.small
        case "title": return Theme.title
        case "heading": return Theme.heading
        default: return Theme.body
        }
    }
    font.weight: strong ? Font.DemiBold : Font.Normal
    elide: Text.ElideRight
    textFormat: Text.PlainText
    verticalAlignment: Text.AlignVCenter
}
