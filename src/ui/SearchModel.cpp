#include "SearchModel.h"

#include "Paths.h"
#include "Reader.h"
#include "Reference.h"
#include "SearchQuery.h"
#include "Theme.h"

#include <QLocale>
#include <QSettings>

#include <utility>

namespace {

// Lives on the search thread with a connection of its own.
class Worker : public QObject
{
    Q_OBJECT
public:
    void run(quint64 generation, const QString &translationKey, const QString &fts)
    {
        if (!m_bible)
            m_bible = std::make_unique<Bible>(Paths::biblePath());
        const Translation *t = Translations::find(translationKey);
        QList<SearchHit> hits;
        int total = 0;
        if (t && m_bible->isOpen()) {
            hits = m_bible->search(*t, fts);
            total = int(hits.size());
        }
        emit done(generation, hits, total);
    }

signals:
    void done(quint64 generation, const QList<SearchHit> &hits, int total);

private:
    std::unique_ptr<Bible> m_bible;
};

QString escaped(const QString &s)
{
    QString out;
    out.reserve(s.size() + 16);
    for (const QChar c : s) {
        if (c == '<')
            out += QLatin1String("&lt;");
        else if (c == '>')
            out += QLatin1String("&gt;");
        else if (c == '&')
            out += QLatin1String("&amp;");
        else
            out += c;
    }
    return out;
}

// The verse as StyledText, its matched words in the theme's accent.
QString marked(const QString &text)
{
    const Theme *theme = Theme::instance();
    const QString color = theme ? theme->accent().name() : QStringLiteral("#e68e0d");
    QString out = escaped(text);
    out.replace(QChar(1), QStringLiteral("<font color=\"%1\">").arg(color));
    out.replace(QChar(2), QStringLiteral("</font>"));
    return out;
}

} // namespace

SearchModel::SearchModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qRegisterMetaType<QList<SearchHit>>();
    auto *worker = new Worker;
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &SearchModel::run, worker, &Worker::run);
    connect(worker, &Worker::done, this, &SearchModel::finished);
    m_thread.setObjectName(QStringLiteral("search"));
    m_thread.start();

    m_history.restore(QSettings().value("search/history").toStringList());
    if (Reader *reader = Reader::instance())
        connect(reader, &Reader::translationChanged, this, &SearchModel::rebuild);
    if (Theme *theme = Theme::instance())
        connect(theme, &Theme::changed, this, [this] {
            if (!m_rows.isEmpty())
                emit dataChanged(index(0), index(int(m_rows.size()) - 1), {TextRole});
        });
    rebuild();
}

SearchModel::~SearchModel()
{
    m_thread.quit();
    m_thread.wait();
}

int SearchModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_rows.size()); }

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case KindRole:
        return r.kind == Row::Verse ? QStringLiteral("verse") : r.kind == Row::GoTo ? QStringLiteral("goto")
                                                                                    : QStringLiteral("past");
    case ReferenceRole:
        return r.reference;
    case TextRole:
        return r.kind == Row::Verse ? marked(r.text) : escaped(r.text);
    case BookRole:
        return r.book;
    case ChapterRole:
        return r.chapter;
    case VerseRole:
        return r.verse;
    }
    return {};
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {{KindRole, "kind"},   {ReferenceRole, "reference"}, {TextRole, "text"},
            {BookRole, "book"},   {ChapterRole, "chapter"},     {VerseRole, "verse"}};
}

void SearchModel::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query;
    emit queryChanged();
    rebuild();
}

void SearchModel::setSelected(int row)
{
    row = std::clamp(row, 0, std::max(0, int(m_rows.size()) - 1));
    m_picked = true;
    if (row == m_selected)
        return;
    m_selected = row;
    emit selectedChanged();
}

QString SearchModel::summary() const
{
    const QString tr = Reader::instance() ? Reader::instance()->translationAbbrev() : QString();
    switch (m_status) {
    case Status::History:
        return m_history.isExamples() ? QStringLiteral("Example searches") : QStringLiteral("Recent searches");
    case Status::NothingToSearch:
        return QStringLiteral("Type a word to search the %1").arg(tr);
    case Status::Searching:
        if (m_total == 0)
            return QStringLiteral("Searching the %1…").arg(tr);
        break;
    case Status::Done:
        break;
    }
    if (m_total == 0)
        return QStringLiteral("No verses in the %1").arg(tr);
    return QStringLiteral("%1 %2 in the %3")
        .arg(QLocale().toString(m_total), m_total == 1 ? QStringLiteral("verse") : QStringLiteral("verses"), tr);
}

void SearchModel::refresh() { rebuild(); }

void SearchModel::rebuild()
{
    ++m_generation; // anything still running is stale
    m_waiting = false;
    const QString q = m_query.trimmed();
    if (q.isEmpty()) {
        QList<Row> past;
        for (const QString &item : m_history.items())
            past.append({Row::Past, 0, 0, 0, item, item});
        setRows(Status::History, past, 0);
        return;
    }

    // "gen 9", "john 3:16": somewhere to go, first.
    QList<Row> head;
    const bool hasDigit = std::any_of(q.cbegin(), q.cend(), [](QChar c) { return c.isDigit(); });
    if (hasDigit) {
        const Book *current = nullptr;
        if (Reader *r = Reader::instance())
            current = Books::byNumber(r->bookNumber());
        if (const auto ref = Reference::parse(q, current); ref && ref->chapter > 0)
            head.append({Row::GoTo, ref->book->number, ref->chapter, ref->verse, ref->label(),
                         QStringLiteral("Go to %1").arg(ref->label())});
    }

    const QString fts = SearchQuery::build(m_query);
    if (fts.isEmpty() || !Reader::instance()) {
        setRows(Status::NothingToSearch, head, 0);
        return;
    }
    // Keep showing the last results until the new ones come, so the list
    // doesn't flicker as you type.
    QList<Row> rows = head;
    for (const Row &r : std::as_const(m_rows))
        if (r.kind == Row::Verse)
            rows.append(r);
    setRows(Status::Searching, rows, m_status == Status::Done || m_status == Status::Searching ? m_total : 0);
    emit run(m_generation, Reader::instance()->translationKey(), fts);
}

void SearchModel::finished(quint64 generation, const QList<SearchHit> &hits, int total)
{
    if (generation != m_generation || m_status != Status::Searching)
        return;
    QList<Row> rows;
    for (const Row &r : std::as_const(m_rows))
        if (r.kind == Row::GoTo)
            rows.append(r);
    rows.reserve(rows.size() + hits.size());
    for (const SearchHit &h : hits)
        rows.append({Row::Verse, h.book->number, h.chapter, h.verse,
                     QStringLiteral("%1 %2:%3").arg(h.book->name).arg(h.chapter).arg(h.verse), h.text});
    setRows(Status::Done, rows, total);
    if (std::exchange(m_waiting, false))
        activate(0);
}

// The one place the rows change: each status shows its own rows, and the
// selection goes back to the first.
void SearchModel::setRows(Status status, const QList<Row> &rows, int total)
{
    beginResetModel();
    m_status = status;
    m_rows = rows;
    m_total = total;
    endResetModel();
    m_selected = 0;
    m_picked = false;
    emit selectedChanged();
    emit resultsChanged();
}

void SearchModel::activate(int row)
{
    if (row >= 0)
        setSelected(row);
    // Enter as you type: the rows are still the last query's, so wait for
    // this one's. A row you picked by hand opens now.
    if (m_status == Status::Searching && !m_picked) {
        m_waiting = true;
        return;
    }
    if (m_selected >= 0 && m_selected < m_rows.size())
        open(m_rows.at(m_selected));
}

void SearchModel::open(const Row &r)
{
    Reader *reader = Reader::instance();
    switch (r.kind) {
    case Row::Past:
        setQuery(r.text);
        return;
    case Row::GoTo:
        remember();
        if (reader)
            reader->goTo(r.reference);
        break;
    case Row::Verse:
        remember();
        if (reader)
            reader->openVerse(r.book, r.chapter, r.verse);
        break;
    }
    emit opened();
}

void SearchModel::closed()
{
    m_waiting = false;
    remember();
}

void SearchModel::remember()
{
    const bool wasExamples = m_history.isExamples();
    m_history.add(m_query);
    saveHistory();
    if (wasExamples != m_history.isExamples())
        emit resultsChanged();
}

void SearchModel::clearHistory()
{
    m_history.clear();
    saveHistory();
    rebuild();
}

void SearchModel::saveHistory()
{
    QSettings().setValue("search/history", m_history.isExamples() ? QStringList() : m_history.items());
}

#include "SearchModel.moc"
