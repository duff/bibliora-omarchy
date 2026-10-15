#pragma once

#include <QString>

namespace Paths {

// bibles.sqlite3: --bible or $BIBLIORA_DB if given, else the first of
// ~/.local/share/bibliora/ and the system data dirs (/usr/share/bibliora/…)
// that has it. The iOS app's database, copied as is.
QString biblePath();
void setBiblePath(const QString &path);

// Where `bin/relaunch` and the README say to put it.
QString defaultBiblePath();

} // namespace Paths
