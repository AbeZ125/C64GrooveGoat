//
// C64 GrooveGoat - MIDI byte stream parser (pure: no system calls, no editor state)
//
// Turns the bytes of a MIDI input into events. Handles running status, note on with velocity 0 (= note off),
// real time bytes in the middle of a message, and skips system exclusive and the other system messages.
//

#ifndef GMIDIPARSE_H
#define GMIDIPARSE_H

#define MIDI_NOTEON 1
#define MIDI_NOTEOFF 2
#define MIDI_CONTROL 3
#define MIDI_PROGRAM 4
#define MIDI_PITCHBEND 5

typedef struct
{
  int type;                       // MIDI_*
  int channel;                    // 0-15
  int a;                          // note, controller or program number; pitch bend 0-16383 (8192 = centre)
  int b;                          // velocity or controller value
} GGMIDIEVENT;

typedef struct
{
  int status;                     // running status (0x80-0xEF), 0 = none
  int have;                       // data bytes collected for the message in progress
  unsigned char d[2];
} GGMIDIPARSER;

void midiparse_init(GGMIDIPARSER *p);

// Feeds one byte. Returns 1 and fills *ev when the byte completes an event.
int midiparse_byte(GGMIDIPARSER *p, unsigned char byte, GGMIDIEVENT *ev);

// The tracker note for a MIDI note number (60 = middle C = C-4), or -1 if the tracker has no such note.
// Returns the pattern note value (FIRSTNOTE-based, see gcommon.h) given firstnote and lastnote.
int midi_tracker_note(int midinote, int firstnote, int lastnote);

#endif
