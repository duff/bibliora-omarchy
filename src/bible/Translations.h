#pragma once

#include <QList>
#include <QString>

// The translations in bibles.sqlite3, in the iOS app's order. `key` is the
// table name and what a copied passage ends with ("-- John 3:16, NASB1995"),
// as on iOS; `abbrev` is what the header shows.
struct Translation
{
    QString key;    // "NKJV", "NASB1995"
    QString abbrev; // "NKJV", "NASB 95"
    QString name;   // "New King James Version"
    QString notice; // the copyright notice, as the iOS app gives it (and packaging/translations.txt)
    QString site;   // about the translation, as the iOS app links it

    QString table() const { return key.toLower(); }
};

namespace Translations {

const QList<Translation> &all();
const Translation *find(const QString &keyOrAbbrev); // case-insensitive; nullptr if unknown
const Translation &fallback();                       // KJV, the iOS default

} // namespace Translations
