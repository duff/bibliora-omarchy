#include "PageItem.h"

#include "Reader.h"
#include "Theme.h"

#include <QDateTime>
#include <QQuickWindow>
#include <QSGRectangleNode>
#include <QSGTextNode>

namespace {

QSGTextNode::RenderType renderType()
{
    // Native glyphs are the crispest at Omarchy's usual 2x scale; the others
    // are here for comparing (BIBLIORA_TEXT_RENDER=qt|curve).
    static const QSGTextNode::RenderType type = [] {
        const QString env = qEnvironmentVariable("BIBLIORA_TEXT_RENDER");
        if (env == QLatin1String("qt"))
            return QSGTextNode::QtRendering;
        if (env == QLatin1String("curve"))
            return QSGTextNode::CurveRendering;
        return QSGTextNode::NativeRendering;
    }();
    return type;
}

QColor withAlpha(QColor c, qreal a)
{
    c.setAlphaF(std::clamp(a, 0.0, 1.0));
    return c;
}

} // namespace

PageItem::PageItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    if (Reader *reader = Reader::instance())
        connect(reader, &Reader::pageContentChanged, this, &QQuickItem::update);
    if (Theme *theme = Theme::instance())
        connect(theme, &Theme::changed, this, &QQuickItem::update);
}

QSGNode *PageItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    // A page is rebuilt whole when it changes; that's a few hundred glyph
    // runs, far under a frame.
    delete old;
    Reader *reader = Reader::instance();
    const auto layout = reader ? reader->layout() : nullptr;
    if (!layout || !window())
        return nullptr;
    const int page = reader->page();
    if (page < 0 || page >= layout->pageCount())
        return nullptr;

    auto *root = new QSGNode;
    const Theme *theme = Theme::instance();
    const QColor accent = theme ? theme->accent() : QColor("#e68e0d");
    const qreal pad = std::round(layout->lineHeight * 0.12);

    auto addRects = [&](const QList<QRectF> &rects, const QColor &color) {
        for (const QRectF &r : rects) {
            QSGRectangleNode *n = window()->createRectangleNode();
            n->setRect(r.adjusted(-pad, 0, pad, 0));
            n->setColor(color);
            root->appendChildNode(n);
        }
    };

    if (const int verse = reader->flashHere())
        addRects(layout->verseRects(page, {verse}), withAlpha(accent, 0.26 * reader->flashStrength()));
    const QSet<int> selected = reader->selectedHere();
    if (!selected.isEmpty()) {
        addRects(layout->verseRects(page, selected), withAlpha(accent, 0.17));
        if (const int cursor = reader->cursorHere()) {
            // The moving end of the selection gets a mark in the margin.
            const QList<QRectF> rects = layout->verseRects(page, {cursor});
            if (!rects.isEmpty()) {
                QSGRectangleNode *mark = window()->createRectangleNode();
                const QRectF first = rects.first();
                const QRectF last = rects.last();
                const qreal x = -std::round(layout->lineHeight * 0.5);
                mark->setRect(QRectF(x, first.top(), 2, last.bottom() - first.top()));
                mark->setColor(accent);
                root->appendChildNode(mark);
            }
        }
    }

    QSGTextNode *text = window()->createTextNode();
    text->setRenderType(renderType());
    text->setColor(theme ? theme->foreground() : QColor("#cccccc"));
    text->setFiltering(QSGTexture::Linear);
    for (const ChapterLayout::Piece &piece : layout->pages.at(page).pieces)
        text->addTextLayout(QPointF(0, 0), layout->layouts.at(piece.block).get(), -1, -1, piece.lineStart,
                            piece.lineCount);
    for (const ChapterLayout::Mark &mark : layout->marks)
        if (mark.page == page)
            text->addTextLayout(QPointF(0, 0), mark.layout.get());
    root->appendChildNode(text);
    return root;
}

void PageItem::mousePressEvent(QMouseEvent *event)
{
    // Taken, even where it does nothing, so a double click comes here too.
    event->accept();
    Reader *reader = Reader::instance();
    const auto layout = reader ? reader->layout() : nullptr;
    if (!layout)
        return;
    if (event->button() == Qt::RightButton) {
        const QString word = layout->wordAt(reader->page(), event->position());
        if (!word.isEmpty())
            emit wordRequested(word);
        return;
    }
    const auto hit = layout->hitTest(reader->page(), event->position());
    if (hit.verse > 0 && (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
        reader->clickVerse(hit.verse, int(event->modifiers()));
    else if (reader->selecting())
        reader->clearSelection(); // a plain click lets go
}

void PageItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    Reader *reader = Reader::instance();
    const auto layout = reader ? reader->layout() : nullptr;
    if (!layout || event->button() != Qt::LeftButton)
        return;
    const QString word = layout->wordAt(reader->page(), event->position());
    if (!word.isEmpty())
        emit wordRequested(word);
}

void PageItem::wheelEvent(QWheelEvent *event)
{
    wheel(event->angleDelta().y(), event->angleDelta().x(), !event->pixelDelta().isNull());
    event->accept();
}

void PageItem::wheel(int angleY, int angleX, bool smooth)
{
    Reader *reader = Reader::instance();
    if (!reader)
        return;
    // One notch is one page. A touchpad's stream of small steps adds up to
    // one, and then rests a moment so a single swipe turns a single page.
    const int delta = angleY != 0 ? angleY : -angleX;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool notched = !smooth && std::abs(delta) >= 120;
    if (!notched && now - m_lastTurn < 380)
        return;
    m_wheel += delta;
    if (std::abs(m_wheel) >= 120) {
        if (m_wheel < 0)
            reader->nextPage();
        else
            reader->previousPage();
        m_wheel = 0;
        m_lastTurn = now;
    }
}
