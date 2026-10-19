#include "Reader.h"

#include "Paths.h"
#include "ReadingFonts.h"
#include "Reference.h"
#include "SearchQuery.h"
#include "Theme.h"

#include <QClipboard>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QSettings>

#include <algorithm>
#include <climits>
#include <cmath>

namespace {

Reader *g_reader = nullptr;

constexpr int kCacheSize = 16;
constexpr int kMinTextSize = 12;
constexpr int kMaxTextSize = 48;
constexpr int kMaxSelection = 3000; // verses; a whole book is plenty

QString cacheKey(const Translation &t, const Book &b, int chapter)
{
    return t.key + '/' + b.key + '/' + QString::number(chapter);
}

} // namespace

Reader::Reader(QObject *parent)
    : QObject(parent)
{
    g_reader = this;

    m_bible = std::make_unique<Bible>(Paths::biblePath());
    if (!m_bible->isOpen())
        m_problem = m_bible->error();

    QSettings s;
    m_translation = Translations::find(s.value("reading/translation").toString());
    if (!m_translation)
        m_translation = &Translations::fallback();
    m_verseNumbers = s.value("reading/verse_numbers", false).toBool();
    m_fontIndex = std::clamp(s.value("reading/font", 0).toInt(), 0, ReadingFonts::count() - 1);
    m_sizeOffset = s.value("reading/size_offset", 0).toInt();
    m_history.restore(s.value("history/places").toStringList(), s.value("history/index", 0).toInt());

    m_relayoutTimer.setSingleShot(true);
    m_relayoutTimer.setInterval(40);
    connect(&m_relayoutTimer, &QTimer::timeout, this, &Reader::relayout);

    m_prefetchTimer.setSingleShot(true);
    m_prefetchTimer.setInterval(120);
    connect(&m_prefetchTimer, &QTimer::timeout, this, &Reader::prefetch);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &Reader::save);

    m_flashAnimation.setStartValue(1.0);
    m_flashAnimation.setKeyValueAt(0.55, 0.85);
    m_flashAnimation.setEndValue(0.0);
    m_flashAnimation.setDuration(2400);
    m_flashAnimation.setEasingCurve(QEasingCurve::InOutQuad);
    connect(&m_flashAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_flashStrength = v.toReal();
        emit pageContentChanged();
    });

    if (Theme *theme = Theme::instance())
        connect(theme, &Theme::changed, this, &Reader::restyle);

    // Where you left off. The page is laid out once the window says how big
    // it is (setViewport).
    const Place start = m_history.current();
    const Book *book = Books::byNumber(start.book);
    m_book = book ? book : Books::byNumber(1);
    m_chapter = std::clamp(start.chapter, 1, m_book->chapters);
    m_pendingVerse = start.verse;
}

Reader::~Reader()
{
    save();
    if (g_reader == this)
        g_reader = nullptr;
}

Reader *Reader::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(g_reader, QJSEngine::CppOwnership);
    return g_reader;
}

Reader *Reader::instance() { return g_reader; }

// ------------------------------------------------------------------ place

QString Reader::bookName() const { return m_book ? m_book->name : QString(); }
int Reader::bookNumber() const { return m_book ? m_book->number : 0; }
int Reader::chapterCount() const { return m_book ? m_book->chapters : 0; }
int Reader::pageCount() const { return m_layout ? m_layout->pageCount() : 0; }

QString Reader::placeLabel() const
{
    return m_book ? m_book->name + ' ' + QString::number(m_chapter) : QString();
}

Place Reader::currentPlace() const
{
    Place p;
    p.book = bookNumber();
    p.chapter = m_chapter;
    p.verse = m_layout ? 1 : std::max(1, m_pendingVerse);
    if (m_layout && m_page < m_layout->pageCount())
        p.verse = std::max(1, m_layout->pages.at(m_page).firstVerse);
    return p;
}

QVariantList Reader::translations() const
{
    QVariantList out;
    for (const Translation &t : Translations::all())
        out.append(QVariantMap{
            {"key", t.key}, {"abbrev", t.abbrev}, {"name", t.name}, {"notice", t.notice}, {"site", t.site}});
    return out;
}

// ------------------------------------------------------------------ layout

QString Reader::fontName() const { return ReadingFonts::name(m_fontIndex); }

QString Reader::fontFamily() const { return m_style.font.family(); }

int Reader::textSize() const
{
    const int base = Theme::instance() ? Theme::instance()->fontBase() : 12;
    // The Bible reads larger than the interface: 21 px beside Omarchy's 12.
    return std::clamp(int(std::lround(base * 1.75)) + m_sizeOffset, kMinTextSize, kMaxTextSize);
}

void Reader::setViewport(qreal width, qreal height)
{
    width = std::floor(width);
    height = std::floor(height);
    if (width == m_viewportWidth && height == m_viewportHeight)
        return;
    const bool first = m_viewportWidth <= 0;
    m_viewportWidth = width;
    m_viewportHeight = height;
    if (first)
        restyle(); // the first page, at once
    else
        m_relayoutTimer.start();
}

// Works out the type and the page from the font, the size and the window.
void Reader::restyle()
{
    const Theme *theme = Theme::instance();
    const int size = textSize();
    const QString omarchy = theme ? theme->fontFamily() : QStringLiteral("monospace");
    TypeStyle st;
    st.font = ReadingFonts::font(m_fontIndex, size, omarchy);
    st.realSmallCaps = ReadingFonts::realSmallCaps(m_fontIndex);
    const QColor fg = theme ? theme->foreground() : QColor("#cccccc");
    const QColor bg = theme ? theme->background() : QColor("#121212");
    st.text = fg;
    // Verse numbers sit back from the text, as on iOS.
    st.verseNumber = QColor::fromRgbF(fg.redF() * 0.5 + bg.redF() * 0.5, fg.greenF() * 0.5 + bg.greenF() * 0.5,
                                      fg.blueF() * 0.5 + bg.blueF() * 0.5);
    st.lineHeight = std::round(size * ReadingFonts::lineHeight(m_fontIndex));
    st.indent = std::round(size * 1.4);
    st.hang = std::round(size * 1.4);
    st.centerInset = std::round(size * 1.8);

    // About 70 characters to a line: long enough to read in phrases, short
    // enough for the eye to find the next line.
    // (Measured on ordinary prose: a font's own "average" counts every glyph.)
    static const QString prose = QStringLiteral("In the beginning God created the heavens and the earth. "
                                                "The earth was without form, and void; and darkness was on "
                                                "the face of the deep.");
    const qreal perChar = QFontMetricsF(st.font).horizontalAdvance(prose) / prose.size();
    const qreal measure = std::round(perChar * 70);
    const qreal width = m_viewportWidth > 0 ? m_viewportWidth : 900;
    const qreal height = m_viewportHeight > 0 ? m_viewportHeight : 1000;
    st.pageWidth = std::max<qreal>(size * 12, std::min(measure, width));
    const int lines = std::max(3, int(std::floor(height / st.lineHeight)));
    st.pageHeight = lines * st.lineHeight;

    const bool layoutMoved = st.pageWidth != m_style.pageWidth || st.pageHeight != m_pageHeight
                             || st.lineHeight != m_style.lineHeight;
    m_style = st;
    m_pageHeight = st.pageHeight;

    // Keep the verse at the top of the page where it is.
    const int anchor = m_layout ? currentPlace().verse : m_pendingVerse;
    m_cache.clear();
    m_cacheOrder.clear();
    if (m_book && ready() && m_viewportWidth > 0) {
        m_layout = layoutFor(m_book, m_chapter);
        m_page = m_layout ? m_layout->pageOfVerse(anchor) : 0;
    }
    emit styleChanged();
    if (layoutMoved)
        emit layoutChanged();
    emit placeChanged();
    emit pageContentChanged();
    m_prefetchTimer.start();
}

void Reader::relayout() { restyle(); }

std::shared_ptr<ChapterLayout> Reader::layoutFor(const Book *book, int chapter)
{
    if (!book || !ready())
        return {};
    const QString key = cacheKey(*m_translation, *book, chapter);
    if (const auto it = m_cache.constFind(key); it != m_cache.cend()) {
        m_cacheOrder.removeOne(key);
        m_cacheOrder.append(key);
        return *it;
    }
    auto layout
        = Typesetter::set(Markup::parse(m_bible->chapterMarkup(*m_translation, *book, chapter), m_verseNumbers), m_style);
    m_cache.insert(key, layout);
    m_cacheOrder.append(key);
    while (m_cacheOrder.size() > kCacheSize)
        m_cache.remove(m_cacheOrder.takeFirst());
    return layout;
}

// The chapters either side, set ahead of time so turning into them is instant.
void Reader::prefetch()
{
    if (!m_book || !ready())
        return;
    if (m_chapter < m_book->chapters)
        layoutFor(m_book, m_chapter + 1);
    else if (const Book *next = Books::byNumber(m_book->number + 1))
        layoutFor(next, 1);
    if (m_chapter > 1)
        layoutFor(m_book, m_chapter - 1);
    else if (const Book *prev = Books::byNumber(m_book->number - 1))
        layoutFor(prev, prev->chapters);
}

// ------------------------------------------------------------------ moving

void Reader::show(const Book *book, int chapter, int verse, int page, Visit visit, int direction)
{
    if (!book)
        return;
    chapter = std::clamp(chapter, 1, book->chapters);
    m_book = book;
    m_chapter = chapter;
    // Before the window has said how big a page is, just note the place.
    m_pendingVerse = std::max(1, verse);
    m_layout = m_viewportWidth > 0 ? layoutFor(book, chapter) : nullptr;
    const int count = m_layout ? m_layout->pageCount() : 1;
    if (verse > 0 && m_layout)
        m_page = m_layout->pageOfVerse(verse);
    else
        m_page = page < 0 ? count - 1 : std::clamp(page, 0, count - 1);

    switch (visit) {
    case Visit::Add:
        m_history.add(currentPlace());
        break;
    case Visit::Replace:
        m_history.replaceCurrent(currentPlace());
        break;
    case Visit::Keep:
        break;
    }
    emit placeChanged();
    emit historyChanged();
    emit pageContentChanged();
    if (direction)
        emit turned(direction);
    m_prefetchTimer.start();
    scheduleSave();
}

void Reader::showPlace(const Place &place, Visit visit)
{
    show(Books::byNumber(place.book), place.chapter, place.verse, 0, visit);
}

void Reader::nextPage()
{
    if (!m_book)
        return;
    if (m_page + 1 < pageCount()) {
        show(m_book, m_chapter, 0, m_page + 1, Visit::Replace, 1);
    } else if (m_chapter < m_book->chapters) {
        show(m_book, m_chapter + 1, 0, 0, Visit::Replace, 1);
    } else if (const Book *next = Books::byNumber(m_book->number + 1)) {
        show(next, 1, 0, 0, Visit::Replace, 1);
    }
}

void Reader::previousPage()
{
    if (!m_book)
        return;
    if (m_page > 0) {
        show(m_book, m_chapter, 0, m_page - 1, Visit::Replace, -1);
    } else if (m_chapter > 1) {
        show(m_book, m_chapter - 1, 0, -1, Visit::Replace, -1);
    } else if (const Book *prev = Books::byNumber(m_book->number - 1)) {
        show(prev, prev->chapters, 0, -1, Visit::Replace, -1);
    }
}

void Reader::firstPage()
{
    if (m_page != 0)
        show(m_book, m_chapter, 0, 0, Visit::Replace, -1);
}

void Reader::lastPage()
{
    if (m_page != pageCount() - 1)
        show(m_book, m_chapter, 0, -1, Visit::Replace, 1);
}

void Reader::nextChapter()
{
    if (!m_book)
        return;
    if (m_chapter < m_book->chapters)
        show(m_book, m_chapter + 1, 0, 0, Visit::Replace, 1);
    else if (const Book *next = Books::byNumber(m_book->number + 1))
        show(next, 1, 0, 0, Visit::Replace, 1);
}

void Reader::previousChapter()
{
    if (!m_book)
        return;
    // From inside a chapter, back to its start first.
    if (m_page > 0)
        show(m_book, m_chapter, 0, 0, Visit::Replace, -1);
    else if (m_chapter > 1)
        show(m_book, m_chapter - 1, 0, 0, Visit::Replace, -1);
    else if (const Book *prev = Books::byNumber(m_book->number - 1))
        show(prev, prev->chapters, 0, 0, Visit::Replace, -1);
}

void Reader::nextBook()
{
    if (const Book *next = m_book ? Books::byNumber(m_book->number + 1) : nullptr)
        show(next, 1, 0, 0, Visit::Add, 1);
}

void Reader::previousBook()
{
    if (!m_book)
        return;
    if (m_chapter > 1 || m_page > 0)
        show(m_book, 1, 0, 0, Visit::Add, -1);
    else if (const Book *prev = Books::byNumber(m_book->number - 1))
        show(prev, 1, 0, 0, Visit::Add, -1);
}

bool Reader::goTo(const QString &reference)
{
    const auto ref = Reference::parse(reference, m_book);
    if (!ref)
        return false;
    const int chapter = std::max(1, ref->chapter);
    show(ref->book, chapter, ref->verse, 0, Visit::Add);
    if (ref->verse) {
        flash({ref->book->number, chapter, ref->verse});
        if (ref->endVerse) {
            // A range comes up selected, ready to copy.
            QSet<VerseKey> verses;
            for (int v = ref->verse; v <= ref->endVerse; ++v)
                verses.insert({ref->book->number, chapter, v});
            m_anchor = {ref->book->number, chapter, ref->verse};
            m_cursor = {ref->book->number, chapter, ref->endVerse};
            m_selecting = true;
            setSelection(verses);
        }
    }
    return true;
}

void Reader::openChapter(int book, int chapter) { show(Books::byNumber(book), chapter, 0, 0, Visit::Add); }

void Reader::openVerse(int book, int chapter, int verse)
{
    show(Books::byNumber(book), chapter, verse, 0, Visit::Add);
    flash({book, chapter, verse});
}

void Reader::back()
{
    if (const auto p = m_history.back())
        showPlace(*p, Visit::Keep);
}

void Reader::forward()
{
    if (const auto p = m_history.forward())
        showPlace(*p, Visit::Keep);
}

void Reader::goToHistory(int index)
{
    if (const auto p = m_history.goTo(index))
        showPlace(*p, Visit::Keep);
}

QVariantList Reader::historyEntries() const
{
    QVariantList out;
    const auto &places = m_history.places();
    for (int i : m_history.backIndices())
        out.append(QVariantMap{{"index", i}, {"label", places.at(i).label()}, {"forward", false}});
    for (int i : m_history.forwardIndices())
        out.append(QVariantMap{{"index", i}, {"label", places.at(i).label()}, {"forward", true}});
    return out;
}

// ------------------------------------------------------------------ settings

void Reader::setTranslation(const QString &key)
{
    const Translation *t = Translations::find(key);
    if (!t || t == m_translation)
        return;
    const Place here = currentPlace();
    m_translation = t;
    m_layout.reset();
    m_pendingVerse = here.verse;
    emit translationChanged();
    restyle();
    m_history.replaceCurrent(currentPlace());
    emit toast(t->name);
    scheduleSave();
}

void Reader::setVerseNumbers(bool on)
{
    if (on == m_verseNumbers)
        return;
    m_verseNumbers = on;
    restyle();
    emit toast(on ? QStringLiteral("Verse numbers on") : QStringLiteral("Verse numbers off"));
    scheduleSave();
}

void Reader::nextFont(int step)
{
    const int n = ReadingFonts::count();
    m_fontIndex = ((m_fontIndex + step) % n + n) % n;
    restyle();
    emit toast(fontName());
    scheduleSave();
}

void Reader::changeTextSize(int step)
{
    const int before = textSize();
    m_sizeOffset += step;
    const int base = textSize() - m_sizeOffset;
    m_sizeOffset = std::clamp(m_sizeOffset, kMinTextSize - base, kMaxTextSize - base);
    if (textSize() == before)
        return;
    restyle();
    emit toast(QStringLiteral("Text size %1 px").arg(textSize()));
    scheduleSave();
}

void Reader::resetTextSize()
{
    m_sizeOffset = 0;
    restyle();
    emit toast(QStringLiteral("Text size %1 px, Bibliora's").arg(textSize()));
    scheduleSave();
}

// ------------------------------------------------------------------ selection

QString Reader::selectionLabel() const
{
    if (m_selection.isEmpty() || !ready())
        return {};
    const auto runs = Passage::runs(*m_bible, *m_translation, QList<VerseKey>(m_selection.begin(), m_selection.end()));
    QStringList labels;
    for (const auto &run : runs)
        labels.append(Passage::location(run));
    if (labels.size() > 3)
        return QStringLiteral("%1 passages").arg(labels.size());
    return labels.join(QStringLiteral("; "));
}

QSet<int> Reader::selectedHere() const
{
    QSet<int> out;
    for (const VerseKey &k : m_selection)
        if (k.book == bookNumber() && k.chapter == m_chapter)
            out.insert(k.verse);
    return out;
}

int Reader::cursorHere() const
{
    return m_selecting && m_cursor.book == bookNumber() && m_cursor.chapter == m_chapter ? m_cursor.verse : 0;
}

int Reader::flashHere() const
{
    return m_flashStrength > 0 && m_flash.book == bookNumber() && m_flash.chapter == m_chapter ? m_flash.verse : 0;
}

void Reader::flash(const VerseKey &key)
{
    m_flash = key;
    m_flashAnimation.stop();
    m_flashAnimation.start();
}

void Reader::setSelection(const QSet<VerseKey> &verses)
{
    m_selection = verses;
    if (m_selection.isEmpty())
        m_selecting = false;
    emit selectionChanged();
    emit pageContentChanged();
}

VerseKey Reader::step(const VerseKey &from, int delta) const
{
    VerseKey k = from;
    const Book *book = Books::byNumber(k.book);
    if (!book)
        return from;
    while (delta > 0) {
        const int count = m_bible->verseCount(*m_translation, *book, k.chapter);
        if (k.verse < count) {
            ++k.verse;
        } else if (k.chapter < book->chapters) {
            k = {k.book, k.chapter + 1, 1};
        } else if (const Book *next = Books::byNumber(k.book + 1)) {
            book = next;
            k = {next->number, 1, 1};
        } else {
            break;
        }
        --delta;
    }
    while (delta < 0) {
        if (k.verse > 1) {
            --k.verse;
        } else if (k.chapter > 1) {
            k = {k.book, k.chapter - 1, m_bible->verseCount(*m_translation, *book, k.chapter - 1)};
        } else if (const Book *prev = Books::byNumber(k.book - 1)) {
            book = prev;
            k = {prev->number, prev->chapters, m_bible->verseCount(*m_translation, *prev, prev->chapters)};
        } else {
            break;
        }
        ++delta;
    }
    return k;
}

QList<VerseKey> Reader::range(VerseKey a, VerseKey b) const
{
    if (b < a)
        std::swap(a, b);
    QList<VerseKey> out{a};
    while (out.last() < b && out.size() < kMaxSelection) {
        const VerseKey next = step(out.last(), 1);
        if (next == out.last())
            break;
        out.append(next);
    }
    return out;
}

// Brings a verse into view: its chapter, and the page it starts on.
void Reader::reveal(const VerseKey &key)
{
    if (key.book != bookNumber() || key.chapter != m_chapter) {
        const bool ahead = std::tie(key.book, key.chapter) > std::tie(m_book->number, m_chapter);
        show(Books::byNumber(key.book), key.chapter, key.verse, 0, Visit::Replace, ahead ? 1 : -1);
        return;
    }
    if (!m_layout)
        return;
    const int page = m_layout->pageOfVerse(key.verse);
    // A verse already in view stays put, even if it began a page earlier.
    const QList<int> here = m_layout->versesOnPage(m_page);
    if (page != m_page && !here.contains(key.verse))
        show(m_book, m_chapter, 0, page, Visit::Replace, page > m_page ? 1 : -1);
}

void Reader::startSelection()
{
    if (!m_book || !m_layout)
        return;
    if (m_selecting) {
        // Again: start over from the cursor.
        m_anchor = m_cursor;
        setSelection({m_cursor});
        return;
    }
    const Place here = currentPlace();
    m_anchor = m_cursor = {here.book, here.chapter, here.verse};
    m_selecting = true;
    setSelection({m_cursor});
}

void Reader::moveCursor(int delta)
{
    if (!m_selecting) {
        startSelection();
        return;
    }
    m_cursor = step(m_cursor, delta);
    const QList<VerseKey> verses = range(m_anchor, m_cursor);
    m_selecting = true;
    setSelection(QSet<VerseKey>(verses.begin(), verses.end()));
    reveal(m_cursor);
}

void Reader::clickVerse(int verse, int modifiers)
{
    if (verse <= 0 || !m_book)
        return;
    const VerseKey key{m_book->number, m_chapter, verse};
    if ((modifiers & Qt::ShiftModifier) && m_selecting) {
        m_cursor = key;
        const QList<VerseKey> verses = range(m_anchor, m_cursor);
        setSelection(QSet<VerseKey>(verses.begin(), verses.end()));
        return;
    }
    // Otherwise it adds the verse, or drops it if it's already picked.
    QSet<VerseKey> verses = m_selecting ? m_selection : QSet<VerseKey>{};
    if (verses.contains(key))
        verses.remove(key);
    else
        verses.insert(key);
    m_anchor = m_cursor = key;
    m_selecting = true;
    setSelection(verses);
}

void Reader::copySelection()
{
    if (m_selection.isEmpty() || !ready())
        return;
    const QList<VerseKey> verses(m_selection.begin(), m_selection.end());
    const QString text = Passage::clipboardText(*m_bible, *m_translation, verses);
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
    const QString label = selectionLabel();
    clearSelection();
    emit toast(QStringLiteral("Copied %1, %2").arg(label, m_translation->abbrev));
}

void Reader::clearSelection()
{
    m_selecting = false;
    setSelection({});
}

// ------------------------------------------------------------------ dictionary

QVariantMap Reader::define(const QString &word)
{
    if (!ready() || word.trimmed().isEmpty())
        return {};
    const DictionaryEntry e = m_bible->define(word);
    if (!e.isValid())
        return {};
    QVariantList senses;
    for (const auto &[definition, example] : e.senses)
        senses.append(QVariantMap{{"definition", definition}, {"example", example}});
    const int verses = m_bible->searchCount(*m_translation, SearchQuery::build(e.word + ' '));
    return {{"word", e.word},
            {"pronunciation", e.pronunciations.join(QStringLiteral(", "))},
            {"senses", senses},
            {"synonyms", e.synonyms.join(QStringLiteral(", "))},
            {"verses", verses},
            {"translation", m_translation->abbrev}};
}

QStringList Reader::dictionaryWords(const QString &typed)
{
    if (!ready() || !m_layout)
        return {};
    // The layout changes with the font and its size, and with it the page.
    const QString here = QStringLiteral("%1 %2 %3 %4 %5")
                             .arg(m_translation->key, m_book->key)
                             .arg(m_chapter)
                             .arg(m_page)
                             .arg(quintptr(m_layout.get()));
    if (here != m_pageWordsFor) {
        QList<std::pair<int, QString>> ranked;
        QSet<QString> seen;
        for (const QString &w : m_layout->wordsOn(m_page)) {
            const DictionaryEntry e = m_bible->define(w);
            if (!e.isValid() || seen.contains(e.word))
                continue;
            seen.insert(e.word);
            const QString family = SearchQuery::family(e.word);
            const int uses = family.isEmpty() ? INT_MAX : m_bible->searchCount(*m_translation, family);
            ranked.append({uses, e.word});
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        m_pageWords.clear();
        for (const auto &[count, word] : std::as_const(ranked))
            m_pageWords.append(word);
        m_pageWordsFor = here;
    }

    const QString t = typed.trimmed();
    if (t.isEmpty())
        return m_pageWords;
    QStringList out;
    const DictionaryEntry exact = m_bible->define(t);
    if (exact.isValid())
        out.append(exact.word);
    for (const QString &w : std::as_const(m_pageWords))
        if (w.startsWith(t, Qt::CaseInsensitive))
            out.append(w);
    out.append(m_bible->dictionaryWords(t, 50));
    out.removeDuplicates();
    return out;
}

// ------------------------------------------------------------------ saving

void Reader::scheduleSave() { m_saveTimer.start(); }

void Reader::save()
{
    m_saveTimer.stop();
    QSettings s;
    s.setValue("reading/translation", m_translation->key);
    s.setValue("reading/verse_numbers", m_verseNumbers);
    s.setValue("reading/font", m_fontIndex);
    s.setValue("reading/size_offset", m_sizeOffset);
    s.setValue("history/places", m_history.serialize());
    s.setValue("history/index", m_history.index());
}
