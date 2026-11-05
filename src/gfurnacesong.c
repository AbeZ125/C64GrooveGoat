//
// C64 GrooveGoat - export of a whole song to a Furnace module (.fur), see gfurnacesong.h
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "gfurnacesong.h"

#define FXN 4                            // effects in a row (the file has this many effect columns)
#ifndef FSONG_SLIDE_K
#define FSONG_SLIDE_K 32.0              // Furnace slide units for a slide of one semitone each tick (measured in Furnace 0.6.8.3)
#endif

typedef struct
{
  int note;                              // -1 none, 0..179 Furnace note, 180 note off
  int ins;                               // Furnace instrument number (GoatTracker's minus one), -1 none
  int fx[FXN], fxval[FXN];                   // Furnace effects and values, fx -1 for none, filled from the first
} CELL;

static void addfx(CELL *c, int fx, int val)
{
  int i;
  for (i = 0; i < FXN; i++)
    if (c->fx[i] < 0) { c->fx[i] = fx; c->fxval[i] = val; return; }
}

static void clearcell(CELL *c)
{
  int i;
  c->note = -1; c->ins = -1;
  for (i = 0; i < FXN; i++) { c->fx[i] = -1; c->fxval[i] = 0; }
}

typedef struct
{
  unsigned char *p;
  int size, cap, failed;
} BUF;

static void put(BUF *b, int v)
{
  if (b->size >= b->cap) { b->failed = 1; return; }
  b->p[b->size++] = v & 0xff;
}
static void put16(BUF *b, int v) { put(b, v); put(b, v >> 8); }
static void put32(BUF *b, unsigned v) { put(b, v); put(b, v >> 8); put(b, v >> 16); put(b, v >> 24); }
static void putf(BUF *b, float f)
{
  unsigned u;
  memcpy(&u, &f, 4);
  put32(b, u);
}
static void puts0(BUF *b, const char *s)
{
  while (s && *s) put(b, *s++);
  put(b, 0);
}
static void putn(BUF *b, const unsigned char *s, int n)
{
  while (n-- > 0) put(b, *s++);
}
static void set32(BUF *b, int at, unsigned v)
{
  if (at + 4 > b->size) return;
  b->p[at] = v; b->p[at + 1] = v >> 8; b->p[at + 2] = v >> 16; b->p[at + 3] = v >> 24;
}

static int gcd(int a, int b)
{
  while (b) { int t = a % b; a = b; b = t; }
  return a;
}

// One channel played out: the rows, and where its loop starts (-1: it ends)
typedef struct
{
  CELL *row;
  int len;                               // rows of one pass
  int loop;                              // row the loop jumps back to, -1 if the channel ends
  int truncated;
} STREAM;

static int furnace_note(int n)
{
  // GoatTracker C-0 is 0x60, Furnace C-0 is 60
  int f = n - FIRSTNOTE + 60;
  if (f < 0) f = 0;
  if (f > 179) f = 179;
  return f;
}

// What a channel was doing, for the commands that go on until something stops them in Furnace but last one row in
// GoatTracker
#define MAXVARIANTS 160                  // instruments made for notes that start a table program (8XY, 9XY, AXY)

typedef struct { int base, w, p, f; } VARIANT;   // base instrument and the table pointers it starts with

typedef struct
{
  int first;                             // Furnace number of the first variant (the one after the instruments)
  int count;
  VARIANT v[MAXVARIANTS];
} VARIANTS;

typedef struct
{
  int lastnote;                          // semitones above C-0 of the note that was played last
  int active;                            // 0 nothing, 1 slide up, 2 slide down, 3 tone portamento, 4 vibrato
} CHSTATE;

// The SID frequency of a note, semitones above C-0
static double sidfreq(const FSONGIN *in, int k)
{
  double clk = in->ntsc ? 1022727.0 : 985248.0;
  return 16.3516 * pow(2.0, k / 12.0) * 16777216.0 / clk;
}

// A speed table entry of a slide as semitones each tick: the entry is a frequency step (so more semitones the lower the
// note) or, with bit 15 set, a fraction of the way to the next note
static double slidesemitones(const FSONGIN *in, int param, int k)
{
  int l, r;
  unsigned speed;
  if (!param) return 0;
  l = in->ltable[STBL * MAX_TABLELEN + param - 1];
  r = in->rtable[STBL * MAX_TABLELEN + param - 1];
  speed = (l << 8) | r;
  if (k < 0) k = 0;
  if (k > 94) k = 94;
  if (speed >= 0x8000) return 1.0 / (double)(1 << (r & 15));
  return speed / (sidfreq(in, k + 1) - sidfreq(in, k));
}

static int slideunits(const FSONGIN *in, int param, int k)
{
  double u = slidesemitones(in, param, k) * FSONG_SLIDE_K;
  int v = (int)(u + 0.5);
  if (u > 0 && v < 1) v = 1;
  return v > 255 ? 255 : v;
}

// Vibrato as vibrato() in gplay.c plays it: the pitch moves by one step each tick and the direction is flipped by a counter
// that is compared with the left side of the speedtable entry. Returns the period in ticks and the peak swing in semitones
// at the note k. Furnace's vibrato (04xy) is a sine of 64 / x ticks and y / 16 semitones, as measured in Furnace 0.6.8.3.
static void vibratoshape(const FSONGIN *in, int param, int k, int *period, double *peak)
{
  enum { N = 600 };
  double off[N], lo = 1e9, hi = -1e9, o = 0, speed = 0, f0;
  unsigned char vt[N], vibtime = 0;
  int l = 0, r = 0, cmp, i, p;

  *period = 0; *peak = 0;
  if (param)
  {
    l = in->ltable[STBL * MAX_TABLELEN + param - 1];
    r = in->rtable[STBL * MAX_TABLELEN + param - 1];
  }
  if (k < 0) k = 0;
  if (k > 94) k = 94;
  f0 = sidfreq(in, k);
  speed = r;
  cmp = l;
  if (cmp >= 0x80)
  {
    cmp &= 0x7f;
    speed = (sidfreq(in, k + 1) - f0) / (double)(1 << (r & 15));
  }
  if (!l && !r) return;
  for (i = 0; i < N; i++)
  {
    if (vibtime < 0x80 && vibtime > cmp) vibtime ^= 0xff;
    vibtime += 2;
    vt[i] = vibtime;
    if (vibtime & 1) o -= speed; else o += speed;
    off[i] = o;
  }
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
  *peak = 12.0 * log2((f0 + (hi - lo) / 2) / f0);
}

static int vibratoeffect(const FSONGIN *in, int param, int k)
{
  int period, x, y;
  double peak;
  vibratoshape(in, param, k, &period, &peak);
  if (!period) return 0;
  x = (int)(64.0 / period + 0.5);
  if (x < 1) x = 1;
  if (x > 15) x = 15;
  y = (int)(peak * 16.0 + 0.5);
  if (y < 1) y = 1;
  if (y > 15) y = 15;
  return (x << 4) | y;
}

static void stopeffect(CELL *cell, int active)
{
  if (active == 1) addfx(cell, 0x01, 0);
  else if (active == 2) addfx(cell, 0x02, 0);
  else if (active == 3) addfx(cell, 0x03, 0);
  else if (active == 4) addfx(cell, 0x04, 0);
}

// The pattern command of a row. Tempo and funktempo become speeds (Furnace has one speed for all channels, as the tempo
// commands 03-7F and E have; the ones for one channel only, 80-FF, have no counterpart). Slides, tone portamento and
// vibrato are stopped by the next row in GoatTracker, so Furnace is told to stop them.
static void mapcommand(const FSONGIN *in, int cmd, int data, int funk, CELL *cell, CHSTATE *st, VARIANTS *var, int instr, int *warn)
{
  int kind = cmd == CMD_PORTAUP ? 1 : cmd == CMD_PORTADOWN ? 2 : cmd == CMD_TONEPORTA ? 3 : cmd == CMD_VIBRATO ? 4 : 0;

  if (st->active && st->active != kind) stopeffect(cell, st->active);
  st->active = kind;
  switch (cmd)
  {
    case CMD_DONOTHING:
    break;

    case CMD_PORTAUP:
    addfx(cell, 0x01, slideunits(in, data, st->lastnote));
    break;

    case CMD_PORTADOWN:
    addfx(cell, 0x02, slideunits(in, data, st->lastnote));
    break;

    case CMD_TONEPORTA:
    addfx(cell, 0x03, data ? slideunits(in, data, st->lastnote) : 0xff);
    break;

    case CMD_VIBRATO:
    addfx(cell, 0x04, vibratoeffect(in, data, st->lastnote));
    break;

    case CMD_SETAD:
    addfx(cell, 0x20, data);
    break;

    case CMD_SETSR:
    addfx(cell, 0x21, data);
    break;

    case CMD_SETWAVE:
    addfx(cell, 0x10, (data >> 4) & 15);
    break;

    case CMD_SETFILTERCTRL:
    addfx(cell, 0x13, (data >> 4) & 15);
    if (!data) addfx(cell, 0x14, 0);
    break;

    case CMD_SETFILTERCUTOFF:
    addfx(cell, 0x40 | ((data << 3) >> 8), (data << 3) & 255);
    break;

    case CMD_SETTEMPO:
    if (data >= 0x80) *warn |= FSONG_W_CHANTEMPO;
    else if (data >= 3)
    {
      addfx(cell, 0x09, data);
      if (funk) addfx(cell, 0x0f, data);
    }
    else if (data)
    {
      addfx(cell, 0x09, data == 2 ? 3 : data);
      if (funk) addfx(cell, 0x0f, data == 2 ? 3 : data);
    }
    else *warn |= FSONG_W_COMMANDS;
    break;

    case CMD_FUNKTEMPO:
    if (data >= 1 && data <= MAX_TABLELEN)
    {
      int l = in->ltable[STBL * MAX_TABLELEN + data - 1], r = in->rtable[STBL * MAX_TABLELEN + data - 1];
      addfx(cell, 0x09, l);
      addfx(cell, 0x0f, r);
    }
    else *warn |= FSONG_W_COMMANDS;
    break;

    case CMD_SETWAVEPTR:
    case CMD_SETPULSEPTR:
    case CMD_SETFILTERPTR:
    {
      // A note that starts with another table pointer is another instrument in Furnace (the note starts it)
      int w = in->instr[instr].ptr[WTBL], p = in->instr[instr].ptr[PTBL], f = in->instr[instr].ptr[FTBL], i;
      if (cell->note < 0 || cell->note > 179 || instr < 1 || cell->ins < 0) { *warn |= FSONG_W_COMMANDS; break; }
      if (cmd == CMD_SETWAVEPTR) w = data;
      else if (cmd == CMD_SETPULSEPTR) p = data;
      else f = data;
      for (i = 0; i < var->count; i++)
        if (var->v[i].base == instr && var->v[i].w == w && var->v[i].p == p && var->v[i].f == f) break;
      if (i == var->count)
      {
        if (var->count >= MAXVARIANTS) { *warn |= FSONG_W_COMMANDS; break; }
        var->v[i].base = instr; var->v[i].w = w; var->v[i].p = p; var->v[i].f = f;
        var->count++;
      }
      cell->ins = var->first + i;
    }
    break;

    default:
    // the master volume has no counterpart
    *warn |= FSONG_W_COMMANDS;
    break;
  }
}

// Plays the order list of a channel out
static void unroll(const FSONGIN *in, int c, STREAM *s, int funk, VARIANTS *var, int *warn)
{
  const unsigned char *ord = in->order[c];
  int songlen = in->songlen[c];
  int songptr = 0, trans = 0, instr = 0, guard = 0;
  CHSTATE st = { 0, 0 };
  int rowsofpos[MAX_SONGLEN + 2];
  int i;

  s->len = 0; s->loop = -1; s->truncated = 0;
  for (i = 0; i < MAX_SONGLEN + 2; i++) rowsofpos[i] = -1;
  s->row = (CELL *)calloc(FSONG_MAXROWS, sizeof(CELL));
  if (!s->row) return;

  while (songptr <= songlen && guard++ < 4 * MAX_SONGLEN)
  {
    int repeat = 0, patt, k, r;

    if (ord[songptr] == LOOPSONG)
    {
      int target = ord[songptr + 1];
      if (target < songlen && rowsofpos[target] >= 0) s->loop = rowsofpos[target];
      break;
    }
    if (songptr >= songlen) break;       // the list ends without a loop
    if (rowsofpos[songptr] < 0) rowsofpos[songptr] = s->len;
    if (ord[songptr] >= TRANSDOWN && ord[songptr] < LOOPSONG)
    {
      trans = ord[songptr] - TRANSUP;
      songptr++;
      if (rowsofpos[songptr] < 0) rowsofpos[songptr] = s->len;
    }
    if (ord[songptr] >= REPEAT && ord[songptr] < TRANSDOWN)
    {
      repeat = ord[songptr] - REPEAT;
      songptr++;
      if (rowsofpos[songptr] < 0) rowsofpos[songptr] = s->len;
    }
    patt = ord[songptr];
    songptr++;
    if (patt >= MAX_PATT) break;
    for (k = 0; k <= repeat; k++)
    {
      const unsigned char *pp = in->pattern[patt];
      for (r = 0; r < in->pattlen[patt]; r++)
      {
        int note = pp[r * 4], ins = pp[r * 4 + 1];
        CELL *cell;
        if (s->len >= FSONG_MAXROWS) { s->truncated = 1; return; }
        cell = &s->row[s->len++];
        clearcell(cell);
        if (ins) instr = ins;
        if (note >= FIRSTNOTE && note <= LASTNOTE)
        {
          int n = note + trans;
          cell->note = furnace_note(n);
          if (instr >= 1 && instr < MAX_INSTR && pp[r * 4 + 2] != CMD_TONEPORTA) cell->ins = instr - 1;
          if (pp[r * 4 + 2] != CMD_TONEPORTA) st.lastnote = n - FIRSTNOTE;
        }
        else if (note == KEYOFF)
          cell->note = 180;
        mapcommand(in, pp[r * 4 + 2], pp[r * 4 + 3], funk, cell, &st, var, instr, warn);
      }
    }
  }
}

// How many orders a row stream of this many rows takes
static int ordersfor(int rows, int plen)
{
  return (rows + plen - 1) / plen;
}

static int sameslice(const CELL *a, const CELL *b, int n)
{
  int i;
  for (i = 0; i < n; i++)
  {
    int e;
    if (a[i].note != b[i].note || a[i].ins != b[i].ins) return 0;
    for (e = 0; e < FXN; e++)
      if (a[i].fx[e] != b[i].fx[e] || (a[i].fx[e] >= 0 && a[i].fxval[e] != b[i].fxval[e])) return 0;
  }
  return 1;
}

// One pattern of a channel as Furnace stores it
static void writepattern(BUF *b, int chan, int index, const CELL *row, int plen)
{
  int start, r, empty = 0;

  put(b, 'P'); put(b, 'A'); put(b, 'T'); put(b, 'N');
  start = b->size;
  put32(b, 0);
  put(b, 0);                             // subsong
  put(b, chan);
  put16(b, index);
  put(b, 0);                             // name
  for (r = 0; r < plen; r++)
  {
    const CELL *c = &row[r];
    int mask = 0;
    int nfx = 0, e;
    for (e = 0; e < FXN; e++) if (c->fx[e] >= 0) nfx = e + 1;
    if (c->note >= 0) mask |= 1;
    if (c->ins >= 0) mask |= 2;
    if (nfx == 1) mask |= 8 | 16;
    else if (nfx > 1) mask |= 32;
    if (!mask) { empty++; continue; }
    while (empty > 0)
    {
      if (empty == 1) { put(b, 0); empty = 0; }
      else
      {
        int n = empty > 129 ? 129 : empty;
        put(b, 0x80 | (n - 2));
        empty -= n;
      }
    }
    put(b, mask);
    if (mask & 32)
    {
      int bits = 0;
      for (e = 0; e < nfx; e++) bits |= 3 << (2 * e);
      put(b, bits);
    }
    if (c->note >= 0) put(b, c->note);
    if (c->ins >= 0) put(b, c->ins);
    for (e = 0; e < nfx; e++) { put(b, c->fx[e]); put(b, c->fxval[e]); }
  }
  put(b, 0xff);
  set32(b, start, b->size - start - 4);
}

static void writeadir(BUF *b, int n)
{
  int start, i;
  put(b, 'A'); put(b, 'D'); put(b, 'I'); put(b, 'R');
  start = b->size;
  put32(b, 0);
  if (n > 0)
  {
    put32(b, 1);
    put(b, 0);
    put16(b, n);
    for (i = 0; i < n; i++) put(b, i);
  }
  else
    put32(b, 0);
  set32(b, start, b->size - start - 4);
}

int furnace_song(unsigned char *out, int outsize, const FSONGIN *in, FSONGINFO *info)
{
  STREAM st[MAX_CHN];
  static VARIANTS var;
  BUF b;
  int nch = in->numsids * 3, c, i, r, k;
  int funk = 0, g = 0, plen, total = 0, rows, orders, lastinstr = 0, ninst;
  int loopfrom = -1, looping = 0, period = 1, ends = 0, loopmismatch = 0, prefix = 0;
  int **slot = NULL;                     // slot[c][order] = pattern index of that channel
  int *npat = NULL;
  const CELL **firstrow = NULL;          // the first slice of each pattern, by running number
  int *patchan = NULL, *patidx = NULL, patcount = 0;
  float hz;
  unsigned char tmp[FURNACE_MAXFILE];
  int insptr[MAX_INSTR + MAXVARIANTS + 1], patptr[4096], infosize, infopos, ptrpos, adirpos[3], flagpos[MAX_SIDS];
  int result = 0;

  memset(info, 0, sizeof(*info));
  memset(st, 0, sizeof(st));
  memset(&var, 0, sizeof(var));
  b.p = out; b.size = 0; b.cap = outsize; b.failed = 0;
  if (nch < 3 || nch > MAX_CHN) { snprintf(info->error, sizeof(info->error), "no channels"); return 0; }

  // Which instruments the patterns use: export 1 .. the highest used
  {
    int p, r2;
    for (c = 0; c < nch; c++)
      for (i = 0; i < in->songlen[c]; i++)
      {
        int v = in->order[c][i];
        if (v >= MAX_PATT) continue;
        for (r2 = 0; r2 < in->pattlen[v]; r2++)
          if (in->pattern[v][r2 * 4 + 1] > lastinstr) lastinstr = in->pattern[v][r2 * 4 + 1];
        (void)p;
      }
  }
  if (lastinstr >= MAX_INSTR) lastinstr = MAX_INSTR - 1;

  for (c = 0; c < nch; c++)
    for (i = 0; i < in->songlen[c]; i++)
    {
      int v = in->order[c][i], r2;
      if (v < MAX_PATT)
        for (r2 = 0; r2 < in->pattlen[v]; r2++)
          if (in->pattern[v][r2 * 4 + 2] == CMD_FUNKTEMPO && in->pattern[v][r2 * 4 + 3]) funk = 1;
    }
  var.first = lastinstr;
  for (c = 0; c < nch; c++) unroll(in, c, &st[c], funk, &var, &info->warnings);
  for (c = 0; c < nch; c++)
    if (!st[c].row) { snprintf(info->error, sizeof(info->error), "out of memory"); goto done; }

  ninst = lastinstr + var.count;

  // Pattern length: a multiple of the gcd of the pattern lengths in use, so that every order boundary stays on one
  for (c = 0; c < nch; c++)
    for (i = 0; i < in->songlen[c]; i++)
    {
      int v = in->order[c][i];
      if (v < MAX_PATT && in->pattlen[v] > 0) g = g ? gcd(g, in->pattlen[v]) : in->pattlen[v];
    }
  if (g < 1) g = 64;
  plen = g;                              // for the limit on the length below; the real one is chosen after it

  // How long the streams are: the song, then the loops lined up
  for (c = 0; c < nch; c++)
  {
    if (st[c].len > total) total = st[c].len;
    if (st[c].loop >= 0)
    {
      int per = st[c].len - st[c].loop;
      if (per < 1) per = 1;
      if (!looping) { loopfrom = st[c].loop; period = per; looping = 1; }
      else
      {
        if (st[c].loop != loopfrom) loopmismatch = 1;
        if (st[c].loop > loopfrom) loopfrom = st[c].loop;
        {
          long l = (long)period / gcd(period, per) * per;
          period = l > FSONG_MAXROWS ? FSONG_MAXROWS : (int)l;
        }
      }
    }
    else
      ends = 1;
  }
  if (looping)
  {
    long want = (long)loopfrom + period;
    prefix = loopfrom;
    if (want < total) want = total;
    if (want > 256L * plen) { want = total; info->warnings |= FSONG_W_LOOPS; }
    total = (int)want;
  }
  if (total < 1) total = 1;
  if (total > 256 * plen) { total = 256 * plen; info->warnings |= FSONG_W_TOOLONG; looping = 0; }
  if (loopmismatch) info->warnings |= FSONG_W_LOOPS;

  // The pattern length: a multiple of the gcd, 64 or less, that the whole length is a multiple of if the song loops
  // (the jump back is on the very last row), so that what is played after the lined up loops is the loop again
  if (g > 64) plen = g > 256 ? 256 : g;
  else
  {
    int m = 64 / g;
    plen = g * m;
    if (looping)
      for (; m > 1; m--)
        if (total % (g * m) == 0) { plen = g * m; break; }
    if (looping && total % plen) plen = g;
  }
  if (total > 256 * plen) { total = 256 * plen; info->warnings |= FSONG_W_TOOLONG; looping = 0; }

  // Stretch the loops, line the loop point up with a pattern
  rows = ordersfor(total, plen) * plen;
  orders = rows / plen;
  if (looping && loopfrom % plen)
  {
    info->warnings |= FSONG_W_LOOPUNALIGNED;
    loopfrom -= loopfrom % plen;
  }
  for (c = 0; c < nch; c++)
  {
    CELL *s = (CELL *)realloc(st[c].row, (size_t)rows * sizeof(CELL));
    int len = st[c].len;
    if (!s) { snprintf(info->error, sizeof(info->error), "out of memory"); goto done; }
    st[c].row = s;
    for (r = len; r < rows; r++)
    {
      CELL *cell = &s[r];
      if (st[c].loop >= 0 && len > st[c].loop)
        *cell = s[st[c].loop + (r - len) % (len - st[c].loop)];
      else
      {
        clearcell(cell);
      }
    }
    if (len > rows) st[c].len = rows;
  }

  // The song ends or loops with an effect on the last row of the first channel
  if (looping) addfx(&st[0].row[rows - 1], 0x0b, loopfrom / plen);
  else addfx(&st[0].row[rows - 1], 0xff, 0);
  (void)ends; (void)prefix;

  // Patterns: a slice of rows that is the same as an earlier one of the channel is used again
  slot = (int **)calloc(nch, sizeof(int *));
  npat = (int *)calloc(nch, sizeof(int));
  firstrow = (const CELL **)calloc((size_t)nch * orders, sizeof(CELL *));
  patchan = (int *)calloc((size_t)nch * orders, sizeof(int));
  patidx = (int *)calloc((size_t)nch * orders, sizeof(int));
  if (!slot || !npat || !firstrow || !patchan || !patidx) { snprintf(info->error, sizeof(info->error), "out of memory"); goto done; }
  for (c = 0; c < nch; c++)
  {
    slot[c] = (int *)calloc(orders, sizeof(int));
    if (!slot[c]) { snprintf(info->error, sizeof(info->error), "out of memory"); goto done; }
    for (i = 0; i < orders; i++)
    {
      const CELL *sl = &st[c].row[i * plen];
      int found = -1;
      for (k = 0; k < patcount; k++)
        if (patchan[k] == c && sameslice(firstrow[k], sl, plen)) { found = patidx[k]; break; }
      if (found < 0)
      {
        found = npat[c]++;
        firstrow[patcount] = sl;
        patchan[patcount] = c;
        patidx[patcount] = found;
        patcount++;
      }
      slot[c][i] = found;
    }
  }
  if (patcount > 4096) { snprintf(info->error, sizeof(info->error), "too many patterns"); goto done; }

  hz = (in->ntsc ? 60.0f : 50.124542f) * in->multiplier;

  // File header
  putn(&b, (const unsigned char *)"-Furnace module-", 16);
  put16(&b, FSONG_VERSION);
  put16(&b, 0);
  put32(&b, 32);
  for (i = 0; i < 8; i++) put(&b, 0);

  // Song info
  put(&b, 'I'); put(&b, 'N'); put(&b, 'F'); put(&b, 'O');
  infopos = b.size;
  put32(&b, 0);
  put(&b, 0);                            // time base
  put(&b, 6 * in->multiplier);           // speed 1, 2
  put(&b, 6 * in->multiplier);
  put(&b, 1);                            // arpeggio speed
  putf(&b, hz);
  put16(&b, plen);
  put16(&b, orders);
  put(&b, 4); put(&b, 16);               // row highlight
  put16(&b, ninst);
  put16(&b, 0);                          // wavetables
  put16(&b, 0);                          // samples
  put32(&b, patcount);
  for (i = 0; i < 32; i++) put(&b, i < in->numsids ? (in->sidmodel ? 0x07 : 0x47) : 0);
  for (i = 0; i < 32; i++) put(&b, 64);
  for (i = 0; i < 32; i++) put(&b, 0);
  flagpos[0] = b.size;
  for (i = 0; i < 32; i++) put32(&b, 0);
  puts0(&b, in->name);
  puts0(&b, in->author);
  putf(&b, 440.0f);
  {
    static const unsigned char compat[20] = { 0, 2, 2, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1 };
    putn(&b, compat, 20);
  }
  ptrpos = b.size;
  for (i = 0; i < ninst; i++) put32(&b, 0);
  for (i = 0; i < patcount; i++) put32(&b, 0);
  for (c = 0; c < nch; c++)
    for (i = 0; i < orders; i++) put(&b, slot[c][i]);
  for (c = 0; c < nch; c++) put(&b, FXN);  // effect columns
  for (c = 0; c < nch; c++) put(&b, 3);  // channel shown
  for (c = 0; c < nch; c++) put(&b, 0);  // collapsed
  for (c = 0; c < nch; c++) put(&b, 0);  // channel names (empty)
  for (c = 0; c < nch; c++) put(&b, 0);  // short names
  put(&b, 0);                            // comment
  putf(&b, 1.0f);
  {
    static const unsigned char ext[28] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 4, 0, 0, 1, 1, 0, 0, 0, 0, 2, 0, 1, 0, 0, 0 };
    putn(&b, ext, 28);
  }
  put16(&b, 150); put16(&b, 150);        // virtual tempo
  put(&b, 0); put(&b, 0);                // first subsong name, comment
  put(&b, 0); put(&b, 0); put(&b, 0); put(&b, 0); // no more subsongs
  puts0(&b, in->sidmodel ? "MOS Technology SID (8580)" : "MOS Technology SID (6581)");
  for (i = 0; i < 5; i++) put(&b, 0);
  for (c = 0; c < in->numsids; c++) { putf(&b, 1.0f); putf(&b, 0.0f); putf(&b, 0.0f); }
  put32(&b, 0);                          // patchbay: none, made by Furnace
  put(&b, 1);                            // automatic patchbay
  for (i = 0; i < 8; i++) put(&b, 0);
  put(&b, 1);                            // speed pattern
  put(&b, 6 * in->multiplier);
  for (i = 1; i < 16; i++) put(&b, 6 * in->multiplier);
  put(&b, 0);                            // no grooves
  adirpos[0] = b.size; put32(&b, 0);
  adirpos[1] = b.size; put32(&b, 0);
  adirpos[2] = b.size; put32(&b, 0);
  infosize = b.size - infopos - 4;
  set32(&b, infopos, infosize);

  // Chip flags: PAL clock
  if (!in->ntsc)
    for (c = 0; c < in->numsids; c++)
    {
      int s;
      static const char text[] = "clockSel=1\n";
      set32(&b, flagpos[0] + c * 4, b.size);
      put(&b, 'F'); put(&b, 'L'); put(&b, 'A'); put(&b, 'G');
      s = b.size;
      put32(&b, 0);
      putn(&b, (const unsigned char *)text, sizeof(text));
      set32(&b, s, b.size - s - 4);
    }

  // Directories of instruments, wavetables, samples
  set32(&b, adirpos[0], b.size); writeadir(&b, ninst);
  set32(&b, adirpos[1], b.size); writeadir(&b, 0);
  set32(&b, adirpos[2], b.size); writeadir(&b, 0);

  // Instruments
  for (i = 1; i <= ninst; i++)
  {
    FURNACEINFO fi;
    INSTR copy;
    const INSTR *src = &in->instr[i <= lastinstr ? i : var.v[i - lastinstr - 1].base];
    int n;
    if (i > lastinstr)
    {
      const VARIANT *vr = &var.v[i - lastinstr - 1];
      copy = *src;
      copy.ptr[WTBL] = vr->w; copy.ptr[PTBL] = vr->p; copy.ptr[FTBL] = vr->f;
      src = &copy;
    }
    n = furnace_instrument(tmp, sizeof(tmp), src,
                           in->ltable + WTBL * MAX_TABLELEN, in->rtable + WTBL * MAX_TABLELEN,
                           in->ltable + PTBL * MAX_TABLELEN, in->rtable + PTBL * MAX_TABLELEN,
                           in->ltable + FTBL * MAX_TABLELEN, in->rtable + FTBL * MAX_TABLELEN, &fi);
    if (n < 12) { snprintf(info->error, sizeof(info->error), "instrument %d could not be written", i); goto done; }
    info->instrwarnings |= fi.warnings;
    insptr[i] = b.size;
    put(&b, 'I'); put(&b, 'N'); put(&b, 'S'); put(&b, '2');
    put32(&b, n - 4);
    putn(&b, tmp + 4, n - 4);
  }

  // Patterns
  for (k = 0; k < patcount; k++)
  {
    if (k < 4096) patptr[k] = b.size;
    writepattern(&b, patchan[k], patidx[k], firstrow[k], plen);
  }
  if (b.failed) { snprintf(info->error, sizeof(info->error), "the file is too big"); goto done; }
  for (i = 1; i <= ninst; i++) set32(&b, ptrpos + (i - 1) * 4, insptr[i]);
  for (k = 0; k < patcount; k++) set32(&b, ptrpos + ninst * 4 + k * 4, patptr[k]);

  info->size = b.size;
  info->instruments = ninst;
  info->patterns = patcount;
  info->orders = orders;
  info->patternlen = plen;
  info->rows = rows;
  result = b.size;

done:
  for (c = 0; c < nch; c++) free(st[c].row);
  if (slot) { for (c = 0; c < nch; c++) free(slot[c]); free(slot); }
  free(npat); free(firstrow); free(patchan); free(patidx);
  (void)total;
  return result;
}

void furnace_songwarningtext(char *buf, int size, int warnings)
{
  static const struct { int bit; const char *text; } list[] =
  {
    { FSONG_W_LOOPS, "channels that loop at different rows (the loop is approximated)" },
    { FSONG_W_TOOLONG, "more than 256 orders (cut)" },
    { FSONG_W_LOOPUNALIGNED, "a loop point inside a pattern (moved to the pattern start)" },
    { FSONG_W_INSTR, "instruments that are not exported fully" },
    { FSONG_W_COMMANDS, "pattern commands without a counterpart (master volume, table pointers on a row without a note)" },
    { FSONG_W_CHANTEMPO, "a tempo for one channel only (all channels follow the others)" },
  };
  int i, n = 0;
  if (size < 1) return;
  buf[0] = 0;
  for (i = 0; i < (int)(sizeof(list) / sizeof(list[0])); i++)
    if (warnings & list[i].bit)
      n += snprintf(buf + n, size - n > 0 ? size - n : 0, "%s%s", n ? ", " : "", list[i].text);
}
