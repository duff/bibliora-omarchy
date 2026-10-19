#include "SearchQuery.h"

#include <QStringList>

namespace SearchQuery {

namespace {

// Lower-cased runs of letters and digits; apostrophes and the like split a
// word the way FTS5's unicode61 tokenizer does.
QStringList words(const QString &s)
{
    QStringList out;
    QString cur;
    for (const QChar c : s) {
        if (c.isLetterOrNumber()) {
            cur.append(c.toLower());
        } else if (!cur.isEmpty()) {
            out.append(cur);
            cur.clear();
        }
    }
    if (!cur.isEmpty())
        out.append(cur);
    return out;
}

QString phrase(const QStringList &w) { return '"' + w.join(' ') + '"'; }

} // namespace

QString build(const QString &typed)
{
    QStringList terms;
    bool lastIsWord = false;
    bool lastPrefix = false;
    int i = 0;
    const int n = int(typed.size());
    while (i < n) {
        const QChar c = typed.at(i);
        if (c == '"') {
            const int close = typed.indexOf('"', i + 1);
            const QString inner = typed.mid(i + 1, close < 0 ? -1 : close - i - 1);
            const QStringList w = words(inner);
            if (!w.isEmpty())
                terms.append(phrase(w));
            lastIsWord = false;
            i = close < 0 ? n : close + 1;
            continue;
        }
        if (c.isSpace()) {
            ++i;
            continue;
        }
        // A word runs to the next space or quote.
        int j = i;
        while (j < n && !typed.at(j).isSpace() && typed.at(j) != '"')
            ++j;
        const QString raw = typed.mid(i, j - i);
        const QStringList w = words(raw);
        if (!w.isEmpty()) {
            const bool star = raw.endsWith('*');
            terms.append(phrase(w) + (star ? QStringLiteral("*") : QString()));
            lastIsWord = true;
            lastPrefix = star;
        }
        i = j;
    }
    if (terms.isEmpty())
        return {};
    // A word you're still typing ends the text; a space or a full stop after
    // it says it's done.
    const bool stillTyping = !typed.isEmpty() && typed.back().isLetterOrNumber();
    if (lastIsWord && stillTyping && !lastPrefix)
        terms.last() += '*';
    QString joined = terms.join(' ');
    // A lone letter as you start typing would match half the Bible.
    if (terms.size() == 1 && words(typed).join(QString()).size() < 2)
        return {};
    return joined;
}

QString family(const QString &word)
{
    QString stem = words(word).join(QString());
    if (stem.size() < 3)
        return {};
    for (const QString &ending : {QStringLiteral("ings"), QStringLiteral("ing"), QStringLiteral("edst"),
                                  QStringLiteral("eth"), QStringLiteral("est"), QStringLiteral("ies"),
                                  QStringLiteral("ed"), QStringLiteral("es"), QStringLiteral("er"),
                                  QStringLiteral("s")}) {
        if (stem.endsWith(ending) && stem.size() - ending.size() >= 4) {
            stem.chop(ending.size());
            break;
        }
    }
    return phrase({stem}) + '*';
}

} // namespace SearchQuery
