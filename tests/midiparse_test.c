// Tests of the MIDI parser: random valid streams with running status and real time bytes in between must give
// exactly the events they were made from; random garbage must never give an event out of range.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gmidiparse.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 20) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
  GGMIDIPARSER p;
  GGMIDIEVENT ev, want[4000];
  unsigned char buf[40000];
  int it, i, nb, nw, got;

  srand(11);
  for (it = 0; it < 3000; it++)
  {
    int running = 0, status = 0;
    nb = 0; nw = 0;
    for (i = 0; i < 1 + rand() % 60; i++)
    {
      int kind = rand() % 12;
      if (kind == 0) { buf[nb++] = 0xf8 + rand() % 8; if (buf[nb - 1] == 0xf9 || buf[nb - 1] == 0xfd) nb--; continue; }   // real time (F9, FD are undefined, still ignored)
      if (kind == 1) { buf[nb++] = 0xf0; buf[nb++] = rand() % 128; buf[nb++] = rand() % 128; buf[nb++] = 0xf7; running = 0; continue; }
      if (kind == 2) { buf[nb++] = 0xf2; buf[nb++] = rand() % 128; buf[nb++] = rand() % 128; running = 0; continue; }
      {
        int t = 0x80 + (rand() % 7) * 0x10, ch = rand() % 16, d1 = rand() % 128, d2 = rand() % 128;
        int len = (t == 0xc0 || t == 0xd0) ? 1 : 2;
        if (!(running && status == (t | ch) && rand() % 2)) { buf[nb++] = t | ch; }
        status = t | ch; running = 1;
        buf[nb++] = d1;
        if (rand() % 6 == 0) buf[nb++] = 0xf8;     // a clock tick inside the message
        if (len == 2) buf[nb++] = d2;
        if (t == 0x90) { want[nw].type = d2 ? MIDI_NOTEON : MIDI_NOTEOFF; want[nw].channel = ch; want[nw].a = d1; want[nw].b = d2; nw++; }
        else if (t == 0x80) { want[nw].type = MIDI_NOTEOFF; want[nw].channel = ch; want[nw].a = d1; want[nw].b = d2; nw++; }
        else if (t == 0xb0) { want[nw].type = MIDI_CONTROL; want[nw].channel = ch; want[nw].a = d1; want[nw].b = d2; nw++; }
        else if (t == 0xc0) { want[nw].type = MIDI_PROGRAM; want[nw].channel = ch; want[nw].a = d1; want[nw].b = 0; nw++; }
        else if (t == 0xe0) { want[nw].type = MIDI_PITCHBEND; want[nw].channel = ch; want[nw].a = d1 | (d2 << 7); want[nw].b = 0; nw++; }
      }
    }
    midiparse_init(&p);
    got = 0;
    for (i = 0; i < nb; i++)
      if (midiparse_byte(&p, buf[i], &ev))
      {
        CHECK(got < nw);
        if (got < nw) CHECK(ev.type == want[got].type && ev.channel == want[got].channel && ev.a == want[got].a && ev.b == want[got].b);
        got++;
      }
    CHECK(got == nw);
  }

  // garbage
  for (it = 0; it < 200; it++)
  {
    midiparse_init(&p);
    for (i = 0; i < 5000; i++)
      if (midiparse_byte(&p, (unsigned char)rand(), &ev))
      {
        CHECK(ev.type >= MIDI_NOTEON && ev.type <= MIDI_PITCHBEND);
        CHECK(ev.channel >= 0 && ev.channel < 16 && ev.a >= 0 && ev.a < 16384 && ev.b >= 0 && ev.b < 128);
      }
  }

  // a few exact cases
  midiparse_init(&p);
  CHECK(!midiparse_byte(&p, 0x90, &ev) && !midiparse_byte(&p, 60, &ev) && midiparse_byte(&p, 100, &ev) && ev.type == MIDI_NOTEON && ev.a == 60 && ev.b == 100);
  CHECK(!midiparse_byte(&p, 62, &ev) && midiparse_byte(&p, 0, &ev) && ev.type == MIDI_NOTEOFF && ev.a == 62);   // running status, velocity 0
  CHECK(!midiparse_byte(&p, 0xfe, &ev));                                                                        // active sensing changes nothing
  CHECK(!midiparse_byte(&p, 64, &ev) && midiparse_byte(&p, 1, &ev) && ev.type == MIDI_NOTEON);
  CHECK(!midiparse_byte(&p, 0xf0, &ev) && !midiparse_byte(&p, 0x7e, &ev) && !midiparse_byte(&p, 0xf7, &ev));
  CHECK(!midiparse_byte(&p, 60, &ev) && !midiparse_byte(&p, 60, &ev));                                          // no running status after sysex

  CHECK(midi_tracker_note(12, 0x60, 0xbc) == 0x60);
  CHECK(midi_tracker_note(60, 0x60, 0xbc) == 0x60 + 48);
  CHECK(midi_tracker_note(11, 0x60, 0xbc) == -1);
  CHECK(midi_tracker_note(12 + 0xbc - 0x60, 0x60, 0xbc) == 0xbc);
  CHECK(midi_tracker_note(13 + 0xbc - 0x60, 0x60, 0xbc) == -1);
  CHECK(midi_tracker_note(127, 0x60, 0xbc) == -1);

  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
