#include "Typesetter.h"

#include <QFontMetricsF>
#include <QTextBoundaryFinder>

#include <cmath>

namespace {

// "LORD’s" looks up "LORD".
QString lookupForm(QString word)
{
    for (const QString &suffix : {QStringLiteral("’s"), QStringLiteral("'s")})
        if (word.endsWith(suffix))
            word.chop(suffix.size());
    return word.trimmed();
}

QFont numberFont(const TypeStyle &style)
{
    QFont f = style.font;
    f.setPixelSize(std::max(8, int(std::lround(style.font.pixelSize() * 0.62))));
    f.setFeature(QFont::Tag("lnum"), 1);
    return f;
}

QList<QTextLayout::FormatRange> formatsFor(const Markup::Block &b, const TypeStyle &style)
{
    QList<QTextLayout::FormatRange> out;

    QTextCharFormat smallCaps;
    if (style.realSmallCaps)
        smallCaps.setFontFeatures({{QFont::Tag("smcp"), 1}, {QFont::Tag("c2sc"), 0}});
    else
        smallCaps.setFontCapitalization(QFont::SmallCaps);
    smallCaps.setFontLetterSpacingType(QFont::PercentageSpacing);
    smallCaps.setFontLetterSpacing(104);

    if (b.kind == Markup::Block::Title) {
        out.append({0, int(b.text.size()), smallCaps});
    }

    // The number holds its place, unseen; ChapterLayout::marks draws it raised.
    QTextCharFormat number;
    number.setForeground(Qt::transparent);
    number.setFont(numberFont(style), QTextCharFormat::FontPropertiesSpecifiedOnly);

    for (const Markup::Span &s : b.spans) {
        if (s.style == Markup::Span::VerseNumber)
            out.append({s.start, s.length, number});
        else if (b.kind != Markup::Block::Title)
            out.append({s.start, s.length, smallCaps});
    }
    return out;
}

} // namespace

int ChapterLayout::pageOfVerse(int verse) const
{
    if (verse <= 0 || pages.isEmpty())
        return 0;
    if (const auto it = verseStartPage.constFind(verse); it != verseStartPage.cend())
        return *it;
    // A verse this translation doesn't have (3 John 1:15): the nearest before it.
    int best = 0;
    for (auto it = verseStartPage.cbegin(); it != verseStartPage.cend(); ++it)
        if (it.key() < verse)
            best = std::max(best, *it);
    return best;
}

QList<int> ChapterLayout::versesOnPage(int page) const
{
    QList<int> out;
    if (page < 0 || page >= pages.size())
        return out;
    for (const Piece &piece : pages.at(page).pieces) {
        const Markup::Block &b = chapter.blocks.at(piece.block);
        const QTextLayout *layout = layouts.at(piece.block).get();
        const QTextLine first = layout->lineAt(piece.lineStart);
        const QTextLine last = layout->lineAt(piece.lineStart + piece.lineCount - 1);
        const int from = first.textStart();
        const int to = last.textStart() + last.textLength();
        for (const Markup::VerseRange &v : b.verses)
            if (v.start < to && v.end > from && !out.contains(v.verse))
                out.append(v.verse);
    }
    std::sort(out.begin(), out.end());
    return out;
}

ChapterLayout::Hit ChapterLayout::hitTest(int page, QPointF p) const
{
    Hit hit;
    if (page < 0 || page >= pages.size())
        return hit;
    for (const Piece &piece : pages.at(page).pieces) {
        const QTextLayout *layout = layouts.at(piece.block).get();
        for (int i = piece.lineStart; i < piece.lineStart + piece.lineCount; ++i) {
            const qreal top = places.at(piece.block).at(i).top;
            if (p.y() < top || p.y() >= top + lineHeight)
                continue;
            const QTextLine line = layout->lineAt(i);
            const QRectF r = line.naturalTextRect();
            if (p.x() < r.left() - lineHeight || p.x() > r.right() + lineHeight)
                return hit;
            hit.block = piece.block;
            hit.pos = line.xToCursor(p.x(), QTextLine::CursorOnCharacter);
            hit.verse = chapter.blocks.at(piece.block).verseAt(hit.pos);
            return hit;
        }
    }
    return hit;
}

QString ChapterLayout::wordAt(int page, QPointF p) const
{
    const Hit hit = hitTest(page, p);
    if (hit.block < 0)
        return {};
    const QString &text = chapter.blocks.at(hit.block).text;
    if (hit.pos < 0 || hit.pos >= text.size() || !text.at(hit.pos).isLetter())
        return {};
    QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
    finder.setPosition(hit.pos);
    int start = finder.isAtBoundary() && (finder.boundaryReasons() & QTextBoundaryFinder::StartOfItem)
                    ? hit.pos
                    : finder.toPreviousBoundary();
    finder.setPosition(hit.pos);
    int end = finder.toNextBoundary();
    if (start < 0 || end < 0)
        return {};
    return lookupForm(text.mid(start, end - start));
}

QStringList ChapterLayout::wordsOn(int page) const
{
    QStringList out;
    if (page < 0 || page >= pages.size())
        return out;
    QSet<QString> seen;
    for (const Piece &piece : pages.at(page).pieces) {
        const QString &text = chapter.blocks.at(piece.block).text;
        const QTextLayout *layout = layouts.at(piece.block).get();
        const QTextLine last = layout->lineAt(piece.lineStart + piece.lineCount - 1);
        const int to = last.textStart() + last.textLength();
        QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
        int from = layout->lineAt(piece.lineStart).textStart();
        finder.setPosition(from);
        for (int end = finder.toNextBoundary(); end > 0 && from < to; from = end, end = finder.toNextBoundary()) {
            if (!text.at(from).isLetter())
                continue; // spaces, punctuation, the hidden verse numbers
            const QString word = lookupForm(text.mid(from, end - from));
            if (!seen.contains(word.toLower())) {
                seen.insert(word.toLower());
                out.append(word);
            }
        }
    }
    return out;
}

QList<QRectF> ChapterLayout::verseRects(int page, const QSet<int> &verses) const
{
    QList<QRectF> out;
    if (verses.isEmpty() || page < 0 || page >= pages.size())
        return out;
    for (const Piece &piece : pages.at(page).pieces) {
        const Markup::Block &b = chapter.blocks.at(piece.block);
        const QTextLayout *layout = layouts.at(piece.block).get();
        for (int i = piece.lineStart; i < piece.lineStart + piece.lineCount; ++i) {
            const QTextLine line = layout->lineAt(i);
            const int lineFrom = line.textStart();
            int lineTo = line.textStart() + line.textLength();
            // Leave out the space a line ends on.
            while (lineTo > lineFrom && b.text.at(lineTo - 1).isSpace())
                --lineTo;
            int from = -1, to = -1;
            for (const Markup::VerseRange &v : b.verses) {
                if (!verses.contains(v.verse))
                    continue;
                const int s = std::max(v.start, lineFrom);
                const int e = std::min(v.end, lineTo);
                if (s >= e)
                    continue;
                from = from < 0 ? s : std::min(from, s);
                to = std::max(to, e);
            }
            if (from < 0)
                continue;
            const qreal x1 = line.cursorToX(from);
            const qreal x2 = line.cursorToX(to);
            const qreal top = places.at(piece.block).at(i).top;
            // cursorToX already counts the line's indent and alignment.
            out.append(QRectF(QPointF(std::min(x1, x2), top), QPointF(std::max(x1, x2), top + lineHeight)));
        }
    }
    return out;
}

namespace Typesetter {

std::shared_ptr<ChapterLayout> set(Markup::Chapter chapter, const TypeStyle &style)
{
    auto out = std::make_shared<ChapterLayout>();
    out->chapter = std::move(chapter);
    out->lineHeight = style.lineHeight;

    const QFontMetricsF fm(style.font);
    const qreal lh = style.lineHeight;
    // The baseline sits where it would if the line's extra leading were shared
    // above and below, so a superscript never pushes a line down.
    const qreal baseline = std::round((lh - (fm.ascent() + fm.descent())) / 2 + fm.ascent());
    const int linesPerPage = std::max(3, int(std::floor(style.pageHeight / lh)));

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // Psalm 119's titles start with a Hebrew letter ("א ALEPH"); left to
    // itself the layout would set them right to left, as iOS does.
    option.setTextDirection(Qt::LeftToRight);
    option.setUseDesignMetrics(true);

    const auto &blocks = out->chapter.blocks;
    out->layouts.reserve(blocks.size());
    out->places.resize(blocks.size());

    int page = 0;
    int row = 0; // the next free line on the page
    ChapterLayout::Page current;
    auto flush = [&] {
        out->pages.append(current);
        current = ChapterLayout::Page();
        ++page;
        row = 0;
    };

    for (int bi = 0; bi < blocks.size(); ++bi) {
        const Markup::Block &b = blocks.at(bi);
        auto layout = std::make_unique<QTextLayout>(b.text, style.font);
        QTextOption o = option;
        o.setAlignment(b.kind == Markup::Block::Centered ? Qt::AlignHCenter : Qt::AlignLeft);
        layout->setTextOption(o);
        layout->setFormats(formatsFor(b, style));
        layout->setCacheEnabled(true);

        // Break the block into lines first, so pagination knows its length.
        const qreal inset = b.kind == Markup::Block::Centered ? style.centerInset : 0;
        layout->beginLayout();
        int n = 0;
        for (;;) {
            QTextLine line = layout->createLine();
            if (!line.isValid())
                break;
            qreal indent = 0;
            if (n == 0 && b.indent)
                indent = style.indent;
            else if (n > 0 && b.kind == Markup::Block::Poetry)
                indent = style.hang;
            line.setLineWidth(std::max(lh, style.pageWidth - indent - 2 * inset));
            line.setPosition(QPointF(indent + inset, 0));
            ++n;
        }
        layout->endLayout();

        // A blank line before a stanza or a title, unless it would open a page.
        if (b.gapBefore && row > 0)
            ++row;
        // Keep a title with the line after it, and don't leave a paragraph's
        // first line alone at the bottom of a page.
        const bool title = b.kind == Markup::Block::Title;
        const int keep = title ? n + 1 : std::min(n, 2);
        if (row > 0 && row + keep > linesPerPage)
            flush();

        auto &places = out->places[bi];
        places.resize(n);
        for (int i = 0; i < n; ++i) {
            const int left = n - i;
            // Don't carry a paragraph's last line alone onto the next page.
            if (row > 0 && n >= 3 && left == 2 && row + 1 == linesPerPage)
                flush();
            if (row >= linesPerPage)
                flush();
            QTextLine line = layout->lineAt(i);
            const qreal top = row * lh;
            line.setPosition(QPointF(line.x(), top + baseline - line.ascent()));
            places[i] = {page, top};
            if (!current.pieces.isEmpty() && current.pieces.last().block == bi)
                ++current.pieces.last().lineCount;
            else
                current.pieces.append({bi, i, 1});
            ++row;
        }
        out->layouts.push_back(std::move(layout));
    }
    if (!current.pieces.isEmpty() || out->pages.isEmpty())
        out->pages.append(current);

    // Verse numbers sit with their tops a little under the capitals, as on iOS.
    const QFont small = numberFont(style);
    const qreal raise = std::round(style.font.pixelSize() * 0.24);
    const qreal smallAscent = QFontMetricsF(small).ascent();
    QTextCharFormat numberColor;
    numberColor.setForeground(style.verseNumber);
    for (int bi = 0; bi < blocks.size(); ++bi) {
        const QTextLayout *layout = out->layouts.at(bi).get();
        for (const Markup::Span &span : blocks.at(bi).spans) {
            if (span.style != Markup::Span::VerseNumber)
                continue;
            const QTextLine line = layout->lineForTextPosition(span.start);
            if (!line.isValid())
                continue;
            const QString digits = blocks.at(bi).text.mid(span.start, span.length).trimmed();
            auto mark = std::make_unique<QTextLayout>(digits, small);
            mark->setFormats({{0, int(digits.size()), numberColor}});
            mark->beginLayout();
            QTextLine m = mark->createLine();
            m.setLineWidth(style.lineHeight * 4);
            mark->endLayout();
            const auto &place = out->places[bi][line.lineNumber()];
            const qreal x = line.cursorToX(span.start);
            m.setPosition(QPointF(x, place.top + baseline - raise - smallAscent));
            out->marks.push_back({place.page, std::move(mark)});
        }
    }

    for (int bi = 0; bi < blocks.size(); ++bi) {
        const QTextLayout *layout = out->layouts.at(bi).get();
        for (const Markup::VerseRange &v : blocks.at(bi).verses) {
            if (out->verseStartPage.contains(v.verse))
                continue;
            const int line = std::max(0, layout->lineForTextPosition(v.start).lineNumber());
            out->verseStartPage.insert(v.verse, out->places[bi][line].page);
        }
    }
    for (int p = 0; p < out->pages.size(); ++p) {
        const QList<int> verses = out->versesOnPage(p);
        if (verses.isEmpty())
            continue;
        ChapterLayout::Page &page = out->pages[p];
        page.lastVerse = verses.last();
        page.firstVerse = verses.first();
        for (const int v : verses)
            if (out->verseStartPage.value(v, -1) == p) {
                page.firstVerse = v;
                break;
            }
    }
    return out;
}

} // namespace Typesetter
