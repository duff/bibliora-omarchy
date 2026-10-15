#include "SearchQuery.h"

#include <QTest>

class TestSearchQuery : public QObject
{
    Q_OBJECT

private slots:
    void build_data()
    {
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("fts");
        QTest::newRow("words, still typing") << "foolish wise" << "\"foolish\" \"wise\"*";
        QTest::newRow("words, done") << "foolish wise " << "\"foolish\" \"wise\"";
        QTest::newRow("case and punctuation") << "Bosom, Abraham!" << "\"bosom\" \"abraham\"";
        QTest::newRow("apostrophe") << "lord's " << "\"lord s\"";
        QTest::newRow("curly apostrophe") << "lord’s " << "\"lord s\"";
        QTest::newRow("phrase") << "\"in the beginning\" god" << "\"in the beginning\" \"god\"*";
        QTest::newRow("open phrase") << "\"in the beg" << "\"in the beg\"";
        QTest::newRow("prefix") << "shep* lord " << "\"shep\"* \"lord\"";
        QTest::newRow("one letter") << "g" << "";
        QTest::newRow("nothing") << "  " << "";
        QTest::newRow("operators are words") << "faith OR hope " << "\"faith\" \"or\" \"hope\"";
    }

    void build()
    {
        QFETCH(QString, typed);
        QFETCH(QString, fts);
        QCOMPARE(SearchQuery::build(typed), fts);
    }
};

QTEST_GUILESS_MAIN(TestSearchQuery)
#include "tst_searchquery.moc"
