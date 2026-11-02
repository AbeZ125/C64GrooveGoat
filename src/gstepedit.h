//
// C64 GrooveGoat - the step editor model: a wave or pulse program as one value per tick
//
// The user edits "tick 1 is a square, tick 2 a square, tick 3 a pulse, ...". This module turns such a list into
// table rows (and the other way round, using the table summaries), and checks that the rows fit in the table.
// No drawing and no editor state in here.
//

#ifndef GSTEPEDIT_H
#define GSTEPEDIT_H

#define STEP_MAX 32               // ticks that can be edited
#define STEP_WAVE 0
#define STEP_PULSE 1
#define STEP_FILTER 2

typedef struct
{
  int mode;                       // STEP_WAVE or STEP_PULSE
  int n;                          // number of ticks (1..STEP_MAX)
  int wave[STEP_MAX];             // waveform register value of each tick (wave mode)
  int note[STEP_MAX];             // the note column of the tick: 00-5F up, 60-7F down, 81-DF absolute (wave mode)
  int width[STEP_MAX];            // 12-bit pulse width of each tick (pulse mode)
  int ftype;                      // filter mode: the passband bits 10 low, 20 band, 40 high
  int fres;                       // filter mode: resonance 0-F
  int fvoices;                    // filter mode: voices bit mask 1, 2, 4, 8 (external)
  int cutoff[STEP_MAX];           // filter mode: the 8-bit cutoff of each tick
  int loop;                       // 0 = the program ends and the last value stays, k = jump back to tick k (1 based)
} STEPMODEL;

#define STEP_OK 0
#define STEP_COMMANDS 1           // the program uses pattern commands: not editable as steps
#define STEP_TOOLONG 2            // more than STEP_MAX ticks
#define STEP_COMPLEX 3            // the filter program changes the passband or voices on the way

void step_default(STEPMODEL *m, int mode);

// Reads the program that starts at startrow (1 based). An empty program gives the default model.
int step_import(STEPMODEL *m, int mode, const unsigned char *left, const unsigned char *right, int startrow);

// True if the waveform value can be written in the wavetable (values E0-FF would be taken as other things)
int step_wavevalid(int wave);

// The table rows of the model, written to left/right (at least 255 entries each). Returns the number of rows.
// startrow is needed for the loop jump. The rows end with the FF jump.
int step_generate(const STEPMODEL *m, unsigned char *left, unsigned char *right, int startrow);

// How many rows are available for a program that starts at startrow: the rows of the program that is there now
// (up to and including its FF row) plus the empty rows (00 00) after it.
int step_capacity(const unsigned char *left, const unsigned char *right, int startrow);

// Number of rows up to the last one that is not empty (00 00).
int step_tablelen(const unsigned char *left, const unsigned char *right);

// Writes the rows over the old program (the rest of the old program is cleared). Returns 0, and changes nothing,
// if they do not fit.
int step_write(unsigned char *left, unsigned char *right, int startrow, const unsigned char *gl, const unsigned char *gr, int nrows);

// Drawing with the mouse. A value is a fraction 0.0 (bottom) to 1.0 (top) of the pad.
// Pulse: the width. Filter: the cutoff. Wave: the relative note, -24 to +24 semitones.
// fine = 0 snaps to steps (1.6% width, 4 cutoff steps, whole semitones are always whole); fine = 1 does not.
// step_padget returns -1.0 if the tick can not be drawn (an absolute note in wave mode).
double step_padget(const STEPMODEL *m, int tick);
void step_padset(STEPMODEL *m, int tick, double v, int fine);
// Paints from (tick0, v0) to (tick1, v1), in a straight line, so a fast stroke leaves no gaps.
void step_paint(STEPMODEL *m, int tick0, double v0, int tick1, double v1, int fine);

// Arpeggios (wave mode). An arpeggio is a list of semitone intervals above the played note, e.g. 0 4 7 for a major chord.
#define ARP_MAXNOTES 16
#define ARP_PRESETS 12
const char *step_arp_name(int preset);                         // "MAJ", "MIN", ...
int step_arp_preset(int preset, int *semis);                   // fills the intervals, returns how many
int step_arp_ticks(int count, int hold, int pingpong);         // ticks the arpeggio takes
// Parses "0 4 7", "0,3,7,12", "0 -5 4". Returns the number of intervals, or -1 if there is none or the text is wrong.
int step_arp_parse(const char *text, int *semis, int max);
// Makes the wave program: every interval is held for 'hold' ticks, pingpong goes back down without repeating the
// ends (0 4 7 4), the waveform is 'wave', and the program loops from the first tick. Returns 0, and changes
// nothing, if it needs more than STEP_MAX ticks or an interval is outside -32..95.
int step_arp(STEPMODEL *m, const int *semis, int count, int hold, int pingpong, int wave);

#endif
