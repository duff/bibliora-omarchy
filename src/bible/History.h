#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// A place you were reading: a chapter, and the verse at the top of the page.
// The iOS app kept a page number; pages here move as the window resizes, so
// the verse is what's kept.
struct Place
{
    int book = 1; // canonical number, 1–66
    int chapter = 1;
    int verse = 1;

    bool operator==(const Place &o) const = default;
    QString label() const;       // "John 3", or "John 3:16" when not at the start
    QString serialize() const;   // "43:3:16"
    static std::optional<Place> parse(const QString &s);
};

// Back and forward through the places you've been, with the iOS app's rules
// (History.swift, HistoryTests.swift):
//
// - add() drops anything forward of the current place, removes an earlier
//   visit to the same chapter, keeps at most 100, and moves to the new end.
// - Turning pages calls replaceCurrent(), so the current entry follows you
//   without the list growing.
// - goTo() moves within the list and keeps what's forward of it.
class History
{
public:
    static constexpr int kLimit = 100;
    static constexpr int kMenuLimit = 15;

    History();

    const QList<Place> &places() const { return m_places; }
    int index() const { return m_index; }
    Place current() const { return m_places.value(m_index); }

    void add(const Place &place);
    void replaceCurrent(const Place &place);
    std::optional<Place> back();
    std::optional<Place> forward();
    std::optional<Place> goTo(int index);
    bool hasBack() const { return m_index > 0; }
    bool hasForward() const { return m_index < m_places.size() - 1; }

    QList<int> backIndices() const;    // nearest first, at most kMenuLimit
    QList<int> forwardIndices() const; // nearest first, at most kMenuLimit

    QStringList serialize() const;
    void restore(const QStringList &places, int index);

private:
    QList<Place> m_places;
    int m_index = 0;
};

// Searches you've made, newest first, as on iOS (SearchHistory.swift): at
// most 50, no repeats, nothing shorter than two characters. Until there are
// any, a few examples stand in.
class SearchHistory
{
public:
    static constexpr int kLimit = 50;
    static QStringList examples();

    SearchHistory();
    const QStringList &items() const { return m_items; }
    bool isExamples() const { return m_items == examples(); }
    void add(const QString &search);
    void clear();
    void restore(const QStringList &items);

private:
    QStringList m_items;
};
