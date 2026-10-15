#include "Theme.h"

#include "Toml.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtMath>

namespace {

Theme *g_theme = nullptr;

QString stateDir() { return QDir::homePath() + "/.local/state/omarchy/current"; }

QString themeDir()
{
    const QString pinned = qEnvironmentVariable("BIBLIORA_THEME_DIR");
    return pinned.isEmpty() ? stateDir() + "/theme" : pinned;
}

QString userShellToml() { return QDir::homePath() + "/.config/omarchy/shell.toml"; }
QString fontsConf() { return QDir::homePath() + "/.config/fontconfig/fonts.conf"; }

QColor parseColor(const QString &raw, const QColor &fallback)
{
    const QColor c(raw.trimmed());
    return c.isValid() ? c : fallback;
}

QColor over(const QColor &fg, qreal a, const QColor &bg)
{
    a = std::clamp(a, 0.0, 1.0);
    return QColor::fromRgbF(fg.redF() * a + bg.redF() * (1 - a), fg.greenF() * a + bg.greenF() * (1 - a),
                            fg.blueF() * a + bg.blueF() * (1 - a));
}

QColor withAlpha(QColor c, qreal a)
{
    c.setAlphaF(std::clamp(a, 0.0, 1.0));
    return c;
}

qreal luminance(const QColor &c)
{
    auto ch = [](qreal v) { return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}

qreal hueDistance(const QColor &a, const QColor &b)
{
    const qreal ha = a.hslHueF() * 360.0;
    const qreal hb = b.hslHueF() * 360.0;
    if (ha < 0 || hb < 0)
        return 0;
    const qreal d = qAbs(ha - hb);
    return d > 180 ? 360 - d : d;
}

bool chromatic(const QColor &c) { return c.hslSaturationF() > 0.22 && c.lightnessF() > 0.12; }

qreal parseReal(const QHash<QString, QString> &v, const QString &key, qreal fallback)
{
    bool ok = false;
    const qreal n = v.value(key).toDouble(&ok);
    return ok ? n : fallback;
}

} // namespace

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    g_theme = this;
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (path == fontsConf())
            queryFont();
        else
            m_debounce.start();
    });

    // Qt resolves fontconfig's monospace alias in-process, so the first
    // frame already has the Omarchy font without waiting on fc-match.
    m_fontFamily = QFontInfo(QFont("monospace")).family();

    reload();
    queryRadius();
}

Theme *Theme::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(g_theme, QJSEngine::CppOwnership);
    return g_theme;
}

Theme *Theme::instance() { return g_theme; }

int Theme::sp(qreal px) const
{
    const qreal scale = m_spacingScale * (m_spacingScaleWithFont ? m_fontBase / 12.0 : 1.0);
    return px <= 0 ? 0 : std::max(1, qRound(px * scale));
}

QColor Theme::alpha(const QColor &c, qreal a) const { return withAlpha(c, a); }

QColor Theme::mix(const QColor &a, const QColor &b, qreal t) const { return over(b, t, a); }

void Theme::reload()
{
    loadPalette();
    loadShellTokens();
    deriveColors();
    rewatch();
    emit changed();
}

void Theme::loadPalette()
{
    const auto c = Toml::readFlat(themeDir() + "/colors.toml");
    m_dark = c.value("mode", "dark") != "light";

    m_background = parseColor(c.value("background"), m_dark ? QColor("#101315") : QColor("#fafafa"));
    m_foreground = parseColor(c.value("foreground"), m_dark ? QColor("#cacccc") : QColor("#1c1c1c"));
    m_backgroundDark = parseColor(c.value("dark_background"),
                                  over(m_dark ? QColor(Qt::black) : m_foreground, 0.06, m_background));
    m_foregroundBright = parseColor(c.value("bright_foreground"), m_foreground);
    m_accent = parseColor(c.value("accent"), parseColor(c.value("blue"), m_foreground));

    m_red = parseColor(c.value("red", c.value("color1")), QColor("#d35f5f"));
    m_green = parseColor(c.value("green", c.value("color2")), QColor("#6aa56a"));
    m_yellow = parseColor(c.value("yellow", c.value("color3")), QColor("#c9a54d"));
    m_blue = parseColor(c.value("blue", c.value("color4")), QColor("#5f8fd3"));
    m_magenta = parseColor(c.value("magenta", c.value("color5")), QColor("#a07acc"));
    m_cyan = parseColor(c.value("cyan", c.value("color6")), QColor("#4fb3b3"));
    m_orange = parseColor(c.value("orange"), m_yellow);
}

void Theme::loadShellTokens()
{
    auto values = Toml::readFlat(themeDir() + "/shell.toml");
    const auto user = Toml::readFlat(userShellToml());
    for (auto it = user.begin(); it != user.end(); ++it)
        values.insert(it.key(), it.value());

    m_fontBase = std::max(8, int(parseReal(values, "font.base-size", 12)));
    auto px = [&](const char *key, qreal mult) {
        const qreal pinned = parseReal(values, QString("font.") + key, -1);
        return pinned > 0 ? qRound(pinned) : std::max(1, qRound(m_fontBase * mult));
    };
    m_caption = px("caption", 0.833);
    m_small = px("body-small", 0.917);
    m_body = px("body", 1.0);
    m_title = px("title", 1.167);
    m_heading = px("heading", 1.333);

    m_spacingScale = parseReal(values, "spacing.scale", 1.0);
    m_spacingScaleWithFont = values.value("spacing.scale-with-font", "true") != "false";
    m_hoverFillAlpha = parseReal(values, "controls.hover-cursor-fill-alpha", 0.08);
    m_selectedFillAlpha = parseReal(values, "controls.selected-fill-alpha", 0.18);
}

void Theme::deriveColors()
{
    const QColor &bg = m_background;
    const QColor &fg = m_foreground;

    // Surfaces lift slightly toward the foreground in dark themes and sink
    // slightly in light ones, so panels read as layers without heavy borders.
    m_surface = over(fg, m_dark ? 0.035 : 0.025, bg);
    m_surfaceRaised = over(fg, m_dark ? 0.07 : 0.0, m_dark ? bg : QColor(Qt::white));
    m_sunken = over(fg, m_dark ? 0.0 : 0.035, m_dark ? m_backgroundDark : bg);
    if (m_dark && m_sunken == bg)
        m_sunken = over(QColor(Qt::black), 0.18, bg);
    m_hairline = over(fg, m_dark ? 0.09 : 0.1, bg);
    m_border = over(fg, m_dark ? 0.16 : 0.18, bg);
    m_textDim = over(fg, 0.68, bg);
    m_textFaint = over(fg, 0.46, bg);
    m_onAccent = luminance(m_accent) > 0.42 ? QColor("#0b0b0b") : QColor("#fafafa");
    m_scrim = withAlpha(m_dark ? QColor(Qt::black) : QColor("#202020"), m_dark ? 0.55 : 0.28);

    // Prefer the theme's green and red; when a theme collapses them (Hackerman
    // is all greens, White all greys) take the palette color furthest from
    // positive, else plain dim text.
    m_positive = chromatic(m_green) ? m_green : fg;
    QColor negative = m_red;
    if (!chromatic(negative) || hueDistance(negative, m_positive) < 45) {
        QColor best;
        qreal bestDistance = 0;
        for (const QColor &candidate : {m_red, m_orange, m_magenta, m_yellow, m_blue, m_cyan}) {
            const qreal d = chromatic(candidate) ? hueDistance(candidate, m_positive) : 0;
            if (d > bestDistance) {
                bestDistance = d;
                best = candidate;
            }
        }
        negative = (best.isValid() && bestDistance >= 45) ? best : m_textDim;
    }
    m_negative = negative;
    m_warning = chromatic(m_yellow) ? m_yellow : m_accent;
    if (hueDistance(m_warning, m_positive) < 25 && chromatic(m_orange))
        m_warning = m_orange;

    m_hoverFill = withAlpha(fg, m_hoverFillAlpha);
    m_selectedFill = withAlpha(fg, m_selectedFillAlpha);
    m_focusBorder = withAlpha(m_accent, 0.9);
}

// After `omarchy font set`, ask fontconfig again, in the background.
void Theme::queryFont()
{
    auto *fc = new QProcess(this);
    connect(fc, &QProcess::finished, this, [this, fc] {
        const QString family = QString::fromUtf8(fc->readAllStandardOutput()).section(',', 0, 0).trimmed();
        fc->deleteLater();
        if (!family.isEmpty() && family != m_fontFamily) {
            m_fontFamily = family;
            emit changed();
        }
    });
    fc->start("fc-match", {"monospace", "-f", "%{family}"});
}

void Theme::queryRadius()
{
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
        return;
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc] {
        const auto json = QJsonDocument::fromJson(proc->readAllStandardOutput()).object();
        proc->deleteLater();
        if (!json.contains("int"))
            return;
        const int r = std::clamp(json.value("int").toInt(), 0, 24);
        if (r != m_radius) {
            m_radius = r;
            emit changed();
        }
    });
    proc->start("hyprctl", {"getoption", "decoration:rounding", "-j"});
}

void Theme::rewatch()
{
    const QStringList watched = m_watcher->files() + m_watcher->directories();
    if (!watched.isEmpty())
        m_watcher->removePaths(watched);
    QStringList paths;
    for (const QString &p : {stateDir(), themeDir(), userShellToml(), fontsConf()})
        if (QFileInfo::exists(p))
            paths << p;
    if (!paths.isEmpty())
        m_watcher->addPaths(paths);
}
