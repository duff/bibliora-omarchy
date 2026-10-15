// History and search history, after the iOS app's HistoryTests.swift and
// SearchHistoryTests.swift. One deliberate difference: a place is a chapter
// and the verse at the top of the page, and an earlier visit to the same
// chapter gives way to a new one (iOS compared exact pages).
#include "History.h"

#include <QTest>

namespace {
Place at(int book, int chapter, int verse = 1) { return {book, chapter, verse}; }
} // namespace

class TestHistory : public QObject
{
    Q_OBJECT

private slots:
    void startsAtGenesis()
    {
        History h;
        QCOMPARE(h.places(), QList<Place>({at(1, 1)}));
        QCOMPARE(h.index(), 0);
        QVERIFY(!h.back());
        QVERIFY(!h.forward());
    }

    void addAFewAndNavigate()
    {
        History h;
        QVERIFY(!h.hasBack());
        h.add(at(2, 4));
        QVERIFY(h.hasBack());
        QVERIFY(!h.hasForward());
        h.add(at(3, 2, 30));
        h.add(at(45, 9));
        h.add(at(41, 16, 3));
        QCOMPARE(h.index(), 4);
        QVERIFY(!h.forward());

        QCOMPARE(*h.back(), at(45, 9));
        QVERIFY(h.hasBack() && h.hasForward());
        QCOMPARE(*h.back(), at(3, 2, 30));
        QCOMPARE(*h.forward(), at(45, 9));
        QCOMPARE(*h.back(), at(3, 2, 30));

        // Going somewhere new drops what was ahead.
        h.add(at(42, 3, 8));
        QCOMPARE(h.places().size(), 4);
        QCOMPARE(h.index(), 3);
        QCOMPARE(*h.back(), at(3, 2, 30));
        QCOMPARE(*h.back(), at(2, 4));
        QCOMPARE(*h.back(), at(1, 1));
        QVERIFY(!h.hasBack());
        QVERIFY(h.hasForward());
    }

    void replaceCurrent()
    {
        History h;
        h.add(at(2, 4));
        h.add(at(3, 2));
        h.replaceCurrent(at(43, 1, 13));
        QCOMPARE(h.places(), QList<Place>({at(1, 1), at(2, 4), at(43, 1, 13)}));
    }

    void anEarlierVisitGivesWay()
    {
        History h;
        h.add(at(2, 4));
        h.add(at(3, 2));
        h.add(at(42, 3));
        h.add(at(3, 2, 20)); // the same chapter, further down
        QCOMPARE(h.places(), QList<Place>({at(1, 1), at(2, 4), at(42, 3), at(3, 2, 20)}));
        QCOMPARE(h.index(), 3);

        h.add(at(27, 3));
        h.add(at(27, 3));
        QCOMPARE(h.places().last(), at(27, 3));
        QCOMPARE(h.places().size(), 5);
    }

    void backBackAndAdd()
    {
        History h;
        h.add(at(2, 4));
        h.add(at(3, 2));
        h.add(at(42, 3));
        h.add(at(65, 1));
        QCOMPARE(*h.back(), at(42, 3));
        QCOMPARE(*h.back(), at(3, 2));
        h.add(at(41, 8));
        QCOMPARE(h.places(), QList<Place>({at(1, 1), at(2, 4), at(3, 2), at(41, 8)}));
        QCOMPARE(h.index(), 3);
    }

    void goToKeepsWhatsAhead()
    {
        History h;
        h.add(at(2, 4));
        h.add(at(3, 2));
        h.add(at(42, 3));
        QCOMPARE(*h.goTo(1), at(2, 4));
        QCOMPARE(h.index(), 1);
        QVERIFY(h.hasForward());
        QCOMPARE(*h.goTo(3), at(42, 3));
        QVERIFY(!h.hasForward());
        QVERIFY(!h.goTo(9));
    }

    void menuIndices()
    {
        History h;
        h.add(at(2, 4));
        h.add(at(3, 2));
        h.add(at(42, 3));
        QCOMPARE(h.backIndices(), QList<int>({2, 1, 0}));
        h.goTo(1);
        QCOMPARE(h.backIndices(), QList<int>({0}));
        QCOMPARE(h.forwardIndices(), QList<int>({2, 3}));
        h.goTo(3);
        QCOMPARE(h.forwardIndices(), QList<int>());
    }

    void menuIndicesRespectTheLimit()
    {
        History h;
        for (int i = 1; i <= 20; ++i)
            h.add(at(19, i));
        QCOMPARE(h.backIndices().size(), 15);
        QCOMPARE(h.backIndices().first(), 19);
        QCOMPARE(h.backIndices().last(), 5);
    }

    void keepsAHundred()
    {
        History h;
        for (int i = 1; i <= 105; ++i)
            h.add(at(19, i));
        QCOMPARE(h.places().size(), 100);
        QCOMPARE(h.places().first(), at(19, 6));
        QCOMPARE(h.places().last(), at(19, 105));
        QCOMPARE(h.index(), 99);
        QCOMPARE(*h.back(), at(19, 104));
    }

    void savesAndRestores()
    {
        History h;
        h.add(at(43, 3, 16));
        h.add(at(19, 23));
        h.back();
        History again;
        again.restore(h.serialize(), h.index());
        QCOMPARE(again.places(), h.places());
        QCOMPARE(again.index(), 1);
        QCOMPARE(at(43, 3, 16).serialize(), QStringLiteral("43:3:16"));
        QCOMPARE(*Place::parse("genesis:1:0"), at(1, 1)); // the iOS form
        QVERIFY(!Place::parse("nowhere"));
        QCOMPARE(at(43, 3, 16).label(), QStringLiteral("John 3:16"));
        QCOMPARE(at(43, 3).label(), QStringLiteral("John 3"));
    }

    void searchHistory()
    {
        SearchHistory s;
        QVERIFY(s.isExamples());
        QCOMPARE(s.items(), SearchHistory::examples());
        s.add("  love  ");
        QCOMPARE(s.items(), QStringList({"love"})); // the examples step aside
        s.add("a");                                 // too short
        s.add("grace");
        s.add("love");
        QCOMPARE(s.items(), QStringList({"love", "grace"}));
        for (int i = 0; i < 60; ++i)
            s.add(QStringLiteral("word %1").arg(i));
        QCOMPARE(s.items().size(), SearchHistory::kLimit);
        QCOMPARE(s.items().first(), QStringLiteral("word 59"));
        s.clear();
        QVERIFY(s.isExamples());
    }
};

QTEST_GUILESS_MAIN(TestHistory)
#include "tst_history.moc"
