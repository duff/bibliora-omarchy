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
#include <QRegularExpression>
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

// One line, so agents and scripts read as little as possible; pipe it to jq
// to read it yourself.
void printJson(const QJsonValue &v)
{
    const QJsonDocument doc = v.isArray() ? QJsonDocument(v.toArray()) : QJsonDocument(v.toObject());
    print(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)) + '\n');
}

int fail(const QString &message, int code = 1)
{
    std::fflush(stdout); // so a message lands in place among what's printed
    std::fprintf(stderr, "bibliora: %s\n", message.toUtf8().constData());
    return code;
}

// -t nkjv, -t kjv,nkjv or -t all; the KJV if not given. Empty for a name
// that isn't a translation, which is put in `unknown`.
QList<const Translation *> translationsFrom(const QCommandLineParser &parser, QString *unknown)
{
    if (!parser.isSet("translation"))
        return {&Translations::fallback()};
    QList<const Translation *> out;
    for (const QString &name : parser.value("translation").split(',', Qt::SkipEmptyParts)) {
        QList<const Translation *> these;
        if (name.trimmed().compare(QLatin1String("all"), Qt::CaseInsensitive) == 0) {
            for (const Translation &t : Translations::all())
                these.append(&t);
        } else if (const Translation *t = Translations::find(name)) {
            these.append(t);
        } else {
            *unknown = name.trimmed();
            return {};
        }
        for (const Translation *t : std::as_const(these))
            if (!out.contains(t))
                out.append(t);
    }
    return out;
}

QList<VerseText> passage(Bible &bible, const Translation &t, const Reference &ref)
{
    QList<VerseText> verses;
    const int firstChapter = std::max(1, ref.chapter);
    const int lastChapter = ref.chapter ? ref.chapter : ref.book->chapters;
    for (int c = firstChapter; c <= lastChapter; ++c)
        verses += bible.verses(t, *ref.book, c, ref.verse ? ref.verse : 1,
                               ref.endVerse ? ref.endVerse : ref.verse ? ref.verse : 0);
    return verses;
}

// bibliora show "rom 8:28-30; john 3:16" -t nkjv
//
// Each reference in each translation, in that order. One that isn't there is
// reported in its place and the rest still print, so a whole document's
// references can be checked at once.
int show(Bible &bible, const QList<const Translation *> &translations, const QString &text)
{
    QStringList wanted;
    for (const QString &part : text.split(QRegularExpression(QStringLiteral("[;\n]")), Qt::SkipEmptyParts))
        if (!part.trimmed().isEmpty())
            wanted.append(part.trimmed());
    if (wanted.isEmpty())
        return fail(QStringLiteral("Say what to show, e.g. bibliora show john 3:16."), 2);
    QJsonArray passages;
    bool missing = false;
    bool any = false;
    for (const QString &one : std::as_const(wanted)) {
        const auto ref = Reference::parse(one);
        if (!ref) {
            const QString problem = QStringLiteral("“%1” isn't a book, chapter or verse.").arg(one);
            missing = true;
            if (g_json)
                passages.append(QJsonObject{{"reference", one}, {"error", problem}});
            else
                fail(problem);
            continue;
        }
        for (const Translation *t : translations) {
            const QList<VerseText> verses = passage(bible, *t, *ref);
            if (verses.isEmpty()) {
                const QString problem = QStringLiteral("%1 isn't in the %2.").arg(ref->label(), t->abbrev);
                missing = true;
                if (g_json)
                    passages.append(QJsonObject{{"reference", ref->label()}, {"translation", t->key}, {"error", problem}});
                else
                    fail(problem);
                continue;
            }
            if (g_json) {
                QJsonArray list;
                for (const VerseText &v : verses)
                    list.append(QJsonObject{{"chapter", v.chapter}, {"verse", v.verse}, {"text", v.text}});
                passages.append(QJsonObject{{"reference", ref->label()}, {"translation", t->key}, {"verses", list}});
                continue;
            }
            if (any)
                print("\n");
            any = true;
            const bool chapters = verses.first().chapter != verses.last().chapter;
            int chapter = 0;
            for (const VerseText &v : verses) {
                if (v.chapter != chapter && chapters)
                    print(QStringLiteral("%1%2 %3\n").arg(chapter ? "\n" : "", v.book->name).arg(v.chapter));
                chapter = v.chapter;
                print(QStringLiteral("%1 %2\n").arg(v.verse).arg(v.text));
            }
            print(QStringLiteral("-- %1, %2\n").arg(ref->label(), t->key));
        }
    }
    if (g_json)
        printJson(passages);
    return missing ? 4 : 0;
}

// bibliora search "bosom abraham" -t nkjv, or with --count only how many
// verses, in all and in each book.
int search(Bible &bible, const QList<const Translation *> &translations, const QString &words, int limit,
           bool countOnly)
{
    const QString fts = SearchQuery::build(words + ' ');
    if (fts.isEmpty())
        return fail(QStringLiteral("Search for a word or two."), 2);
    const bool tty = isatty(fileno(stdout));
    QJsonArray results;
    int found = 0;
    for (const Translation *t : translations) {
        const int total = bible.searchCount(*t, fts);
        found += total;
        if (!g_json && t != translations.first())
            print("\n");
        if (countOnly) {
            const auto books = bible.searchCountByBook(*t, fts);
            if (g_json) {
                QJsonArray list;
                for (const auto &[book, count] : books)
                    list.append(QJsonObject{{"book", book->name}, {"count", count}});
                results.append(QJsonObject{{"query", words}, {"translation", t->key}, {"count", total}, {"books", list}});
                continue;
            }
            qsizetype width = 0;
            for (const auto &[book, count] : books)
                width = std::max(width, book->name.size());
            for (const auto &[book, count] : books)
                print(QStringLiteral("%1  %2\n").arg(book->name, -int(width)).arg(count));
        } else {
            const auto hits = bible.search(*t, fts, limit > 0 ? limit : -1);
            if (g_json) {
                QJsonArray list;
                for (const SearchHit &h : hits) {
                    QString text = h.text;
                    text.remove(QChar(1)).remove(QChar(2));
                    list.append(QJsonObject{{"reference", QStringLiteral("%1 %2:%3").arg(h.book->name).arg(h.chapter).arg(h.verse)},
                                            {"text", text}});
                }
                results.append(QJsonObject{{"query", words}, {"translation", t->key}, {"count", total}, {"verses", list}});
                continue;
            }
            for (const SearchHit &h : hits) {
                QString text = h.text;
                text.replace(QChar(1), tty ? QStringLiteral("\x1b[1m") : QString());
                text.replace(QChar(2), tty ? QStringLiteral("\x1b[0m") : QString());
                print(QStringLiteral("%1 %2:%3  %4\n").arg(h.book->name).arg(h.chapter).arg(h.verse).arg(text));
            }
            if (hits.size() < total) {
                print(QStringLiteral("%1 %2 in the %3 (%4 shown)\n")
                          .arg(total)
                          .arg(total == 1 ? "verse" : "verses", t->abbrev)
                          .arg(hits.size()));
                continue;
            }
        }
        print(QStringLiteral("%1 %2 in the %3\n").arg(total).arg(total == 1 ? "verse" : "verses", t->abbrev));
    }
    if (g_json)
        printJson(results);
    return found ? 0 : 4;
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
        "  bibliora show <references>         print passages: john 3:16-18, ps 23, jude; several with ;\n"
        "  bibliora search <words>            print the verses that have every word\n"
        "  bibliora define <word>             print the dictionary entry\n"
        "  bibliora skill install|remove      link the skill for AI agents, or take it away");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"t", "translation"},
         "With show or search: ASV, KJV, LSB, NASB or NKJV, several with commas, or all (KJV if not given).",
         "names"},
        {"json", "With show, search or define: print JSON, on one line."},
        {"limit", "With search: print at most <n> verses from each translation.", "n"},
        {"count", "With search: print how many verses, in all and in each book, not the verses."},
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
        QString unknown;
        const QList<const Translation *> translations = translationsFrom(parser, &unknown);
        if (translations.isEmpty())
            return fail(QStringLiteral("There's no translation called “%1”. Try ASV, KJV, LSB, NASB, NKJV or all.")
                            .arg(unknown),
                        2);
        if (words.isEmpty())
            return fail(QStringLiteral("Say what to %1, e.g. bibliora %1 %2.")
                            .arg(command, !std::strcmp(command, "show")     ? "john 3:16"
                                          : !std::strcmp(command, "search") ? "bosom abraham"
                                                                            : "firmament"),
                        2);
        if (!std::strcmp(command, "show"))
            return show(bible, translations, words.join(' '));
        if (!std::strcmp(command, "search"))
            return search(bible, translations, words.join(' '), parser.value("limit").toInt(), parser.isSet("count"));
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
