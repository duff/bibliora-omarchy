#include "History.h"

#include "Books.h"

QString Place::label() const
{
    const Book *b = Books::byNumber(book);
    if (!b)
        return {};
    QString s = b->name + ' ' + QString::number(chapter);
    if (verse > 1)
        s += ':' + QString::number(verse);
    return s;
}

QString Place::serialize() const
{
    return QStringLiteral("%1:%2:%3").arg(book).arg(chapter).arg(verse);
}

std::optional<Place> Place::parse(const QString &s)
{
    const QStringList parts = s.split(':');
    if (parts.size() < 2)
        return std::nullopt;
    Place p;
    bool ok = false;
    p.book = parts.at(0).toInt(&ok);
    const Book *book = ok ? Books::byNumber(p.book) : Books::byKey(parts.at(0));
    if (!book)
        return std::nullopt;
    p.book = book->number;
    p.chapter = std::clamp(parts.at(1).toInt(), 1, book->chapters);
    p.verse = std::max(1, parts.value(2).toInt());
    return p;
}

History::History()
    : m_places{Place()}
{
}

void History::add(const Place &place)
{
    if (m_index < m_places.size() - 1)
        m_places.erase(m_places.begin() + m_index + 1, m_places.end());

    // An earlier visit to the same chapter gives way to this one.
    const auto sameChapter = [&place](const Place &p) { return p.book == place.book && p.chapter == place.chapter; };
    const qsizetype before = m_places.size();
    m_places.removeIf(sameChapter);
    if (m_places.size() != before)
        m_index = int(m_places.size()) - 1;

    const int extra = int(m_places.size()) + 1 - kLimit;
    if (extra > 0) {
        m_places.erase(m_places.begin(), m_places.begin() + extra);
        m_index = int(m_places.size()) - 1;
    }

    m_places.append(place);
    m_index = int(m_places.size()) - 1;
}

void History::replaceCurrent(const Place &place)
{
    if (m_index >= 0 && m_index < m_places.size())
        m_places[m_index] = place;
}

std::optional<Place> History::back()
{
    if (!hasBack())
        return std::nullopt;
    return m_places.at(--m_index);
}

std::optional<Place> History::forward()
{
    if (!hasForward())
        return std::nullopt;
    return m_places.at(++m_index);
}

std::optional<Place> History::goTo(int index)
{
    if (index < 0 || index >= m_places.size())
        return std::nullopt;
    m_index = index;
    return m_places.at(m_index);
}

QList<int> History::backIndices() const
{
    QList<int> out;
    for (int i = m_index - 1; i >= 0 && i >= m_index - kMenuLimit; --i)
        out.append(i);
    return out;
}

QList<int> History::forwardIndices() const
{
    QList<int> out;
    for (int i = m_index + 1; i < m_places.size() && i <= m_index + kMenuLimit; ++i)
        out.append(i);
    return out;
}

QStringList History::serialize() const
{
    QStringList out;
    for (const Place &p : m_places)
        out.append(p.serialize());
    return out;
}

void History::restore(const QStringList &places, int index)
{
    QList<Place> parsed;
    for (const QString &s : places)
        if (const auto p = Place::parse(s))
            parsed.append(*p);
    if (parsed.isEmpty())
        return;
    m_places = parsed;
    m_index = std::clamp(index, 0, int(m_places.size()) - 1);
}

QStringList SearchHistory::examples()
{
    return {QStringLiteral("foolish wise"), QStringLiteral("gen 9"), QStringLiteral("bosom abraham")};
}

SearchHistory::SearchHistory()
    : m_items(examples())
{
}

void SearchHistory::add(const QString &search)
{
    const QString s = search.simplified();
    if (s.size() <= 1)
        return;
    if (isExamples())
        m_items.clear();
    m_items.removeAll(s);
    while (m_items.size() >= kLimit)
        m_items.removeLast();
    m_items.prepend(s);
}

void SearchHistory::clear() { m_items = examples(); }

void SearchHistory::restore(const QStringList &items)
{
    m_items = items.isEmpty() ? examples() : items;
}
