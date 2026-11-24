# Implicit state machines in Bibliora for Omarchy

A read-only review, 2026-11-24. Nothing here has been changed. Each finding is
some state the app tracks in loose flags or fields, changed from several places
with no single place that says which change may follow which. For each one:
where it is, what its states are, a bug it causes (or could), what an explicit
state machine would look like, and roughly how big a change that is.

Line numbers are for master at `2275ae8`. **Reproduced** means the bug was seen
in a screenshot from a scratch build (`--screenshot -platform offscreen`, with
its own settings folder). **Verified** means traced through the code but not
run, usually because it needs the mouse or a second launch, which screenshot
mode can't do. **Possible** means the structure allows it, but something may
stop it happening.

Reviewed: everything in `src/` and `qml/`. Neither `Reader` nor `SearchModel`
has a unit test today, so none of the state below is under test.

| # | State | Worth it? | Size |
|---|---|---|---|
| 1 | Which verse stays in view when the page is laid out again | Yes: two reproduced bugs | Medium |
| 2 | Picking verses (`m_selecting`, anchor, cursor) | Yes: loses a history entry; wrong range after a ctrl-click | Small to medium |
| 3 | Search status, and Enter waiting for results | Yes: stale results under "No verses"; a jump after closing | Small to medium |
| 4 | Which sheet is open | Lower: robustness against outside jumps | Small |
| 5 | Smaller ones | Low | Small |

Some things that sound like state machines turned out not to be. Keys have no
count prefix and no pending prefix: `t5n` is three separate keys. History's
back and forward is a small class with its own tests (`tst_history`). Theme
reloads go through one debounce timer, and fonts are loaded through a cache.

---

## 1. Which verse stays in view when the page is laid out again

**Where:** `src/ui/Reader.h:189-190` (`m_page`, `m_pendingVerse`), `:201`
(`m_relayoutTimer`); `src/ui/Reader.cpp:113-122` (`currentPlace`), `:146`
(`setViewport`), `:162-214` (`restyle`: the anchor at `:201`, the page at
`:206`), `:254-287` (`show`, which sets `m_pendingVerse` at `:262`), `:433-447`
(`setTranslation`), `:75` (every `Theme::changed` calls `restyle`);
`src/bible/Typesetter.cpp:338-347` (`Page::firstVerse`).

**States:** *no page yet* (no layout, only `m_pendingVerse` counts: at start,
and when `main.cpp:492` opens a reference before the window exists), *laid
out* (`m_page` counts; `m_pendingVerse` is still written by every `show` but
ignored), *switching translation* (`:440-441` drops the layout and sets
`m_pendingVerse`), and *waiting to re-lay out* after a resize (the 40 ms
timer). The verse that must stay in view is never stored. Every `restyle`
works it out again from `currentPlace().verse`, which is the page's
`firstVerse`: the first verse that *starts* on the page, or if none does, the
verse running on from the page before. `restyle` runs after a resize, a theme
change (colors only included), a font, size or verse-number change, and a
translation change.

**Bugs:**

- **Reproduced: opening a verse and then resizing loses the verse.**
  `bibliora "jn 3:16"` first lays out at the window's default 1000×1240
  (`qml/Main.qml:9-10`), where the page shows John 3:1-20 with verse 16
  flashed. The window is then resized, here with `--size 900x500`. The new
  layout keeps that page's `firstVerse`, 3:1, so the page shows 3:1-7 and 3:16
  is not on screen. Under Hyprland a tiled window is resized by its first
  configure, so opening a reference or a `bibliora://` link when Bibliora
  isn't already running very likely does this every time (not run under
  Hyprland itself). Any resize after a search or Go to jump into the middle of
  a chapter does it too, and so does any `--screenshot` with a verse and a
  `--size` other than 1000×1240.
- **Reproduced: a page inside one long verse moves back a page on any
  re-layout.** In a 420×420 window, Go to
  Esther 8:9 and press `l` to reach a page that lies wholly inside that verse.
  Press `n` twice (verse numbers off and on, no net change): you're on the page
  before. `firstVerse` falls back to the verse that started on an earlier page
  (`Typesetter.cpp:342`), and `pageOfVerse` returns the page where it starts. A
  color-only theme change, `f`, Ctrl+plus/minus or a resize does the same. It
  needs a verse longer than a page, so a small window or large text.

**Explicit version:** store the place to keep, set only by navigation:

```cpp
struct Anchor {
    VerseKey verse;      // a jump: the target; a page turn: the verse at the top
    int offsetInVerse;   // how far into that verse the page starts
};
```

`show()` sets it (offset 0 for a jump; the page's first character for a page
turn), and `restyle()` maps it to a page and never derives it from the layout
it is replacing. `m_pendingVerse` and the *no page yet* case fold into it: the
anchor is valid before there's a window, and the layout is simply absent. The
first character on a page is known from the typesetter's line starts, and
`Markup::Block::verses` maps it to a verse.

**Size:** about 60-100 lines in `Reader.cpp`/`.h`, plus a "page of this text
position" helper in `Typesetter`, which can be unit-tested. Medium risk: every
way of moving goes through it.

---

## 2. Picking verses

**Where:** `src/ui/Reader.h:207-209` (`m_selecting`, `m_selection`,
`m_anchor`, `m_cursor`); `src/ui/Reader.cpp:387` (a Go to range turns picking
on), `:530-537` (`setSelection`, which can only turn it off), `:590-604`
(`reveal`), `:606-620` (`startSelection`), `:622-633` (`moveCursor`),
`:635-655` (`clickVerse`), `:671-675` (`clearSelection`); keys at
`qml/Main.qml:57-72`; clicks from `src/ui/PageItem.cpp:124`.

**States:** *not picking* (empty) or *picking*, where the set is either a
range from the anchor to the cursor (`v`, `j`/`k`, shift-click, a Go to range)
or a set of separate verses (ctrl-click). Nothing records which kind it is,
and the anchor and cursor only mean something for a range. Every caller must
set `m_selecting = true` before calling `setSelection` (`:387`, `:618`,
`:630`, `:653`). The selection is kept when you jump elsewhere, as on iOS,
but here a keyboard cursor stays behind with it.

**Bugs:**

- **Reproduced (saved history): `j` after a jump overwrites the jump's
  history entry.** Press `v` (picking at Genesis 1), then
  Go to John 3 with `:jn 3<Enter>`, then `j`. The cursor is still in Genesis 1,
  so `moveCursor` extends the range there and `reveal` brings it into view with
  `Visit::Replace` (`Reader.cpp:594`), which overwrites the history entry for
  John 3 with Genesis 1. Saved history becomes `1:1:1, 1:1:1` instead of
  `1:1:1, 43:3:1`, and Alt+Right never gets back to John 3.
- **Verified: ctrl-clicking a verse out of a range, then `k`, gives the wrong
  range.** `v`, `j`, `j` picks Genesis 1:1-3. Ctrl-click verse 3 to drop it:
  the set is {1, 2}, but `clickVerse` sets the anchor and cursor to verse 3,
  the verse just dropped (`:652`). `k` then picks the range from 3 to 2: verse
  1 is lost and verse 3 is back. Likewise, `j` after ctrl-picking separate
  verses throws them all away for a range from the last one clicked.

**Explicit version:** a plain class like `History`, unit-tested:

```cpp
class Selection {
public:
    enum class Kind { None, Range, Picked };
    void start(VerseKey at);
    void extend(int delta);         // j / k
    void toggle(VerseKey key);      // ctrl-click
    void extendTo(VerseKey key);    // shift-click
    void clear();
private:
    Kind m_kind = Kind::None;
    VerseKey m_anchor, m_cursor;
    QSet<VerseKey> m_verses;
};
```

`Picked` decides what `j` and `k` do (extend from the nearest picked verse,
say). For the history bug, pick one rule: `reveal` adds a history entry when
it leaves the current chapter, or a jump drops the keyboard cursor so `j`
starts again from the top of the new page while the picked verses stay.

**Size:** about 50-80 lines in `Reader` plus a test. Low risk.

---

## 3. Search status, and Enter waiting for results

**Where:** `src/ui/SearchModel.cpp:148-159` (`summary`), `:163-210`
(`rebuild`: old verse rows kept at `:197-202`, `m_busy` at `:203`, `m_total`
at `:204-205`), `:212-232` (`finished`, which resets `m_selected` at `:229`),
`:234` (`activate`); `qml/SearchSheet.qml:20` (`goWhenReady`), `:22-34`
(`go`), `:36-45` (its `Connections`), `:50` (`onClosed`).

**States:** *history* (empty query), *searching* (`m_busy`, showing the
previous query's verses so the list doesn't flicker), *done*, and *nothing to
search*: a query with no searchable word, such as a single letter, for which
`SearchQuery::build` returns nothing. A second state lives in QML:
`goWhenReady`, "Enter was pressed, go when the results come". It is cleared
by `go()` and by a new query, but not by closing the sheet.

**Bugs:**

- **Reproduced: "No verses" over the old results, and Enter opens one.**
  Search for `love`, let the results come, then backspace to `l`. A single
  letter has nothing to search, so `rebuild` keeps the old verse rows
  (`:197-202`) but sets the total to 0: the sheet says "No verses in the KJV"
  above a full list of "love" verses. Enter then opens the first, Genesis
  22:2. Typing `lo` and one backspace does it too.
- **Reproduced (saved history): Enter while searching, then Esc, still
  jumps after the sheet has closed.** From Psalm 23, type
  `/the<Enter><Esc>`. Enter arms `goWhenReady`, Esc closes the sheet without
  clearing it, and about 70 ms later the results come and `go()` opens Genesis
  1:1 behind the closed sheet. It needs a search slower than the gap between
  Enter and Esc, so it's rare by hand.
- **Verified: clicking a row while a search is running goes to a different
  verse.** Clicking one of the old rows (or arrowing to it and pressing Enter)
  while busy selects it and arms `goWhenReady`. When the results come,
  `finished` sets the selection back to row 0 (`:229`), so you land on the new
  first result, not the verse you clicked.

**Explicit version:** `enum class Status { History, NothingToSearch,
Searching, Done }` with one place that sets the rows and the summary for each.
`NothingToSearch` drops the verse rows. The pending Enter moves into the
model as `std::optional<int> m_pendingRow`, holding the row actually chosen,
and is cancelled by a new query, by closing the sheet, and by a new search
starting.

**Size:** about 50-70 lines in `SearchModel.cpp`/`.h` and `SearchSheet.qml`.
Low risk, and a natural place to add a `tst_searchmodel`.

---

## 4. Which sheet is open

**Where:** `qml/Main.qml:229-275` (six lazily loaded sheets, each with its
own `open`, `qml/Sheet.qml:8`, `:15-20`); `src/app/main.cpp:476-482` (a second
launch hands its reference to this one, which jumps without closing
anything); `src/ui/GoToModel.cpp:86`, `:145` (Go to's recent rows store a
history *index*); `src/bible/History.cpp:43-62` (`add` removes the earlier
visit to the same chapter, which renumbers the list).

**States:** *reading*, *reading and picking*, or *sheet X open*, held in six
booleans and keyboard focus. Two sheets at once isn't reachable today: keys
reach the reader only while it has focus, and the one sheet-to-sheet step
(Dictionary to Search, `Main.qml:258-261`) closes first. But things that
don't go through focus still move the reader under an open sheet.

**Bugs:**

- **Verified: a second launch while Go to is open makes its recent rows go to
  the wrong place.** History is A, B, C, at C. Open Go to: the recent rows are
  B (index 1) and A (index 0). Now run `bibliora <A>` from a terminal or an
  agent. `History::add` removes the old A and appends it: B, C, A. Pick the B
  row: `goToHistory(1)` opens C. The page also changes under any open sheet
  without it noticing; the Dictionary's "on this page" words are then for
  another page. (Not reproduced: screenshot mode skips the single-instance
  handoff.)
- **Possible:** a sheet asked to open before its loader has finished does
  nothing (`Main.qml:237` and the others: `if (item) item.show()`), so `g` or
  `/` pressed straight after launch can be dropped. Not measured.
- **Possible:** the mouse wheel may still turn the page under an open sheet,
  since the sheet's mouse areas don't take wheel events. Not run.

**Explicit version:** in `Main.qml`, one `property string sheet: ""` with
`openSheet(name, arg)` and `closeSheet()`, each sheet's `open` bound to
`window.sheet === name`; a jump from outside closes the sheet; a request made
before the sheet has loaded is kept until it has. Go to's recent rows carry
the place, not an index (or are rebuilt when history changes).

**Size:** about 40 lines of QML plus a few in `GoToModel`. Low risk.

---

## 5. Smaller ones

- **Single instance** (`src/app/Instance.cpp:48-80`): *primary*,
  *secondary*, and two rare cases. If pinging the running copy fails and
  listening fails twice, `claim` carries on without serving, so later launches
  are never handed to this window. `takeOver` (used only by `bin/relaunch`)
  retries every 200 ms with no end. Both are possible narrow races; an
  `enum class Role` would document them but changes nothing a user sees.
- **The reader moves while the Bible is missing:** `Reader.ready` is fixed at
  start, but keys still call `show()`, so with no Bible, Space moves through
  chapters you can't see and saves that history. Trivial.
- **"On this page" cache key** (`src/ui/Reader.cpp:708`, `:724`): it includes
  the layout's address, which a new layout can reuse after the old one is
  freed, giving the Dictionary a stale word list. Possible; a layout
  generation number fixes it in a few lines.
- **Looked at, no change needed:** the flash animation, History, the theme
  watcher (whose only harm is that every theme change, colors only included,
  re-lays out the page and so triggers item 1), and screenshot mode's fixed
  timers.

## Suggested order

1. Item 1: two reproduced bugs, one likely on every launch with a reference.
2. Item 3: small, easy to hit, and the stale list is plainly wrong.
3. Item 2: a lost history entry and wrong ranges.
4. Item 4: only if outside jumps while a sheet is open turn out to matter.
