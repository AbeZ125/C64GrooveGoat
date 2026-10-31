//
// C64 GrooveGoat - plain language summary of the filtertable
//
// Like gsummary.c and gwsummary.c: no drawing, a replay of the program the way the playroutine runs it
// (filterexec() in gplay.c) and a description of the outcome.
//

#ifndef GFSUMMARY_H
#define GFSUMMARY_H

#include "gsummary.h"

#define FSUM_MAXTICKS 4096

typedef struct
{
  int startrow;
  int nrows;
  int moreRows;
  SUMROW rows[SUM_MAXROWS];

  int ticks;
  unsigned short cutoff[FSUM_MAXTICKS];     // the 8-bit cutoff after each tick
  unsigned char type[FSUM_MAXTICKS];        // the passband bits (10 low, 20 band, 40 high)
  unsigned char ctrl[FSUM_MAXTICKS];        // resonance (high nybble) and voices (low nybble)
  unsigned char rowof[FSUM_MAXTICKS];       // the row that ran on the tick (0 = none)

  int stopped;
  int stoptick;
  int looped;
  int loopstart;
  int looplen;
  int capped;
  int mincut, maxcut;                       // of the repeating part when it repeats
  int moves;                                // the cutoff ever changes
  int wraps;                                // the cutoff goes past 0 or 255 and comes round
  int setsfilter;                           // the program sets passband / resonance / voices
  int setscutoff;                           // the program sets the cutoff itself (so the start is known)

  char filter[80];                          // "low-pass, resonance F, voices 1 3"
  char range[56];                           // "cutoff 10% to 90%"
  char shape[56];
  char timing[56];
  char note[72];
  char oneline[96];
} FILTSUM;

void summarize_filter(FILTSUM *s, const unsigned char *left, const unsigned char *right, int startrow, double tickrate);
int filter_rowtext(char *buf, int size, int row, int l, int r);

// "low-pass", "notch (low+high)", ... from the passband bits; "no passband" for none
const char *filter_passband(int type);
// "1 3" voices of a filter control byte, "none"
void filter_voices(char *buf, int size, int ctrl);
// The cutoff in Hz on an 8580 (a 6581 differs per chip) and as percent
double filter_cutoffhz(int cutoff);

#endif
