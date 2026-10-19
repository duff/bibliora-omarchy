#include "Translations.h"

namespace Translations {

const QList<Translation> &all()
{
    static const QList<Translation> list = {
        {QStringLiteral("ASV"), QStringLiteral("ASV"), QStringLiteral("American Standard Version"),
         QStringLiteral("American Standard Version, 1901. Public domain."), {}},
        {QStringLiteral("KJV"), QStringLiteral("KJV"), QStringLiteral("King James Version"),
         QStringLiteral("King James Version. Public domain."), QStringLiteral("https://www.thomasnelsonbibles.com/kjv-bible/")},
        {QStringLiteral("LSB"), QStringLiteral("LSB"), QStringLiteral("Legacy Standard Bible"),
         QStringLiteral("Legacy Standard Bible, Copyright © 2021 by The Lockman Foundation. All rights reserved. "
                        "Managed in partnership with Three Sixteen Publishing Inc. lsbible.org, 316publishing.com"),
         QStringLiteral("https://lsbible.org/foreword/")},
        {QStringLiteral("NASB1995"), QStringLiteral("NASB 95"), QStringLiteral("New American Standard Bible 1995"),
         QStringLiteral("New American Standard Bible® (NASB), Copyright © 1960, 1971, 1977, 1995 by The Lockman "
                        "Foundation. Used by permission. All rights reserved. www.lockman.org"),
         QStringLiteral("https://www.lockman.org/nasb-bible/")},
        {QStringLiteral("NKJV"), QStringLiteral("NKJV"), QStringLiteral("New King James Version"),
         QStringLiteral("New King James Version®, Copyright © 1982, Thomas Nelson. All rights reserved. "
                        "www.thomasnelsonbibles.com"),
         QStringLiteral("https://www.thomasnelsonbibles.com/about-nkjv-history/")},
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
