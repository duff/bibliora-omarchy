#pragma once

#include "Bible.h"
#include "History.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QThread>

// Search, as the Search singleton: what you type, the verses that match in
// the current translation (in Bible order, the words that matched marked),
// and your past searches while the field is empty.
//
// Queries run on a thread of their own with its own connection, newest wins,
// so typing never waits on the database. A query that reads as a reference
// ("gen 9", "jn 3:16") also offers to go there, first.
class SearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Search)
    QML_SINGLETON

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(int count READ count NOTIFY resultsChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY resultsChanged)
    Q_PROPERTY(bool showingHistory READ showingHistory NOTIFY resultsChanged)
    Q_PROPERTY(bool historyIsExamples READ historyIsExamples NOTIFY resultsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY resultsChanged)

public:
    enum Role { KindRole = Qt::UserRole + 1, ReferenceRole, TextRole, BookRole, ChapterRole, VerseRole };

    explicit SearchModel(QObject *parent = nullptr);
    ~SearchModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    int selected() const { return m_selected; }
    void setSelected(int row);
    int count() const { return m_total; }
    QString summary() const;
    bool showingHistory() const { return m_rows.isEmpty() || m_rows.first().kind == Row::Past; }
    bool historyIsExamples() const { return m_history.isExamples(); }
    bool busy() const { return m_busy; }

    // Opens the selected row: a verse or a reference goes there and returns
    // true (the sheet closes); a past search fills the field and returns false.
    Q_INVOKABLE bool activate(int row = -1);
    Q_INVOKABLE void remember(); // keep this search in the history
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void refresh(); // the translation changed

signals:
    void queryChanged();
    void selectedChanged();
    void resultsChanged();
    void run(quint64 generation, const QString &translationKey, const QString &fts);

private:
    struct Row
    {
        enum Kind { Verse, GoTo, Past };
        Kind kind = Verse;
        int book = 0, chapter = 0, verse = 0;
        QString reference;
        QString text;
    };

    void rebuild();
    void finished(quint64 generation, const QList<SearchHit> &hits, int total);
    void saveHistory();

    QString m_query;
    QList<Row> m_rows;
    int m_selected = 0;
    int m_total = 0;
    bool m_busy = false;
    quint64 m_generation = 0;
    QThread m_thread;
    SearchHistory m_history;
};
