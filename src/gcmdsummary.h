//
// C64 GrooveGoat - plain language summary of a pattern command (0XY - FXY)
//
// Pure analysis, no drawing: takes the command, its data byte and the instrument tables (for the commands that
// use a speedtable index or point into a table) and describes what the playroutine will do with it.
//

#ifndef GCMDSUMMARY_H
#define GCMDSUMMARY_H

typedef struct
{
  const unsigned char *l[4];      // left columns of the tables, indexed WTBL, PTBL, FTBL, STBL
  const unsigned char *r[4];      // right columns
} CMDTABLES;

typedef struct
{
  char name[28];                  // "Vibrato"
  char text[100];                 // what the command does, one sentence
  char detail[3][100];            // extra lines (what the speedtable entry makes of it, timing, ...)
  int ndetail;
} CMDSUM;

// Supplied by the program (gadsr.c): the SID envelope times in milliseconds
float adsr_attackms(int a);
float adsr_decayms(int d);

// clock: the SID clock in Hz (985248 PAL, 1022727 NTSC), tickrate: ticks per second
void summarize_command(CMDSUM *s, int cmd, int data, const CMDTABLES *t, double tickrate, double clock);

// The vibrato of the playroutine replayed: ticks in one swing cycle and the swing (peak to peak, in cents at A-4)
void sum_vibrato(int xx, int yy, double clock, int *period, double *cents);

// The pitch offset (in SID frequency register units) after each of n ticks of a vibrato, exactly as the playroutine
// does it. A note independent entry (xx >= $80) is calculated for A-4.
void sum_vibrato_curve(int xx, int yy, double clock, double *off, int n);

#endif
