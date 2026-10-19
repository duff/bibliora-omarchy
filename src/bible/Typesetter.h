#pragma once

#include "Markup.h"

#include <QColor>
#include <QHash>
#include <QFont>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QTextLayout>

#include <memory>
#include <vector>

// How a chapter is set: the reading font and the page it fills.
struct TypeStyle
{
    QFont font;                    // the reading font, sized in pixels
    bool realSmallCaps = true;     // the font has smcp; otherwise Qt fakes them
    QColor text, verseNumber;
    qreal pageWidth = 600;
    qreal pageHeight = 800;
    qreal lineHeight = 30;         // every line sits on this grid
    qreal indent = 24;             // a paragraph's first line
    qreal hang = 24;               // a poetry line's wrapped lines
    qreal centerInset = 36;        // each side of a centered line, as on iOS
};

// A chapter set into pages. Each chapter starts on a page of its own, as on
// iOS. Lines are positioned relative to their page's top left.
class ChapterLayout
{
public:
    struct Piece
    {
        int block = 0;
        int lineStart = 0;
        int lineCount = 0;
    };
    struct Page
    {
        QList<Piece> pieces;
        int firstVerse = 0; // the first verse that starts here (or runs on from before, if none does)
        int lastVerse = 0;
    };
    struct Hit
    {
        int block = -1;
        int pos = -1;
        int verse = 0;
    };

    Markup::Chapter chapter;
    std::vector<std::unique_ptr<QTextLayout>> layouts; // one per block
    QList<Page> pages;
    qreal lineHeight = 0;

    int pageCount() const { return int(pages.size()); }
    int pageOfVerse(int verse) const; // the page it starts on
    QHash<int, int> verseStartPage;
    QList<int> versesOnPage(int page) const;

    Hit hitTest(int page, QPointF p) const;
    QString wordAt(int page, QPointF p) const;
    // Every word on a page, once each (whatever its case), in reading order.
    QStringList wordsOn(int page) const;
    // Rectangles covering the given verses on a page, one per line they touch.
    QList<QRectF> verseRects(int page, const QSet<int> &verses) const;

    // Verse numbers, raised: the text keeps an invisible copy of each so lines
    // break around it, and these are drawn above it (Qt's glyph runs, which the
    // scene graph draws from, leave superscripts on the baseline).
    struct Mark
    {
        int page = 0;
        std::unique_ptr<QTextLayout> layout;
    };
    std::vector<Mark> marks;

    // Where each line of each block sits: its page and the top of its line box.
    struct LinePlace
    {
        int page = 0;
        qreal top = 0;
    };
    std::vector<std::vector<LinePlace>> places;
};

namespace Typesetter {

std::shared_ptr<ChapterLayout> set(Markup::Chapter chapter, const TypeStyle &style);

} // namespace Typesetter
