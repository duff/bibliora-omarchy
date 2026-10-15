#pragma once

#include "Bible.h"

#include <QList>
#include <QString>

// A chapter's text as blocks of running text, from the database's display
// markup (formatted_text). The markup isn't XML: `</paragraph>` and `</br>`
// are bare markers. Each verse's row looks like
//
//   </paragraph><citation id="1"><v>1</v>In the beginning God … </citation>
//   <psalm-title>A Psalm of David.</psalm-title></br><citation id="14237">
//       <v>1</v>The <small-caps>Lord</small-caps> is my shepherd;</br>…
//
// and is read the way the iOS app reads it (Chapter.swift):
//
//   </paragraph>                      a new paragraph, first line indented
//   </br>                             a new line (poetry); two make a blank line
//   <psalm-title>, <speaker-title>    a line of their own, in small caps
//   <centered>                        a centered line ("HOLINESS TO THE LORD.")
//   <small-caps>                      LORD, GOD
//   <heading>                         section headings: left out, as on iOS,
//                                     with a </br></br> right after one
//   <v>N</v>                          the verse number, shown only on request
namespace Markup {

struct Span
{
    enum Style { SmallCaps, VerseNumber };
    int start = 0;
    int length = 0;
    Style style = SmallCaps;
};

// The part of a block that belongs to one verse: [start, end).
struct VerseRange
{
    int verse = 0;
    int start = 0;
    int end = 0;
};

struct Block
{
    enum Kind { Prose, Poetry, Title, Centered };
    Kind kind = Prose;
    bool indent = false;    // a new paragraph: indent the first line
    bool gapBefore = false; // a blank line before it (a stanza break)
    QString text;
    QList<Span> spans;
    QList<VerseRange> verses;

    int verseAt(int pos) const; // 0 if none
};

struct Chapter
{
    QList<Block> blocks;
};

Chapter parse(const QList<VerseMarkup> &verses, bool verseNumbers);

} // namespace Markup
