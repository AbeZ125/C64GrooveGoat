//
// C64 GrooveGoat - MIDI byte stream parser
//

#include "gmidiparse.h"

void midiparse_init(GGMIDIPARSER *p)
{
  p->status = 0;
  p->have = 0;
}

static int datalen(int status)
{
  int t = status & 0xf0;
  return (t == 0xc0 || t == 0xd0) ? 1 : 2;
}

int midiparse_byte(GGMIDIPARSER *p, unsigned char byte, GGMIDIEVENT *ev)
{
  if (byte >= 0xf8) return 0;                      // real time: can come anywhere, changes nothing
  if (byte & 0x80)
  {
    p->have = 0;
    if (byte >= 0xf0) p->status = 0;               // system exclusive and system common: skipped, and cancel running status
    else p->status = byte;
    return 0;
  }
  if (!p->status) return 0;                        // data without a status (inside system exclusive, or garbage)
  p->d[p->have++] = byte;
  if (p->have < datalen(p->status)) return 0;
  p->have = 0;
  ev->channel = p->status & 0x0f;
  switch (p->status & 0xf0)
  {
    case 0x90:
    ev->type = p->d[1] ? MIDI_NOTEON : MIDI_NOTEOFF;
    ev->a = p->d[0];
    ev->b = p->d[1];
    return 1;

    case 0x80:
    ev->type = MIDI_NOTEOFF;
    ev->a = p->d[0];
    ev->b = p->d[1];
    return 1;

    case 0xb0:
    ev->type = MIDI_CONTROL;
    ev->a = p->d[0];
    ev->b = p->d[1];
    return 1;

    case 0xc0:
    ev->type = MIDI_PROGRAM;
    ev->a = p->d[0];
    ev->b = 0;
    return 1;

    case 0xe0:
    ev->type = MIDI_PITCHBEND;
    ev->a = p->d[0] | (p->d[1] << 7);
    ev->b = 0;
    return 1;
  }
  return 0;                                        // polyphonic and channel pressure: ignored
}

int midi_tracker_note(int midinote, int firstnote, int lastnote)
{
  // MIDI 12 is C-0, the first note of the tracker
  int n = firstnote + (midinote - 12);
  if ((midinote < 12) || (n > lastnote)) return -1;
  return n;
}
