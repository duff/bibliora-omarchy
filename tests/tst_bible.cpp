// Against the real bibles.sqlite3, where the app finds it; skipped without it.
// The copy format is the iOS app's SelectionTests.swift, case for case.
#include "Bible.h"
#include "Markup.h"
#include "Passage.h"
#include "Paths.h"
#include "SearchQuery.h"
#include "Typesetter.h"

#include <QFileInfo>
#include <QTest>

class TestBible : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Bible> m_bible;
    const Translation &nkjv() { return *Translations::find("NKJV"); }

private slots:
    void initTestCase()
    {
        if (!QFileInfo::exists(Paths::biblePath()))
            QSKIP("No bibles.sqlite3 here; copy it to ~/.local/share/bibliora/ to run these.");
        m_bible = std::make_unique<Bible>(Paths::biblePath());
        QVERIFY2(m_bible->isOpen(), qPrintable(m_bible->error()));
    }

    void everyTranslationIsThere()
    {
        for (const Translation &t : Translations::all()) {
            QVERIFY2(!m_bible->chapterMarkup(t, *Books::byNumber(1), 1).isEmpty(), qPrintable(t.key));
            QCOMPARE(m_bible->verseCount(t, *Books::byKey("revelation"), 22), 21);
        }
        QCOMPARE(m_bible->verseCount(*Translations::find("LSB"), *Books::byKey("third_john"), 1), 15);
        QCOMPARE(m_bible->verseCount(*Translations::find("KJV"), *Books::byKey("third_john"), 1), 14);
    }

    void copyOneVerse()
    {
        QCOMPARE(Passage::clipboardText(*m_bible, nkjv(), {{1, 1, 1}}),
                 QStringLiteral("In the beginning God created the heavens and the earth.\n-- Genesis 1:1, NKJV"));
    }

    void copyAcrossBooks()
    {
        QCOMPARE(Passage::clipboardText(*m_bible, nkjv(), {{2, 1, 1}, {1, 50, 26}}),
                 QStringLiteral("So Joseph died, being one hundred and ten years old; and they embalmed him, and he "
                                "was put in a coffin in Egypt. Now these are the names of the children of Israel who "
                                "came to Egypt; each man and his household came with Jacob:\n"
                                "-- Genesis 50:26 - Exodus 1:1, NKJV"));
    }

    void copyAcrossChapters()
    {
        QCOMPARE(Passage::clipboardText(*m_bible, nkjv(), {{1, 1, 31}, {1, 2, 1}}),
                 QStringLiteral("Then God saw everything that He had made, and indeed it was very good. So the "
                                "evening and the morning were the sixth day. Thus the heavens and the earth, and all "
                                "the host of them, were finished.\n-- Genesis 1:31 - 2:1, NKJV"));
    }

    void copyWithinAChapter()
    {
        QCOMPARE(Passage::clipboardText(*m_bible, nkjv(), {{1, 1, 2}, {1, 1, 1}, {1, 1, 3}}),
                 QStringLiteral("In the beginning God created the heavens and the earth. The earth was without "
                                "form, and void; and darkness was on the face of the deep. And the Spirit of God was "
                                "hovering over the face of the waters. Then God said, “Let there be light”; "
                                "and there was light.\n-- Genesis 1:1-3, NKJV"));
    }

    void copyVersesApart()
    {
        QCOMPARE(Passage::clipboardText(*m_bible, nkjv(), {{1, 1, 1}, {1, 1, 3}}),
                 QStringLiteral("In the beginning God created the heavens and the earth.\n-- Genesis 1:1, NKJV\n\n"
                                "Then God said, “Let there be light”; and there was light.\n"
                                "-- Genesis 1:3, NKJV"));
    }

    void search()
    {
        const auto hits = m_bible->search(nkjv(), SearchQuery::build("bosom abraham "));
        QCOMPARE(hits.size(), 2);
        QCOMPARE(hits.first().book->name, QStringLiteral("Luke"));
        QCOMPARE(hits.first().verse, 22);
        QVERIFY(hits.first().text.contains(QStringLiteral("\x01" "bosom" "\x02")));
        QCOMPARE(m_bible->searchCount(nkjv(), SearchQuery::build("bosom abraham ")), 2);
        // In Bible order.
        const auto love = m_bible->search(nkjv(), SearchQuery::build("love "), 50);
        for (int i = 1; i < love.size(); ++i)
            QVERIFY(std::tie(love[i - 1].book->number, love[i - 1].chapter, love[i - 1].verse)
                    < std::tie(love[i].book->number, love[i].chapter, love[i].verse));
    }

    void dictionary()
    {
        const DictionaryEntry e = m_bible->define("firmament");
        QVERIFY(e.isValid());
        QCOMPARE(e.word, QStringLiteral("firmament"));
        QVERIFY(!e.pronunciations.isEmpty());
        QVERIFY(m_bible->define("Firmament’s").isValid());
        QVERIFY(!m_bible->define("zzzzqx").isValid());
    }

    // Every chapter of every translation sets into pages without losing a verse.
    void everyChapterSets()
    {
        TypeStyle style;
        style.font.setPixelSize(20);
        style.lineHeight = 32;
        style.pageWidth = 560;
        style.pageHeight = 32 * 24;
        const int linesPerPage = 24;
        for (const Translation &t : {*Translations::find("NKJV"), *Translations::find("KJV")}) {
            for (const Book &b : Books::all()) {
                for (int c = 1; c <= b.chapters; ++c) {
                    const auto rows = m_bible->chapterMarkup(t, b, c);
                    QVERIFY(!rows.isEmpty());
                    const auto layout = Typesetter::set(Markup::parse(rows, true), style);
                    QVERIFY(layout->pageCount() >= 1);
                    for (const VerseMarkup &v : rows) {
                        const QString where = QStringLiteral("%1 %2 %3:%4").arg(t.key, b.name).arg(c).arg(v.verse);
                        QVERIFY2(layout->verseStartPage.contains(v.verse), qPrintable(where));
                    }
                    for (const auto &page : layout->pages) {
                        int lines = 0;
                        for (const auto &piece : page.pieces)
                            lines += piece.lineCount;
                        QVERIFY(lines <= linesPerPage);
                    }
                }
            }
        }
    }
};

QTEST_MAIN(TestBible)
#include "tst_bible.moc"
