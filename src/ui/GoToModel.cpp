#include "GoToModel.h"

#include "Reader.h"
#include "Reference.h"

namespace {

// How a chapter or verse begins, to recognise it by: "The LORD is my shepherd…"
QString opening(const Book *book, int chapter, int verse)
{
    Reader *reader = Reader::instance();
    if (!reader || !reader->bible())
        return {};
    const auto verses
        = reader->bible()->verses(reader->currentTranslation(), *book, chapter, std::max(1, verse), std::max(1, verse));
    return verses.isEmpty() ? QString() : verses.first().text;
}

QVariantMap placeRow(const Book *book, int chapter, int verse, int endVerse = 0)
{
    Reference r;
    r.book = book;
    r.chapter = chapter;
    r.verse = verse;
    r.endVerse = endVerse;
    return {{"kind", "place"}, {"label", r.label()}, {"detail", opening(book, chapter, verse)},
            {"target", r.label()}};
}

QVariantMap bookRow(const Book *b)
{
    return {{"kind", "book"},
            {"label", b->name},
            {"detail", b->chapters == 1 ? QStringLiteral("1 chapter") : QStringLiteral("%1 chapters").arg(b->chapters)},
            {"target", b->name},
            {"chapters", b->chapters}};
}

} // namespace

GoToModel::GoToModel(QObject *parent)
    : QObject(parent)
{
    rebuild();
}

void GoToModel::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    rebuild();
}

void GoToModel::setSelected(int row)
{
    row = std::clamp(row, 0, std::max(0, int(m_rows.size()) - 1));
    if (row == m_selected)
        return;
    m_selected = row;
    emit selectedChanged();
}

void GoToModel::reset(const QString &text)
{
    m_text = text;
    rebuild();
}

void GoToModel::rebuild()
{
    m_rows.clear();
    m_note.clear();
    Reader *reader = Reader::instance();
    const Book *current = reader ? Books::byNumber(reader->bookNumber()) : nullptr;
    const QString t = m_text.trimmed();

    if (t.isEmpty()) {
        if (reader) {
            for (const QVariant &v : reader->historyEntries()) {
                QVariantMap e = v.toMap();
                m_rows.append(QVariantMap{{"kind", "recent"},
                                          {"label", e.value("label")},
                                          {"detail", e.value("forward").toBool() ? QStringLiteral("ahead")
                                                                                 : QStringLiteral("back")},
                                          {"index", e.value("index")},
                                          {"target", e.value("label")}});
            }
        }
        const bool recents = !m_rows.isEmpty();
        for (const Book &b : Books::all()) {
            QVariantMap row = bookRow(&b);
            if (recents && b.number == 1)
                row.insert("groupStart", true); // a rule between where you've been and the books
            m_rows.append(row);
        }
    } else {
        const QList<Reference> refs = Reference::candidates(t, current);
        const bool hasNumber = std::any_of(refs.cbegin(), refs.cend(), [](const Reference &r) { return r.chapter > 0; });
        if (hasNumber) {
            for (const Reference &r : refs)
                m_rows.append(placeRow(r.book, r.chapter, r.verse, r.endVerse));
            // "ps 2": Psalm 2, then the chapters that start with 2.
            if (refs.size() == 1 && !refs.first().verse) {
                const Reference &r = refs.first();
                const QString digits = QString::number(r.chapter);
                for (int c = 1; c <= r.book->chapters; ++c)
                    if (c != r.chapter && QString::number(c).startsWith(digits))
                        m_rows.append(placeRow(r.book, c, 0));
            }
        } else {
            QList<const Book *> books;
            for (const Reference &r : refs)
                books.append(r.book);
            if (books.size() == 1) {
                const Book *b = books.first();
                for (int c = 1; c <= b->chapters; ++c)
                    m_rows.append(placeRow(b, c, 0));
            } else {
                for (const Book *b : books)
                    m_rows.append(bookRow(b));
            }
        }
        if (m_rows.isEmpty())
            m_note = QStringLiteral("Nothing called “%1”").arg(t);
    }

    m_selected = 0;
    emit changed();
    emit selectedChanged();
}

bool GoToModel::activate(int row)
{
    if (row < 0)
        row = m_selected;
    if (row < 0 || row >= m_rows.size())
        return false;
    const QVariantMap r = m_rows.at(row).toMap();
    Reader *reader = Reader::instance();
    if (!reader)
        return false;
    const QString kind = r.value("kind").toString();
    if (kind == QLatin1String("recent")) {
        reader->goToHistory(r.value("index").toInt());
        return true;
    }
    if (kind == QLatin1String("book") && r.value("chapters").toInt() > 1) {
        setText(r.value("target").toString() + ' ');
        return false;
    }
    return reader->goTo(r.value("target").toString());
}

void GoToModel::complete()
{
    if (m_selected < 0 || m_selected >= m_rows.size())
        return;
    const QVariantMap r = m_rows.at(m_selected).toMap();
    const QString target = r.value("target").toString();
    setText(r.value("kind").toString() == QLatin1String("book") ? target + ' ' : target);
}
