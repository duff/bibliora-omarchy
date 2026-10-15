#pragma once

#include "Books.h"

#include <QList>
#include <QString>

#include <optional>

// A place in the Bible as typed or linked: a book, maybe a chapter, maybe a
// verse or a verse range within that chapter. Chapter 0 means just the book.
struct Reference
{
    const Book *book = nullptr;
    int chapter = 0;
    int verse = 0;
    int endVerse = 0; // 0 unless a range like 3:16-18

    bool isValid() const { return book != nullptr; }
    QString label() const; // "John", "John 3", "John 3:16", "John 3:16-18"

    // "jn 3:16", "1 cor 13", "ps 23 4", "song of songs 2.1", "rev 22:20-21",
    // or a bibliora:// link. Without a book name, `current` stands in, so
    // "3:16" in John is John 3:16. A chapter past the end of the book is no
    // reference at all. For an ambiguous name ("jo 3") the first book in
    // canonical order wins; candidates() lists them all.
    static std::optional<Reference> parse(const QString &text, const Book *current = nullptr);

    // Every reading of `text`, one per book its name could mean.
    static QList<Reference> candidates(const QString &text, const Book *current = nullptr);

    // bibliora://{book}[.{chapter}[.{verse}]], book 1–66 in canonical order,
    // as the iOS app and Reading Plan use them. The chapter defaults to 1 and
    // is clamped to the book, as on iOS.
    static std::optional<Reference> fromLink(const QString &link);
    QString link() const;
};
