#include "Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace Paths {

namespace {
QString g_override;
}

void setBiblePath(const QString &path) { g_override = path; }

QString defaultBiblePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/bibliora/bibles.sqlite3";
}

QString biblePath()
{
    if (!g_override.isEmpty())
        return g_override;
    if (const QString env = qEnvironmentVariable("BIBLIORA_DB"); !env.isEmpty())
        return env;
    const QString found = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "bibliora/bibles.sqlite3");
    return found.isEmpty() ? defaultBiblePath() : found;
}

} // namespace Paths
