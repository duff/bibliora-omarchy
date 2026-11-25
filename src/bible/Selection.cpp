#include "Selection.h"

#include <algorithm>

void Selection::start(const VerseKey &at) { startRange(at, at); }

void Selection::startRange(const VerseKey &from, const VerseKey &to)
{
    m_kind = Kind::Range;
    m_kept.clear();
    m_anchor = from;
    m_cursor = to;
    m_hasCursor = true;
    update();
}

void Selection::extend(int delta, const VerseKey &top)
{
    if (m_kind == Kind::None) {
        start(top);
        return;
    }
    if (!m_hasCursor) {
        // After a jump: start again here, keeping what's picked.
        m_kept = m_verses;
        m_kind = Kind::Picked;
        m_anchor = m_cursor = top;
        m_hasCursor = true;
    } else {
        m_cursor = m_step(m_cursor, delta);
    }
    update();
}

void Selection::extendTo(const VerseKey &key)
{
    if (m_kind == Kind::None) {
        toggle(key);
        return;
    }
    if (!m_hasCursor) {
        m_kept = m_verses;
        m_kind = Kind::Picked;
        m_anchor = key;
        m_hasCursor = true;
    }
    m_cursor = key;
    update();
}

void Selection::toggle(const VerseKey &key)
{
    m_kept = m_verses;
    if (m_kept.remove(key)) {
        // Dropped: the cursor goes to the nearest verse still picked, the one
        // before if there is one.
        if (m_kept.isEmpty()) {
            clear();
            return;
        }
        const VerseKey *before = nullptr, *after = nullptr;
        for (const VerseKey &k : std::as_const(m_kept)) {
            if (k < key && (!before || *before < k))
                before = &k;
            if (key < k && (!after || k < *after))
                after = &k;
        }
        m_cursor = before ? *before : *after;
    } else {
        m_kept.insert(key);
        m_cursor = key;
    }
    m_kind = Kind::Picked;
    m_anchor = m_cursor;
    m_hasCursor = true;
    update();
}

void Selection::dropCursor()
{
    if (m_kind == Kind::None || !m_hasCursor)
        return;
    m_kept = m_verses;
    m_kind = Kind::Picked;
    m_hasCursor = false;
}

void Selection::clear()
{
    m_kind = Kind::None;
    m_kept.clear();
    m_verses.clear();
    m_anchor = m_cursor = {};
    m_hasCursor = false;
}

void Selection::update()
{
    m_verses = m_kept;
    VerseKey a = m_anchor, b = m_cursor;
    if (b < a)
        std::swap(a, b);
    for (VerseKey k = a; int(m_verses.size()) < kMaxVerses;) {
        m_verses.insert(k);
        if (!(k < b))
            break;
        const VerseKey next = m_step(k, 1);
        if (next == k)
            break;
        k = next;
    }
}
