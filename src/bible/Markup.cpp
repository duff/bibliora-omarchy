#include "Markup.h"

#include <QRegularExpression>

#include <optional>

namespace Markup {

namespace {

struct Token
{
    enum Type { Text, Open, Close, VerseStart };
    Type type = Text;
    QString name; // tag name, or the text
    int verse = 0;
};

QList<Token> tokenize(const QList<VerseMarkup> &verses)
{
    static const QRegularExpression tag(QStringLiteral("<(/?)([a-z][a-z-]*)(?:\\s[^>]*)?/?>"));
    QList<Token> out;
    for (const VerseMarkup &v : verses) {
        out.append({Token::VerseStart, {}, v.verse});
        int at = 0;
        auto it = tag.globalMatch(v.markup);
        while (it.hasNext()) {
            const auto m = it.next();
            if (m.capturedStart() > at)
                out.append({Token::Text, v.markup.mid(at, m.capturedStart() - at), v.verse});
            out.append({m.captured(1).isEmpty() ? Token::Open : Token::Close, m.captured(2), v.verse});
            at = m.capturedEnd();
        }
        if (at < v.markup.size())
            out.append({Token::Text, v.markup.mid(at), v.verse});
    }
    return out;
}

QString collapseSpaces(const QString &s)
{
    QString out;
    out.reserve(s.size());
    bool space = false;
    for (const QChar c : s) {
        if (c.isSpace() && c != QChar(0x00A0)) {
            space = true;
            continue;
        }
        if (space)
            out.append(' ');
        space = false;
        out.append(c);
    }
    if (space)
        out.append(' ');
    return out;
}

class Builder
{
public:
    explicit Builder(bool verseNumbers)
        : m_verseNumbers(verseNumbers)
    {
    }

    Chapter run(const QList<Token> &tokens)
    {
        for (int i = 0; i < tokens.size(); ++i) {
            const Token &t = tokens.at(i);
            switch (t.type) {
            case Token::VerseStart:
                m_verse = t.verse;
                break;
            case Token::Text:
                if (m_heading == 0 && !m_inVerseNumber)
                    text(t.name);
                else if (m_inVerseNumber)
                    m_verseNumber += t.name;
                break;
            case Token::Open:
                open(t.name);
                break;
            case Token::Close:
                if (t.name == QLatin1String("heading")) {
                    m_heading = std::max(0, m_heading - 1);
                    // As on iOS, a heading takes a blank line after it along.
                    if (i + 2 < tokens.size() && isBreak(tokens.at(i + 1)) && isBreak(tokens.at(i + 2)))
                        i += 2;
                } else {
                    close(t.name);
                }
                break;
            }
        }
        finish();
        return std::move(m_chapter);
    }

private:
    static bool isBreak(const Token &t) { return t.type == Token::Close && t.name == QLatin1String("br"); }

    void open(const QString &name)
    {
        if (name == QLatin1String("heading")) {
            ++m_heading;
        } else if (name == QLatin1String("small-caps")) {
            ++m_smallCaps;
        } else if (name == QLatin1String("v")) {
            m_inVerseNumber = true;
            m_verseNumber.clear();
        } else if (name == QLatin1String("psalm-title") || name == QLatin1String("speaker-title")) {
            startBlock(Block::Title, false);
        } else if (name == QLatin1String("centered")) {
            startBlock(Block::Centered, false);
        }
        // <citation id="…">: the verse comes from the row itself.
    }

    void close(const QString &name)
    {
        if (name == QLatin1String("paragraph")) {
            finish();
            m_next = Block::Prose;
            m_nextIndent = true;
            m_breaks = 0;
        } else if (name == QLatin1String("br")) {
            if (m_open && !m_block.text.trimmed().isEmpty()) {
                finish();
                m_breaks = 1;
            } else if (++m_breaks >= 2) {
                m_gap = true;
            }
            m_next = Block::Poetry;
            m_nextIndent = false;
        } else if (name == QLatin1String("small-caps")) {
            m_smallCaps = std::max(0, m_smallCaps - 1);
        } else if (name == QLatin1String("v")) {
            m_inVerseNumber = false;
            if (m_verseNumbers && !m_verseNumber.trimmed().isEmpty() && m_heading == 0) {
                // A no-break space keeps the number with its first word.
                append(m_verseNumber.trimmed() + QChar(0x00A0), Span::VerseNumber);
            }
        } else if (name == QLatin1String("psalm-title") || name == QLatin1String("speaker-title")
                   || name == QLatin1String("centered")) {
            finish();
            m_next = Block::Poetry;
            m_nextIndent = false;
            // The title ends its own line, so a </br> after it makes a blank
            // line, as on iOS ("A Psalm of David." stands apart).
            m_breaks = name == QLatin1String("centered") ? 0 : 1;
        }
    }

    void text(const QString &raw)
    {
        QString s = collapseSpaces(raw);
        if (s.isEmpty())
            return;
        if (!m_open) {
            if (s.trimmed().isEmpty())
                return;
            startBlock(m_next, m_nextIndent);
        }
        if (m_block.text.isEmpty() || m_block.text.endsWith(' ') || m_block.text.endsWith(QChar(0x00A0)))
            while (s.startsWith(' '))
                s.remove(0, 1);
        if (s.isEmpty())
            return;
        append(s, m_smallCaps > 0 ? std::optional<Span::Style>(Span::SmallCaps) : std::nullopt);
        m_breaks = 0;
    }

    void append(const QString &s, std::optional<Span::Style> style)
    {
        if (!m_open)
            startBlock(m_next, m_nextIndent);
        const int start = int(m_block.text.size());
        m_block.text += s;
        const int end = int(m_block.text.size());
        if (style) {
            if (!m_block.spans.isEmpty() && m_block.spans.last().style == *style
                && m_block.spans.last().start + m_block.spans.last().length == start)
                m_block.spans.last().length += end - start;
            else
                m_block.spans.append({start, end - start, *style});
        }
        if (!m_block.verses.isEmpty() && m_block.verses.last().verse == m_verse && m_block.verses.last().end == start)
            m_block.verses.last().end = end;
        else
            m_block.verses.append({m_verse, start, end});
    }

    void startBlock(Block::Kind kind, bool indent)
    {
        finish();
        m_block = Block();
        m_block.kind = kind;
        m_block.indent = indent;
        // A title after text always stands apart (a heading left out above
        // one can take its blank line along).
        const bool afterText = !m_chapter.blocks.isEmpty() && m_chapter.blocks.last().kind != Block::Title;
        m_block.gapBefore = !m_chapter.blocks.isEmpty() && (m_gap || (kind == Block::Title && afterText));
        m_gap = false;
        m_open = true;
        m_next = Block::Poetry;
        m_nextIndent = false;
    }

    void finish()
    {
        if (!m_open)
            return;
        m_open = false;
        Block &b = m_block;
        while (!b.text.isEmpty() && b.text.back().isSpace())
            b.text.chop(1);
        const int n = int(b.text.size());
        if (n == 0) {
            m_gap = m_gap || b.gapBefore;
            return;
        }
        auto clamp = [n](int &start, int &end) {
            start = std::min(start, n);
            end = std::min(end, n);
        };
        QList<Span> spans;
        for (Span s : std::as_const(b.spans)) {
            int end = s.start + s.length;
            clamp(s.start, end);
            s.length = end - s.start;
            if (s.length > 0)
                spans.append(s);
        }
        b.spans = spans;
        QList<VerseRange> verses;
        for (VerseRange v : std::as_const(b.verses)) {
            clamp(v.start, v.end);
            if (v.end > v.start)
                verses.append(v);
        }
        b.verses = verses;
        m_chapter.blocks.append(std::move(b));
    }

    const bool m_verseNumbers;
    Chapter m_chapter;
    Block m_block;
    bool m_open = false;
    Block::Kind m_next = Block::Prose; // a chapter's first block, unless markup says otherwise
    bool m_nextIndent = false;
    bool m_gap = false;
    int m_breaks = 0;
    int m_heading = 0;
    int m_smallCaps = 0;
    int m_verse = 0;
    bool m_inVerseNumber = false;
    QString m_verseNumber;
};

} // namespace

int Block::verseAt(int pos) const
{
    for (const VerseRange &v : verses)
        if (pos >= v.start && pos < v.end)
            return v.verse;
    // Past the end of a line: the last verse that starts before it.
    int best = 0;
    for (const VerseRange &v : verses)
        if (v.start <= pos)
            best = v.verse;
    return best;
}

Chapter parse(const QList<VerseMarkup> &verses, bool verseNumbers)
{
    return Builder(verseNumbers).run(tokenize(verses));
}

} // namespace Markup
