//
// C64 GrooveGoat - export of a whole song to a Furnace module (.fur), see gfurnacesong.h
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gfurnacesong.h"

typedef struct
{
  int note;                              // -1 none, 0..179 Furnace note, 180 note off
  int ins;                               // Furnace instrument number (GoatTracker's minus one), -1 none
  int fx[3], fxval[3];                   // Furnace effects and values, fx -1 for none, filled from the first
} CELL;

static void addfx(CELL *c, int fx, int val)
{
  int i;
  for (i = 0; i < 3; i++)
    if (c->fx[i] < 0) { c->fx[i] = fx; c->fxval[i] = val; return; }
}

static void clearcell(CELL *c)
{
  c->note = -1; c->ins = -1;
  c->fx[0] = c->fx[1] = c->fx[2] = -1;
  c->fxval[0] = c->fxval[1] = c->fxval[2] = 0;
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

// The pattern command of a row: tempo and funktempo become speeds. Furnace has one speed for all channels, as the
// tempo commands of GoatTracker (03-7F, E) have; the ones for one channel only (80-FF) have no counterpart.
static void mapcommand(const FSONGIN *in, int cmd, int data, int funk, CELL *cell, int *warn)
{
  if (cmd == CMD_SETTEMPO)
  {
    if (data >= 0x80) *warn |= FSONG_W_CHANTEMPO;
    else if (data >= 3)
    {
      addfx(cell, 0x09, data);
      if (funk) addfx(cell, 0x0f, data);
    }
    else if (data) addfx(cell, 0x09, data == 2 ? 3 : data), (funk ? addfx(cell, 0x0f, data == 2 ? 3 : data) : (void)0);
    else *warn |= FSONG_W_COMMANDS;
  }
  else if (cmd == CMD_FUNKTEMPO)
  {
    if (data >= 1 && data <= MAX_TABLELEN)
    {
      int l = in->ltable[STBL * MAX_TABLELEN + data - 1], r = in->rtable[STBL * MAX_TABLELEN + data - 1];
      addfx(cell, 0x09, l);
      addfx(cell, 0x0f, r);
    }
    else *warn |= FSONG_W_COMMANDS;
  }
}

// Plays the order list of a channel out
static void unroll(const FSONGIN *in, int c, STREAM *s, int funk, int *warn)
{
  const unsigned char *ord = in->order[c];
  int songlen = in->songlen[c];
  int songptr = 0, trans = 0, instr = 0, guard = 0;
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
          if (instr >= 1 && instr < MAX_INSTR) cell->ins = instr - 1;
        }
        else if (note == KEYOFF)
          cell->note = 180;
        mapcommand(in, pp[r * 4 + 2], pp[r * 4 + 3], funk, cell, warn);
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
    for (e = 0; e < 3; e++)
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
    for (e = 0; e < 3; e++) if (c->fx[e] >= 0) nfx = e + 1;
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
  BUF b;
  int nch = in->numsids * 3, c, i, r, k;
  int funk = 0, g = 0, plen, total = 0, rows, orders, lastinstr = 0;
  int loopfrom = -1, looping = 0, period = 1, ends = 0, loopmismatch = 0, prefix = 0;
  int **slot = NULL;                     // slot[c][order] = pattern index of that channel
  int *npat = NULL;
  const CELL **firstrow = NULL;          // the first slice of each pattern, by running number
  int *patchan = NULL, *patidx = NULL, patcount = 0;
  float hz;
  unsigned char tmp[FURNACE_MAXFILE];
  int insptr[MAX_INSTR], patptr[4096], infosize, infopos, ptrpos, adirpos[3], flagpos[MAX_SIDS];
  int result = 0;

  memset(info, 0, sizeof(*info));
  memset(st, 0, sizeof(st));
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
  for (c = 0; c < nch; c++) unroll(in, c, &st[c], funk, &info->warnings);
  for (c = 0; c < nch; c++)
    if (!st[c].row) { snprintf(info->error, sizeof(info->error), "out of memory"); goto done; }

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
  put16(&b, lastinstr);
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
  for (i = 0; i < lastinstr; i++) put32(&b, 0);
  for (i = 0; i < patcount; i++) put32(&b, 0);
  for (c = 0; c < nch; c++)
    for (i = 0; i < orders; i++) put(&b, slot[c][i]);
  for (c = 0; c < nch; c++) put(&b, 3);  // effect columns
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
  set32(&b, adirpos[0], b.size); writeadir(&b, lastinstr);
  set32(&b, adirpos[1], b.size); writeadir(&b, 0);
  set32(&b, adirpos[2], b.size); writeadir(&b, 0);

  // Instruments
  for (i = 1; i <= lastinstr; i++)
  {
    FURNACEINFO fi;
    int n = furnace_instrument(tmp, sizeof(tmp), &in->instr[i],
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
  for (i = 1; i <= lastinstr; i++) set32(&b, ptrpos + (i - 1) * 4, insptr[i]);
  for (k = 0; k < patcount; k++) set32(&b, ptrpos + lastinstr * 4 + k * 4, patptr[k]);

  info->size = b.size;
  info->instruments = lastinstr;
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
    { FSONG_W_COMMANDS, "pattern commands that are not exported" },
    { FSONG_W_CHANTEMPO, "a tempo for one channel only (all channels follow the others)" },
  };
  int i, n = 0;
  if (size < 1) return;
  buf[0] = 0;
  for (i = 0; i < (int)(sizeof(list) / sizeof(list[0])); i++)
    if (warnings & list[i].bit)
      n += snprintf(buf + n, size - n > 0 ? size - n : 0, "%s%s", n ? ", " : "", list[i].text);
}
