#pragma once

#include "Books.h"
#include "Translations.h"

#include <QHash>
#include <QList>
#include <QString>

#include <memory>

struct sqlite3;
struct sqlite3_stmt;

// One verse's display markup, as the database has it (see Markup.h).
struct VerseMarkup
{
    int verse = 0;
    QString markup;
};

// One verse's plain text, the `text` column: what search indexes and what a
// copied passage holds.
struct VerseText
{
    const Book *book = nullptr;
    int chapter = 0;
    int verse = 0;
    QString text;
};

struct SearchHit
{
    const Book *book = nullptr;
    int chapter = 0;
    int verse = 0;
    QString text; // with \x01 before and \x02 after each matched word
};

struct DictionaryEntry
{
    QString word;
    QStringList pronunciations;
    QStringList synonyms;
    QList<std::pair<QString, QString>> senses; // definition, example
    bool isValid() const { return !word.isEmpty() && !senses.isEmpty(); }
};

// Read-only access to bibles.sqlite3, the iOS app's database. Each thread
// opens its own Bible; statements are prepared once and reused, so a chapter
// is one indexed lookup.
class Bible
{
public:
    explicit Bible(const QString &path);
    ~Bible();
    Bible(const Bible &) = delete;
    Bible &operator=(const Bible &) = delete;

    bool isOpen() const { return m_db != nullptr; }
    QString error() const { return m_error; }
    QString path() const { return m_path; }

    QList<VerseMarkup> chapterMarkup(const Translation &t, const Book &book, int chapter);
    int verseCount(const Translation &t, const Book &book, int chapter);

    // Verses from..to (inclusive) of one chapter; to = 0 means to the end.
    QList<VerseText> verses(const Translation &t, const Book &book, int chapter, int from = 1, int to = 0);

    // Full-text search in one translation, canonical order. `ftsQuery` is
    // already in FTS5 syntax (SearchQuery::build). limit < 0: everything.
    QList<SearchHit> search(const Translation &t, const QString &ftsQuery, int limit = -1);
    int searchCount(const Translation &t, const QString &ftsQuery);

    // The Grok dictionary, as on iOS: the exact word, then without 's, then
    // in other cases ("LORD" → "Lord" → "lord").
    DictionaryEntry define(const QString &word);
    // The dictionary's words that start with `prefix`, whatever its case, in
    // alphabetical order.
    QStringList dictionaryWords(const QString &prefix, int limit);

private:
    sqlite3_stmt *statement(const QString &sql);

    sqlite3 *m_db = nullptr;
    QString m_path;
    QString m_error;
    QHash<QString, sqlite3_stmt *> m_statements;
};
