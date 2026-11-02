//
// C64 GrooveGoat - which instruments use which table program
//
// An instrument points at a row of the wave, pulse, filter and speed table (its Wavetable Pos and so on). This module
// counts, for every row, how many instruments start there. Used for the marks next to the table rows.
//

#ifndef GINSUSE_H
#define GINSUSE_H

#define INSUSE_TABLES 4
#define INSUSE_ROWS 256

typedef struct
{
  unsigned char count[INSUSE_TABLES][INSUSE_ROWS];      // instruments that point at the row (0-based row index), saturates at 255
  unsigned char first[INSUSE_TABLES][INSUSE_ROWS];      // the lowest numbered instrument among them
  unsigned char mine[INSUSE_TABLES][INSUSE_ROWS];       // 1 if the current instrument is one of them
} INSUSE;

// ptr[i][t] is the 1-based row that instrument i points at in table t (0 = none). Instruments 1 .. ninstr-1 are used
// (instrument 0 is the empty one).
void insuse_build(INSUSE *u, int ninstr, const unsigned char ptr[][INSUSE_TABLES], int current);

// The two characters shown next to a row: the instrument number as two hex digits if one instrument uses it, "x3"
// for three, "x+" for ten or more. Returns 0 (and an empty string) if no instrument uses the row, 1 if some do, 2 if
// the current instrument is one of them.
int insuse_label(const INSUSE *u, int table, int row, char *out);

// The instruments that use a row, in order. Returns how many there are in total (out holds at most max of them).
int insuse_list(int ninstr, const unsigned char ptr[][INSUSE_TABLES], int table, int row, int *out, int max);

#endif
