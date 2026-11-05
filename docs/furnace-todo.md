# Furnace exchange: what is missing or left out

Kept so these can be improved later. Written 05-11-2026, for the work in `src/gfurnace*.c`, `gfurnaceimp.c`,
`gfurnacesong.c`, `gfurnacesongimp.c` and the Furnace dialog in `gfurnaceui.c`.

## Instrument export (.fui)
- Vibrato of the instrument (delay and speedtable entry) is not exported.
- Pattern commands inside the wavetable are not exported.
- Gate timer / hard restart is not exported (Furnace has 15xx envelope reset time that could carry it).
- Ring or sync that is on for only part of the ticks: the file has one setting.
- A filter that picks voices: Furnace filters the voice the instrument plays on.
- Macros are cut at 255 ticks.

## Instrument import (.fui)
- Release point of a macro, ADSR and LFO macros, volume and other macros are ignored.
- Per-voice filter routing is lost (a routed filter filters all three voices).
- Pitch steps outside -32..+95 semitones are clamped; programs longer than 256 ticks are cut.
- Pulse width or pitch of the very first tick cannot be kept.
- An instrument whose program needs more than 255 table rows is refused (for example 257 rows in `africa.fur`).

## Song export (.fur)
- Written in song layout version 200. Not tried: Furnace newer than 0.6.8.3, and no listening test yet.
- Not exported: master volume (DXY), filter voices (BXY), the instrument's own vibrato, hard restart.
- Table pointers (8XY, 9XY, AXY) only work on a row with a note (they become an extra instrument).
- A tempo for one channel only (80-FF) has no Furnace counterpart.
- Channels that loop at different rows are lined up by playing until they meet (up to 256 orders); otherwise approximated.
- Pattern length is 16 to 64 rows, not the original lengths.
- Slide speeds were calibrated at one speed and checked at a few; vibrato speed is a whole number in Furnace.
- Pitch of notes is only checked where the instrument does not move the pitch itself.

## Song import (.fur)
- Refused: other chips, and the newest song layout (version 240 and up, INF2 block). Reading INF2 would help.
- Only the first subsong; grooves (speed patterns of more than two speeds) are approximated.
- Speeds below 3 (and above 127) are changed: timing drifts (3 of 298 test files).
- One command per row: the most important is kept. Several effects in a row lose all but one.
- Not carried over: volume column, pulse slides (22xx, 23xx), cutoff slides (24xx, 25xx), filter mode (14xx), pulse width
  (3xxx), note cut and delay (ECxx, EDxx), retrigger (0Cxx), pitch effects like E5xx. Pulse and cutoff slides were the most
  common (about a third of the files); a pulsetable or filtertable program per effect could carry them.
- The arpeggio replaces the instrument's own wavetable program while it plays.
- Jumps that are not the loop, and pattern breaks with a start row, are not followed exactly.
- More than 208 patterns, more than 8 SIDs, or an order list over 254 entries do not fit.
- Notes outside C-0..top of the GrooveGoat range are moved by octaves.
- Instruments above 63 are ignored; instruments with too many macro ticks are left empty.
- The patterns use no REPEAT or transpose to save order list space.

## Testing not done
- No listening test of any export or import; nothing run on Windows.
- Windows: the first report was that single instrument export wrote no file; the cause was never found (a failure dialog
  was added). Waiting for a test of a newer Windows build.
