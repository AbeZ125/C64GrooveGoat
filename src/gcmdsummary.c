//
// C64 GrooveGoat - plain language summary of a pattern command
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gcmdsummary.h"
#include "gwsummary.h"
#include "gcommon.h"

static void msstr(char *buf, int size, float ms)
{
  if (ms < 1000) snprintf(buf, size, "%d ms", (int)ms);
  else if ((int)ms % 1000) snprintf(buf, size, "%.1f s", ms / 1000.0f);
  else snprintf(buf, size, "%d s", (int)(ms / 1000));
}

// The SID frequency register of A-4 (440 Hz)
static double a4reg(double clock) { return 440.0 * 16777216.0 / clock; }

// Vibrato exactly as vibrato() in gplay.c: the pitch swings by speed per tick, the direction is flipped by a
// counter that is compared with the left side of the speedtable entry
void sum_vibrato_curve(int xx, int yy, double clock, double *off, int n)
{
  unsigned char vibtime = 0;
  double speed = yy, o = 0;
  int cmp = xx, i;
  if (cmp >= 0x80)
  {
    cmp &= 0x7f;
    speed = a4reg(clock) * (pow(2.0, 1.0 / 12) - 1) / pow(2.0, yy > 15 ? 15 : yy);
  }
  if (!xx && !yy) speed = 0;
  for (i = 0; i < n; i++)
  {
    if ((vibtime < 0x80) && (vibtime > cmp)) vibtime ^= 0xff;
    vibtime += 2;
    if (vibtime & 1) o -= speed; else o += speed;
    off[i] = o;
  }
}

void sum_vibrato(int xx, int yy, double clock, int *period, double *cents)
{
  enum { N = 1200 };
  static double off[N];
  static unsigned char vt[N];
  unsigned char vibtime = 0;
  int cmp = xx & 0x7f, i, p;
  double lo = 1e9, hi = -1e9;
  *period = 0;
  *cents = 0;
  if (!xx && !yy) return;
  sum_vibrato_curve(xx, yy, clock, off, N);
  for (i = 0; i < N; i++)
  {
    if ((vibtime < 0x80) && (vibtime > cmp)) vibtime ^= 0xff;
    vibtime += 2;
    vt[i] = vibtime;
  }
  // the period: the smallest shift after which the counter state repeats (checked past the start-up)
  for (p = 1; p < N / 3; p++)
  {
    int ok = 1;
    for (i = N / 2; i < N / 2 + 64; i++)
      if (vt[i] != vt[i + p]) { ok = 0; break; }
    if (ok) break;
  }
  if (p >= N / 3) return;
  for (i = N / 2; i < N / 2 + p; i++)
  {
    if (off[i] < lo) lo = off[i];
    if (off[i] > hi) hi = off[i];
  }
  *period = p;
  {
    double reg = a4reg(clock);
    double top = reg + (hi - lo) / 2, bot = reg - (hi - lo) / 2;
    if (bot < 1) bot = 1;
    *cents = 1200.0 * log(top / bot) / log(2.0);
  }
}

// How a speedtable entry moves the pitch per tick (portamento): sets text, returns 0 when there is no movement
static void speeddesc(char *buf, int size, int data, const CMDTABLES *t, double tickrate, double clock)
{
  int l, r, s;
  double hzpt, semis;
  if (!data) { snprintf(buf, size, "speedtable entry 00 means no movement"); return; }
  l = t->l[STBL][data - 1];
  r = t->r[STBL][data - 1];
  s = (l << 8) | r;
  if (s >= 0x8000)
  {
    snprintf(buf, size, "row %02X: %02X %02X = 1/%d of the step to the next note per tick (%.1f semitones/s)",
      data, l, r, 1 << (r > 15 ? 15 : r), tickrate / (1 << (r > 15 ? 15 : r)));
    return;
  }
  if (!s) { snprintf(buf, size, "row %02X is empty (00 00): no movement", data); return; }
  hzpt = s * clock / 16777216.0;
  semis = hzpt * tickrate / (440.0 * (pow(2.0, 1.0 / 12) - 1));
  snprintf(buf, size, "row %02X: %02X %02X = $%04X per tick, %.1f Hz/tick, about %.1f semitones/s at A-4", data, l, r, s, hzpt, semis);
}

static void adddetail(CMDSUM *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void adddetail(CMDSUM *s, const char *fmt, ...)
{
  va_list ap;
  if (s->ndetail >= 3) return;
  va_start(ap, fmt);
  vsnprintf(s->detail[s->ndetail++], sizeof s->detail[0], fmt, ap);
  va_end(ap);
}

void summarize_command(CMDSUM *s, int cmd, int data, const CMDTABLES *t, double tickrate, double clock)
{
  char a[96], b[48];
  static const char *names[16] = {"Do nothing", "Portamento up", "Portamento down", "Toneportamento", "Vibrato", "Set attack/decay",
    "Set sustain/release", "Set waveform", "Set wavetable pointer", "Set pulsetable pointer", "Set filtertable pointer",
    "Set filter control", "Set filter cutoff", "Master volume / timing mark", "Funktempo", "Set tempo"};
  cmd &= 15;
  data &= 255;
  memset(s, 0, sizeof *s);
  snprintf(s->name, sizeof s->name, "%s", names[cmd]);
  switch (cmd)
  {
    case CMD_DONOTHING:
    snprintf(s->text, sizeof s->text, "no effect; it ends a running 1XY-4XY command");
    adddetail(s, "The instrument's own vibrato takes over again (if it has one).");
    if (data) adddetail(s, "The data byte should be 00.");
    break;

    case CMD_PORTAUP:
    case CMD_PORTADOWN:
    speeddesc(a, sizeof a, data, t, tickrate, clock);
    snprintf(s->text, sizeof s->text, "slide the pitch %s on every tick until another command", cmd == CMD_PORTAUP ? "up" : "down");
    adddetail(s, "%s", a);
    break;

    case CMD_TONEPORTA:
    if (!data)
    {
      snprintf(s->text, sizeof s->text, "tie note: go to the new note at once, without restarting the sound");
    }
    else
    {
      speeddesc(a, sizeof a, data, t, tickrate, clock);
      snprintf(s->text, sizeof s->text, "slide from the old pitch to the new note, then stay on it");
      adddetail(s, "%s", a);
    }
    break;

    case CMD_VIBRATO:
    if (!data)
    {
      snprintf(s->text, sizeof s->text, "no pitch movement (entry 00 is empty)");
    }
    else
    {
      int xx = t->l[STBL][data - 1], yy = t->r[STBL][data - 1];
      int period;
      double cents;
      sum_vibrato(xx, yy, clock, &period, &cents);
      if (period) snprintf(s->text, sizeof s->text, "vibrato: %.1f Hz, depth about %.0f cents at A-4", tickrate / period, cents);
      else snprintf(s->text, sizeof s->text, "vibrato with no movement (speedtable row %02X is 00 00)", data);
      if (xx >= 0x80) adddetail(s, "row %02X: %02X %02X = depth follows the note: 1/%d of a semitone per tick", data, xx, yy, 1 << (yy > 15 ? 15 : yy));
      else adddetail(s, "row %02X: %02X %02X = direction changes about every %d ticks, depth %d per tick", data, xx, yy, period / 2, yy);
    }
    break;

    case CMD_SETAD:
    msstr(a, sizeof a, adsr_attackms(data >> 4));
    msstr(b, sizeof b, adsr_decayms(data & 15));
    snprintf(s->text, sizeof s->text, "attack %X (%s), decay %X (%s)", data >> 4, a, data & 15, b);
    adddetail(s, "Takes effect from this row. The instrument's own value is not changed.");
    break;

    case CMD_SETSR:
    msstr(b, sizeof b, adsr_decayms(data & 15));
    snprintf(s->text, sizeof s->text, "sustain %X (%d%%), release %X (%s)", data >> 4, (data >> 4) * 100 / 15, data & 15, b);
    break;

    case CMD_SETWAVE:
    wave_name(a, sizeof a, data);
    snprintf(s->text, sizeof s->text, "waveform $%02X: %s", data, a);
    adddetail(s, "No effect while the wavetable itself changes the waveform.");
    break;

    case CMD_SETWAVEPTR:
    if (!data) snprintf(s->text, sizeof s->text, "stop the wavetable: the waveform and pitch stay as they are");
    else
    {
      static WAVESUM ws;
      summarize_wave(&ws, t->l[WTBL], t->r[WTBL], data, tickrate);
      snprintf(s->text, sizeof s->text, "start the wavetable at row %02X", data);
      adddetail(s, "%s", ws.oneline);
    }
    break;

    case CMD_SETPULSEPTR:
    if (!data) snprintf(s->text, sizeof s->text, "stop the pulsetable: the pulse width stays as it is");
    else
    {
      static PULSESUM ps;
      summarize_pulse(&ps, t->l[PTBL], t->r[PTBL], data, tickrate);
      snprintf(s->text, sizeof s->text, "start the pulsetable at row %02X", data);
      adddetail(s, "%s", ps.oneline);
    }
    break;

    case CMD_SETFILTERPTR:
    if (!data) snprintf(s->text, sizeof s->text, "stop the filtertable");
    else snprintf(s->text, sizeof s->text, "start the filtertable at row %02X", data);
    adddetail(s, "It controls the filter of the SID this voice is on.");
    break;

    case CMD_SETFILTERCTRL:
    if (!data) snprintf(s->text, sizeof s->text, "filter off, and the filtertable stops");
    else
    {
      char v[24];
      v[0] = 0;
      if (data & 1) strcat(v, " 1");
      if (data & 2) strcat(v, " 2");
      if (data & 4) strcat(v, " 3");
      if (data & 8) strcat(v, " ext");
      snprintf(s->text, sizeof s->text, "resonance %X, filter on voices%s", data >> 4, v[0] ? v : " (none)");
    }
    adddetail(s, "Voices are those of this voice's SID.");
    break;

    case CMD_SETFILTERCUTOFF:
    snprintf(s->text, sizeof s->text, "filter cutoff $%02X (%d%% of the range)", data, data * 100 / 255);
    break;

    case CMD_SETMASTERVOL:
    if (data < 0x10) snprintf(s->text, sizeof s->text, "master volume %X of F (%d%%) for this SID", data, data * 100 / 15);
    else
    {
      snprintf(s->text, sizeof s->text, "timing mark $%02X: no sound effect", data);
      adddetail(s, "Copied to the player address + $3F (only with \"Store author-info\").");
    }
    break;

    case CMD_FUNKTEMPO:
    if (!data) snprintf(s->text, sizeof s->text, "use the funktempo values that are already set");
    else
    {
      int l = t->l[STBL][data - 1], r = t->r[STBL][data - 1];
      snprintf(s->text, sizeof s->text, "alternate between tempo %d and %d on every row", l, r);
      adddetail(s, "row %02X: %02X %02X; the rows take %.0f ms and %.0f ms, on average %.1f frames per row", data, l, r,
        l * 1000.0 / tickrate, r * 1000.0 / tickrate, (l + r) / 2.0);
    }
    adddetail(s, "Affects all voices.");
    break;

    case CMD_SETTEMPO:
    {
      int tempo = data & 0x7f;
      if (tempo < 2) snprintf(s->text, sizeof s->text, "go back to the funktempo values (command E)");
      else if (tempo == 2) snprintf(s->text, sizeof s->text, "tempo 2 is not supported normally (use 3 or more)");
      else
      {
        snprintf(s->text, sizeof s->text, "tempo %d: %d frames per row (%.0f ms) for %s", tempo, tempo, tempo * 1000.0 / tickrate,
          data >= 0x80 ? "this voice only" : "all voices");
        adddetail(s, "About %.0f BPM at 4 rows per beat.", tickrate * 60.0 / (tempo * 4));
      }
    }
    break;
  }
}
