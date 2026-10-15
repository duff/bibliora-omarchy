#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// The Go to palette, as the GoTo singleton: Omarchy's launcher in place of
// the iOS book and chapter grids. Type and the list narrows:
//
//   (nothing)     where you've been, then every book
//   "jo"          the books it could be: Joshua, Job, Joel, Jonah, John
//   "john"        John's chapters, each with how it begins
//   "ps 2"        Psalm 2, then 20–29
//   "jn 3:16"     that verse
//
// Enter goes to the row; on a book it lists the book's chapters. Tab writes
// the row into the field.
class GoToModel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GoTo)
    QML_SINGLETON

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QString note READ note NOTIFY changed)

public:
    explicit GoToModel(QObject *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);
    QVariantList rows() const { return m_rows; }
    int selected() const { return m_selected; }
    void setSelected(int row);
    QString note() const { return m_note; }

    Q_INVOKABLE void reset(const QString &text = {});
    // True when it went somewhere and the palette can close.
    Q_INVOKABLE bool activate(int row = -1);
    Q_INVOKABLE void complete();

signals:
    void changed();
    void selectedChanged();

private:
    void rebuild();

    QString m_text;
    QVariantList m_rows;
    int m_selected = 0;
    QString m_note;
};
