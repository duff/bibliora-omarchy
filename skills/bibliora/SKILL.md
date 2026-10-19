---
name: bibliora
description: >
  Quote, find, compare and check Bible passages with the `bibliora` command,
  which reads the translations installed with Bibliora (ASV, KJV, LSB, NASB
  1995 and NKJV) word for word. Use it whenever the Bible is quoted or checked,
  instead of quoting from memory: putting a passage into a document, finding a
  verse someone half remembers, comparing translations, counting and listing
  where a word or phrase appears, checking the Bible references and quotations
  in a document, defining an old or unusual word, or opening a passage in the
  Bibliora window. Triggers: Bible, Scripture, verse, passage, chapter, a
  reference like "John 3:16" or "Rom 8:28-30", KJV, NKJV, NASB, LSB, ASV,
  Bibliora.
---

# Bibliora

`bibliora` is the command line of Bibliora, a Bible reader for Omarchy. It
reads the translations installed with it, on this computer, with no internet.
Quote the Bible from it, never from memory: memory gets wording wrong and mixes
translations, and this text is exact.

## Commands

- `bibliora show <references> -t <translations>` prints passages, a verse a
  line, each ending `-- Romans 8:28-30, NKJV`, with a blank line between them.
- `bibliora search <words> -t <translations>` prints every verse that has all
  the words, in Bible order, then how many there are. `--count` prints only
  how many, in all and in each book. `--limit <n>` prints at most n verses.
- `bibliora define <word>` prints the dictionary entry.
- `setsid -f bibliora <reference> >/dev/null 2>&1` opens the Bibliora window
  at a passage.

## Ask once, read little

Each command takes about 10 ms; what's slow is every extra command you run
and every line you read. So:

- Put everything in one command. `show` takes any number of references
  separated by `;` (`"rom 8:28-30; jn 3:16; ps 23"`), and `-t` takes several
  translations separated by commas (`-t kjv,nkjv`) or `-t all`.
- Read text unless you need to process the result; it's half the size of
  JSON.
- For how many or where, use `search --count`, not the verses. Add `--limit`
  when a few verses will do.

`--json` prints one line:

- `show`: `[{reference, translation, verses: [{chapter, verse, text}]}]`, one
  per reference and translation. One that isn't there is
  `{reference, translation, error}` in its place, and the rest still print.
- `search`: `[{query, translation, count, verses: [{reference, text}]}]`, one
  per translation; with `--count`, `books: [{book, count}]` instead of
  `verses`. `count` is always the full number of verses.
- `define`: `{word, pronunciations, senses: [{definition, example}], synonyms}`

They exit 0 when everything was found, 4 when something wasn't (a reference
that isn't there, no matches, no entry), 2 for a mistake in the command, and 1
when the Bible can't be read. `show` exits 4 if any reference is missing, but
still prints the others.

## Translations

`-t` takes `ASV`, `KJV`, `LSB`, `NASB` (the NASB 1995) or `NKJV`, in any case,
several separated by commas, or `all` (in that order). Without it, `show` and
`search` use the KJV, so pass `-t` whenever the user names a translation or
the document uses one. These five are all there is. If someone asks for
another (ESV, NIV, …), say Bibliora doesn't have it and offer these.

## References

`show` takes a book, a chapter, a verse, or a range of verses within one
chapter: `jude`, `ps 23`, `john 3:16`, `rom 8:28-30`, `1 cor 13:4-7`.
Abbreviations and any case work. A passage that runs across chapters, such as
Genesis 1:31–2:3, is one reference per chapter: `"gen 1:31; gen 2:1-3"`.

## Quoting

Copy the text exactly as `show` prints it: its words, punctuation, capitals
(LORD) and quotation marks. Don't modernize, correct or blend it. Unless the
user wants verse numbers, write the verses as one paragraph joined by spaces
and end with the reference and translation, the way Bibliora copies a passage:

```
And we know that all things work together for good to those who love God, …
-- Romans 8:28-30, NKJV
```

## Finding a verse someone half remembers

`search` needs words that are in the verse. Every word has to appear; case and
punctuation don't matter; `"quoted words"` must appear together; `word*`
matches the start of a word (`endur*`). Turn the memory into a few distinctive
words and try several searches: other words for the same idea, fewer words, and
other translations, whose wording differs. "Running the race with endurance"
is `race endurance` in the NKJV but `race patience` in the KJV (Hebrews 12:1).
Read the likely verses with `show`, offer the best with their references, and
say so when you aren't sure.

## Comparing translations

`bibliora show "esther 2:8" -t all` prints the passage in all five, each
ending with its translation's name. Set them side by side.

## How words are used

`bibliora search propitiation --count -t all` answers how often and where in
one command: the count in each book, for each translation. Counts are verses,
not occurrences: a verse with the word twice counts once. Say which
translation a count is for; counts differ between translations. Print the
verses themselves only when they're wanted. `faith works` finds verses with
both words; `"faith and works"` finds the phrase.

## Checking references in a document

Collect every reference in the document and `show` them all in one command,
in the translation the document uses or the user names:
`bibliora show "rom 8:28; jn 3:16; heb 12:1" -t nkjv --json`. A passage with
`error` doesn't exist. Compare any quoted wording with the translation
exactly: words, punctuation and capitals. Report each problem with its
reference, what the document has, and what the translation has. Change the
document only if asked.

## Opening the window

`setsid -f bibliora romans 1 >/dev/null 2>&1` opens Bibliora at Romans 1, or
moves its open window there. A verse (`john 3:16`) works too. Use `setsid -f`,
or the command holds the shell until the window closes. Open it only when the
user asks.

## The dictionary

`define` is the app's dictionary, general English written with Grok: good for
old or unusual words such as firmament, propitiation or sackcloth. It isn't a
Bible dictionary or a Greek or Hebrew lexicon; say so if that's what's wanted.

## Keep to the text

Bibliora has the text, a dictionary and the reader, nothing else: no
commentary, notes or cross-references. Anything you say about what a passage
means is your own; don't present it as Bibliora's. When something will quote a
lot of a copyrighted translation (LSB, NASB or NKJV), the translations'
copyright notices are in `/usr/share/licenses/bibliora/translations.txt`.
