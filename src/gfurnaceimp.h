//
// C64 GrooveGoat - import of a Furnace C64 instrument file (.fui) as a GoatTracker instrument
//
// The reverse of gfurnace.c: the ADSR, the first waveform and the macros of the file become an instrument with
// step programs (STEPMODEL, see gstepedit.h), from which the editor makes wavetable, pulsetable and filtertable
// rows (step_generate). The macros are replayed tick by tick, with their delay, speed and loop, so what the file
// does tick by tick is what the programs do. No drawing and no files in here, only bytes.
//
// Timing is the reverse of the export: the first tick of a note only sets the first waveform, so tick 0 of the
// macros is the first waveform and tick k+1 is step k of the wave and pulse programs; the filter runs on every tick.
//

#ifndef GFURNACEIMP_H
#define GFURNACEIMP_H

#include "gcommon.h"
#include "gstepedit.h"

#define FIMP_OK 0
#define FIMP_NOTFUI 1                    // not a Furnace instrument file (or one this reader cannot parse)
#define FIMP_NOTC64 2                    // an instrument of another chip
#define FIMP_BAD 3                       // damaged: a block runs past the end of the file

// Things in the file that the instrument does not carry the same way (bit mask of warnings)
#define FIMP_W_RELEASE 0x01              // a macro has a release point: it is played through without waiting for note off
#define FIMP_W_MODE 0x02                 // an ADSR or LFO macro (not a sequence): ignored
#define FIMP_W_TOOLONG 0x04              // a program needed more than STEP_MAX ticks: it is cut
#define FIMP_W_NOTERANGE 0x08            // a pitch step outside -32..+95 semitones: clamped
#define FIMP_W_VOICES 0x10               // the filter routes this voice: the instrument filters all three voices
#define FIMP_W_OTHER 0x20                // macros of other kinds (volume, panning ...) or extended settings: ignored
#define FIMP_W_TICK0 0x40                // the pitch or pulse width of the very first tick differs: not expressible

typedef struct
{
  char name[MAX_INSTRNAMELEN + 1];       // "" if the file has none
  int ad, sr;                            // the same bytes as the SID registers
  int firstwave;                         // the waveform register value of the first tick
  int haswave, haspulse, hasfilter;      // 1 if the program is needed (wave, pulse and filter step programs below)
  STEPMODEL wave, pulse, filter;
  int filtervaries;                      // 1 if the passband, resonance or voices change on the way (see below)
  int ftype[STEP_MAX], fctrl[STEP_MAX];  // then, per tick of the filter program: the passband bits and resonance/voices
  int warnings;                          // FIMP_W_...
} FURNACEIMPORT;

// Reads a .fui file image. Returns FIMP_OK (out is filled) or an error. out is large: keep it static.
int furnace_import(const unsigned char *data, int size, FURNACEIMPORT *out);

// The filtertable rows of the imported filter program, written to l/r (at least 255 entries each), starting at table row
// startrow (needed for the loop jump). Returns the number of rows. A program whose passband, resonance or voices stay
// the same is made like the step editor's (step_generate); one that changes them gets a set row at every change.
int furnace_filterrows(const FURNACEIMPORT *imp, unsigned char *l, unsigned char *r, int startrow);

// A plain language list of the warnings, "" if there are none
void furnace_importwarningtext(char *buf, int size, int warnings);

#endif
