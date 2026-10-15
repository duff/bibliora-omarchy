#pragma once

#include <QHash>
#include <QString>

// Just enough TOML for Omarchy's colors.toml and shell.toml: [sections],
// `key = "string" | number | bool`, and # comments. Keys are flattened to
// "section.key" (top-level keys have no prefix). Values keep their raw text
// with surrounding quotes removed.
namespace Toml {

QHash<QString, QString> parseFlat(const QString &text);
QHash<QString, QString> readFlat(const QString &path);

} // namespace Toml
