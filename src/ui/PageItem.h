#pragma once

#include <QQuickItem>

// Draws Reader's current page: the lines Typesetter placed, straight into the
// scene graph as glyphs, with the selection and a found verse behind them.
//
// A click on a verse selects it (Shift extends, Ctrl adds), a double click on
// a word asks for its dictionary entry, and the wheel turns pages.
class PageItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit PageItem(QQuickItem *parent = nullptr);

    // The wheel anywhere in the window turns pages; QML passes it on here.
    Q_INVOKABLE void wheel(int angleY, int angleX, bool smooth);

signals:
    void wordRequested(const QString &word);

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int m_wheel = 0;
    qint64 m_lastTurn = 0;
};
