//
// C64 GrooveGoat - export of a whole song to a Furnace module (.fur)
//
// Furnace (the multi-system tracker) keeps one rectangular grid of rows: every channel has patterns of the same
// length and its own list of which pattern plays in each order. A GoatTracker song has an order list per channel
// with patterns of different lengths, repeats, transposes and its own loop point for each channel. The export
// therefore plays each order list out into a stream of rows (what the playroutine would play), stretches the streams of
// channels that loop until they line up, and cuts the streams into patterns of one length.
//
// Written in the older song layout (version 200) that Furnace 0.6 and newer read. C64 chips only: one chip for
// each SID, 6581 or 8580 as the song says, PAL or NTSC. No drawing and no files in here, only bytes.
//

#ifndef GFURNACESONG_H
#define GFURNACESONG_H

#include "gcommon.h"
#include "gfurnace.h"

#define FSONG_VERSION 200                // the song format version written
#define FSONG_MAXROWS 65536              // rows of one channel that are written (256 orders of up to 256 rows)

// Things in the song that the file does not carry the same way (bit mask of info->warnings)
#define FSONG_W_LOOPS 0x01               // the channels loop at different rows: the loop is approximated
#define FSONG_W_TOOLONG 0x02             // the song needs more than 256 orders: it is cut
#define FSONG_W_LOOPUNALIGNED 0x04       // the loop point is not at the start of a pattern: moved to one
#define FSONG_W_INSTR 0x08               // some instruments could not be exported fully (see FURNACE_W_...)
#define FSONG_W_COMMANDS 0x10            // pattern commands without a counterpart in Furnace
#define FSONG_W_CHANTEMPO 0x20           // a tempo for one channel only: Furnace has one speed for all

typedef struct
{
  const unsigned char *order[MAX_CHN];   // the order list of each channel (songlen entries, then LOOPSONG and a position)
  int songlen[MAX_CHN];
  const unsigned char (*pattern)[MAX_PATTROWS*4+4];
  const int *pattlen;
  int numsids;                           // 1 .. MAX_SIDS
  int ntsc, sidmodel;                    // 0 = PAL, 1 = NTSC; 0 = 6581, 1 = 8580
  int multiplier;                        // 1 .. 25: player calls per frame
  const INSTR *instr;                    // MAX_INSTR entries (entry 0 is not used)
  const unsigned char *ltable, *rtable;  // the four table pairs, MAX_TABLES * MAX_TABLELEN each
  const char *name, *author;
} FSONGIN;

typedef struct
{
  int size;                              // bytes written, 0 on failure
  int warnings;                          // FSONG_W_...
  int instrwarnings;                     // FURNACE_W_... of all instruments together
  int instruments, patterns, orders;     // how many of each were written
  int patternlen;                        // rows in a pattern
  int rows;                              // rows of each channel
  char error[96];                        // why it failed, "" if it did not
} FSONGINFO;

// Writes the song as a .fur file image into out (outsize bytes; FSONG_MAXFILE is always enough).
// Returns the size, or 0 if it failed (info->error says why).
#define FSONG_MAXFILE (4 * 1024 * 1024)
int furnace_song(unsigned char *out, int outsize, const FSONGIN *in, FSONGINFO *info);

// A plain language list of the warnings, "" if there are none
void furnace_songwarningtext(char *buf, int size, int warnings);

#endif
