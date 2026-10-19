#pragma once

#include <QString>

namespace SearchQuery {

// Turns what you type into an FTS5 query: every word must appear, "quoted
// words" must appear together, and the last word also matches as the start of
// a word (so results come as you type, and "shep" finds shepherd). A * after
// any word does the same for it. Punctuation is ignored; "LORD's" is the
// phrase "lord s", the way the index splits it. Empty when there is nothing
// to look for.
QString build(const QString &typed);

// A word and its other forms, roughly: an ending like -ing, -eth or -s comes
// off and what's left matches as the start of a word ("gathering" is
// "gather"*). How often the Bible uses a word's family says better than the
// word alone how unusual it is. Empty for a word too short to say.
QString family(const QString &word);

} // namespace SearchQuery
