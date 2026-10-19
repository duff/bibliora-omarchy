#include "Bible.h"
#include "Instance.h"
#include "Paths.h"
#include "Reader.h"
#include "Reference.h"
#include "SearchQuery.h"
#include "Skill.h"
#include "Theme.h"

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSet>
#include <QSettings>
#include <QSocketNotifier>
#include <QTimer>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

namespace {

bool g_json = false;
int g_signalFds[2];

// SIGTERM (logging out, bin/relaunch) and Ctrl+C quit the normal way, so the
// place you're reading is saved.
void quitOnTermination()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFds) != 0)
        return;
    auto *notifier = new QSocketNotifier(g_signalFds[1], QSocketNotifier::Read, qApp);
    QObject::connect(notifier, &QSocketNotifier::activated, qApp, [] { QCoreApplication::quit(); });
    struct sigaction sa = {};
    sa.sa_handler = [](int) {
        const char c = 1;
        (void)!::write(g_signalFds[0], &c, 1);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
}

void print(const QString &s) { std::fputs(s.toUtf8().constData(), stdout); }

void printJson(const QJsonValue &v)
{
    const QJsonDocument doc = v.isArray() ? QJsonDocument(v.toArray()) : QJsonDocument(v.toObject());
    print(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

int fail(const QString &message, int code = 1)
{
    std::fprintf(stderr, "bibliora: %s\n", message.toUtf8().constData());
    return code;
}

const Translation *translationFrom(const QCommandLineParser &parser)
{
    if (!parser.isSet("translation"))
        return &Translations::fallback();
    return Translations::find(parser.value("translation"));
}

// bibliora show john 3:16-18
int show(Bible &bible, const Translation &t, const QString &text)
{
    const auto ref = Reference::parse(text);
    if (!ref)
        return fail(QStringLiteral("“%1” isn't a book, chapter or verse.").arg(text), 4);
    QList<VerseText> verses;
    const int firstChapter = std::max(1, ref->chapter);
    const int lastChapter = ref->chapter ? ref->chapter : ref->book->chapters;
    for (int c = firstChapter; c <= lastChapter; ++c)
        verses += bible.verses(t, *ref->book, c, ref->verse ? ref->verse : 1,
                               ref->endVerse ? ref->endVerse : ref->verse ? ref->verse : 0);
    if (verses.isEmpty())
        return fail(QStringLiteral("%1 isn't in the %2.").arg(ref->label(), t.abbrev), 4);
    if (g_json) {
        QJsonArray list;
        for (const VerseText &v : std::as_const(verses))
            list.append(QJsonObject{{"book", v.book->name}, {"chapter", v.chapter}, {"verse", v.verse}, {"text", v.text}});
        printJson(QJsonObject{{"reference", ref->label()}, {"translation", t.key}, {"verses", list}});
        return 0;
    }
    int chapter = 0;
    for (const VerseText &v : std::as_const(verses)) {
        if (v.chapter != chapter && lastChapter > firstChapter)
            print(QStringLiteral("%1%2 %3\n").arg(chapter ? "\n" : "", v.book->name).arg(v.chapter));
        chapter = v.chapter;
        print(QStringLiteral("%1 %2\n").arg(v.verse).arg(v.text));
    }
    print(QStringLiteral("-- %1, %2\n").arg(ref->label(), t.key));
    return 0;
}

// bibliora search "bosom abraham"
int search(Bible &bible, const Translation &t, const QString &words, int limit)
{
    const QString fts = SearchQuery::build(words + ' ');
    if (fts.isEmpty())
        return fail(QStringLiteral("Search for a word or two."), 2);
    const auto hits = bible.search(t, fts, limit > 0 ? limit : -1);
    const int total = bible.searchCount(t, fts);
    const bool tty = isatty(fileno(stdout));
    if (g_json) {
        QJsonArray list;
        for (const SearchHit &h : hits) {
            QString text = h.text;
            text.remove(QChar(1)).remove(QChar(2));
            list.append(QJsonObject{{"book", h.book->name}, {"chapter", h.chapter}, {"verse", h.verse},
                                    {"reference", QStringLiteral("%1 %2:%3").arg(h.book->name).arg(h.chapter).arg(h.verse)},
                                    {"text", text}});
        }
        printJson(QJsonObject{{"query", words}, {"translation", t.key}, {"count", total}, {"verses", list}});
        return 0;
    }
    for (const SearchHit &h : hits) {
        QString text = h.text;
        text.replace(QChar(1), tty ? QStringLiteral("\x1b[1m") : QString());
        text.replace(QChar(2), tty ? QStringLiteral("\x1b[0m") : QString());
        print(QStringLiteral("%1 %2:%3  %4\n").arg(h.book->name).arg(h.chapter).arg(h.verse).arg(text));
    }
    print(QStringLiteral("%1 %2 in the %3%4\n")
              .arg(total)
              .arg(total == 1 ? "verse" : "verses", t.abbrev,
                   hits.size() < total ? QStringLiteral(" (%1 shown)").arg(hits.size()) : QString()));
    return total ? 0 : 4;
}

// bibliora define firmament
int define(Bible &bible, const QString &word)
{
    const DictionaryEntry e = bible.define(word);
    if (!e.isValid())
        return fail(QStringLiteral("No entry for “%1”.").arg(word), 4);
    if (g_json) {
        QJsonArray senses;
        for (const auto &[definition, example] : e.senses)
            senses.append(QJsonObject{{"definition", definition}, {"example", example}});
        printJson(QJsonObject{{"word", e.word},
                              {"pronunciations", QJsonArray::fromStringList(e.pronunciations)},
                              {"senses", senses},
                              {"synonyms", QJsonArray::fromStringList(e.synonyms)}});
        return 0;
    }
    print(e.word + (e.pronunciations.isEmpty() ? QString() : "  [" + e.pronunciations.join(", ") + "]") + "\n");
    int n = 0;
    for (const auto &[definition, example] : e.senses) {
        print(QStringLiteral("%1. %2\n").arg(++n).arg(definition));
        if (!example.isEmpty())
            print(QStringLiteral("   “%1”\n").arg(example));
    }
    if (!e.synonyms.isEmpty())
        print("Synonyms: " + e.synonyms.join(", ") + "\n");
    return 0;
}

// The skills folders of the AI agents set up for this person.
QStringList agentFolders()
{
    return Skill::agentFolders(QDir::homePath(), qEnvironmentVariable("CLAUDE_CONFIG_DIR"),
                               qEnvironmentVariable("CODEX_HOME"));
}

// Each person gets the agent skill without a step of their own: the window
// links it when it opens. After bibliora skill remove, it stays away.
const QString kSkillDeclined = QStringLiteral("agents/skill_declined");

// bibliora skill install|remove
int skill(const QString &action)
{
    QSettings settings;
    if (action == QLatin1String("remove")) {
        settings.setValue(kSkillDeclined, true);
        const Skill::Links links = Skill::unlink(agentFolders());
        for (const QString &path : links.changed)
            print(QStringLiteral("Removed %1\n").arg(path));
        for (const QString &path : links.skipped)
            print(QStringLiteral("Left %1 alone: it isn't Bibliora's.\n").arg(path));
        if (!links.error.isEmpty())
            return fail(links.error);
        print(QStringLiteral("Bibliora won't link the skill again until bibliora skill install.\n"));
        return 0;
    }
    if (action != QLatin1String("install"))
        return fail(QStringLiteral("Say bibliora skill install or bibliora skill remove."), 2);
    const QString dir = Skill::installedDir(QCoreApplication::applicationDirPath());
    if (dir.isEmpty())
        return fail(QStringLiteral("This bibliora has no skill installed beside it."));
    settings.remove(kSkillDeclined);
    const QStringList folders = agentFolders();
    if (folders.isEmpty())
        return fail(QStringLiteral("There are no AI agents set up here (~/.claude, ~/.codex, ~/.pi/agent or ~/.agents)."));
    const Skill::Links links = Skill::link(dir, folders);
    for (const QString &path : links.changed)
        print(QStringLiteral("Linked the Bibliora skill in %1\n").arg(path));
    for (const QString &path : links.unchanged)
        print(QStringLiteral("Already linked in %1\n").arg(path));
    for (const QString &path : links.skipped)
        print(QStringLiteral("Left %1 alone: it isn't Bibliora's.\n").arg(path));
    return links.error.isEmpty() ? 0 : fail(links.error);
}

// The first word that isn't an option or an option's value, so options can
// come before the command too ("bibliora -t nkjv show john 3:16").
const char *commandWord(int argc, char *argv[])
{
    static const QSet<QByteArray> takeValues = {"-t",    "--translation", "--limit", "--bible", "--screenshot",
                                                "--keys", "--size",        "-platform"};
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "--")
            return i + 1 < argc ? argv[i + 1] : "";
        if (!arg.startsWith('-'))
            return argv[i];
        if (takeValues.contains(arg))
            ++i;
    }
    return "";
}

// Brings the window forward for a second launch. Qt hands the launcher's
// activation token to the compositor; Hyprland is also asked directly, which
// takes it to the window's workspace either way.
void present(QQuickWindow *window, const QString &token)
{
    if (!token.isEmpty())
        qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
    window->show();
    window->raise();
    window->requestActivate();
    QProcess::startDetached("hyprctl", {"dispatch", QString("hl.dsp.focus({ window = \"pid:%1\" })")
                                                        .arg(QCoreApplication::applicationPid())});
}

// For --keys: "jj<Space>?" types j, j, Space, ?. Named keys go in <>.
void typeKeys(QQuickWindow *window, const QString &keys)
{
    static const QHash<QString, std::pair<int, QString>> named = {
        {"Esc", {Qt::Key_Escape, ""}},  {"Enter", {Qt::Key_Return, "\r"}}, {"Space", {Qt::Key_Space, " "}},
        {"Down", {Qt::Key_Down, ""}},   {"Up", {Qt::Key_Up, ""}},          {"Left", {Qt::Key_Left, ""}},
        {"Right", {Qt::Key_Right, ""}}, {"Tab", {Qt::Key_Tab, "\t"}},      {"BS", {Qt::Key_Backspace, ""}},
        {"PgDn", {Qt::Key_PageDown, ""}}, {"PgUp", {Qt::Key_PageUp, ""}},
    };
    for (int i = 0; i < keys.size(); ++i) {
        int key = 0;
        QString text;
        const int close = keys.indexOf('>', i);
        Qt::KeyboardModifiers mods = Qt::NoModifier;
        const QString token = close > i ? keys.mid(i + 1, close - i - 1) : QString();
        if (keys[i] == '<' && named.contains(token)) {
            std::tie(key, text) = named.value(token);
            i = close;
        } else if (keys[i] == '<' && token.size() >= 3 && (token.startsWith("C-") || token.startsWith("A-"))) {
            mods = token.startsWith("C-") ? Qt::ControlModifier : Qt::AltModifier;
            key = named.contains(token.mid(2)) ? named.value(token.mid(2)).first : token[2].toUpper().unicode();
            i = close;
        } else {
            text = keys[i];
            key = keys[i].toUpper().unicode();
        }
        if (!text.isEmpty() && text[0].isUpper())
            mods |= Qt::ShiftModifier;
        QKeyEvent press(QEvent::KeyPress, key, mods, text);
        QKeyEvent release(QEvent::KeyRelease, key, mods, text);
        QCoreApplication::sendEvent(window, &press);
        QCoreApplication::sendEvent(window, &release);
        QCoreApplication::processEvents();
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QElapsedTimer started;
    started.start();

    // Everything Bibliora shows comes from the Omarchy theme, so Qt's platform
    // theme has nothing to offer it, and Omarchy's (gtk3) takes about 45 ms to
    // load: half of starting up.
    qunsetenv("QT_QPA_PLATFORMTHEME");
    QGuiApplication::setApplicationName("bibliora");
    QGuiApplication::setApplicationDisplayName("Bibliora");
    QGuiApplication::setOrganizationName("bibliora");
    QGuiApplication::setDesktopFileName("bibliora");
    QGuiApplication::setApplicationVersion(BIBLIORA_VERSION);

    // Commands run without a window, so they work over ssh and in scripts.
    const char *command = commandWord(argc, argv);
    const bool cli = !std::strcmp(command, "show") || !std::strcmp(command, "search") || !std::strcmp(command, "define")
                     || !std::strcmp(command, "skill");
    std::unique_ptr<QCoreApplication> app(cli ? new QCoreApplication(argc, argv) : new QGuiApplication(argc, argv));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Read the Bible.\n\n"
        "  bibliora [reference]               open the window (at John 3:16, a bibliora:// link, …)\n"
        "  bibliora show <reference>          print a passage: john 3:16-18, ps 23, jude\n"
        "  bibliora search <words>            print the verses that have every word\n"
        "  bibliora define <word>             print the dictionary entry\n"
        "  bibliora skill install|remove      link the skill for AI agents, or take it away");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"t", "translation"}, "With show or search: ASV, KJV, LSB, NASB or NKJV (KJV if not given).", "name"},
        {"json", "With show, search or define: print JSON."},
        {"limit", "With search: print at most <n> verses.", "n"},
        {"bible", "Read the Bible from <file> instead of the installed one.", "file"},
    });
    QCommandLineOption screenshotOption("screenshot", "Render to <png> and exit (with -platform offscreen).", "png");
    QCommandLineOption keysOption("keys", "Type these keys before --screenshot, e.g. \"jj<Space>?\".", "keys");
    QCommandLineOption sizeOption("size", "With --screenshot, the window's size, e.g. 1280x1400.", "size");
    QCommandLineOption replaceOption("replace", "Take over from the running Bibliora as it quits (bin/relaunch).");
    for (QCommandLineOption *hidden : {&screenshotOption, &keysOption, &sizeOption, &replaceOption}) {
        hidden->setFlags(QCommandLineOption::HiddenFromHelp);
        parser.addOption(*hidden);
    }
    parser.addPositionalArgument("reference", "Where to open, e.g. \"john 3:16\".", "[reference]");
    parser.process(*app);

    if (parser.isSet("bible"))
        Paths::setBiblePath(parser.value("bible"));
    g_json = parser.isSet("json");
    QStringList words = parser.positionalArguments();

    if (cli) {
        words.removeFirst();
        if (!std::strcmp(command, "skill"))
            return skill(words.value(0));
        Bible bible(Paths::biblePath());
        if (!bible.isOpen())
            return fail(bible.error());
        const Translation *t = translationFrom(parser);
        if (!t)
            return fail(QStringLiteral("There's no translation called “%1”. Try ASV, KJV, LSB, NASB or NKJV.")
                            .arg(parser.value("translation")),
                        2);
        if (words.isEmpty())
            return fail(QStringLiteral("Say what to %1, e.g. bibliora %1 %2.")
                            .arg(command, !std::strcmp(command, "show")     ? "john 3:16"
                                          : !std::strcmp(command, "search") ? "bosom abraham"
                                                                            : "firmament"),
                        2);
        if (!std::strcmp(command, "show"))
            return show(bible, *t, words.join(' '));
        if (!std::strcmp(command, "search"))
            return search(bible, *t, words.join(' '), parser.value("limit").toInt());
        return define(bible, words.join(' '));
    }

    // Where to open: words ("john 3:16") or a bibliora:// link from xdg-open.
    const QString open = words.join(' ');

    // A screenshot stands alone: no instance, and its own remembered place.
    const bool screenshot = parser.isSet(screenshotOption);
    if (screenshot)
        QGuiApplication::setOrganizationName("bibliora-screenshot");

    // Before anything else, so a second launch hands over and exits at once.
    Instance instance;
    QQuickWindow *window = nullptr;
    const auto raise = [&window](const QString &token, const QString &where) {
        if (!window)
            return;
        present(window, token);
        if (!where.isEmpty() && Reader::instance())
            Reader::instance()->goTo(where);
    };
    if (parser.isSet(replaceOption))
        instance.takeOver(raise);
    else if (!screenshot && !instance.claim(raise, qEnvironmentVariable("XDG_ACTIVATION_TOKEN"), open))
        return 0;

    new Theme(app.get());
    auto *reader = new Reader(app.get());
    quitOnTermination();
    if (!open.isEmpty())
        reader->goTo(open);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, app.get(),
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Bibliora", "Main");
    window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));

    // BIBLIORA_TIMING=1 says how long the first page took to reach the screen.
    if (window && qEnvironmentVariableIsSet("BIBLIORA_TIMING")) {
        std::fprintf(stderr, "bibliora: window loaded after %lld ms\n", static_cast<long long>(started.elapsed()));
        auto *once = new QObject(window);
        QObject::connect(window, &QQuickWindow::frameSwapped, once, [once, &started] {
            std::fprintf(stderr, "bibliora: first frame after %lld ms\n", static_cast<long long>(started.elapsed()));
            delete once;
            if (qEnvironmentVariable("BIBLIORA_TIMING") == QLatin1String("exit"))
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        });
    }

    if (!screenshot) {
        QTimer::singleShot(1500, reader, [reader] {
            if (QSettings().value(kSkillDeclined).toBool())
                return;
            const QString dir = Skill::installedDir(QCoreApplication::applicationDirPath());
            if (dir.isEmpty())
                return;
            const Skill::Links links = Skill::link(dir, agentFolders());
            if (!links.changed.isEmpty() && links.error.isEmpty())
                emit reader->toast(QStringLiteral("Linked the Bibliora skill for your AI agents"));
        });
    }

    if (screenshot) {
        if (!window)
            return 1;
        const QStringList size = parser.value(sizeOption).split('x');
        if (size.size() == 2)
            window->resize(size.at(0).toInt(), size.at(1).toInt());
        window->requestActivate();
        QTimer::singleShot(200, window, [window, &parser, keysOption, screenshotOption] {
            typeKeys(window, parser.value(keysOption));
            QTimer::singleShot(450, window, [window, &parser, screenshotOption] {
                const bool saved = window->grabWindow().save(parser.value(screenshotOption));
                QCoreApplication::exit(saved ? 0 : 1);
            });
        });
    }

    return app->exec();
}
