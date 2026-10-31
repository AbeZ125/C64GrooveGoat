// macOS only: makes a virtual MIDI keyboard, plays two notes on it and checks that gmidi.c (CoreMIDI) delivers them.
// cc -DGG_COREMIDI -o build/midi_system_test tests/midi_system_test.c src/gmidi.c src/gmidiparse.c -framework CoreMIDI -framework CoreFoundation
#include <stdio.h>
#include <unistd.h>
#include <CoreMIDI/CoreMIDI.h>
#include "../src/gmidi.h"

int main(void)
{
  MIDIClientRef cl;
  MIDIEndpointRef src;
  GGMIDIEVENT ev;
  int got = 0, i, ok = 1;
  unsigned char msg[3 + 3 + 3] = {0x90, 60, 100, 0x90, 64, 90, 0x90, 60, 0};   // on, on (running status not used), off by velocity 0
  Byte buf[256];
  MIDIPacketList *pl = (MIDIPacketList *)buf;
  MIDIPacket *pk;

  midi_init();
  MIDIClientCreate(CFSTR("test"), NULL, NULL, &cl);
  if (MIDISourceCreate(cl, CFSTR("Test Keyboard"), &src) != noErr) { printf("no virtual source\n"); return 1; }
  for (i = 0; i < 40 && !midi_sources(); i++) { GGMIDIEVENT d; midi_poll(&d); usleep(100000); }   // the rescan runs once a second
  printf("sources: %d (%s)\n", midi_sources(), midi_names());
  ok &= midi_sources() >= 1;
  pk = MIDIPacketListInit(pl);
  MIDIPacketListAdd(pl, sizeof buf, pk, 0, sizeof msg, msg);
  MIDIReceived(src, pl);
  usleep(200000);
  while (midi_poll(&ev)) { printf("event type %d note %d vel %d\n", ev.type, ev.a, ev.b); got++; if (got == 1) ok &= ev.type == MIDI_NOTEON && ev.a == 60 && ev.b == 100; if (got == 3) ok &= ev.type == MIDI_NOTEOFF && ev.a == 60; }
  ok &= got == 3;
  printf(ok ? "OK\n" : "FAILED\n");
  midi_shutdown();
  return !ok;
}
