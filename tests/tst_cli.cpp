// The command line as agents use it (skills/bibliora/SKILL.md describes these
// formats), run as a process against the real Bible. Every run names a
// command, so none of them reaches an open window.
#include "Paths.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTest>

namespace {

struct Run
{
    int code = -1;
    QString out, err;
};

Run bibliora(const QStringList &args)
{
    QProcess p;
    p.start(BIBLIORA_BINARY, args);
    p.waitForFinished();
    return {p.exitCode(), QString::fromUtf8(p.readAllStandardOutput()), QString::fromUtf8(p.readAllStandardError())};
}

} // namespace

class TestCli : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!QFileInfo::exists(Paths::biblePath()))
            QSKIP("No bibles.sqlite3 here; copy it to ~/.local/share/bibliora/ to run these.");
    }

    void showSeveralReferences()
    {
        const Run r = bibliora({"show", "rom 8:28-30; jn 3:16", "-t", "nkjv"});
        QCOMPARE(r.code, 0);
        QVERIFY(r.out.contains("-- Romans 8:28-30, NKJV\n\n16 For God so loved"));
        QVERIFY(r.out.endsWith("-- John 3:16, NKJV\n"));
    }

    void showEveryTranslation()
    {
        const Run r = bibliora({"-t", "all", "show", "esther 2:8"});
        QCOMPARE(r.code, 0);
        QCOMPARE(r.out.count("-- Esther 2:8, "), 5);
        QVERIFY(r.out.indexOf(", ASV") < r.out.indexOf(", KJV"));
        QVERIFY(r.out.indexOf(", NASB1995") < r.out.indexOf(", NKJV"));
    }

    // A reference that isn't there is reported in its place; the rest print.
    void showReportsWhatIsMissing()
    {
        const Run r = bibliora({"show", "jn 11:35; hezekiah 1:1; 3 jn 1:20", "-t", "kjv,nkjv", "--json"});
        QCOMPARE(r.code, 4);
        const QJsonArray passages = QJsonDocument::fromJson(r.out.toUtf8()).array();
        QCOMPARE(passages.size(), 5);
        QCOMPARE(passages.at(1).toObject().value("translation").toString(), QStringLiteral("NKJV"));
        QCOMPARE(passages.at(1).toObject().value("verses").toArray().at(0).toObject().value("text").toString(),
                 QStringLiteral("Jesus wept."));
        QVERIFY(passages.at(2).toObject().contains("error"));
        QVERIFY(passages.at(4).toObject().value("error").toString().contains("isn't in the NKJV"));
    }

    void countByBook()
    {
        const Run r = bibliora({"search", "propitiation", "--count", "-t", "nasb"});
        QCOMPARE(r.code, 0);
        QCOMPARE(r.out, QStringLiteral("Romans   1\nHebrews  1\n1 John   2\n4 verses in the NASB 95\n"));
    }

    void jsonIsOneLine()
    {
        const Run r = bibliora({"search", "faith", "works", "-t", "nasb", "--json", "--limit", "2"});
        QCOMPARE(r.code, 0);
        QCOMPARE(r.out.count('\n'), 1);
        const QJsonObject result = QJsonDocument::fromJson(r.out.toUtf8()).array().at(0).toObject();
        QCOMPARE(result.value("count").toInt(), 14);
        QCOMPARE(result.value("verses").toArray().size(), 2);
    }

    void mistakes()
    {
        QCOMPARE(bibliora({"search", "zzqxj"}).code, 4);
        QCOMPARE(bibliora({"show", ";"}).code, 2);
        const Run r = bibliora({"-t", "nope", "show", "jn 1:1"});
        QCOMPARE(r.code, 2);
        QVERIFY(r.err.contains("no translation called “nope”"));
    }
};

QTEST_GUILESS_MAIN(TestCli)
#include "tst_cli.moc"
