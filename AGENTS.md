# Bibliora for Omarchy

A native C++20 / Qt 6 (Qt Quick) Bible reader for Omarchy: the desktop port of
the Bibliora iOS app (`../ios`, SwiftUI). Built like Stocky and Billy: CMake,
Ninja, GCC, Qt 6.11, and a `Theme` singleton that follows the live Omarchy
theme. Three standing requirements weigh on every change: it must be **fast**,
**keyboard-first** with Omarchy-style keys, and **readable** above all.

## Build, run, test

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/bibliora
```

`bin/release` publishes the version in `CMakeLists.txt` as a GitHub release
with the Bible attached, which the Omarchy package downloads. It asks before
publishing; never run it unless the user asks for a release.

`bin/relaunch` (a Stop hook in `.claude/settings.json`) builds `build-release`,
installs to `~/.local`, and reopens the installed app on its workspace. The
user reads the installed app, not `./build/bibliora`.

Look at UI changes instead of guessing. `--screenshot` renders offscreen and
exits; `--keys` types first (named keys in `<>`: `<Esc>`, `<Enter>`, `<C-k>`);
`--size` sets the window. Screenshots keep their own settings
(`~/.config/bibliora-screenshot/`), so delete that folder for a clean start.

```bash
QT_SCALE_FACTOR=2 ./build/bibliora --screenshot /tmp/p.png -platform offscreen --size 1000x1240 "ps 23" --keys "t5n"
BIBLIORA_THEME_DIR=/usr/share/omarchy/themes/catppuccin-latte ./build/bibliora --screenshot …   # another theme
BIBLIORA_TIMING=exit QT_QPA_PLATFORM=offscreen ./build/bibliora --screenshot /dev/null   # ms to first frame
```

Check a dark and a light theme. Always pass `-platform offscreen` with
`--screenshot`, or a window flashes up on the user's screen.

## The Bible

`bibles.sqlite3` is the iOS app's database. It is read as is, read-only,
opened with `immutable=1`, at `~/.local/share/bibliora/bibles.sqlite3`
(`Paths`). It is not in git. **Don't invent schema here:** the database is
built elsewhere, for the iOS app, and changes to it belong there. The schema,
as used:

- `asv`, `kjv`, `lsb`, `nasb1995`, `nkjv`: `book` (snake_case key: `genesis`,
  `first_samuel`), `chapter`, `verse`, `text` (plain), `formatted_text`
  (markup), `id` (per translation; don't key on it, it drifts by one after
  Jude in LSB and NASB). Verses are keyed by book, chapter and verse
  (`VerseKey`, `Place`).
- `<t>_fts`: FTS5 over the plain text, rowids in canonical order.
- `words_grok`: the dictionary (`words_openai` is unused, as on iOS).

`Markup.h` documents the display markup and how it's read, which follows
`Chapter.swift`. Section headings are deliberately hidden, as on iOS. No red
letters (the database has none).

## Layout

```
src/app/     main.cpp (command line, single instance, screenshots), Instance
src/core/    Paths, Toml, Skill (linking the agent skill into agents' folders)
src/bible/   No UI, unit-tested: Books, Translations, Reference (parsing
             "jn 3:16" and bibliora:// links), Bible (SQLite), Markup,
             Typesetter (pages), History, Passage (the copy format), SearchQuery
src/ui/      QML-facing: Theme, Reader (the controller), PageItem (draws a
             page into the scene graph), SearchModel, GoToModel, ReadingFonts
qml/         Main.qml and the sheets (GoTo, Search, Translation, Dictionary, About,
             Keys)
resources/fonts/  The reading faces (OFL), loaded on first use
skills/bibliora/  The agent skill, installed with the app. It describes the
             command line, so change it when the commands change
tests/       Qt Test, one executable per tst_*.cpp; tst_bible needs the database
```

Sources and QML are globbed; adding a file needs only a re-run of
`cmake -S . -B build`. Header and QML names must be unique and must not shadow
QtQuick types (a C++ singleton named like one is silently ignored).

## How the page works

`Reader` owns where you are and lays chapters out with `Typesetter`, which
uses `QTextLayout` on a fixed baseline grid: a chapter starts on its own page
(as on iOS), paragraphs indent their first line, poetry lines hang, titles
keep with the next line, and a paragraph never leaves one line alone at a page
edge. `PageItem` draws a page's lines with `QSGTextNode`. Glyph runs ignore
superscript offsets, so verse numbers stay in the text invisibly (for line
breaking) and are drawn raised as separate `marks`. Navigation goes through
`Reader::show`, which sets book, chapter and page together (the iOS app's
ADR 014). Page turns replace the current history entry; jumps add one.

The iOS app's tests are the spec for history (`tst_history`) and the copy
format (`tst_bible`). Keep them passing.

## Conventions

- Colors, the interface font and sizes come from `Theme` (and `Theme.sp()`);
  the reading font and its size are `Reader`'s. Never hard-code a color.
- Keys: add new ones to `KeysSheet.qml` and the README.
- Copy is sentence case and plain, like the rest of the user's apps.
- Commit messages are one plain sentence in the imperative, like the history.
  No AI attribution lines.
