#include "Skill.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Skill {

namespace {

const QString kName = QStringLiteral("bibliora");

// A link this install made, or an older install's, ends in the same path.
bool isBiblioraLink(const QFileInfo &entry)
{
    return entry.isSymLink() && entry.symLinkTarget().endsWith(QStringLiteral("/bibliora/skills/bibliora"));
}

} // namespace

QString installedDir(const QString &binDir)
{
    const QString dir = QDir::cleanPath(binDir + QStringLiteral("/../share/bibliora/skills/bibliora"));
    return QFileInfo::exists(dir + QStringLiteral("/SKILL.md")) ? dir : QString();
}

QStringList agentFolders(const QString &home, const QString &claudeConfigDir, const QString &codexHome)
{
    const QString claude = claudeConfigDir.isEmpty() ? home + QStringLiteral("/.claude") : claudeConfigDir;
    const QString codex = codexHome.isEmpty() ? home + QStringLiteral("/.codex") : codexHome;
    QStringList folders;
    for (const QString &agent : {claude, codex, home + QStringLiteral("/.pi/agent"), home + QStringLiteral("/.agents")})
        if (QFileInfo(agent).isDir())
            folders.append(agent + QStringLiteral("/skills"));
    return folders;
}

Links link(const QString &skillDir, const QStringList &folders)
{
    Links result;
    for (const QString &folder : folders) {
        const QString path = folder + '/' + kName;
        const QFileInfo entry(path);
        if (entry.isSymLink()) {
            if (entry.symLinkTarget() == skillDir) {
                result.unchanged.append(path);
                continue;
            }
            QFile::remove(path);
        } else if (entry.exists()) {
            result.skipped.append(path);
            continue;
        }
        if (!QDir().mkpath(folder) || !QFile::link(skillDir, path)) {
            result.error = QStringLiteral("Couldn't link %1.").arg(path);
            return result;
        }
        result.changed.append(path);
    }
    return result;
}

Links unlink(const QStringList &folders)
{
    Links result;
    for (const QString &folder : folders) {
        const QString path = folder + '/' + kName;
        const QFileInfo entry(path);
        if (isBiblioraLink(entry)) {
            if (!QFile::remove(path)) {
                result.error = QStringLiteral("Couldn't remove %1.").arg(path);
                return result;
            }
            result.changed.append(path);
        } else if (entry.exists() || entry.isSymLink()) {
            result.skipped.append(path);
        }
    }
    return result;
}

} // namespace Skill
