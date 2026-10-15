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

} // namespace SearchQuery
