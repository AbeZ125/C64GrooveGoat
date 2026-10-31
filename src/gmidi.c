//
// C64 GrooveGoat - MIDI input (system part)
//

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "gmidi.h"

#define QUEUE 512
static GGMIDIEVENT queue[QUEUE];
static long long qtime[QUEUE];          // when each event arrived (ms)
static unsigned qhead, qtail;           // the MIDI thread writes qhead, the main thread writes qtail
static char names[256] = "";
static int nsources = 0;
static int changedflag = 0;

#if defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
static long long nowms(void)
{
  return (long long)GetTickCount64();
}
#else
static long long nowms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

static void push(const GGMIDIEVENT *ev)
{
  unsigned h = __atomic_load_n(&qhead, __ATOMIC_RELAXED), t = __atomic_load_n(&qtail, __ATOMIC_ACQUIRE);
  if (h - t >= QUEUE) return;           // full: drop
  queue[h % QUEUE] = *ev;
  qtime[h % QUEUE] = nowms();
  __atomic_store_n(&qhead, h + 1, __ATOMIC_RELEASE);
}

int midi_poll_queue(GGMIDIEVENT *ev)
{
  for (;;)
  {
    unsigned t = __atomic_load_n(&qtail, __ATOMIC_RELAXED), h = __atomic_load_n(&qhead, __ATOMIC_ACQUIRE);
    long long age;
    if (t == h) return 0;
    *ev = queue[t % QUEUE];
    age = nowms() - qtime[t % QUEUE];
    __atomic_store_n(&qtail, t + 1, __ATOMIC_RELEASE);
    if ((age < 400) || (ev->type == MIDI_NOTEOFF)) return 1;   // notes that waited longer (a dialog was open) are dropped, not played late; a key up is always passed on so no key stays held
  }
}

int midi_sources(void) { return nsources; }
const char *midi_names(void) { return names; }
int midi_changed(void) { int c = changedflag; changedflag = 0; return c; }

#ifdef GG_COREMIDI

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <time.h>

#define MAXSRC 32
static MIDIClientRef client;
static MIDIPortRef port;
static GGMIDIPARSER parsers[MAXSRC];
static int started = 0;
static ItemCount lastcount = (ItemCount)-1;
static time_t lastscan = 0;

static void readproc(const MIDIPacketList *list, void *refcon, void *srcref)
{
  GGMIDIPARSER *p = (GGMIDIPARSER *)srcref;
  const MIDIPacket *pk = &list->packet[0];
  unsigned n, i;
  (void)refcon;
  for (n = 0; n < list->numPackets; n++)
  {
    for (i = 0; i < pk->length; i++)
    {
      GGMIDIEVENT ev;
      if (midiparse_byte(p, pk->data[i], &ev)) push(&ev);
    }
    pk = MIDIPacketNext(pk);
  }
}

static void scan(void)
{
  ItemCount count = MIDIGetNumberOfSources(), i;
  int used = 0;
  names[0] = 0;
  if (count > MAXSRC) count = MAXSRC;
  for (i = 0; i < count; i++)
  {
    MIDIEndpointRef src = MIDIGetSource(i);
    CFStringRef name = NULL;
    midiparse_init(&parsers[i]);
    if (MIDIPortConnectSource(port, src, &parsers[i]) != noErr) continue;
    used++;
    if (MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &name) == noErr && name)
    {
      char buf[80];
      if (CFStringGetCString(name, buf, sizeof buf, kCFStringEncodingUTF8) && (strlen(names) + strlen(buf) + 3 < sizeof names))
      {
        if (names[0]) strcat(names, ", ");
        strcat(names, buf);
      }
      CFRelease(name);
    }
  }
  nsources = used;
}

int midi_init(void)
{
  if (started) return nsources;
  if (MIDIClientCreate(CFSTR("C64 GrooveGoat"), NULL, NULL, &client) != noErr) return 0;
  if (MIDIInputPortCreate(client, CFSTR("Input"), readproc, NULL, &port) != noErr)
  {
    MIDIClientDispose(client);
    return 0;
  }
  started = 1;
  scan();
  lastcount = MIDIGetNumberOfSources();
  lastscan = time(NULL);
  return nsources;
}

void midi_shutdown(void)
{
  if (!started) return;
  MIDIPortDispose(port);
  MIDIClientDispose(client);
  started = 0;
  nsources = 0;
}

int midi_poll(GGMIDIEVENT *ev)
{
  if (started)
  {
    time_t now = time(NULL);
    if (now != lastscan)                // once a second: has a device been plugged in or removed?
    {
      ItemCount count = MIDIGetNumberOfSources();
      lastscan = now;
      if (count != lastcount)
      {
        MIDIPortDispose(port);              // a new port: forgets every connection, then connect to what is there now
        if (MIDIInputPortCreate(client, CFSTR("Input"), readproc, NULL, &port) != noErr) { started = 0; nsources = 0; return midi_poll_queue(ev); }
        scan();
        lastcount = count;
        changedflag = 1;
      }
    }
  }
  return midi_poll_queue(ev);
}

#elif defined(__WIN32__) || defined(_WIN32)

// Windows: WinMM (winmm.dll, already linked). Each device is opened with a callback that gets whole short messages.

#include <windows.h>
#include <mmsystem.h>
#include <time.h>

#define MAXSRC 32
static HMIDIIN hin[MAXSRC];
static GGMIDIPARSER parsers[MAXSRC];
static int nopen = 0;
static int started = 0;
static UINT lastcount = (UINT)-1;
static time_t lastscan = 0;

static void CALLBACK midicb(HMIDIIN h, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
  (void)h; (void)p2;
  if (msg == MIM_DATA)                  // p1: status, data 1, data 2 in the three low bytes
  {
    GGMIDIPARSER *p = (GGMIDIPARSER *)inst;
    GGMIDIEVENT ev;
    unsigned char st = (unsigned char)(p1 & 0xff), d1 = (unsigned char)((p1 >> 8) & 0x7f), d2 = (unsigned char)((p1 >> 16) & 0x7f);
    if (midiparse_byte(p, st, &ev)) push(&ev);
    if (st >= 0xf0) return;
    if (midiparse_byte(p, d1, &ev)) push(&ev);
    if (((st & 0xf0) != 0xc0) && ((st & 0xf0) != 0xd0))
      if (midiparse_byte(p, d2, &ev)) push(&ev);
  }
}

static void closeall(void)
{
  int i;
  for (i = 0; i < nopen; i++)
  {
    midiInStop(hin[i]);
    midiInReset(hin[i]);
    midiInClose(hin[i]);
  }
  nopen = 0;
}

static void scan(void)
{
  UINT count = midiInGetNumDevs(), i;
  names[0] = 0;
  nsources = 0;
  if (count > MAXSRC) count = MAXSRC;
  for (i = 0; i < count; i++)
  {
    MIDIINCAPSA caps;
    char buf[100];
    int ok = 0;
    if (midiInGetDevCapsA(i, &caps, sizeof caps) != MMSYSERR_NOERROR) continue;
    midiparse_init(&parsers[nopen]);
    if (midiInOpen(&hin[nopen], i, (DWORD_PTR)midicb, (DWORD_PTR)&parsers[nopen], CALLBACK_FUNCTION) == MMSYSERR_NOERROR)
    {
      midiInStart(hin[nopen]);
      nopen++;
      nsources++;
      ok = 1;
    }
    snprintf(buf, sizeof buf, ok ? "%s" : "%s (in use by another program)", caps.szPname);
    if (strlen(names) + strlen(buf) + 3 < sizeof names)
    {
      if (names[0]) strcat(names, ", ");
      strcat(names, buf);
    }
  }
}

int midi_init(void)
{
  if (started) return nsources;
  started = 1;
  scan();
  lastcount = midiInGetNumDevs();
  lastscan = time(NULL);
  return nsources;
}

void midi_shutdown(void)
{
  if (!started) return;
  closeall();
  started = 0;
  nsources = 0;
}

int midi_poll(GGMIDIEVENT *ev)
{
  if (started)
  {
    time_t now = time(NULL);
    if (now != lastscan)                // once a second: has a device been plugged in or removed?
    {
      UINT count = midiInGetNumDevs();
      lastscan = now;
      if (count != lastcount)
      {
        closeall();
        scan();
        lastcount = count;
        changedflag = 1;
      }
    }
  }
  return midi_poll_queue(ev);
}

#else

int midi_init(void) { return 0; }
void midi_shutdown(void) {}
int midi_poll(GGMIDIEVENT *ev) { return midi_poll_queue(ev); }

#endif
