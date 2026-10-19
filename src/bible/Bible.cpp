#include "Bible.h"

#include <QFileInfo>
#include <QUrl>

#include <sqlite3.h>

namespace {

// Binds and resets around one use of a cached statement.
class Use
{
public:
    explicit Use(sqlite3_stmt *s)
        : m_s(s)
    {
    }
    ~Use()
    {
        if (m_s) {
            sqlite3_reset(m_s);
            sqlite3_clear_bindings(m_s);
        }
    }
    explicit operator bool() const { return m_s != nullptr; }
    void bind(int i, const QString &v)
    {
        const QByteArray utf8 = v.toUtf8();
        sqlite3_bind_text(m_s, i, utf8.constData(), int(utf8.size()), SQLITE_TRANSIENT);
    }
    void bind(int i, int v) { sqlite3_bind_int(m_s, i, v); }
    bool step() { return sqlite3_step(m_s) == SQLITE_ROW; }
    void reset() { sqlite3_reset(m_s); }
    int integer(int col) const { return sqlite3_column_int(m_s, col); }
    QString text(int col) const
    {
        const auto *p = reinterpret_cast<const char *>(sqlite3_column_text(m_s, col));
        return p ? QString::fromUtf8(p, sqlite3_column_bytes(m_s, col)) : QString();
    }

private:
    sqlite3_stmt *m_s;
};

QStringList commaList(const QString &s)
{
    QStringList out;
    for (const QString &part : s.split(',', Qt::SkipEmptyParts))
        if (const QString t = part.trimmed(); !t.isEmpty())
            out.append(t);
    return out;
}

} // namespace

Bible::Bible(const QString &path)
    : m_path(path)
{
    if (!QFileInfo::exists(path)) {
        m_error = QStringLiteral("There's no Bible at %1.").arg(path);
        return;
    }
    // immutable=1: nothing writes the file while we read it, so SQLite can
    // skip locking and change detection altogether.
    const QByteArray uri = "file:" + QUrl::toPercentEncoding(path, "/") + "?immutable=1";
    const int rc = sqlite3_open_v2(uri.constData(), &m_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        m_error = m_db ? QString::fromUtf8(sqlite3_errmsg(m_db)) : QStringLiteral("out of memory");
        sqlite3_close(m_db);
        m_db = nullptr;
        return;
    }
    // A file that isn't the Bible opens fine; check for a table we need.
    if (!statement(QStringLiteral("SELECT 1 FROM kjv LIMIT 1"))) {
        m_error = QStringLiteral("%1 isn't a Bibliora database").arg(path);
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

Bible::~Bible()
{
    for (sqlite3_stmt *s : std::as_const(m_statements))
        sqlite3_finalize(s);
    if (m_db)
        sqlite3_close(m_db);
}

sqlite3_stmt *Bible::statement(const QString &sql)
{
    if (!m_db)
        return nullptr;
    if (auto it = m_statements.constFind(sql); it != m_statements.cend())
        return *it;
    sqlite3_stmt *s = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v3(m_db, utf8.constData(), int(utf8.size()), SQLITE_PREPARE_PERSISTENT, &s, nullptr)
        != SQLITE_OK) {
        m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
        return nullptr;
    }
    m_statements.insert(sql, s);
    return s;
}

QList<VerseMarkup> Bible::chapterMarkup(const Translation &t, const Book &book, int chapter)
{
    QList<VerseMarkup> out;
    Use q(statement(QStringLiteral("SELECT verse, formatted_text FROM %1 WHERE book = ?1 AND chapter = ?2 "
                                   "ORDER BY verse")
                        .arg(t.table())));
    if (!q)
        return out;
    q.bind(1, book.key);
    q.bind(2, chapter);
    while (q.step())
        out.append({q.integer(0), q.text(1)});
    return out;
}

int Bible::verseCount(const Translation &t, const Book &book, int chapter)
{
    Use q(statement(QStringLiteral("SELECT MAX(verse) FROM %1 WHERE book = ?1 AND chapter = ?2").arg(t.table())));
    if (!q)
        return 0;
    q.bind(1, book.key);
    q.bind(2, chapter);
    return q.step() ? q.integer(0) : 0;
}

QList<VerseText> Bible::verses(const Translation &t, const Book &book, int chapter, int from, int to)
{
    QList<VerseText> out;
    Use q(statement(QStringLiteral("SELECT verse, text FROM %1 WHERE book = ?1 AND chapter = ?2 "
                                   "AND verse BETWEEN ?3 AND ?4 ORDER BY verse")
                        .arg(t.table())));
    if (!q)
        return out;
    q.bind(1, book.key);
    q.bind(2, chapter);
    q.bind(3, std::max(1, from));
    q.bind(4, to > 0 ? to : 1000);
    while (q.step())
        out.append({&book, chapter, q.integer(0), q.text(1).trimmed()});
    return out;
}

QList<SearchHit> Bible::search(const Translation &t, const QString &ftsQuery, int limit)
{
    QList<SearchHit> out;
    if (ftsQuery.isEmpty())
        return out;
    // The FTS rowids follow the verses' canonical order.
    Use q(statement(QStringLiteral("SELECT book, chapter, verse, highlight(%1_fts, 4, char(1), char(2)) "
                                   "FROM %1_fts WHERE text MATCH ?1 ORDER BY rowid LIMIT ?2")
                        .arg(t.table())));
    if (!q)
        return out;
    q.bind(1, ftsQuery);
    q.bind(2, limit < 0 ? 1000000 : limit);
    while (q.step()) {
        const Book *book = Books::byKey(q.text(0));
        if (book)
            out.append({book, q.integer(1), q.integer(2), q.text(3).trimmed()});
    }
    return out;
}

int Bible::searchCount(const Translation &t, const QString &ftsQuery)
{
    if (ftsQuery.isEmpty())
        return 0;
    Use q(statement(QStringLiteral("SELECT count(*) FROM %1_fts WHERE text MATCH ?1").arg(t.table())));
    if (!q)
        return 0;
    q.bind(1, ftsQuery);
    return q.step() ? q.integer(0) : 0;
}

DictionaryEntry Bible::define(const QString &word)
{
    const QString w = word.trimmed();
    QStringList tries{w};
    for (const QString &suffix : {QStringLiteral("’s"), QStringLiteral("'s")})
        if (w.endsWith(suffix, Qt::CaseInsensitive))
            tries.append(w.chopped(suffix.size()));
    const QStringList base = tries;
    for (const QString &t : base) {
        if (t.isEmpty())
            continue;
        tries.append(t.left(1).toUpper() + t.mid(1).toLower());
        tries.append(t.toLower());
    }
    tries.removeDuplicates();

    Use q(statement(QStringLiteral("SELECT word, pronunciation, synonyms, definition_1, example_1, definition_2, "
                                   "example_2, definition_3, example_3, definition_4, example_4, definition_5, "
                                   "example_5 FROM words_grok WHERE word = ?1")));
    if (!q)
        return {};
    for (const QString &t : std::as_const(tries)) {
        q.reset();
        q.bind(1, t);
        if (!q.step())
            continue;
        DictionaryEntry e;
        e.word = q.text(0);
        e.pronunciations = commaList(q.text(1));
        e.synonyms = commaList(q.text(2));
        for (int i = 0; i < 5; ++i) {
            const QString def = q.text(3 + 2 * i).trimmed();
            if (!def.isEmpty())
                e.senses.append({def, q.text(4 + 2 * i).trimmed()});
        }
        if (e.isValid())
            return e;
    }
    return {};
}

QStringList Bible::dictionaryWords(const QString &prefix, int limit)
{
    QStringList out;
    QString pattern = prefix.trimmed();
    if (pattern.isEmpty())
        return out;
    pattern.replace('\\', QStringLiteral("\\\\")).replace('%', QStringLiteral("\\%")).replace('_', QStringLiteral("\\_"));
    Use q(statement(QStringLiteral("SELECT word FROM words_grok WHERE word LIKE ?1 ESCAPE '\\' "
                                   "ORDER BY lower(word), word LIMIT ?2")));
    if (!q)
        return out;
    q.bind(1, pattern + '%');
    q.bind(2, limit);
    while (q.step())
        out.append(q.text(0));
    return out;
}
