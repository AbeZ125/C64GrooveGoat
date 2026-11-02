//
// C64 GrooveGoat - the step editor model
//

#include <string.h>
#include "gstepedit.h"
#include "gsummary.h"
#include "gwsummary.h"
#include "gfsummary.h"

void step_default(STEPMODEL *m, int mode)
{
  int i;
  memset(m, 0, sizeof *m);
  m->mode = mode;
  m->n = 1;
  for (i = 0; i < STEP_MAX; i++)
  {
    m->wave[i] = 0x41;
    m->width[i] = 0x800;
    m->cutoff[i] = 0x80;
  }
  m->ftype = 0x10;
  m->fres = 8;
  m->fvoices = 1;
}

int step_wavevalid(int wave)
{
  return (wave & 0xff) < 0xe0;
}

static int emptyrow(const unsigned char *l, const unsigned char *r, int row)
{
  return !l[row - 1] && !r[row - 1];
}

int step_import(STEPMODEL *m, int mode, const unsigned char *left, const unsigned char *right, int startrow)
{
  int t, n;
  step_default(m, mode);
  if ((startrow < 1) || (startrow > 255) || emptyrow(left, right, startrow)) return STEP_OK;
  if (mode == STEP_WAVE)
  {
    static WAVESUM s;
    summarize_wave(&s, left, right, startrow, 50);
    if (s.hascmds) return STEP_COMMANDS;
    n = s.looped ? s.loopstart + s.looplen : s.ticks;
    if (n > STEP_MAX) return STEP_TOOLONG;
    if (n < 1) return STEP_OK;
    for (t = 0; t < n; t++)
    {
      int p = s.pitch[t];
      m->wave[t] = s.wave[t] ? s.wave[t] : 0x41;
      if (s.pitchabs[t]) m->note[t] = 0x80 | (p < 1 ? 1 : p);
      else m->note[t] = p >= 0 ? p : p + 128;
    }
    m->n = n;
    m->loop = s.looped ? s.loopstart + 1 : 0;
  }
  else if (mode == STEP_FILTER)
  {
    static FILTSUM s;
    summarize_filter(&s, left, right, startrow, 50);
    n = s.looped ? s.loopstart + s.looplen : s.ticks;
    if (n > STEP_MAX) return STEP_TOOLONG;
    if (n < 1) return STEP_OK;
    for (t = 0; t < n; t++)
    {
      if (s.setsfilter && (s.type[t] != s.type[0] || s.ctrl[t] != s.ctrl[0])) return STEP_COMPLEX;
      m->cutoff[t] = s.cutoff[t];
    }
    if (s.setsfilter)
    {
      m->ftype = s.type[0];
      m->fres = s.ctrl[0] >> 4;
      m->fvoices = s.ctrl[0] & 15;
    }
    m->n = n;
    m->loop = s.looped ? s.loopstart + 1 : 0;
  }
  else
  {
    static PULSESUM s;
    summarize_pulse(&s, left, right, startrow, 50);
    n = s.looped ? s.loopstart + s.looplen : s.ticks;
    if (n > STEP_MAX) return STEP_TOOLONG;
    if (n < 1) return STEP_OK;
    for (t = 0; t < n; t++) m->width[t] = s.width[t];
    m->n = n;
    m->loop = s.looped ? s.loopstart + 1 : 0;
  }
  return STEP_OK;
}

static int wavecode(int w)
{
  if (!(w & 0xf0)) return 0xe0 | (w & 0x0f);
  return w & 0xff;
}

int step_generate(const STEPMODEL *m, unsigned char *left, unsigned char *right, int startrow)
{
  int rows = 0, i;
  int rowofloop = 0;
  int loopidx = m->loop - 1;                      // tick the loop jumps back to (-1 = none)

  if (m->mode == STEP_WAVE)
  {
    for (i = 0; i < m->n;)
    {
      int j = i + 1, len, rem;
      while ((j < m->n) && (j != loopidx) && (m->wave[j] == m->wave[i]) && (m->note[j] == m->note[i])) j++;
      len = j - i;
      if (i == loopidx) rowofloop = startrow + rows;
      left[rows] = (unsigned char)wavecode(m->wave[i]);
      right[rows] = (unsigned char)m->note[i];
      rows++;
      // the other ticks of the run: a delay row of D takes D+1 ticks
      rem = len - 1;
      while (rem > 0)
      {
        int d = rem - 1 > 15 ? 15 : rem - 1;
        left[rows] = (unsigned char)d;
        right[rows] = 0x80;
        rows++;
        rem -= d + 1;
      }
      i = j;
    }
  }
  else if (m->mode == STEP_FILTER)
  {
    // tick 1: the filter settings and the cutoff, which the playroutine does in one tick
    left[rows] = (unsigned char)(0x80 | (m->ftype & 0x70));
    right[rows] = (unsigned char)((m->fres << 4) | (m->fvoices & 15));
    rows++;
    if (loopidx == 0) rowofloop = startrow;
    left[rows] = 0x00;
    right[rows] = (unsigned char)m->cutoff[0];
    rows++;
    for (i = 1; i < m->n;)
    {
      if (i == loopidx)
      {
        rowofloop = startrow + rows;
        left[rows] = 0x00;
        right[rows] = (unsigned char)m->cutoff[i];
        rows++;
        i++;
        continue;
      }
      {
        int d = (signed char)((m->cutoff[i] - m->cutoff[i - 1]) & 0xff), k = 1;
        while ((i + k < m->n) && (i + k != loopidx) && (k < 127) &&
               ((signed char)((m->cutoff[i + k] - m->cutoff[i + k - 1]) & 0xff) == d)) k++;
        left[rows] = (unsigned char)k;
        right[rows] = (unsigned char)(d & 0xff);
        rows++;
        i += k;
      }
    }
  }
  else
  {
    i = 0;
    while (i < m->n)
    {
      int w = m->width[i];
      if (i == loopidx) rowofloop = startrow + rows;
      if (i == 0 || i == loopidx)
      {
        // the first tick, and the tick that the loop returns to, are set rows
        left[rows] = (unsigned char)(0x80 | (w >> 8));
        right[rows] = (unsigned char)(w & 0xff);
        rows++;
        i++;
        continue;
      }
      {
        int d = w - m->width[i - 1];
        int k = 1;
        if (d >= -128 && d <= 127)
        {
          while ((i + k < m->n) && (i + k != loopidx) && (k < 127) && (m->width[i + k] - m->width[i + k - 1] == d)) k++;
          left[rows] = (unsigned char)k;
          right[rows] = (unsigned char)(d & 0xff);
          rows++;
          i += k;
        }
        else
        {
          left[rows] = (unsigned char)(0x80 | (w >> 8));
          right[rows] = (unsigned char)(w & 0xff);
          rows++;
          i++;
        }
      }
    }
  }
  left[rows] = 0xff;
  right[rows] = (unsigned char)(m->loop ? rowofloop : 0);
  rows++;
  return rows;
}

int step_capacity(const unsigned char *left, const unsigned char *right, int startrow)
{
  int k = startrow, cap = 0;
  if ((startrow < 1) || (startrow > 255)) return 0;
  if (!emptyrow(left, right, startrow))
  {
    // the old program: up to and including its FF row
    while (k <= 255)
    {
      cap++;
      if (left[k - 1] == 0xff) { k++; break; }
      k++;
    }
  }
  // the empty rows after it
  while ((k <= 255) && emptyrow(left, right, k))
  {
    cap++;
    k++;
  }
  return cap;
}

int step_tablelen(const unsigned char *left, const unsigned char *right)
{
  int k;
  for (k = 255; k >= 1; k--)
    if (!emptyrow(left, right, k)) break;
  return k;
}

int step_write(unsigned char *left, unsigned char *right, int startrow, const unsigned char *gl, const unsigned char *gr, int nrows)
{
  int cap = step_capacity(left, right, startrow), k;
  if (nrows > cap) return 0;
  for (k = 0; k < cap; k++)
  {
    left[startrow - 1 + k] = k < nrows ? gl[k] : 0;
    right[startrow - 1 + k] = k < nrows ? gr[k] : 0;
  }
  return 1;
}

#define PAD_SEMIS 24

static double clamp01(double v)
{
  return v < 0 ? 0 : (v > 1 ? 1 : v);
}

double step_padget(const STEPMODEL *m, int tick)
{
  if ((tick < 0) || (tick >= m->n)) return -1.0;
  if (m->mode == STEP_PULSE) return m->width[tick] / 4095.0;
  if (m->mode == STEP_FILTER) return m->cutoff[tick] / 255.0;
  if (m->note[tick] >= 0x81) return -1.0;
  {
    int s = m->note[tick] < 0x60 ? m->note[tick] : m->note[tick] - 128;
    return clamp01((s + PAD_SEMIS) / (2.0 * PAD_SEMIS));
  }
}

void step_padset(STEPMODEL *m, int tick, double v, int fine)
{
  if ((tick < 0) || (tick >= m->n)) return;
  v = clamp01(v);
  if (m->mode == STEP_PULSE)
  {
    int w = fine ? (int)(v * 4095 + 0.5) : (int)(v * 64 + 0.5) * 64;
    m->width[tick] = w > 4095 ? 4095 : w;
  }
  else if (m->mode == STEP_FILTER)
  {
    int c = fine ? (int)(v * 255 + 0.5) : (int)(v * 64 + 0.5) * 4;
    m->cutoff[tick] = c > 255 ? 255 : c;
  }
  else
  {
    int s = (int)(v * 2 * PAD_SEMIS + 0.5) - PAD_SEMIS;
    if (m->note[tick] >= 0x81) return;
    m->note[tick] = s >= 0 ? s : s + 128;
  }
}

void step_paint(STEPMODEL *m, int tick0, double v0, int tick1, double v1, int fine)
{
  int a = tick0, b = tick1, k;
  if (tick0 > tick1)
  {
    double t = v0;
    a = tick1; b = tick0;
    v0 = v1; v1 = t;
  }
  for (k = a; k <= b; k++)
    step_padset(m, k, b == a ? v1 : v0 + (v1 - v0) * (k - a) / (b - a), fine);
}

static const struct { const char *name; int n; int s[4]; } arppresets[ARP_PRESETS] = {
  {"MAJ", 3, {0, 4, 7, 0}}, {"MIN", 3, {0, 3, 7, 0}}, {"SUS4", 3, {0, 5, 7, 0}}, {"DIM", 3, {0, 3, 6, 0}},
  {"AUG", 3, {0, 4, 8, 0}}, {"MAJ7", 4, {0, 4, 7, 11}}, {"MIN7", 4, {0, 3, 7, 10}}, {"MAJ+8", 4, {0, 4, 7, 12}},
  {"MIN+8", 4, {0, 3, 7, 12}}, {"OCT", 2, {0, 12, 0, 0}}, {"5TH", 2, {0, 7, 0, 0}}, {"5TH+8", 3, {0, 7, 12, 0}}
};

const char *step_arp_name(int preset)
{
  return (preset >= 0 && preset < ARP_PRESETS) ? arppresets[preset].name : "";
}

int step_arp_preset(int preset, int *semis)
{
  int i;
  if (preset < 0 || preset >= ARP_PRESETS) return 0;
  for (i = 0; i < arppresets[preset].n; i++) semis[i] = arppresets[preset].s[i];
  return arppresets[preset].n;
}

int step_arp_ticks(int count, int hold, int pingpong)
{
  int len = (pingpong && count > 2) ? 2 * count - 2 : count;
  return len * hold;
}

int step_arp_parse(const char *text, int *semis, int max)
{
  int n = 0;
  const char *p = text;
  while (*p)
  {
    int sign = 1, v = 0, digits = 0;
    while (*p == ' ' || *p == ',' || *p == '/' || *p == ';') p++;
    if (!*p) break;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') p++;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); digits++; p++; if (v > 1000) return -1; }
    if (!digits) return -1;
    if (*p && *p != ' ' && *p != ',' && *p != '/' && *p != ';') return -1;
    if (n >= max) return -1;
    v *= sign;
    if (v < -32 || v > 95) return -1;
    semis[n++] = v;
  }
  return n > 0 ? n : -1;
}

int step_arp(STEPMODEL *m, const int *semis, int count, int hold, int pingpong, int wave)
{
  int seq[2 * ARP_MAXNOTES], len = 0, i, k, t = 0;
  STEPMODEL r;
  if (count < 1 || count > ARP_MAXNOTES || hold < 1) return 0;
  for (i = 0; i < count; i++) if (semis[i] < -32 || semis[i] > 95) return 0;
  for (i = 0; i < count; i++) seq[len++] = semis[i];
  if (pingpong && count > 2) for (i = count - 2; i >= 1; i--) seq[len++] = semis[i];
  if (len * hold > STEP_MAX) return 0;
  r = *m;
  r.n = len * hold;
  for (i = 0; i < len; i++)
    for (k = 0; k < hold; k++)
    {
      r.wave[t] = wave;
      r.note[t] = seq[i] >= 0 ? seq[i] : seq[i] + 128;
      t++;
    }
  r.loop = 1;
  if (!step_wavevalid(wave)) return 0;
  *m = r;
  return 1;
}
