//
// C64 GrooveGoat - the speed of a glide (tone portamento, command 3XY)
//
// The playroutine slides the pitch by a fixed number of SID frequency units per tick until it reaches the new note.
// A person thinks "the slide takes 0.2 seconds", whatever the two notes are, so this works out the speed that does it.
// No drawing and no tables in here.
//

#ifndef GGLIDE_H
#define GGLIDE_H

#define GLIDE_MAXSPEED 0x7fff          // a speedtable value of $8000 or more means something else (a part of the note step)

// The speed (frequency units per tick, 1 .. GLIDE_MAXSPEED) that takes a slide from frequency f1 to f2 (SID frequency
// register values) at most "ticks" ticks. Equal notes give 1.
int glide_speed(int f1, int f2, int ticks);

// The ticks the playroutine needs to arrive (at least 1) with this speed.
int glide_ticks(int f1, int f2, int speed);

// ---------------------------------------------------------------------------------------------------------------
// The rows of a slide. The playroutine ends a slide when a row starts without the command (an empty command column
// stops it), so a slide that takes more ticks than a row has must repeat the 3XY on the rows after the note, until
// it has arrived. A pattern is an array of rows of 4 bytes: note, instrument, command, data.
// ---------------------------------------------------------------------------------------------------------------

// Ticks per row at "row": the last tempo command (F0X-F7F, or F80-FFF for this voice only; 3 or more) in rows 0..row,
// else deftempo.
int glide_tempo(const unsigned char *pat, int row, int deftempo);

// The ticks a slide that starts on "row" can take: the rows from there to the row with a note or another command
// (or the end of the pattern: the next pattern starts with an empty command, which ends the slide), times tpr.
// The rows with "olddata" in the command column (the earlier fill of this same glide) do not stop it. At least tpr.
int glide_room(const unsigned char *pat, int len, int row, int tpr, int olddata);

// Writes the command 3 with "data" on "row" and on the empty rows after it that the slide of "ticks" ticks needs; takes
// away the earlier fill (rows with olddata) first. Returns the number of rows the slide uses (1 = only the note row).
// With ticks = 0 (tie note) the rows after are left empty.
int glide_fill(unsigned char *pat, int len, int row, int data, int tpr, int ticks, int olddata);

// What the glide window needs to know about the note under the cursor (filled by pattern_glideinfo, gpattern.c)
typedef struct
{
  int row;                        // the note row
  int prevrow;                    // the note it slides from
  int note, prevnote;             // pattern note values
  int f1, f2;                     // SID frequencies of the two notes
  int tpr;                        // ticks per row
  int tempofound;                 // 1 if a tempo command in the pattern gave tpr, 0 if the default is assumed
  int room;                       // the most ticks the slide can take before a note, a command or the end of the pattern
  int olddata;                    // the data of the 3XY that is on the row now (0 = none)
} GLIDEINFO;

#endif
