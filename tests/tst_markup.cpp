// The database's display markup into blocks, from real rows of bibles.sqlite3.
#include "Markup.h"

#include <QTest>

namespace {

QString texts(const Markup::Chapter &c)
{
    QStringList out;
    for (const Markup::Block &b : c.blocks) {
        QString kind = b.kind == Markup::Block::Prose      ? "P"
                       : b.kind == Markup::Block::Poetry   ? "L"
                       : b.kind == Markup::Block::Title    ? "T"
                                                           : "C";
        out.append((b.gapBefore ? "~" : "") + kind + (b.indent ? ">" : "") + ' ' + b.text);
    }
    return out.join('\n');
}

} // namespace

class TestMarkup : public QObject
{
    Q_OBJECT

private slots:
    void prose()
    {
        const auto c = Markup::parse(
            {{1, "<heading>The History of Creation</heading></paragraph><citation id=\"1\"><v>1</v>In the beginning God "
                 "created the heavens and the earth. </citation>"},
             {2, "<citation id=\"2\"><v>2</v>The earth was without form. </citation>"},
             {3, "</paragraph><citation id=\"3\"><v>3</v>Then God said, “Let there be light”; and there was "
                 "light. </citation>"}},
            false);
        QCOMPARE(texts(c), QStringLiteral("P> In the beginning God created the heavens and the earth. The earth was "
                                          "without form.\nP> Then God said, “Let there be light”; and there "
                                          "was light."));
        // Which verse each part belongs to.
        const Markup::Block &b = c.blocks.first();
        QCOMPARE(b.verses.size(), 2);
        QCOMPARE(b.verseAt(0), 1);
        QCOMPARE(b.verseAt(int(b.text.indexOf("The earth"))), 2);
    }

    void verseNumbers()
    {
        const auto c = Markup::parse({{16, "<citation id=\"26137\"><v>16</v>For God so loved the world </citation>"}},
                                     true);
        const Markup::Block &b = c.blocks.first();
        QCOMPARE(b.text, QStringLiteral("16 For God so loved the world"));
        QCOMPARE(b.spans.size(), 1);
        QCOMPARE(b.spans.first().style, Markup::Span::VerseNumber);
        QCOMPARE(b.spans.first().length, 3);
        QCOMPARE(b.verseAt(0), 16);
    }

    void psalm()
    {
        const auto c = Markup::parse(
            {{1, "<heading>The <small-caps>Lord</small-caps> the Shepherd of His People</heading><psalm-title>A Psalm "
                 "of David.</psalm-title></br><citation id=\"14237\"><v>1</v>The <small-caps>Lord</small-caps> is my "
                 "shepherd;</br>I shall not want.</br> </citation>"},
             {2, "</br><citation id=\"14238\"><v>2</v>He makes me to lie down in green pastures;</br>He leads me "
                 "beside the still waters. </citation>"}},
            false);
        QCOMPARE(texts(c), QStringLiteral("T A Psalm of David.\n~L The Lord is my shepherd;\nL I shall not want.\n"
                                          "~L He makes me to lie down in green pastures;\nL He leads me beside the "
                                          "still waters."));
        const Markup::Block &lord = c.blocks.at(1);
        QCOMPARE(lord.spans.size(), 1);
        QCOMPARE(lord.text.mid(lord.spans.first().start, lord.spans.first().length), QStringLiteral("Lord"));
    }

    void speakersAndAcrostics()
    {
        const auto c = Markup::parse(
            {{1, "<heading>Solomon’s Love</heading></paragraph><citation id=\"17539\"><v>1</v>The song of songs, "
                 "which is Solomon’s. </citation>"},
             {2, "<heading>The Banquet</heading></br></br><speaker-title>The Shulamite</speaker-title><citation "
                 "id=\"17540\"><v>2</v>Let him kiss me</br>For your love is better than wine.</br> </citation>"},
             {9, "</br></br><psalm-title>ב BETH. </psalm-title></br><citation id=\"2\"><v>9</v>How can a young "
                 "man cleanse his way? </citation>"}},
            false);
        QCOMPARE(texts(c), QStringLiteral("P> The song of songs, which is Solomon’s.\n~T The Shulamite\n"
                                          "L Let him kiss me\nL For your love is better than wine.\n"
                                          "~T ב BETH.\n~L How can a young man cleanse his way?"));
    }

    void centered()
    {
        const auto c = Markup::parse(
            {{36, "</paragraph><citation id=\"2330\"><v>36</v>“You shall also make a plate of pure gold, like "
                  "the engraving of a signet:</br><centered>HOLINESS TO THE LORD.</centered> </citation>"},
             {37, "<citation id=\"2331\"><v>37</v>And you shall put it on a blue cord. </citation>"}},
            false);
        QCOMPARE(texts(c), QStringLiteral("P> “You shall also make a plate of pure gold, like the engraving of "
                                          "a signet:\nC HOLINESS TO THE LORD.\nL And you shall put it on a blue "
                                          "cord."));
    }

    void headingInsideAVerse()
    {
        const auto c = Markup::parse(
            {{28, "<citation id=\"1\"><v>28</v>the <small-caps>Lord</small-caps>.<heading>Ezekiel Sent to Rebellious "
                  "Israel</heading></paragraph>So when I saw it, I fell on my face. </citation>"}},
            false);
        QCOMPARE(texts(c), QStringLiteral("P the Lord.\nP> So when I saw it, I fell on my face."));
        QCOMPARE(c.blocks.at(1).verseAt(0), 28);
    }
};

QTEST_GUILESS_MAIN(TestMarkup)
#include "tst_markup.moc"
