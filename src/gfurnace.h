//
// C64 GrooveGoat - export of an instrument to a Furnace instrument file (.fui)
//
// Furnace (the multi-system tracker) has a C64 instrument type with ADSR, waveform flags, ring/sync, a pulse width,
// a filter and macros (a value per tick for the waveform, the pitch offset, the pulse width and the filter cutoff).
// A GoatTracker instrument is a set of programs that change those things tick by tick, so the export replays the
// wavetable, pulsetable and filtertable of the instrument (the same replays as the table summaries, which follow
// the playroutine) and writes what they did as macros. No drawing and no files in here, only bytes.
//
// What is exported: AD/SR, waveform, ring, sync, the pitch steps of the wavetable (relative and absolute notes),
// the pulse width, the filter (passband, resonance, cutoff, voices). What is not: vibrato, pattern commands in the
// wavetable, the gate timer / hard restart, and a filter that changes per voice. FURNACE_W_... tell which of those
// apply, so the editor can say so.
//

#ifndef GFURNACE_H
#define GFURNACE_H

#include "gcommon.h"

#define FURNACE_FILEVERSION 199          // the engine version written to the file (Furnace 0.6 and newer read it)
#define FURNACE_MAXMACRO 255             // a macro has at most this many values (its length is one byte)
#define FURNACE_MAXFILE 4096             // a buffer this size always holds one exported instrument

// Things in the instrument that the file does not carry the same way (bit mask of info->warnings)
#define FURNACE_W_VIBRATO 0x01           // the instrument has vibrato (a delay and a speedtable entry)
#define FURNACE_W_WAVECMDS 0x02          // the wavetable runs pattern commands (portamento, vibrato ...)
#define FURNACE_W_RINGSYNC 0x04          // ring or sync is on for only a part of the ticks (the file has one setting)
#define FURNACE_W_TRUNCATED 0x08         // a macro was cut at 255 ticks (the program goes on without repeating)
#define FURNACE_W_FILTERVOICES 0x10      // the filter table picks voices (the file filters the voice it is played on)
#define FURNACE_W_NOWAVETABLE 0x20       // no wavetable: one waveform for the whole note

typedef struct
{
  int size;                              // bytes written, 0 on failure
  int warnings;                          // FURNACE_W_...
  int macros;                            // number of macros in the file
  int wavelen, pitchlen, dutylen, cutlen; // length of the waveform, pitch, pulse and cutoff macros (0 = none)
  int filter;                            // 1 if the instrument has a filter program
} FURNACEINFO;

// Writes the instrument as a .fui file image into out (outsize bytes, FURNACE_MAXFILE is always enough).
// The tables are the 255 entry columns of the song (the wavetable, pulsetable and filtertable).
// Returns the size, or 0 if it does not fit.
int furnace_instrument(unsigned char *out, int outsize, const INSTR *ins,
                       const unsigned char *wleft, const unsigned char *wright,
                       const unsigned char *pleft, const unsigned char *pright,
                       const unsigned char *fleft, const unsigned char *fright,
                       FURNACEINFO *info);

// A plain language list of the warnings ("vibrato, pattern commands in the wavetable"), "" if there are none
void furnace_warningtext(char *buf, int size, int warnings);

// The file name for an instrument: its name with everything but letters, digits, space, - and _ taken away,
// ".fui" added; "Instr NN.fui" when nothing is left.
void furnace_filename(char *buf, int size, const INSTR *ins, int number);

#endif
