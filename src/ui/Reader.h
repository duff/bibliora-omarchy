#pragma once

#include "Bible.h"
#include "History.h"
#include "Passage.h"
#include "Typesetter.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QTimer>
#include <QVariantAnimation>

#include <memory>

// Where you are and what's on the page: the Bible, the current chapter set
// into pages, history, the selection, and how the text looks. QML reads it as
// the Reader singleton; PageItem draws Reader's current page.
//
// Pages work as on iOS: a chapter starts on a page of its own, and turning
// past its last page goes on into the next chapter (and the next book).
// Everything that moves you somewhere new (go to, search, back/forward) goes
// through show(), which sets book, chapter and page together.
class Reader : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString problem READ problem CONSTANT)

    Q_PROPERTY(QString bookName READ bookName NOTIFY placeChanged)
    Q_PROPERTY(int bookNumber READ bookNumber NOTIFY placeChanged)
    Q_PROPERTY(int chapter READ chapter NOTIFY placeChanged)
    Q_PROPERTY(int chapterCount READ chapterCount NOTIFY placeChanged)
    Q_PROPERTY(int page READ page NOTIFY placeChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY placeChanged)
    Q_PROPERTY(QString placeLabel READ placeLabel NOTIFY placeChanged)

    Q_PROPERTY(QString translation READ translationAbbrev NOTIFY translationChanged)
    Q_PROPERTY(QString translationKey READ translationKey NOTIFY translationChanged)
    Q_PROPERTY(QVariantList translations READ translations CONSTANT)

    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)

    Q_PROPERTY(bool verseNumbers READ verseNumbers WRITE setVerseNumbers NOTIFY styleChanged)
    Q_PROPERTY(QString fontName READ fontName NOTIFY styleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY styleChanged)
    Q_PROPERTY(int textSize READ textSize NOTIFY styleChanged)
    Q_PROPERTY(qreal pageWidth READ pageWidth NOTIFY layoutChanged)
    Q_PROPERTY(qreal pageHeight READ pageHeight NOTIFY layoutChanged)
    Q_PROPERTY(qreal lineHeight READ lineHeight NOTIFY layoutChanged)

    Q_PROPERTY(bool selecting READ selecting NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionLabel READ selectionLabel NOTIFY selectionChanged)

public:
    explicit Reader(QObject *parent);
    ~Reader() override;
    static Reader *create(QQmlEngine *, QJSEngine *);
    static Reader *instance();

    bool ready() const { return m_bible && m_bible->isOpen(); }
    QString problem() const { return m_problem; }
    Bible *bible() const { return m_bible.get(); }

    QString bookName() const;
    int bookNumber() const;
    int chapter() const { return m_chapter; }
    int chapterCount() const;
    int page() const { return m_page; }
    int pageCount() const;
    QString placeLabel() const;
    Place currentPlace() const;

    const Translation &currentTranslation() const { return *m_translation; }
    QString translationAbbrev() const { return m_translation->abbrev; }
    QString translationKey() const { return m_translation->key; }
    QVariantList translations() const;

    bool canGoBack() const { return m_history.hasBack(); }
    bool canGoForward() const { return m_history.hasForward(); }

    bool verseNumbers() const { return m_verseNumbers; }
    void setVerseNumbers(bool on);
    QString fontName() const;
    QString fontFamily() const;
    int textSize() const;
    qreal pageWidth() const { return m_style.pageWidth; }
    qreal pageHeight() const { return m_pageHeight; }
    qreal lineHeight() const { return m_style.lineHeight; }

    bool selecting() const { return m_selecting; }
    int selectedCount() const { return int(m_selection.size()); }
    QString selectionLabel() const;

    // For PageItem.
    std::shared_ptr<ChapterLayout> layout() const { return m_layout; }
    QSet<int> selectedHere() const;
    int cursorHere() const;
    int flashHere() const;
    qreal flashStrength() const { return m_flashStrength; }

    // The window's room for a page; the page is narrower when the window is
    // wide, so lines stay a comfortable length.
    Q_INVOKABLE void setViewport(qreal width, qreal height);

    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void firstPage();
    Q_INVOKABLE void lastPage();
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void nextBook();
    Q_INVOKABLE void previousBook();

    // "jn 3:16", "ps 23", a bibliora:// link. False if it isn't a reference.
    Q_INVOKABLE bool goTo(const QString &reference);
    Q_INVOKABLE void openChapter(int book, int chapter);
    Q_INVOKABLE void openVerse(int book, int chapter, int verse);
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void goToHistory(int index);
    // History nearest first: [{index, label, forward}] behind you, then ahead.
    Q_INVOKABLE QVariantList historyEntries() const;

    Q_INVOKABLE void setTranslation(const QString &key);
    Q_INVOKABLE void toggleVerseNumbers() { setVerseNumbers(!m_verseNumbers); }
    Q_INVOKABLE void nextFont(int step = 1);
    Q_INVOKABLE void changeTextSize(int step);
    Q_INVOKABLE void resetTextSize();

    // Picking verses: `v` starts at the top of the page, j/k move the end.
    Q_INVOKABLE void startSelection();
    Q_INVOKABLE void moveCursor(int step);
    Q_INVOKABLE void clickVerse(int verse, int modifiers);
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void clearSelection();

    // The Grok dictionary entry for a word, with how many verses in this
    // translation use it: {word, pronunciation, senses: [{definition,
    // example}], synonyms, verses}, or {} if there is none.
    Q_INVOKABLE QVariantMap define(const QString &word);

signals:
    void placeChanged();
    void translationChanged();
    void historyChanged();
    void styleChanged();
    void layoutChanged();
    void selectionChanged();
    void pageContentChanged(); // anything PageItem draws
    void turned(int direction);
    void toast(const QString &message);

private:
    enum class Visit { Add, Replace, Keep };

    void show(const Book *book, int chapter, int verse, int page, Visit visit, int direction = 0);
    void showPlace(const Place &place, Visit visit);
    std::shared_ptr<ChapterLayout> layoutFor(const Book *book, int chapter);
    void restyle();
    void relayout();
    void prefetch();
    void flash(const VerseKey &key);
    void setSelection(const QSet<VerseKey> &verses);
    VerseKey step(const VerseKey &from, int delta) const;
    QList<VerseKey> range(VerseKey a, VerseKey b) const;
    void reveal(const VerseKey &key);
    void scheduleSave();
    void save();

    std::unique_ptr<Bible> m_bible;
    QString m_problem;
    const Translation *m_translation = nullptr;
    const Book *m_book = nullptr;
    int m_chapter = 1;
    int m_page = 0;
    int m_pendingVerse = 1; // where to open once there's a page to lay out
    std::shared_ptr<ChapterLayout> m_layout;
    QHash<QString, std::shared_ptr<ChapterLayout>> m_cache;
    QStringList m_cacheOrder;

    TypeStyle m_style;
    qreal m_pageHeight = 0;
    qreal m_viewportWidth = 0, m_viewportHeight = 0;
    int m_fontIndex = 0;
    int m_sizeOffset = 0;
    bool m_verseNumbers = false;
    QTimer m_relayoutTimer;
    QTimer m_prefetchTimer;
    QTimer m_saveTimer;

    History m_history;

    bool m_selecting = false;
    QSet<VerseKey> m_selection;
    VerseKey m_anchor, m_cursor;

    VerseKey m_flash;
    qreal m_flashStrength = 0;
    QVariantAnimation m_flashAnimation;
};
