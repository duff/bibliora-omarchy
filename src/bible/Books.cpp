#include "Books.h"

#include <QHash>
#include <QRegularExpression>

namespace Books {

namespace {

struct Seed
{
    const char *abbrev, *name, *key;
    int chapters;
    const char *aliases;
};

// Aliases follow the SBL handbook and the abbreviations people commonly type.
constexpr Seed kSeeds[] = {
    {"Gen", "Genesis", "genesis", 50, "gen ge gn"},
    {"Exo", "Exodus", "exodus", 40, "exod exo ex"},
    {"Lev", "Leviticus", "leviticus", 27, "lev le lv"},
    {"Num", "Numbers", "numbers", 36, "num nu nm nb"},
    {"Deut", "Deuteronomy", "deuteronomy", 34, "deut de dt"},
    {"Joshua", "Joshua", "joshua", 24, "josh jos jsh"},
    {"Judges", "Judges", "judges", 21, "judg jdg jg jdgs"},
    {"Ruth", "Ruth", "ruth", 4, "ruth rth ru"},
    {"1Sam", "1 Samuel", "first_samuel", 31, "1sam 1sa 1sm 1s"},
    {"2Sam", "2 Samuel", "second_samuel", 24, "2sam 2sa 2sm 2s"},
    {"1Kings", "1 Kings", "first_kings", 22, "1kgs 1ki 1kin 1k"},
    {"2Kings", "2 Kings", "second_kings", 25, "2kgs 2ki 2kin 2k"},
    {"1Chr", "1 Chronicles", "first_chronicles", 29, "1chr 1ch 1chron"},
    {"2Chr", "2 Chronicles", "second_chronicles", 36, "2chr 2ch 2chron"},
    {"Ezra", "Ezra", "ezra", 10, "ezr"},
    {"Neh", "Nehemiah", "nehemiah", 13, "neh ne"},
    {"Esther", "Esther", "esther", 10, "esth est es"},
    {"Job", "Job", "job", 42, "jb"},
    {"Psa", "Psalms", "psalms", 150, "ps psa psalm pss psm"},
    {"Prov", "Proverbs", "proverbs", 31, "prov pr prv pro"},
    {"Ecc", "Ecclesiastes", "ecclesiastes", 12, "eccl ecc ec qoh"},
    {"Song", "Song of Songs", "song_of_solomon", 8, "song sos so ss songofsolomon canticles cant"},
    {"Isaiah", "Isaiah", "isaiah", 66, "isa is"},
    {"Jer", "Jeremiah", "jeremiah", 52, "jer je jr"},
    {"Lam", "Lamentations", "lamentations", 5, "lam la"},
    {"Ezek", "Ezekiel", "ezekiel", 48, "ezek eze ezk"},
    {"Daniel", "Daniel", "daniel", 12, "dan da dn"},
    {"Hosea", "Hosea", "hosea", 14, "hos ho"},
    {"Joel", "Joel", "joel", 3, "jl"},
    {"Amos", "Amos", "amos", 9, "am"},
    {"Obad", "Obadiah", "obadiah", 1, "obad ob"},
    {"Jonah", "Jonah", "jonah", 4, "jon jnh"},
    {"Micah", "Micah", "micah", 7, "mic mc"},
    {"Nahum", "Nahum", "nahum", 3, "nah na"},
    {"Hab", "Habakkuk", "habakkuk", 3, "hab hb"},
    {"Zep", "Zephaniah", "zephaniah", 3, "zeph zep zp"},
    {"Haggai", "Haggai", "haggai", 2, "hag hg"},
    {"Zech", "Zechariah", "zechariah", 14, "zech zec zc"},
    {"Mal", "Malachi", "malachi", 4, "mal ml"},
    {"Matt", "Matthew", "matthew", 28, "matt mat mt"},
    {"Mark", "Mark", "mark", 16, "mk mrk mar"},
    {"Luke", "Luke", "luke", 24, "lk luk"},
    {"John", "John", "john", 21, "jn jhn joh"},
    {"Acts", "Acts", "acts", 28, "ac act"},
    {"Romans", "Romans", "romans", 16, "rom ro rm"},
    {"1Cor", "1 Corinthians", "first_corinthians", 16, "1cor 1co"},
    {"2Cor", "2 Corinthians", "second_corinthians", 13, "2cor 2co"},
    {"Gal", "Galatians", "galatians", 6, "gal ga"},
    {"Eph", "Ephesians", "ephesians", 6, "eph ephes"},
    {"Phil", "Philippians", "philippians", 4, "phil php pp"},
    {"Col", "Colossians", "colossians", 4, "col"},
    {"1Th", "1 Thessalonians", "first_thessalonians", 5, "1thess 1th 1thes"},
    {"2Th", "2 Thessalonians", "second_thessalonians", 3, "2thess 2th 2thes"},
    {"1Tim", "1 Timothy", "first_timothy", 6, "1tim 1ti 1tm"},
    {"2Tim", "2 Timothy", "second_timothy", 4, "2tim 2ti 2tm"},
    {"Titus", "Titus", "titus", 3, "tit"},
    {"Philem", "Philemon", "philemon", 1, "phlm philem phm pm"},
    {"Heb", "Hebrews", "hebrews", 13, "heb"},
    {"James", "James", "james", 5, "jas jm"},
    {"1Pet", "1 Peter", "first_peter", 5, "1pet 1pe 1pt 1p"},
    {"2Pet", "2 Peter", "second_peter", 3, "2pet 2pe 2pt 2p"},
    {"1John", "1 John", "first_john", 5, "1jn 1jhn 1jo 1j"},
    {"2John", "2 John", "second_john", 1, "2jn 2jhn 2jo 2j"},
    {"3John", "3 John", "third_john", 1, "3jn 3jhn 3jo 3j"},
    {"Jude", "Jude", "jude", 1, "jud jd"},
    {"Rev", "Revelation", "revelation", 22, "rev re rv revelations apocalypse"},
};

struct Index
{
    QList<Book> books;
    QHash<QString, int> byKey;
    QHash<QString, int> exact; // normalized name or alias → index
    QList<QString> normalizedNames;
};

const Index &index()
{
    static const Index idx = [] {
        Index i;
        int n = 0;
        for (const Seed &s : kSeeds) {
            Book b;
            b.number = n + 1;
            b.key = QString::fromLatin1(s.key);
            b.name = QString::fromUtf8(s.name);
            b.abbrev = QString::fromLatin1(s.abbrev);
            b.chapters = s.chapters;
            b.newTestament = b.number >= 40;
            b.aliases = QString::fromLatin1(s.aliases).split(' ', Qt::SkipEmptyParts);
            i.byKey.insert(b.key, n);
            const QString name = normalize(b.name);
            i.normalizedNames.append(name);
            i.exact.insert(name, n);
            for (const QString &a : std::as_const(b.aliases))
                i.exact.insert(a, n);
            i.books.append(b);
            ++n;
        }
        return i;
    }();
    return idx;
}

} // namespace

const QList<Book> &all() { return index().books; }

int count() { return index().books.size(); }

const Book *byNumber(int number)
{
    if (number < 1 || number > count())
        return nullptr;
    return &index().books.at(number - 1);
}

const Book *byKey(const QString &key)
{
    const auto &idx = index();
    // The iOS app once spelled it "galations"; old links may still carry it.
    const auto it = idx.byKey.constFind(key == QLatin1String("galations") ? QStringLiteral("galatians") : key);
    return it == idx.byKey.cend() ? nullptr : &idx.books.at(*it);
}

QString normalize(const QString &text)
{
    QString t = text.trimmed().toLower();
    static const QRegularExpression ordinal(
        QStringLiteral("^(iii|ii|i|first|second|third|1st|2nd|3rd)[\\s.]+"));
    const auto m = ordinal.match(t);
    if (m.hasMatch()) {
        const QString w = m.captured(1);
        const QChar digit = (w == QLatin1String("i") || w == QLatin1String("first") || w == QLatin1String("1st"))
                                ? QChar('1')
                            : (w == QLatin1String("ii") || w == QLatin1String("second") || w == QLatin1String("2nd"))
                                ? QChar('2')
                                : QChar('3');
        t = digit + t.mid(m.capturedLength());
    }
    QString out;
    out.reserve(t.size());
    for (const QChar c : std::as_const(t))
        if (c.isLetterOrNumber())
            out.append(c);
    return out;
}

QList<const Book *> match(const QString &text)
{
    const QString n = normalize(text);
    if (n.isEmpty())
        return {};
    const auto &idx = index();
    if (const auto it = idx.exact.constFind(n); it != idx.exact.cend())
        return {&idx.books.at(*it)};
    QList<const Book *> out;
    for (int i = 0; i < idx.books.size(); ++i)
        if (idx.normalizedNames.at(i).startsWith(n))
            out.append(&idx.books.at(i));
    if (out.isEmpty()) {
        // "songofsol…", "revelations": a longer alias that starts with the text.
        for (auto it = idx.exact.cbegin(); it != idx.exact.cend(); ++it)
            if (it.key().size() > 3 && it.key().startsWith(n) && !out.contains(&idx.books.at(*it)))
                out.append(&idx.books.at(*it));
        std::sort(out.begin(), out.end(), [](const Book *a, const Book *b) { return a->number < b->number; });
    }
    return out;
}

const Book *find(const QString &text)
{
    const auto books = match(text);
    return books.isEmpty() ? nullptr : books.first();
}

} // namespace Books
