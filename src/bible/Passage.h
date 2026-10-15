#pragma once

#include "Bible.h"

#include <QList>
#include <QString>

// A verse picked out by where it is, so a selection means the same verses in
// every translation (the iOS app keyed them on per-translation row ids, which
// drift by one after Jude in LSB and NASB).
struct VerseKey
{
    int book = 0; // canonical number
    int chapter = 0;
    int verse = 0;

    bool operator==(const VerseKey &o) const = default;
    auto operator<=>(const VerseKey &o) const = default;
    bool isValid() const { return book > 0 && chapter > 0 && verse > 0; }
};

inline size_t qHash(const VerseKey &k, size_t seed = 0)
{
    return ::qHash(k.book * 1000000 + k.chapter * 1000 + k.verse, seed);
}

namespace Passage {

// The text the iOS app copies (Selection.swift, SelectionTests.swift): runs
// of consecutive verses, each as its verses' text joined by spaces and then
// "-- Genesis 1:1-3, NKJV", the runs separated by a blank line.
//
//   Genesis 1:1, NKJV                  one verse
//   Genesis 1:1-3, NKJV                within a chapter
//   Genesis 1:31 - 2:1, NKJV           across chapters
//   Genesis 50:26 - Exodus 1:1, NKJV   across books
QString clipboardText(Bible &bible, const Translation &t, QList<VerseKey> verses);

// Just the "Genesis 1:1-3" part for consecutive verses (sorted, non-empty).
QString location(const QList<VerseKey> &run);

// Splits sorted verses into runs that follow on from each other, counting
// the last verse of a chapter as followed by the first of the next.
QList<QList<VerseKey>> runs(Bible &bible, const Translation &t, QList<VerseKey> verses);

} // namespace Passage
