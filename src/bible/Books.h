#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// The 66 books in Protestant canonical order, as the iOS app has them. `key`
// is the `book` column in bibles.sqlite3 (genesis, first_samuel, …); `number`
// is the 1-based canonical position, which bibliora:// links use.
struct Book
{
    int number = 0;
    QString key;
    QString name;    // "1 Samuel", "Song of Songs"
    QString abbrev;  // "1Sam", "Song", as on iOS
    int chapters = 0;
    bool newTestament = false;
    QStringList aliases; // lower-case, no spaces: "1sam", "1sa", "1sm"
};

namespace Books {

const QList<Book> &all();
int count();

// nullptr when out of range or unknown.
const Book *byNumber(int number);
const Book *byKey(const QString &key);

// The books a typed name could mean, best first: an exact name or alias
// ("jn", "song of solomon", "1 cor") gives that book alone; otherwise every
// book whose name starts with the text, in canonical order ("jo" → Joshua,
// Job, Joel, Jonah, John). Accepts "1", "i", "first" and "1st" for numbered
// books, and ignores case, spaces and periods.
QList<const Book *> match(const QString &text);

// The single best book for `text`, or nullptr.
const Book *find(const QString &text);

// Lower-case, without spaces or periods, with a leading roman numeral or
// ordinal word turned into a digit: "I John" → "1john", "Second Kings" → "2kings".
QString normalize(const QString &text);

} // namespace Books
