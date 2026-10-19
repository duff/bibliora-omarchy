# Bibliora for Omarchy

The Bible, to read on Omarchy, as a native Qt 6 app. It is the desktop
sibling of [Bibliora for iPhone and iPad](https://bibliora.org): the same
translations, the same quiet book-like pages, and no notes, highlights,
badges or verse of the day. Just reading.

- **Pages, like a book.** A chapter starts on a page of its own and you turn
  through it, on into the next chapter and the next book. Lines are kept to
  a comfortable length however wide the window is.
- **At home in Omarchy.** Colors come from the current theme and change when
  it does; menus and the running head use Omarchy's font and text size.
- **Keyboard first.** Everything is a key or two away, and `?` lists them.
- **Fast.** The first page is up about 50 ms after launch, search answers as
  you type, and the chapters either side are set ahead so turning into them
  is instant.

## Keys

| Keys | Action |
| --- | --- |
| `l` `j` space → ↓ page down | Next page |
| `h` `k` ← ↑ page up | Previous page |
| `]` `[` | Next or previous chapter |
| `}` `{` | Next or previous book |
| home, end | First or last page of the chapter |
| alt ←, alt → (or ctrl o, ctrl i, or the mouse's side buttons) | Back and forward through where you've been |
| `g` or `:` | Go to a book, chapter or verse |
| `c` | This book's chapters |
| `/` (or ctrl f) | Search the translation you're reading |
| `d` | Look up a word on the page (or right-click it) |
| `v` | Pick verses, starting at the top of the page (or ctrl-click a verse) |
| `j` `k` while picking | More or fewer verses (`J` `K` by five) |
| `y`, enter or ctrl c while picking | Copy them, with where they're from |
| `t` | Translation (then 1–5) |
| `n` | Verse numbers on or off |
| `f`, `F` | Next or previous reading font |
| ctrl +, ctrl −, ctrl 0 | Bigger, smaller, or the usual text |
| `?` | All of this |
| esc | Close, or let go of the picked verses |

The mouse works too: click beside the page to turn it, scroll to turn it, and
click the book, chapter or translation at the top to change them. Right-click
or double-click a word to look it up. Ctrl-click picks a verse and adds more,
and shift-click picks the run up to one. A plain click on the page lets go.

### Go to

`g` opens a launcher like Omarchy's own. With nothing typed it lists where
you've been, then every book. Type and it narrows: `jo` offers Joshua, Job,
Joel, Jonah and John; `john` lists John's chapters, each with how it begins;
`ps 2` gives Psalm 2 and then 20 to 29; `jn 3:16` goes straight to the verse
and lights it up for a moment. `3:16` alone means that verse in the book
you're in. A range like `rom 8:28-30` comes up picked, ready to copy. Tab
writes the row into the field.

### Search

`/` searches the translation you're reading as you type. Every word has to
be there, and the last one also matches the start of a word, so `shep` finds
shepherd. Put words in quotes for a phrase. Matches are in Bible order with
the words that matched in the theme's accent. A search that reads as a
reference (`gen 9`) also offers to go there. Esc keeps the search for next
time; with the field empty you get your past searches.

### Looking up a word

`d` opens the dictionary (the iPhone app's, written with Grok) on the words
of the page you're reading, the ones the Bible uses least first, so the word
you're after is usually at the top: on Genesis 1 that's firmament. ↑ ↓ or tab
move through them and the entry follows. Type to narrow them or to look up any
other word. Enter searches for the verses that use it.

### Copying

Picked verses copy the way the iPhone app copies them:

```
In the beginning God created the heavens and the earth. The earth was without
form, and void; and darkness was on the face of the deep. …
-- Genesis 1:1-3, NKJV
```

Verses that aren't next to each other come out as separate passages, each
with its own reference.

### Reading fonts

`f` steps through the faces, all bundled under the SIL Open Font License:

- **Literata**, what the iPhone app uses, made for long reading
- **Source Serif**
- **Charis**, after Matthew Carter's Charter
- **iA Writer Quattro**, a proportional face grown from a monospace one
- **Omarchy**, your Omarchy font

Literata and Source Serif adjust their optical size to the text size. LORD
and the psalm titles are in true small caps where the face has them.

## Command line

```bash
bibliora                         # open the window where you left off
bibliora john 3:16               # open it at John 3:16 (bibliora:// links work too)
bibliora show ps 23 -t nkjv      # print a passage
bibliora search bosom abraham    # print the verses that have every word
bibliora define firmament        # print the dictionary entry
```

`--json` prints JSON from `show`, `search` and `define`, and `-t` picks the
translation (ASV, KJV, LSB, NASB or NKJV; KJV if not given). Launching
Bibliora again brings the open window forward, at the reference if one was
given. Its window class is `bibliora`.

## The Bible

The translations have their own licenses, so they aren't in this
repository. Bibliora looks for them in `~/.local/share/bibliora/`, and
`--bible <file>` or `$BIBLIORA_DB` points at another copy.

## Build and run

```bash
omarchy pkg add cmake ninja            # once
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/bibliora
```

`bin/relaunch` builds a release, installs it to `~/.local`, and when that
changed Bibliora, reopens it where its window was. Claude Code runs it after
every turn (`.claude/settings.json`).

## Install

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-release
cmake --install build-release          # ~/.local/bin/bibliora + launcher entry
```

Then launch it from the apps menu (Super + Alt + Space → Bibliora).

## Settings

Where you are, where you've been, the translation, verse numbers, the font
and the text size are kept in `~/.config/bibliora/bibliora.conf`.
