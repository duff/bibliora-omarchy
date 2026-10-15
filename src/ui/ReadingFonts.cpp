#include "ReadingFonts.h"

#include <QFontDatabase>
#include <QHash>

namespace ReadingFonts {

namespace {

struct Face
{
    const char *name;
    const char *file; // under the module's resources; empty for Omarchy's font
    bool smallCaps;
    bool opticalSize;
    qreal lineHeight;
};

// Literata first: it is what the iOS app reads in.
constexpr Face kFaces[] = {
    {"Literata", "Literata.ttf", true, true, 1.6},
    {"Source Serif", "SourceSerif4.ttf", true, true, 1.6},
    {"Charis", "CharisSIL-Regular.ttf", true, false, 1.55},
    {"iA Writer Quattro", "iAWriterQuattroS-Regular.ttf", false, false, 1.6},
    {"Omarchy", "", false, false, 1.6},
};

QString family(int index)
{
    static QHash<int, QString> loaded;
    if (const auto it = loaded.constFind(index); it != loaded.cend())
        return *it;
    const QString path = QStringLiteral(":/qt/qml/Bibliora/resources/fonts/") + QLatin1String(kFaces[index].file);
    const int id = QFontDatabase::addApplicationFont(path);
    const QString f = id < 0 ? QString() : QFontDatabase::applicationFontFamilies(id).value(0);
    if (f.isEmpty())
        qWarning("Couldn't load the font %s", qPrintable(path));
    loaded.insert(index, f);
    return f;
}

int clampIndex(int index) { return std::clamp(index, 0, count() - 1); }

} // namespace

int count() { return int(std::size(kFaces)); }

QString name(int index) { return QString::fromLatin1(kFaces[clampIndex(index)].name); }

bool realSmallCaps(int index) { return kFaces[clampIndex(index)].smallCaps; }

qreal lineHeight(int index) { return kFaces[clampIndex(index)].lineHeight; }

QFont font(int index, int pixelSize, const QString &omarchyFamily)
{
    index = clampIndex(index);
    const Face &face = kFaces[index];
    QString f = *face.file ? family(index) : omarchyFamily;
    if (f.isEmpty())
        f = QStringLiteral("serif");
    QFont font(f);
    font.setPixelSize(pixelSize);
    font.setStyleStrategy(QFont::PreferAntialias);
    font.setKerning(true);
    if (face.opticalSize)
        font.setVariableAxis(QFont::Tag("opsz"), float(pixelSize));
    return font;
}

} // namespace ReadingFonts
