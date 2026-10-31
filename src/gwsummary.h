//
// C64 GrooveGoat - plain language summary of the wavetable
//
// Like gsummary.c this does not draw anything: it replays the wavetable program the way the playroutine
// runs it and describes the outcome (waveforms, notes, timing).
//

#ifndef GWSUMMARY_H
#define GWSUMMARY_H

#include "gsummary.h"

#define WSUM_MAXTICKS 1024        // how many ticks (frames) of the program are replayed at most

typedef struct
{
  int startrow;                   // the row the program starts at (1 based, 0 = no program)
  int nrows;
  int moreRows;
  SUMROW rows[SUM_MAXROWS];

  int ticks;                      // ticks replayed
  unsigned char wave[WSUM_MAXTICKS];       // the waveform register value after each tick (0 = none chosen yet)
  unsigned char pitchabs[WSUM_MAXTICKS];   // 1 if the pitch is an absolute note, 0 if relative to the played note
  short pitch[WSUM_MAXTICKS];              // semitones relative to the played note, or the absolute note number
  unsigned char row[WSUM_MAXTICKS];        // the row that ran on this tick (0 = none: a delay or the end)

  int stopped;                    // the program ended (FF 00)
  int stoptick;
  int looped;
  int loopstart;
  int looplen;
  int capped;
  int firstwave;                  // first waveform value that is set (0 = none)
  int shapes;                     // bit mask of the waveform shapes used (0x10 tri, 0x20 saw, 0x40 pulse, 0x80 noise)
  int hascmds;                    // the program runs pattern commands

  char waves[56];                 // "pulse, then triangle"
  char notes[56];                 // "arpeggio 0 +4 +7 +12"
  char timing[56];
  char note[56];                  // a remark or ""
  char oneline[96];
} WAVESUM;

void summarize_wave(WAVESUM *s, const unsigned char *left, const unsigned char *right, int startrow, double tickrate);

// One row in plain language. Returns the kind (PK_... of gsummary.h: PK_JUMP, PK_STOP, PK_SET for a waveform...)
int wave_rowtext(char *buf, int size, int row, int l, int r);

// Waveform register value as words: "pulse", "tri+saw", "noise (gate off)", "pulse+ring"
void wave_name(char *buf, int size, int wave);

// Note number (0 = C-0) as "C-0", "C#4"
void wave_notename(char *buf, int size, int note);

#endif
