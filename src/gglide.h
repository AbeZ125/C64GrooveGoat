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

#endif
