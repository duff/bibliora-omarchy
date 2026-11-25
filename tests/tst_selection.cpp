// Picking verses (Selection), over a made-up book of two ten-verse chapters.
#include "Selection.h"

#include <QTest>

namespace {

VerseKey v(int chapter, int verse) { return {1, chapter, verse}; }

VerseKey step(const VerseKey &from, int delta)
{
    int n = (from.chapter - 1) * 10 + from.verse - 1 + delta;
    n = std::clamp(n, 0, 19);
    return v(n / 10 + 1, n % 10 + 1);
}

QSet<VerseKey> verses(int chapter, std::initializer_list<int> numbers)
{
    QSet<VerseKey> out;
    for (int n : numbers)
        out.insert(v(chapter, n));
    return out;
}

} // namespace

class TestSelection : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty()
    {
        Selection s(step);
        QCOMPARE(s.kind(), Selection::Kind::None);
        QVERIFY(!s.hasCursor());
        QVERIFY(s.verses().isEmpty());
    }

    void jAndKMoveTheEndOfARun()
    {
        Selection s(step);
        s.extend(1, v(1, 4)); // the first j just picks the verse at the top
        QCOMPARE(s.kind(), Selection::Kind::Range);
        QCOMPARE(s.verses(), verses(1, {4}));
        s.extend(1, {});
        s.extend(1, {});
        QCOMPARE(s.verses(), verses(1, {4, 5, 6}));
        s.extend(-3, {});
        QCOMPARE(s.verses(), verses(1, {3, 4}));
        QCOMPARE(s.cursor(), v(1, 3));
    }

    void aRunCrossesChapters()
    {
        Selection s(step);
        s.startRange(v(1, 9), v(2, 2));
        QCOMPARE(s.verses(), verses(1, {9, 10}) + verses(2, {1, 2}));
    }

    void startAgainFromTheCursor()
    {
        Selection s(step);
        s.startRange(v(1, 1), v(1, 3));
        s.start(s.cursor());
        QCOMPARE(s.verses(), verses(1, {3}));
    }

    // Dropping a verse from a run keeps the rest, and k carries on from the
    // nearest verse left.
    void ctrlClickOutOfARun()
    {
        Selection s(step);
        s.startRange(v(1, 1), v(1, 3));
        s.toggle(v(1, 3));
        QCOMPARE(s.kind(), Selection::Kind::Picked);
        QCOMPARE(s.verses(), verses(1, {1, 2}));
        QCOMPARE(s.cursor(), v(1, 2));
        s.extend(-1, {});
        QCOMPARE(s.verses(), verses(1, {1, 2}));
        s.extend(2, {});
        QCOMPARE(s.verses(), verses(1, {1, 2, 3}));
    }

    void ctrlClicksThenJAndShiftClick()
    {
        Selection s(step);
        s.toggle(v(1, 1));
        s.toggle(v(1, 5));
        s.extend(1, {});
        QCOMPARE(s.verses(), verses(1, {1, 5, 6}));
        s.extendTo(v(1, 8));
        QCOMPARE(s.verses(), verses(1, {1, 5, 6, 7, 8}));
    }

    void droppingTheLastVerseEndsPicking()
    {
        Selection s(step);
        s.toggle(v(1, 4));
        s.toggle(v(1, 4));
        QCOMPARE(s.kind(), Selection::Kind::None);
        QVERIFY(s.verses().isEmpty());
        QVERIFY(!s.hasCursor());
    }

    // After a jump the verses stay picked, and j starts again where you are.
    void aJumpDropsTheCursor()
    {
        Selection s(step);
        s.startRange(v(1, 1), v(1, 2));
        s.dropCursor();
        QVERIFY(!s.hasCursor());
        QCOMPARE(s.verses(), verses(1, {1, 2}));
        s.extend(1, v(2, 5));
        QCOMPARE(s.verses(), verses(1, {1, 2}) + verses(2, {5}));
        s.extend(1, {});
        QCOMPARE(s.verses(), verses(1, {1, 2}) + verses(2, {5, 6}));
    }

    void clear()
    {
        Selection s(step);
        s.startRange(v(1, 1), v(1, 2));
        s.clear();
        QCOMPARE(s.kind(), Selection::Kind::None);
        QVERIFY(s.verses().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSelection)
#include "tst_selection.moc"
