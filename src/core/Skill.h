#pragma once

#include <QString>
#include <QStringList>

// The agent skill installs with Bibliora, in share/bibliora/skills/bibliora
// beside the binary's bin directory. Agents find skills through links in
// their own folders, the way Omarchy links its skills, so the one installed
// skill serves Claude Code, Codex, pi, and any agent that reads
// ~/.agents/skills. As Peeps does it.
namespace Skill {

// The installed skill for a bibliora binary in `binDir`, or empty when there
// is none, as when running from the build directory.
QString installedDir(const QString &binDir);

// The skills folders of the agents set up in `home`. CLAUDE_CONFIG_DIR and
// CODEX_HOME move those agents' folders; empty means the default.
QStringList agentFolders(const QString &home, const QString &claudeConfigDir = {}, const QString &codexHome = {});

struct Links
{
    QStringList changed;   // made or taken away by this call
    QStringList unchanged; // already pointed at the skill
    QStringList skipped;   // something else is called bibliora there
    QString error;
};

// Links `skillDir` as bibliora in each folder. A link to somewhere else (an
// older install) is replaced; anything else named bibliora is left alone.
Links link(const QString &skillDir, const QStringList &folders);

// Removes the bibliora links that point at a Bibliora skill.
Links unlink(const QStringList &folders);

} // namespace Skill
