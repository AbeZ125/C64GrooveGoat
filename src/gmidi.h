//
// C64 GrooveGoat - MIDI input
//
// Connects to every MIDI source (keyboards, controllers, virtual ports) and queues the events for the main loop.
// The system part is CoreMIDI on macOS (build with -DGG_COREMIDI); other targets get a version that does nothing.
//

#ifndef GMIDI_H
#define GMIDI_H

#include "gmidiparse.h"

int midi_init(void);                    // returns the number of sources connected
void midi_shutdown(void);
int midi_poll(GGMIDIEVENT *ev);           // main thread: next queued event, 0 if none. Also notices plugged or removed devices.
int midi_poll_queue(GGMIDIEVENT *ev);     // (used by midi_poll)
int midi_sources(void);                 // number of sources connected now
const char *midi_names(void);           // the names of the connected sources, comma separated
int midi_changed(void);                 // 1 once after the set of sources has changed (to show a message)

#endif
