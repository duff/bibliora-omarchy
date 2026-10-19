// Linking the agent skill into each agent's skills folder, as Peeps does.
#include "Skill.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class TestSkill : public QObject
{
    Q_OBJECT

private slots:
    void installedNextToTheBinary()
    {
        QTemporaryDir root;
        const QString prefix = root.path() + "/usr";
        QVERIFY(Skill::installedDir(prefix + "/bin").isEmpty()); // a build directory has none
        const QString skill = prefix + "/share/bibliora/skills/bibliora";
        QVERIFY(QDir().mkpath(skill));
        QFile file(skill + "/SKILL.md");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QCOMPARE(Skill::installedDir(prefix + "/bin"), skill);
    }

    void onlyAgentsThatAreSetUp()
    {
        QTemporaryDir root;
        const QString home = root.path() + "/home";
        QVERIFY(QDir().mkpath(home + "/.claude"));
        QVERIFY(QDir().mkpath(home + "/.agents"));
        QCOMPARE(Skill::agentFolders(home), QStringList({home + "/.claude/skills", home + "/.agents/skills"}));
        QVERIFY(QDir().mkpath(root.path() + "/claude-elsewhere"));
        QCOMPARE(Skill::agentFolders(home, root.path() + "/claude-elsewhere").first(),
                 root.path() + "/claude-elsewhere/skills");
    }

    void linksAndUnlinks()
    {
        QTemporaryDir root;
        const QString skill = root.path() + "/usr/share/bibliora/skills/bibliora";
        QVERIFY(QDir().mkpath(skill));
        const QString home = root.path() + "/home";
        QVERIFY(QDir().mkpath(home + "/.claude"));
        // Someone's own skill called bibliora, and an older install's link.
        QVERIFY(QDir().mkpath(home + "/.agents/skills/bibliora"));
        QVERIFY(QDir().mkpath(home + "/.pi/agent/skills"));
        QVERIFY(QFile::link("/old/share/bibliora/skills/bibliora", home + "/.pi/agent/skills/bibliora"));
        const QStringList folders = Skill::agentFolders(home);

        const Skill::Links linked = Skill::link(skill, folders);
        QVERIFY(linked.error.isEmpty());
        QCOMPARE(linked.changed, QStringList({home + "/.claude/skills/bibliora", home + "/.pi/agent/skills/bibliora"}));
        QCOMPARE(linked.skipped, QStringList({home + "/.agents/skills/bibliora"}));
        QCOMPARE(QFileInfo(home + "/.claude/skills/bibliora").symLinkTarget(), skill);
        QCOMPARE(QFileInfo(home + "/.pi/agent/skills/bibliora").symLinkTarget(), skill);

        const Skill::Links again = Skill::link(skill, folders);
        QVERIFY(again.changed.isEmpty());
        QCOMPARE(again.unchanged.size(), 2);

        const Skill::Links removed = Skill::unlink(folders);
        QCOMPARE(removed.changed.size(), 2);
        QCOMPARE(removed.skipped, QStringList({home + "/.agents/skills/bibliora"}));
        QVERIFY(!QFileInfo(home + "/.claude/skills/bibliora").isSymLink());
        QVERIFY(QFileInfo(home + "/.agents/skills/bibliora").isDir());
    }

    // Agents read the front matter to decide when to use the skill.
    void theSkillCoversEveryCommand()
    {
        QFile file(BIBLIORA_SOURCE_DIR "/skills/bibliora/SKILL.md");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString skill = QString::fromUtf8(file.readAll());
        QVERIFY(skill.startsWith("---\nname: bibliora\ndescription: >\n"));
        for (const char *command : {"bibliora show", "bibliora search", "bibliora define", "setsid -f bibliora"})
            QVERIFY2(skill.contains(QLatin1String(command)), command);
    }
};

QTEST_GUILESS_MAIN(TestSkill)
#include "tst_skill.moc"
