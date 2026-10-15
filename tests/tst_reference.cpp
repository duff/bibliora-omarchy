#include "Reference.h"

#include <QTest>

class TestReference : public QObject
{
    Q_OBJECT

private:
    static QString label(const QString &text, const Book *current = nullptr)
    {
        const auto r = Reference::parse(text, current);
        return r ? r->label() : QStringLiteral("(none)");
    }

private slots:
    void books()
    {
        QCOMPARE(Books::count(), 66);
        QCOMPARE(Books::byNumber(43)->name, QStringLiteral("John"));
        QCOMPARE(Books::byKey("song_of_solomon")->name, QStringLiteral("Song of Songs"));
        QCOMPARE(Books::byKey("galations")->name, QStringLiteral("Galatians")); // iOS's old spelling
        QCOMPARE(Books::normalize("I John"), QStringLiteral("1john"));
        QCOMPARE(Books::normalize("Second Kings"), QStringLiteral("2kings"));
        QCOMPARE(Books::normalize("1 Cor."), QStringLiteral("1cor"));
    }

    void matching()
    {
        auto names = [](const QString &text) {
            QStringList out;
            for (const Book *b : Books::match(text))
                out.append(b->name);
            return out;
        };
        QCOMPARE(names("jo"), QStringList({"Joshua", "Job", "Joel", "Jonah", "John"}));
        QCOMPARE(names("jn"), QStringList({"John"}));
        QCOMPARE(names("phil"), QStringList({"Philippians"}));
        QCOMPARE(names("philem"), QStringList({"Philemon"}));
        QCOMPARE(names("song of solomon"), QStringList({"Song of Songs"}));
        QCOMPARE(names("1 j"), QStringList({"1 John"}));
        QCOMPARE(names("ps"), QStringList({"Psalms"}));
        QCOMPARE(names("psalm"), QStringList({"Psalms"}));
        QCOMPARE(names("isa"), QStringList({"Isaiah"}));
        QCOMPARE(names("xyz"), QStringList());
    }

    void parsing()
    {
        QCOMPARE(label("john 3:16"), QStringLiteral("John 3:16"));
        QCOMPARE(label("jn 3.16"), QStringLiteral("John 3:16"));
        QCOMPARE(label("rom 8 28"), QStringLiteral("Romans 8:28"));
        QCOMPARE(label("1 cor 13"), QStringLiteral("1 Corinthians 13"));
        QCOMPARE(label("i john 1:9"), QStringLiteral("1 John 1:9"));
        QCOMPARE(label("ps 23"), QStringLiteral("Psalms 23"));
        QCOMPARE(label("Genesis 1:1-3"), QStringLiteral("Genesis 1:1-3"));
        QCOMPARE(label("gen 1:1–3"), QStringLiteral("Genesis 1:1-3"));
        QCOMPARE(label("gen 1-2"), QStringLiteral("Genesis 1"));
        QCOMPARE(label("song of songs 2:1"), QStringLiteral("Song of Songs 2:1"));
        QCOMPARE(label("jude 3"), QStringLiteral("Jude 1:3"));
        QCOMPARE(label("jude"), QStringLiteral("Jude"));
        QCOMPARE(label("john 3:"), QStringLiteral("John 3")); // half typed
        QCOMPARE(label("gen 51"), QStringLiteral("(none)"));  // past the end
        QCOMPARE(label("hello"), QStringLiteral("(none)"));
        QCOMPARE(label(""), QStringLiteral("(none)"));
    }

    void withoutABook()
    {
        const Book *john = Books::byNumber(43);
        QCOMPARE(label("3:16", john), QStringLiteral("John 3:16"));
        QCOMPARE(label("21", john), QStringLiteral("John 21"));
        QCOMPARE(label("3:16"), QStringLiteral("(none)"));
    }

    void candidates()
    {
        QStringList labels;
        for (const Reference &r : Reference::candidates("jo 3"))
            labels.append(r.label());
        QCOMPARE(labels, QStringList({"Joshua 3", "Job 3", "Joel 3", "Jonah 3", "John 3"}));
    }

    void links()
    {
        QCOMPARE(label("bibliora://43.3.16"), QStringLiteral("John 3:16"));
        QCOMPARE(label("bibliora://19.23"), QStringLiteral("Psalms 23"));
        QCOMPARE(label("bibliora://1"), QStringLiteral("Genesis 1"));
        QCOMPARE(label("bibliora://43.99"), QStringLiteral("John 21")); // clamped, as on iOS
        QCOMPARE(label("bibliora://43.5?x=1#y"), QStringLiteral("John 5"));
        QCOMPARE(label("bibliora:///43.1"), QStringLiteral("John 1"));
        QCOMPARE(label("bibliora://67.1"), QStringLiteral("(none)"));
        QCOMPARE(label("bibliora://abc"), QStringLiteral("(none)"));
        QCOMPARE(Reference::parse("jn 3:16")->link(), QStringLiteral("bibliora://43.3.16"));
    }
};

QTEST_GUILESS_MAIN(TestReference)
#include "tst_reference.moc"
