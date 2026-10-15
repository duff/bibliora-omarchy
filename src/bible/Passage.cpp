#include "Passage.h"

#include <QHash>

namespace Passage {

namespace {

QString place(const VerseKey &k)
{
    const Book *b = Books::byNumber(k.book);
    return QStringLiteral("%1 %2:%3").arg(b ? b->name : QString(), QString::number(k.chapter), QString::number(k.verse));
}

// The verse that comes after `k` in the Bible, by this translation's count.
VerseKey next(Bible &bible, const Translation &t, const VerseKey &k, QHash<quint64, int> &counts)
{
    const Book *b = Books::byNumber(k.book);
    if (!b)
        return {};
    const quint64 id = quint64(k.book) * 1000 + quint64(k.chapter);
    auto it = counts.find(id);
    if (it == counts.end())
        it = counts.insert(id, bible.verseCount(t, *b, k.chapter));
    if (k.verse < *it)
        return {k.book, k.chapter, k.verse + 1};
    if (k.chapter < b->chapters)
        return {k.book, k.chapter + 1, 1};
    if (k.book < Books::count())
        return {k.book + 1, 1, 1};
    return {};
}

} // namespace

QString location(const QList<VerseKey> &run)
{
    if (run.isEmpty())
        return {};
    const VerseKey &first = run.first();
    const VerseKey &last = run.last();
    if (run.size() == 1)
        return place(first);
    if (first.book == last.book && first.chapter == last.chapter)
        return place(first) + '-' + QString::number(last.verse);
    if (first.book == last.book)
        return place(first) + QStringLiteral(" - %1:%2").arg(last.chapter).arg(last.verse);
    return place(first) + QStringLiteral(" - ") + place(last);
}

QList<QList<VerseKey>> runs(Bible &bible, const Translation &t, QList<VerseKey> verses)
{
    std::sort(verses.begin(), verses.end());
    verses.erase(std::unique(verses.begin(), verses.end()), verses.end());
    QList<QList<VerseKey>> out;
    QHash<quint64, int> counts;
    for (const VerseKey &k : std::as_const(verses)) {
        if (!out.isEmpty() && next(bible, t, out.last().last(), counts) == k)
            out.last().append(k);
        else
            out.append({k});
    }
    return out;
}

QString clipboardText(Bible &bible, const Translation &t, QList<VerseKey> verses)
{
    QStringList parts;
    for (const QList<VerseKey> &run : runs(bible, t, std::move(verses))) {
        QStringList texts;
        // One query per chapter the run touches.
        int i = 0;
        while (i < run.size()) {
            int j = i;
            while (j + 1 < run.size() && run.at(j + 1).book == run.at(i).book
                   && run.at(j + 1).chapter == run.at(i).chapter)
                ++j;
            const Book *b = Books::byNumber(run.at(i).book);
            if (b)
                for (const VerseText &v : bible.verses(t, *b, run.at(i).chapter, run.at(i).verse, run.at(j).verse))
                    texts.append(v.text);
            i = j + 1;
        }
        if (texts.isEmpty())
            continue;
        parts.append(texts.join(' ') + QStringLiteral("\n-- ") + location(run) + QStringLiteral(", ") + t.key);
    }
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace Passage
