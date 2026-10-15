#include "Reference.h"

#include <QRegularExpression>

namespace {

struct Parts
{
    QString book;
    int chapter = 0, verse = 0, endVerse = 0;
    bool ok = false;
};

Parts split(const QString &text)
{
    // The book is everything up to the numbers at the end. It must hold a
    // letter, so "1 john" is a book, while "3:16" has none.
    static const QRegularExpression re(
        QStringLiteral("^\\s*(.*?[\\p{L}.])?\\s*(?:(\\d+)(?:\\s*[:.,\\s]\\s*(\\d+))?(?:\\s*[-\\x{2013}\\x{2014}]\\s*(\\d+))?)?\\s*$"),
        QRegularExpression::UseUnicodePropertiesOption);
    Parts p;
    // Half-typed: "john 3:" reads as John 3.
    QString t = text.trimmed();
    while (!t.isEmpty() && QStringLiteral(":.,-\x2013\x2014").contains(t.back()))
        t.chop(1);
    const auto m = re.match(t);
    if (!m.hasMatch())
        return p;
    p.book = m.captured(1).trimmed();
    p.chapter = m.captured(2).toInt();
    p.verse = m.captured(3).toInt();
    p.endVerse = p.verse ? m.captured(4).toInt() : 0; // "gen 1-2" is chapters, not verses
    if (p.endVerse && p.endVerse <= p.verse)
        p.endVerse = 0;
    p.ok = !p.book.isEmpty() || p.chapter > 0;
    return p;
}

std::optional<Reference> make(const Book *book, const Parts &p)
{
    if (!book)
        return std::nullopt;
    Reference r;
    r.book = book;
    // A one-chapter book: "jude 3" means Jude 1:3.
    if (book->chapters == 1 && p.chapter > 1 && !p.verse) {
        r.chapter = 1;
        r.verse = p.chapter;
        return r;
    }
    if (p.chapter > book->chapters)
        return std::nullopt;
    r.chapter = p.chapter;
    r.verse = p.chapter ? p.verse : 0;
    r.endVerse = r.verse ? p.endVerse : 0;
    return r;
}

} // namespace

QString Reference::label() const
{
    if (!book)
        return {};
    if (!chapter)
        return book->name;
    QString s = book->name + ' ' + QString::number(chapter);
    if (verse)
        s += ':' + QString::number(verse);
    if (endVerse)
        s += '-' + QString::number(endVerse);
    return s;
}

std::optional<Reference> Reference::parse(const QString &text, const Book *current)
{
    if (text.trimmed().startsWith(QLatin1String("bibliora:"), Qt::CaseInsensitive))
        return fromLink(text.trimmed());
    const auto all = candidates(text, current);
    if (all.isEmpty())
        return std::nullopt;
    return all.first();
}

QList<Reference> Reference::candidates(const QString &text, const Book *current)
{
    const Parts p = split(text);
    if (!p.ok)
        return {};
    QList<Reference> out;
    if (p.book.isEmpty()) {
        if (const auto r = make(current, p))
            out.append(*r);
        return out;
    }
    // "1 john 3" splits as book "1 john"; "john 1" as book "john".
    for (const Book *b : Books::match(p.book))
        if (const auto r = make(b, p))
            out.append(*r);
    return out;
}

std::optional<Reference> Reference::fromLink(const QString &link)
{
    // Read by hand: QUrl takes "43.3.16" for an IPv4 address and makes it
    // 43.3.0.16.
    const QString trimmed = link.trimmed();
    const int scheme = trimmed.indexOf(QLatin1String("://"));
    if (scheme < 0)
        return std::nullopt;
    QString payload = trimmed.mid(scheme + 3).section('?', 0, 0).section('#', 0, 0);
    while (payload.startsWith('/'))
        payload.remove(0, 1);
    while (payload.endsWith('/'))
        payload.chop(1);
    const QStringList parts = payload.split('.');
    bool ok = false;
    const Book *book = Books::byNumber(parts.value(0).toInt(&ok));
    if (!ok || !book)
        return std::nullopt;
    Reference r;
    r.book = book;
    r.chapter = std::clamp(parts.size() > 1 ? parts.at(1).toInt() : 1, 1, book->chapters);
    r.verse = std::max(0, parts.size() > 2 ? parts.at(2).toInt() : 0);
    return r;
}

QString Reference::link() const
{
    if (!book)
        return {};
    QString s = QStringLiteral("bibliora://%1.%2").arg(book->number).arg(std::max(1, chapter));
    if (verse)
        s += '.' + QString::number(verse);
    return s;
}
