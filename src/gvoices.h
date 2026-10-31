//
// C64 GrooveGoat - which voice (channel) plays which held key
//
// Several keys held on the MIDI keyboard sound at the same time, each on its own channel. The first key takes the
// channel of the cursor (the one that the pattern editor enters notes in), the next ones the channels after it.
// Pure bookkeeping, no player access.
//

#ifndef GVOICES_H
#define GVOICES_H

#define VOICES_MAX 32

typedef struct
{
  int key[VOICES_MAX];            // what identifies the held key (a MIDI note number, or -1 for the mouse)
  int chn[VOICES_MAX];            // the channel it plays on
  int age[VOICES_MAX];            // order of arrival
  int n;                          // number of held keys
  int clock;
} VOICES;

void voices_init(VOICES *v);

// A key goes down. Tries the channels home, home+1 ... home+span-1 (modulo nchan) and takes the first that is free.
// If all of them are taken the oldest held key loses its channel (it is stolen). A key that is already held keeps its
// channel (it is struck again). Returns the channel.
int voices_on(VOICES *v, int key, int home, int nchan, int span);

// A key goes up. Returns the channel to release, or -1 if the key holds none (never held, or its channel was stolen).
int voices_off(VOICES *v, int key);

int voices_count(const VOICES *v);
int voices_key(const VOICES *v, int i);         // the i-th held key (0 .. count-1)

#endif
