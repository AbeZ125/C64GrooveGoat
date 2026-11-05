//
// C64 GrooveGoat - import of a Furnace module (.fur) as a GoatTracker song
//
// The reverse of gfurnacesong.c. Reads the module (decompressing it if it is zlib compressed, which Furnace does by
// default), takes the first subsong, and makes the song the playroutine would play: an order list for each channel,
// patterns of up to 128 rows, a speedtable for the slides, vibratos and funktempo. The instruments are not converted
// here: the caller imports each one with furnace_import (gfurnaceimp.c) into the slot given in imp->instrument[].
//
// Only modules of C64 chips (6581 and 8580) are read, written by Furnace 0.6 and newer in the song layout it
// wrote before version 240 (INFO block, PATN patterns). No drawing and no files in here, only bytes.
//

#ifndef GFURNACESONGIMP_H
#define GFURNACESONGIMP_H

#include "gcommon.h"

#define FSI_OK 0
#define FSI_NOTFUR 1                     // not a Furnace module, or damaged
#define FSI_TOONEW 2                     // a version this reader does not know (before 157 or from 240)
#define FSI_NOTC64 3                     // a chip other than the C64
#define FSI_TOOBIG 4                     // does not fit a GoatTracker song (patterns, speedtable, SIDs)

// Things in the module that the song does not carry the same way (bit mask of imp->warnings)
#define FSI_W_VOLUME 0x0001              // volume column (ignored)
#define FSI_W_EFFECT 0x0002              // an effect with no counterpart (arpeggio, filter mode ... ignored)
#define FSI_W_ONECMD 0x0004              // more than one command in a row: the most important was kept
#define FSI_W_NOTERANGE 0x0008           // a note below C-0 or above the top of the range: moved
#define FSI_W_INSTR 0x0010               // an instrument above 63 (ignored)
#define FSI_W_TIMING 0x0020              // the tick rate is not 50/60 Hz times a whole number: approximated
#define FSI_W_SPEED 0x0040               // a speed below 3, or a groove (speed pattern longer than 2): approximated
#define FSI_W_JUMP 0x0080                // a jump that is not the loop of the song, or a pattern break with a start row
#define FSI_W_SUBSONGS 0x0100            // more than one subsong: only the first is read
#define FSI_W_ORDERS 0x0200              // an order list that is too long: cut
#define FSI_W_SPEEDTABLE 0x0400          // the speedtable is full: some slides and vibratos are dropped
#define FSI_W_WAVETABLE 0x1000           // the wavetable is full: some arpeggios are dropped
#define FSI_W_TUNING 0x0800              // a tuning other than A-4 = 440 Hz (ignored)

#define FSI_MAXINSTR 63

typedef struct
{
  int numsids, ntsc, sidmodel, multiplier;
  char name[MAX_STR], author[MAX_STR];
  unsigned char order[MAX_CHN][MAX_SONGLEN + 2];
  int songlen[MAX_CHN];
  unsigned char pattern[MAX_PATT][MAX_PATTROWS * 4 + 4];
  int pattlen[MAX_PATT];
  int patterns;
  unsigned char stbl_l[MAX_TABLELEN], stbl_r[MAX_TABLELEN];
  int stbllen;
  unsigned char wtbl_l[MAX_TABLELEN], wtbl_r[MAX_TABLELEN];   // wavetable rows of the arpeggios (8XY commands)
  int wtbllen;
  int instruments;                       // instruments in the module that can be imported (slot 1 .. instruments)
  int instroffset[FSI_MAXINSTR + 1];     // where the instrument block of slot n is in the module data, 0 if it has none
  int instrsize[FSI_MAXINSTR + 1];
  int warnings;
  int rows;                              // rows of each channel
  unsigned char *module;                 // the module bytes (decompressed), owned by the import: free with fsi_free
  int modulesize;
} FSONGIMP;

// Reads a module image. Returns FSI_OK (imp is filled; it is big, keep it static) or an error.
int furnace_songimport(const unsigned char *data, int size, FSONGIMP *imp);

// The bytes of instrument slot n as a .fui file image (for furnace_import), in out (outsize bytes), or 0 if there is none
int furnace_songinstrument(const FSONGIMP *imp, int slot, unsigned char *out, int outsize);

// The song as the body of a GoatTracker song file (the bytes after the GTS5 identifier): without instruments, with the
// speedtable. Returns the size, or 0 if it does not fit.
int furnace_songbody(const FSONGIMP *imp, unsigned char *out, int outsize);

void fsi_free(FSONGIMP *imp);

// A plain language list of the warnings, "" if there are none
void furnace_songimportwarningtext(char *buf, int size, int warnings);

// zlib stream (or raw deflate if raw is set) into out. Returns the size, or -1 on error. *out is malloc'ed.
int fsi_inflate(const unsigned char *in, int inlen, unsigned char **out, int raw);

#endif
