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

- `bibliora show <reference> -t <translation>` prints a passage, a verse a
  line, then `-- Romans 8:28-30, NKJV`.
- `bibliora search <words> -t <translation>` prints every verse that has all
  the words, in Bible order, then how many there are.
- `bibliora define <word>` prints the dictionary entry.
- `setsid -f bibliora <reference> >/dev/null 2>&1` opens the Bibliora window
  at a passage.

Pass `--json` when you will read the result:

- `show`: `{reference, translation, verses: [{book, chapter, verse, text}]}`
- `search`: `{query, translation, count, verses: [{reference, book, chapter,
  verse, text}]}`. `--limit <n>` prints fewer verses; `count` still counts them
  all.
- `define`: `{word, pronunciations, senses: [{definition, example}], synonyms}`

They exit 0 when they find something, 4 when there is nothing (no such verse,
no matches, no entry), 2 for a mistake in the command, and 1 when the Bible
can't be read.

## Translations

`-t` takes `ASV`, `KJV`, `LSB`, `NASB` (the NASB 1995) or `NKJV`, in any case.
Without it, `show` and `search` use the KJV, so pass `-t` whenever the user
names a translation or the document uses one. These five are all there is. If
someone asks for another (ESV, NIV, …), say Bibliora doesn't have it and offer
these.

## References

`show` takes a book, a chapter, a verse, or a range of verses within one
chapter: `jude`, `ps 23`, `john 3:16`, `rom 8:28-30`, `1 cor 13:4-7`.
Abbreviations and any case work. A passage that runs across chapters, such as
Genesis 1:31–2:3, takes one `show` per chapter.

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

Run `show` once for each translation (`-t asv`, `kjv`, `lsb`, `nasb`, `nkjv`)
and set the results side by side, each with its translation's name.

## How words are used

`search --json` gives `count` and every verse. Counts are verses, not
occurrences: a verse with the word twice counts once. Group the verses by book
when asked where. Say which translation a count is for; counts differ between
translations, so for "the Bible" either name the translation or give each
one's. `faith works` finds verses with both words; `"faith and works"` finds
the phrase.

## Checking references in a document

For each reference, `show` it in the translation the document uses or the
user names; exit 4 means the reference doesn't exist. Compare any quoted
wording with the translation exactly: words, punctuation and capitals. Report
each problem with its reference, what the document has, and what the
translation has. Change the document only if asked.

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
