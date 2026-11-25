// The window's controllers, driven as the keys and the mouse drive them,
// against the real Bible. Settings go to Qt's test folder, not the user's.
#include "Paths.h"
#include "Reader.h"

#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTest>

#include <memory>

class TestUi : public QObject
{
    Q_OBJECT

    std::unique_ptr<Reader> reader;

    QList<int> versesHere() const { return reader->layout()->versesOnPage(reader->page()); }

    QString shown() const
    {
        const QList<int> v = versesHere();
        return v.isEmpty() ? QString("nothing") : QString("%1-%2").arg(v.first()).arg(v.last());
    }

    // Turns pages until one lies wholly inside the verse (false if none does).
    bool turnIntoVerse(int verse)
    {
        for (int i = 0; i < 20; ++i) {
            if (versesHere() == QList<int>{verse})
                return true;
            reader->nextPage();
        }
        return false;
    }

    void resize(qreal width, qreal height)
    {
        reader->setViewport(width, height);
        QTest::qWait(100); // a resize lays the page out again after 40 ms
    }

private slots:
    void initTestCase()
    {
        const QString bible = Paths::biblePath();
        if (!QFileInfo::exists(bible))
            QSKIP("No bibles.sqlite3 here; copy it to ~/.local/share/bibliora/ to run these.");
        Paths::setBiblePath(bible);
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QSettings().clear();
        reader = std::make_unique<Reader>(nullptr);
        reader->setViewport(900, 1100);
    }

    void cleanup()
    {
        reader.reset();
        QSettings().clear();
    }

    // ------------------------------------------------ keeping your place

    // Opening a verse and then resizing the window keeps that verse in view.
    void resizeKeepsTheVerseOpened()
    {
        QVERIFY(reader->goTo("jn 3:16"));
        QVERIFY(versesHere().contains(16));
        QVERIFY(!versesHere().startsWith(16)); // it starts lower on its page
        resize(800, 400);
        QVERIFY2(versesHere().contains(16), qPrintable("the page shows John 3:" + shown()));
    }

    // A page inside one long verse stays put when it's laid out again.
    void relayoutKeepsAPageInsideAVerse()
    {
        resize(320, 200);
        QVERIFY(reader->goTo("esth 8:9"));
        reader->nextPage();
        QVERIFY2(turnIntoVerse(9), "no page lies wholly inside Esther 8:9 at this size");
        const int page = reader->page();
        reader->toggleVerseNumbers();
        reader->toggleVerseNumbers();
        QCOMPARE(reader->page(), page);
        resize(321, 200); // the same lines, laid out again
        QCOMPARE(reader->page(), page);
    }

    // Where each page starts leads back to that page, verse numbers on or off.
    void everyPageStartLeadsBackToItsPage()
    {
        resize(320, 200);
        for (const bool numbers : {false, true}) {
            reader->setVerseNumbers(numbers);
            for (const QString chapter : {"esth 8", "ps 119", "jn 3"}) {
                QVERIFY(reader->goTo(chapter));
                const auto layout = reader->layout();
                for (int p = 0; p < layout->pageCount(); ++p)
                    QCOMPARE(layout->pageOf(layout->spotAt(p)), p);
            }
        }
    }

    // Each page turn after a resize carries on from the page you were on.
    void resizeAfterTurningKeepsThePage()
    {
        QVERIFY(reader->goTo("ps 119"));
        reader->nextPage();
        reader->nextPage();
        const int first = versesHere().first();
        resize(900, 700);
        QVERIFY(versesHere().contains(first));
        resize(900, 1100);
        QVERIFY(versesHere().contains(first));
    }
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
