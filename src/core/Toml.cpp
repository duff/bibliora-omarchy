#include "Toml.h"

#include <QFile>

namespace Toml {

namespace {

QString stripComment(const QString &line)
{
    bool inString = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == '"' && (i == 0 || line.at(i - 1) != '\\'))
            inString = !inString;
        else if (c == '#' && !inString)
            return line.left(i);
    }
    return line;
}

} // namespace

QHash<QString, QString> parseFlat(const QString &text)
{
    QHash<QString, QString> out;
    QString section;
    const auto lines = text.split('\n');
    for (QString line : lines) {
        line = stripComment(line).trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        if (value.size() >= 2 && ((value.startsWith('"') && value.endsWith('"'))
                                  || (value.startsWith('\'') && value.endsWith('\''))))
            value = value.mid(1, value.size() - 2);
        out.insert(section.isEmpty() ? key : section + '.' + key, value);
    }
    return out;
}

QHash<QString, QString> readFlat(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return parseFlat(QString::fromUtf8(f.readAll()));
}

} // namespace Toml
