//
// C64 GrooveGoat - plain language summaries of the instrument tables
//
// The analysis here does not draw anything and does not need the editor: it takes the left and right
// columns of a table, replays the program the way the playroutine runs it, and describes the outcome.
// The first table that is covered is the pulsetable. The screen drawing is in gdisplay.c / gsummary-ui.c.
//

#ifndef GSUMMARY_H
#define GSUMMARY_H

#define SUM_MAXTICKS 4096         // how many ticks (frames) of the program are replayed at most
#define SUM_MAXROWS 16            // how many table rows are described one by one

// What a pulsetable row does
#define PK_MODULATE 0             // 01-7F: change the pulse width for a number of ticks
#define PK_SET 1                  // 80-FE: set the pulse width
#define PK_JUMP 2                 // FF: jump to another row
#define PK_STOP 3                 // FF 00: stop the pulsetable
#define PK_EMPTY 4                // 00: nothing to do, the table stops here

// What happens on a tick of the replay
#define EV_NONE 0
#define EV_SET 1
#define EV_MODULATE 2

typedef struct
{
  int row;                        // row number as shown in the editor (01-FF)
  int kind;                       // PK_...
  char text[56];                  // one line of plain language, for example "widen 32t, +$40/t = +50%"
} SUMROW;

typedef struct
{
  int startrow;                   // the row the program starts at (1 based, 0 = no program)
  int nrows;                      // rows described in rows[]
  int moreRows;                   // 1 if the program has more rows than fit in rows[]
  SUMROW rows[SUM_MAXROWS];

  int ticks;                      // ticks replayed
  unsigned short width[SUM_MAXTICKS]; // the 12-bit pulse width after each tick
  unsigned char event[SUM_MAXTICKS];  // EV_... of each tick
  unsigned char rowof[SUM_MAXTICKS];  // the table row that ran on each tick (1 based, 0 = none)

  int startknown;                 // 1 if the program starts by setting the width itself
  int startwidth;                 // the width the replay started with (12 bit)
  int stopped;                    // the program ended by itself (a stop, or an empty row)
  int stoptick;                   // the tick it ended on
  int looped;                     // the program repeats
  int loopstart;                  // tick the repeating part starts on
  int looplen;                    // ticks in one repetition
  int capped;                     // the replay ended at SUM_MAXTICKS without ending or repeating
  int minwidth, maxwidth;         // widest and narrowest (12 bit) over the replay; of the repeating part when it repeats
  int wraps;                      // the width goes past 0% or 100% and comes round the other side
  int moves;                      // 1 if the width ever changes

  char range[56];                 // "0.4% to 75%"
  char shape[56];                 // "sweeps up and down", "holds", ...
  char timing[56];                // "repeats every 128t (2.6 s)"
  char note[56];                  // a warning or remark, or ""
  char oneline[64];               // the whole thing in one short line
} PULSESUM;

// Replays the pulsetable program that starts at startrow (1 based, as the instrument's Pulsetable Pos).
// left and right are the table columns (at least 255 entries each). tickrate is how many ticks run per
// second (50 for PAL, 60 for NTSC, times the speed multiplier), used only to give times in seconds.
void summarize_pulse(PULSESUM *s, const unsigned char *left, const unsigned char *right, int startrow, double tickrate);

// One row in plain language, for the one row under the cursor. Returns its kind (PK_...).
int pulse_rowtext(char *buf, int size, int row, int l, int r);

// Formats a 12 bit pulse width as a percentage ("50%", "0.4%").
void sum_percent(char *buf, int size, int width12);

// Formats a number of ticks as a duration ("1.2 s", "340 ms").
void sum_duration(char *buf, int size, double ticks, double tickrate);

#endif
