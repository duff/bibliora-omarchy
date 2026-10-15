#include "Translations.h"

namespace Translations {

const QList<Translation> &all()
{
    static const QList<Translation> list = {
        {QStringLiteral("ASV"), QStringLiteral("ASV"), QStringLiteral("American Standard Version")},
        {QStringLiteral("KJV"), QStringLiteral("KJV"), QStringLiteral("King James Version")},
        {QStringLiteral("LSB"), QStringLiteral("LSB"), QStringLiteral("Legacy Standard Bible")},
        {QStringLiteral("NASB1995"), QStringLiteral("NASB 95"), QStringLiteral("New American Standard Bible 1995")},
        {QStringLiteral("NKJV"), QStringLiteral("NKJV"), QStringLiteral("New King James Version")},
    };
    return list;
}

const Translation *find(const QString &keyOrAbbrev)
{
    QString wanted = keyOrAbbrev.trimmed();
    wanted.remove(' ');
    for (const Translation &t : all()) {
        QString abbrev = t.abbrev;
        abbrev.remove(' ');
        if (wanted.compare(t.key, Qt::CaseInsensitive) == 0 || wanted.compare(abbrev, Qt::CaseInsensitive) == 0)
            return &t;
    }
    if (wanted.compare(QLatin1String("NASB"), Qt::CaseInsensitive) == 0)
        return find(QStringLiteral("NASB1995"));
    return nullptr;
}

const Translation &fallback() { return *find(QStringLiteral("KJV")); }

} // namespace Translations
