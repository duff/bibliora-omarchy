#pragma once

#include "Passage.h"

#include <QSet>

#include <functional>

// The verses picked to copy, and the keyboard cursor that picks them.
//
// None: nothing picked. Range: v, then j/k (or a shift-click) run from the
// anchor to the cursor. Picked: verses that aren't one run (ctrl-clicks, or
// what's kept after a jump); j/k and shift-clicks then add a run from the
// cursor to what's already picked, and never throw it away.
//
// A jump drops the cursor (dropCursor) but keeps the verses, so j and k start
// again from the top of the new page instead of reaching back to the old one.
class Selection
{
public:
    enum class Kind { None, Range, Picked };
    // The verse delta verses on from a key (negative: back), across chapters
    // and books; where it stops at either end of the Bible.
    using Step = std::function<VerseKey(const VerseKey &, int)>;

    static constexpr int kMaxVerses = 3000; // a whole book is plenty

    explicit Selection(Step step)
        : m_step(std::move(step))
    {
    }

    Kind kind() const { return m_kind; }
    const QSet<VerseKey> &verses() const { return m_verses; }
    bool hasCursor() const { return m_kind != Kind::None && m_hasCursor; }
    VerseKey cursor() const { return m_cursor; }

    void start(const VerseKey &at);                      // v: just this verse
    void startRange(const VerseKey &from, const VerseKey &to); // a Go to range
    void extend(int delta, const VerseKey &top);         // j/k; with no cursor, picks top
    void extendTo(const VerseKey &key);                  // shift-click
    void toggle(const VerseKey &key);                    // ctrl-click: add or drop
    void dropCursor();                                   // a jump
    void clear();

private:
    void update();

    Step m_step;
    Kind m_kind = Kind::None;
    QSet<VerseKey> m_kept;  // picked before the current run
    VerseKey m_anchor, m_cursor;
    bool m_hasCursor = false;
    QSet<VerseKey> m_verses; // m_kept and the run from anchor to cursor
};
