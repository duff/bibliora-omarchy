#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

class QFileSystemWatcher;

// The live Omarchy look, as QML properties (adapted from Billy's, which came
// from Stocky's).
//
// Colors come from ~/.local/state/omarchy/current/theme/colors.toml. The type
// scale, spacing scale and control-state alphas come from that theme's
// shell.toml, with ~/.config/omarchy/shell.toml winning, as in the Omarchy
// shell. The corner radius follows Hyprland's decoration:rounding and the font
// is fontconfig's monospace, which `omarchy font set` sets. Everything but the
// Bible's own text uses these; the reading font and its size are Reader's.
//
// `omarchy theme set` swaps the theme directory, so its parent is watched and
// everything reloads; all properties share one NOTIFY so bindings update
// together. BIBLIORA_THEME_DIR points at another theme (for screenshots).
class Theme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool dark MEMBER m_dark NOTIFY changed)

    Q_PROPERTY(QColor background MEMBER m_background NOTIFY changed)
    Q_PROPERTY(QColor backgroundDark MEMBER m_backgroundDark NOTIFY changed)
    Q_PROPERTY(QColor foreground MEMBER m_foreground NOTIFY changed)
    Q_PROPERTY(QColor foregroundBright MEMBER m_foregroundBright NOTIFY changed)
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY changed)

    Q_PROPERTY(QColor surface MEMBER m_surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceRaised MEMBER m_surfaceRaised NOTIFY changed)
    Q_PROPERTY(QColor sunken MEMBER m_sunken NOTIFY changed)
    Q_PROPERTY(QColor hairline MEMBER m_hairline NOTIFY changed)
    Q_PROPERTY(QColor border MEMBER m_border NOTIFY changed)
    Q_PROPERTY(QColor textDim MEMBER m_textDim NOTIFY changed)
    Q_PROPERTY(QColor textFaint MEMBER m_textFaint NOTIFY changed)
    Q_PROPERTY(QColor onAccent MEMBER m_onAccent NOTIFY changed)
    Q_PROPERTY(QColor scrim MEMBER m_scrim NOTIFY changed)

    // Meaning colors. Some themes collapse hues, so the UI always pairs these
    // with a glyph or a word.
    Q_PROPERTY(QColor positive MEMBER m_positive NOTIFY changed)
    Q_PROPERTY(QColor negative MEMBER m_negative NOTIFY changed)
    Q_PROPERTY(QColor warning MEMBER m_warning NOTIFY changed)

    // Omarchy [controls] state tokens.
    Q_PROPERTY(QColor hoverFill MEMBER m_hoverFill NOTIFY changed)
    Q_PROPERTY(QColor selectedFill MEMBER m_selectedFill NOTIFY changed)
    Q_PROPERTY(QColor focusBorder MEMBER m_focusBorder NOTIFY changed)

    Q_PROPERTY(int radius MEMBER m_radius NOTIFY changed)

    Q_PROPERTY(QString fontFamily MEMBER m_fontFamily NOTIFY changed)
    // Omarchy's text size (shell.toml's font.base-size).
    Q_PROPERTY(int fontBase MEMBER m_fontBase NOTIFY changed)
    Q_PROPERTY(int caption MEMBER m_caption NOTIFY changed)
    Q_PROPERTY(int small MEMBER m_small NOTIFY changed)
    Q_PROPERTY(int body MEMBER m_body NOTIFY changed)
    Q_PROPERTY(int title MEMBER m_title NOTIFY changed)
    Q_PROPERTY(int heading MEMBER m_heading NOTIFY changed)

    Q_PROPERTY(int transitionMs MEMBER m_transitionMs CONSTANT)

public:
    // Not default-constructible on purpose: Qt prefers a default constructor
    // over create(), which would give QML its own instance.
    explicit Theme(QObject *parent);

    static Theme *create(QQmlEngine *, QJSEngine *);
    static Theme *instance();

    bool dark() const { return m_dark; }
    QColor background() const { return m_background; }
    QColor foreground() const { return m_foreground; }
    QColor accent() const { return m_accent; }
    QColor textDim() const { return m_textDim; }
    QColor textFaint() const { return m_textFaint; }
    QColor selectedFill() const { return m_selectedFill; }
    QString fontFamily() const { return m_fontFamily; }
    int fontBase() const { return m_fontBase; }

    // Scales a design-space pixel value with the Omarchy font size, as the
    // Omarchy shell scales its spacing.
    Q_INVOKABLE int sp(qreal px) const;
    Q_INVOKABLE QColor alpha(const QColor &c, qreal a) const;
    Q_INVOKABLE QColor mix(const QColor &a, const QColor &b, qreal t) const;

signals:
    void changed();

private:
    void reload();
    void loadPalette();
    void loadShellTokens();
    void deriveColors();
    void queryFont();
    void queryRadius();
    void rewatch();

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_debounce;

    bool m_dark = true;
    QColor m_background, m_backgroundDark, m_foreground, m_foregroundBright, m_accent;
    QColor m_red, m_orange, m_yellow, m_green, m_blue, m_magenta, m_cyan;
    QColor m_surface, m_surfaceRaised, m_sunken, m_hairline, m_border;
    QColor m_textDim, m_textFaint, m_onAccent, m_scrim;
    QColor m_positive, m_negative, m_warning;
    QColor m_hoverFill, m_selectedFill, m_focusBorder;
    qreal m_hoverFillAlpha = 0.08;
    qreal m_selectedFillAlpha = 0.18;

    int m_radius = 0;
    QString m_fontFamily;
    int m_fontBase = 12;
    qreal m_spacingScale = 1.0;
    bool m_spacingScaleWithFont = true;
    int m_caption = 10, m_small = 11, m_body = 12, m_title = 14, m_heading = 16;
    int m_transitionMs = 320;
};
