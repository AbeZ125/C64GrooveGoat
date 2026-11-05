//
// C64 GrooveGoat - import of a Furnace module (.fur) as a GoatTracker song, see gfurnacesongimp.h
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "gfurnacesongimp.h"

// ---------------------------------------------------------------------------------------------------------------------
// Inflate (RFC 1950 / 1951), the small way: canonical Huffman codes decoded a bit at a time
// ---------------------------------------------------------------------------------------------------------------------

typedef struct
{
  const unsigned char *in;
  int len, pos, bitbuf, bitcnt;
  unsigned char *out;
  int outsize, outcap;
  int bad;
} INF;

typedef struct { short count[16]; short symbol[288]; } HUFF;

static int getbits(INF *s, int need)
{
  int val = s->bitbuf;
  while (s->bitcnt < need)
  {
    if (s->pos >= s->len) { s->bad = 1; return 0; }
    val |= (int)s->in[s->pos++] << s->bitcnt;
    s->bitcnt += 8;
  }
  s->bitbuf = val >> need;
  s->bitcnt -= need;
  return val & ((1 << need) - 1);
}

static void putbyte(INF *s, int b)
{
  if (s->outsize >= s->outcap)
  {
    int ncap = s->outcap ? s->outcap * 2 : 65536;
    unsigned char *n;
    if (ncap > 64 * 1024 * 1024) { s->bad = 1; return; }
    n = (unsigned char *)realloc(s->out, ncap);
    if (!n) { s->bad = 1; return; }
    s->out = n; s->outcap = ncap;
  }
  s->out[s->outsize++] = b;
}

static int construct(HUFF *h, const short *length, int n)
{
  int symbol, len, left;
  short offs[16];
  for (len = 0; len <= 15; len++) h->count[len] = 0;
  for (symbol = 0; symbol < n; symbol++) h->count[length[symbol]]++;
  if (h->count[0] == n) return 0;
  left = 1;
  for (len = 1; len <= 15; len++)
  {
    left <<= 1;
    left -= h->count[len];
    if (left < 0) return left;
  }
  offs[1] = 0;
  for (len = 1; len < 15; len++) offs[len + 1] = offs[len] + h->count[len];
  for (symbol = 0; symbol < n; symbol++)
    if (length[symbol]) h->symbol[offs[length[symbol]]++] = symbol;
  return left;
}

static int decode(INF *s, const HUFF *h)
{
  int len, code = 0, first = 0, index = 0;
  for (len = 1; len <= 15; len++)
  {
    int count;
    code |= getbits(s, 1);
    if (s->bad) return -1;
    count = h->count[len];
    if (code - count < first) return h->symbol[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return -1;
}

static int codes(INF *s, const HUFF *lencode, const HUFF *distcode)
{
  static const short lens[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
  static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
  static const short dists[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
  static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
  int symbol;
  do
  {
    symbol = decode(s, lencode);
    if (symbol < 0) return -1;
    if (symbol < 256) putbyte(s, symbol);
    else if (symbol > 256)
    {
      int len, dist;
      symbol -= 257;
      if (symbol >= 29) return -1;
      len = lens[symbol] + getbits(s, lext[symbol]);
      symbol = decode(s, distcode);
      if (symbol < 0 || symbol >= 30) return -1;
      dist = dists[symbol] + getbits(s, dext[symbol]);
      if (s->bad || dist > s->outsize) return -1;
      while (len--) putbyte(s, s->out[s->outsize - dist]);
    }
    if (s->bad) return -1;
  } while (symbol != 256);
  return 0;
}

static int fixed(INF *s)
{
  static HUFF lencode, distcode;
  static int built;
  if (!built)
  {
    short lengths[288];
    int symbol;
    for (symbol = 0; symbol < 144; symbol++) lengths[symbol] = 8;
    for (; symbol < 256; symbol++) lengths[symbol] = 9;
    for (; symbol < 280; symbol++) lengths[symbol] = 7;
    for (; symbol < 288; symbol++) lengths[symbol] = 8;
    construct(&lencode, lengths, 288);
    for (symbol = 0; symbol < 30; symbol++) lengths[symbol] = 5;
    construct(&distcode, lengths, 30);
    built = 1;
  }
  return codes(s, &lencode, &distcode);
}

static int dynamic(INF *s)
{
  static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
  short lengths[320];
  HUFF lencode, distcode;
  int nlen, ndist, ncode, index, err;

  nlen = getbits(s, 5) + 257;
  ndist = getbits(s, 5) + 1;
  ncode = getbits(s, 4) + 4;
  if (s->bad || nlen > 286 || ndist > 30) return -1;
  for (index = 0; index < ncode; index++) lengths[order[index]] = getbits(s, 3);
  for (; index < 19; index++) lengths[order[index]] = 0;
  if (construct(&lencode, lengths, 19) != 0) return -1;
  index = 0;
  while (index < nlen + ndist)
  {
    int symbol = decode(s, &lencode), len, rep;
    if (symbol < 0) return -1;
    if (symbol < 16) lengths[index++] = symbol;
    else
    {
      len = 0;
      if (symbol == 16)
      {
        if (index == 0) return -1;
        len = lengths[index - 1];
        rep = 3 + getbits(s, 2);
      }
      else if (symbol == 17) rep = 3 + getbits(s, 3);
      else rep = 11 + getbits(s, 7);
      if (s->bad || index + rep > nlen + ndist) return -1;
      while (rep--) lengths[index++] = len;
    }
  }
  if (lengths[256] == 0) return -1;
  err = construct(&lencode, lengths, nlen);
  if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) return -1;
  err = construct(&distcode, lengths + nlen, ndist);
  if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) return -1;
  return codes(s, &lencode, &distcode);
}

int fsi_inflate(const unsigned char *in, int inlen, unsigned char **out, int raw)
{
  INF s;
  int last, type;
  memset(&s, 0, sizeof s);
  *out = NULL;
  s.in = in; s.len = inlen;
  if (!raw)
  {
    if (inlen < 6 || (in[0] & 15) != 8 || ((in[0] << 8) | in[1]) % 31) return -1;
    s.pos = 2;
  }
  do
  {
    last = getbits(&s, 1);
    type = getbits(&s, 2);
    if (s.bad) break;
    if (type == 0)
    {
      int len, nlen;
      s.bitbuf = 0; s.bitcnt = 0;
      if (s.pos + 4 > s.len) { s.bad = 1; break; }
      len = s.in[s.pos] | (s.in[s.pos + 1] << 8);
      nlen = s.in[s.pos + 2] | (s.in[s.pos + 3] << 8);
      s.pos += 4;
      if (len != (~nlen & 0xffff) || s.pos + len > s.len) { s.bad = 1; break; }
      while (len--) putbyte(&s, s.in[s.pos++]);
    }
    else if (type == 1) { if (fixed(&s)) s.bad = 1; }
    else if (type == 2) { if (dynamic(&s)) s.bad = 1; }
    else s.bad = 1;
  } while (!last && !s.bad);
  if (s.bad) { free(s.out); return -1; }
  *out = s.out;
  return s.outsize;
}

// ---------------------------------------------------------------------------------------------------------------------
// The module
// ---------------------------------------------------------------------------------------------------------------------

typedef struct
{
  const unsigned char *d;
  int size, pos, bad;
} RD;

static int r8(RD *r) { if (r->pos + 1 > r->size) { r->bad = 1; return 0; } return r->d[r->pos++]; }
static int r16(RD *r) { int a = r8(r); return a | (r8(r) << 8); }
static unsigned r32(RD *r) { unsigned a = r16(r); return a | ((unsigned)r16(r) << 16); }
static float rf(RD *r) { unsigned u = r32(r); float f; memcpy(&f, &u, 4); return f; }
static void rskip(RD *r, int n) { if (n < 0 || r->pos + n > r->size) { r->bad = 1; return; } r->pos += n; }
static void rstr(RD *r, char *out, int cap)
{
  int n = 0;
  while (r->pos < r->size && r->d[r->pos])
  {
    if (out && n < cap - 1) out[n++] = r->d[r->pos];
    r->pos++;
  }
  if (r->pos >= r->size) r->bad = 1; else r->pos++;
  if (out) out[n] = 0;
}

typedef struct
{
  short note;                            // -1 none, 0..179, 180 note off, 181 release, 182 macro release
  short ins;                             // -1 none
  short vol;                             // -1 none
  short fx[8], fv[8];                    // -1 none
} FCELL;

typedef struct
{
  int version, plen, olen, nch, nchips, ni, npat;
  float hz;
  int timebase, speed[16], speedlen, arpspeed;
  int chip0;
  int orders[MAX_CHN][256];
  FCELL *pat[MAX_CHN][256];              // the patterns, by channel and index (NULL = empty)
  unsigned insptr[256];
  char name[MAX_STR], author[MAX_STR];
  float tuning;
  int subsongs;
} MODULE;

static void freemodule(MODULE *m)
{
  int c, i;
  for (c = 0; c < MAX_CHN; c++)
    for (i = 0; i < 256; i++) { free(m->pat[c][i]); m->pat[c][i] = NULL; }
}

static int readpatn(const unsigned char *d, int size, unsigned p, MODULE *m)
{
  RD r;
  unsigned blocksize;
  int sub, ch, idx, row = 0, end;
  FCELL *cells;
  int i;

  if (p + 12 > (unsigned)size || memcmp(d + p, "PATN", 4)) return 0;
  r.d = d; r.size = size; r.pos = p + 4; r.bad = 0;
  blocksize = r32(&r);
  end = p + 8 + blocksize;
  if (end > size) return 0;
  r.size = end;
  sub = r8(&r); ch = r8(&r); idx = r16(&r);
  rstr(&r, NULL, 0);
  if (r.bad) return 0;
  if (sub != 0) return 1;                // another subsong: skipped
  if (ch >= m->nch || idx >= 256) return 1;
  cells = (FCELL *)malloc(sizeof(FCELL) * m->plen);
  if (!cells) return 0;
  for (i = 0; i < m->plen; i++)
  {
    int e;
    cells[i].note = cells[i].ins = cells[i].vol = -1;
    for (e = 0; e < 8; e++) cells[i].fx[e] = cells[i].fv[e] = -1;
  }
  while (r.pos < end)
  {
    int mask = r8(&r), e, bits[2] = { 0, 0 };
    if (r.bad) break;
    if (mask == 0xff) break;
    if (mask & 0x80) { row += (mask & 0x7f) + 2; continue; }
    if (!mask) { row++; continue; }
    if (mask & 32) bits[0] = r8(&r);
    else { if (mask & 8) bits[0] |= 1; if (mask & 16) bits[0] |= 2; }
    if (mask & 64) bits[1] = r8(&r);
    if (row >= m->plen) { r.bad = 1; break; }
    if (mask & 1) cells[row].note = r8(&r);
    if (mask & 2) cells[row].ins = r8(&r);
    if (mask & 4) cells[row].vol = r8(&r);
    for (e = 0; e < 8; e++)
    {
      int b = bits[e / 4] >> (2 * (e % 4));
      if (b & 1) cells[row].fx[e] = r8(&r);
      if (b & 2) cells[row].fv[e] = r8(&r);
    }
    row++;
  }
  if (r.bad) { free(cells); return 0; }
  free(m->pat[ch][idx]);
  m->pat[ch][idx] = cells;
  return 1;
}

static int readmodule(const unsigned char *d, int size, MODULE *m)
{
  RD r;
  unsigned infoptr, flagptr[32];
  int i, ntot, chipids[32], na, nw, ns, speedlen;
  unsigned patptr[1024];

  memset(m, 0, sizeof *m);
  if (size < 64 || memcmp(d, "-Furnace module-", 16)) return FSI_NOTFUR;
  m->version = d[16] | (d[17] << 8);
  if (m->version < 157 || m->version >= 240) return FSI_TOONEW;
  infoptr = d[20] | (d[21] << 8) | (d[22] << 16) | ((unsigned)d[23] << 24);
  if (infoptr + 8 > (unsigned)size || memcmp(d + infoptr, "INFO", 4)) return FSI_NOTFUR;
  r.d = d; r.size = size; r.pos = infoptr + 8; r.bad = 0;
  m->timebase = r8(&r);
  m->speed[0] = r8(&r); m->speed[1] = r8(&r);
  m->speedlen = 2;
  m->arpspeed = r8(&r);
  m->hz = rf(&r);
  m->plen = r16(&r);
  m->olen = r16(&r);
  rskip(&r, 2);                          // row highlight
  m->ni = r16(&r);
  nw = r16(&r);
  ns = r16(&r);
  m->npat = r32(&r);
  ntot = 0;
  for (i = 0; i < 32; i++) chipids[i] = r8(&r);
  rskip(&r, 64);                         // volumes, panning
  for (i = 0; i < 32; i++) flagptr[i] = r32(&r);
  rstr(&r, m->name, MAX_STR);
  rstr(&r, m->author, MAX_STR);
  m->tuning = rf(&r);
  rskip(&r, 20);                         // compatibility flags
  if (r.bad) return FSI_NOTFUR;
  for (i = 0; i < 32; i++)
  {
    if (!chipids[i]) continue;
    if (chipids[i] != 0x07 && chipids[i] != 0x47) return FSI_NOTC64;
    if (!ntot) m->chip0 = chipids[i];
    ntot++;
  }
  if (!ntot) return FSI_NOTFUR;
  if (ntot > MAX_SIDS) return FSI_TOOBIG;
  m->nchips = ntot;
  m->nch = ntot * 3;
  if (m->plen < 1 || m->plen > 256 || m->olen < 1 || m->olen > 256 || m->ni > 256 || m->npat > 1024) return FSI_NOTFUR;
  for (i = 0; i < m->ni; i++) m->insptr[i] = r32(&r);
  rskip(&r, 4 * nw + 4 * ns);
  for (i = 0; i < m->npat; i++) patptr[i] = r32(&r);
  if (r.bad) return FSI_NOTFUR;
  {
    int c, o;
    for (c = 0; c < m->nch; c++)
      for (o = 0; o < m->olen; o++) m->orders[c][o] = r8(&r);
  }
  rskip(&r, 3 * m->nch);                 // effect columns, hidden, collapsed
  for (i = 0; i < 2 * m->nch; i++) rstr(&r, NULL, 0);
  rstr(&r, NULL, 0);                     // comment
  rskip(&r, 4 + 28 + 4);                 // master volume, extended compatibility flags, virtual tempo
  rstr(&r, NULL, 0); rstr(&r, NULL, 0);  // first subsong name, comment
  na = r8(&r);
  rskip(&r, 3 + 4 * na);
  m->subsongs = 1 + na;
  for (i = 0; i < 6; i++) rstr(&r, NULL, 0);
  rskip(&r, 12 * ntot);                  // output volume, panning, balance of each chip
  {
    unsigned pc = r32(&r);
    if (pc > 100000) return FSI_NOTFUR;
    rskip(&r, 4 * (int)pc);
  }
  r8(&r);                                // automatic patchbay
  rskip(&r, (m->version >= 138) + (m->version >= 155) + (m->version >= 168) + (m->version >= 183) +
            (m->version >= 184) + (m->version >= 188) + (m->version >= 191) + (m->version >= 200));
  speedlen = r8(&r);
  if (speedlen >= 1 && speedlen <= 16)
  {
    for (i = 0; i < 16; i++) m->speed[i] = r8(&r);
    m->speedlen = speedlen;
  }
  if (r.bad) return FSI_NOTFUR;
  for (i = 0; i < m->npat; i++)
    if (!readpatn(d, size, patptr[i], m)) { freemodule(m); return FSI_NOTFUR; }
  return FSI_OK;
}

// ---------------------------------------------------------------------------------------------------------------------
// From the module to the song
// ---------------------------------------------------------------------------------------------------------------------

// The SID frequency of a note, semitones above C-0
static double sidfreq(int ntsc, int k)
{
  double clk = ntsc ? 1022727.0 : 985248.0;
  return 16.3516 * pow(2.0, k / 12.0) * 16777216.0 / clk;
}

typedef struct
{
  int slide;                             // 0 none, 1 up, 2 down, 3 tone portamento, 4 vibrato
  int param;                             // the Furnace effect value of it
  int lastnote;                          // semitones above C-0
  int instr;                             // the instrument last written in a pattern
  int arp;                               // the arpeggio, 0xXY + 0x100 (0 = none)
  int arpspeed;                          // ticks between the steps
} CHST;

static int stblfind(FSONGIMP *imp, int l, int r)
{
  int i;
  for (i = 0; i < imp->stbllen; i++)
    if (imp->stbl_l[i] == l && imp->stbl_r[i] == r) return i + 1;
  if (imp->stbllen >= MAX_TABLELEN - 1) { imp->warnings |= FSI_W_SPEEDTABLE; return 0; }
  imp->stbl_l[imp->stbllen] = l;
  imp->stbl_r[imp->stbllen] = r;
  imp->stbllen++;
  return imp->stbllen;
}

static int slideentry(FSONGIMP *imp, int units, int k)
{
  double st = units / 32.0, gap;
  int speed;
  if (k < 0) k = 0;
  if (k > 94) k = 94;
  gap = sidfreq(imp->ntsc, k + 1) - sidfreq(imp->ntsc, k);
  speed = (int)(st * gap + 0.5);
  if (speed < 1) speed = 1;
  if (speed > 0x7fff) speed = 0x7fff;
  return stblfind(imp, speed >> 8, speed & 255);
}

// The period of a GoatTracker vibrato with this left side, and the swing of one step (a table entry of speed 1)
static void vibsim(int cmp, int *period, double *swing)
{
  enum { N = 600 };
  unsigned char vt[N], vibtime = 0;
  double off[N], o = 0, lo = 1e9, hi = -1e9;
  int i, p;
  *period = 0; *swing = 0;
  for (i = 0; i < N; i++)
  {
    if (vibtime < 0x80 && vibtime > cmp) vibtime ^= 0xff;
    vibtime += 2;
    vt[i] = vibtime;
    if (vibtime & 1) o -= 1; else o += 1;
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
  *swing = (hi - lo) / 2;
}

// Furnace's vibrato 04xy is a sine of 64 / x ticks and y / 16 semitones peak (measured in Furnace 0.6.8.3)
static int vibratoentry(FSONGIMP *imp, int x, int y, int k)
{
  int cmp, best = 0, bestdiff = 1 << 30, period, speed;
  double swing, peak, f0, units;
  double want = 64.0 / x;
  if (k < 0) k = 0;
  if (k > 94) k = 94;
  for (cmp = 0; cmp < 128; cmp += 2)
  {
    int diff;
    vibsim(cmp, &period, &swing);
    if (!period) continue;
    diff = (int)fabs((period - want) * 100);
    if (diff < bestdiff) { bestdiff = diff; best = cmp; }
  }
  vibsim(best, &period, &swing);
  if (!period || swing <= 0) return 0;
  peak = y / 16.0;
  f0 = sidfreq(imp->ntsc, k);
  units = f0 * (pow(2.0, peak / 12.0) - 1.0);
  speed = (int)(units / swing + 0.5);
  if (speed < 1) speed = 1;
  if (speed > 255) speed = 255;
  return stblfind(imp, best, speed);
}

// The wavetable rows of an arpeggio: the base note, +x, +y, each for speed ticks, then around again. The program only
// moves the pitch, the waveform stays as it is. Returns the 1-based row where it starts, 0 if the table is full.
static int arpprogram(FSONGIMP *imp, int x, int y, int speed)
{
  int i, d = speed < 1 ? 0 : speed > 16 ? 15 : speed - 1;
  for (i = 0; i + 3 < imp->wtbllen; i++)
    if (imp->wtbl_l[i] == d && imp->wtbl_r[i] == 0 && imp->wtbl_l[i + 1] == d && imp->wtbl_r[i + 1] == x &&
        imp->wtbl_l[i + 2] == d && imp->wtbl_r[i + 2] == y && imp->wtbl_l[i + 3] == 0xff && imp->wtbl_r[i + 3] == i + 1)
      return i + 1;
  if (imp->wtbllen + 4 > MAX_TABLELEN - 1) { imp->warnings |= FSI_W_WAVETABLE; return 0; }
  i = imp->wtbllen;
  imp->wtbl_l[i] = d; imp->wtbl_r[i] = 0;
  imp->wtbl_l[i + 1] = d; imp->wtbl_r[i + 1] = x;
  imp->wtbl_l[i + 2] = d; imp->wtbl_r[i + 2] = y;
  imp->wtbl_l[i + 3] = 0xff; imp->wtbl_r[i + 3] = i + 1;
  imp->wtbllen += 4;
  return i + 1;
}

typedef struct { int cmd, data, prio; } GTCMD;

static int gtnote(int n, int *warn)
{
  int g;
  if (n < 0 || n > 179) return REST;
  g = n + 36;
  while (g < FIRSTNOTE) { g += 12; *warn |= FSI_W_NOTERANGE; }
  while (g > LASTNOTE) { g -= 12; *warn |= FSI_W_NOTERANGE; }
  return g;
}

int furnace_songimport(const unsigned char *data, int size, FSONGIMP *imp)
{
  static MODULE m;
  unsigned char *plain = NULL;
  int plainsize = 0, res, c, i;
  typedef struct { int order, rows; } SEQ;
  SEQ seq[1024];
  int nseq = 0, loopseq = -1, visited[256];
  int s1, s2, alt, cur;
  int lasts1, lasts2, lastalt;
  CHST st[MAX_CHN];

  memset(imp, 0, sizeof *imp);
  if (size >= 2 && data[0] == 0x78)
  {
    plainsize = fsi_inflate(data, size, &plain, 0);
    if (plainsize < 0) return FSI_NOTFUR;
    data = plain; size = plainsize;
  }
  res = readmodule(data, size, &m);
  if (res != FSI_OK) { free(plain); return res; }

  // The song settings
  imp->numsids = m.nchips;
  imp->sidmodel = m.chip0 == 0x07 ? 1 : 0;
  snprintf(imp->name, sizeof imp->name, "%s", m.name);
  snprintf(imp->author, sizeof imp->author, "%s", m.author);
  if (m.subsongs > 1) imp->warnings |= FSI_W_SUBSONGS;
  if (m.tuning > 0 && fabs(m.tuning - 440.0) > 0.5) imp->warnings |= FSI_W_TUNING;
  {
    double hz = m.hz * (m.timebase + 1), pal = 50.124542, ntsc = 60.0;
    if (!(hz >= 1.0 && hz <= 1000.0)) { hz = pal; imp->warnings |= FSI_W_TIMING; }
    int mp = (int)(hz / pal + 0.5), mn = (int)(hz / ntsc + 0.5);
    double dp = mp >= 1 ? fabs(hz - mp * pal) / hz : 1, dn = mn >= 1 ? fabs(hz - mn * ntsc) / hz : 1;
    if (dn < dp) { imp->ntsc = 1; imp->multiplier = mn; if (dn > 0.01) imp->warnings |= FSI_W_TIMING; }
    else { imp->ntsc = 0; imp->multiplier = mp < 1 ? 1 : mp; if (dp > 0.01) imp->warnings |= FSI_W_TIMING; }
    if (imp->multiplier < 1) imp->multiplier = 1;
    if (imp->multiplier > 16) { imp->multiplier = 16; imp->warnings |= FSI_W_TIMING; }
  }

  // The instruments: slot n is Furnace instrument n - 1
  imp->instruments = m.ni > FSI_MAXINSTR ? FSI_MAXINSTR : m.ni;
  if (m.ni > FSI_MAXINSTR) imp->warnings |= FSI_W_INSTR;
  for (i = 0; i < imp->instruments; i++)
  {
    unsigned p = m.insptr[i];
    if (p + 8 <= (unsigned)size && !memcmp(data + p, "INS2", 4))
    {
      unsigned sz = data[p + 4] | (data[p + 5] << 8) | (data[p + 6] << 16) | ((unsigned)data[p + 7] << 24);
      if (p + 8 + sz <= (unsigned)size) { imp->instroffset[i + 1] = p + 8; imp->instrsize[i + 1] = sz; }
    }
  }
  imp->module = (unsigned char *)malloc(size);
  if (!imp->module) { freemodule(&m); free(plain); return FSI_TOOBIG; }
  memcpy(imp->module, data, size);
  imp->modulesize = size;
  free(plain);

  // The order of play: the orders in sequence, until a jump goes back to an order that has been played (the loop) or
  // the song ends. A pattern break (0Dxx) or jump (0Bxx) ends the order after its row, a stop (FFxx) ends the song.
  memset(visited, -1, sizeof visited);
  s1 = m.speed[0]; s2 = m.speedlen > 1 ? m.speed[1] : m.speed[0];
  alt = m.speedlen > 1 && m.speed[0] != m.speed[1];
  lasts1 = lasts2 = 6 * imp->multiplier; lastalt = 0;
  cur = 0;
  while (nseq < 1024)
  {
    int rows = m.plen, kind = 0, target = 0, row, cc;
    if (cur >= m.olen) { loopseq = visited[0] >= 0 ? visited[0] : 0; break; }
    if (visited[cur] >= 0) { loopseq = visited[cur]; break; }
    visited[cur] = nseq;
    for (row = 0; row < m.plen && !kind; row++)
      for (cc = 0; cc < m.nch && !kind; cc++)
      {
        const FCELL *cell;
        int e;
        if (!m.pat[cc][m.orders[cc][cur]]) continue;
        cell = &m.pat[cc][m.orders[cc][cur]][row];
        for (e = 0; e < 8; e++)
        {
          int fx = cell->fx[e], fv = cell->fv[e] < 0 ? 0 : cell->fv[e];
          if (fx == 0x0b) { kind = 1; target = fv; rows = row + 1; break; }
          if (fx == 0x0d) { kind = 3; if (fv) imp->warnings |= FSI_W_JUMP; rows = row + 1; break; }
          if (fx == 0xff) { kind = 2; rows = row + 1; break; }
        }
      }
    seq[nseq].order = cur;
    seq[nseq].rows = rows;
    nseq++;
    if (kind == 1) cur = target;
    else if (kind == 2) { loopseq = -1; break; }
    else cur++;
  }
  if (nseq >= 1024) imp->warnings |= FSI_W_ORDERS;
  if (nseq == 0) { freemodule(&m); return FSI_NOTFUR; }
  for (i = 0; i < nseq; i++) imp->rows += seq[i].rows;

  // The patterns
  memset(st, 0, sizeof st);
  for (c = 0; c < MAX_CHN; c++) { imp->songlen[c] = 0; st[c].lastnote = 48; st[c].arpspeed = m.arpspeed < 1 ? 1 : m.arpspeed; }
  {
    int entry[MAX_CHN][1024];                 // order list index of the first pattern of each sequence entry
    int ei;
    for (c = 0; c < m.nch; c++)
      for (ei = 0; ei < nseq; ei++) entry[c][ei] = -1;
    for (ei = 0; ei < nseq; ei++)
    {
      int rows = seq[ei].rows, start;
      static unsigned char rowbuf[MAX_CHN][256][4];
      int row;
      memset(rowbuf, 0, sizeof rowbuf);
      for (c = 0; c < m.nch; c++)
        for (row = 0; row < rows; row++) { rowbuf[c][row][0] = REST; }
      for (row = 0; row < rows; row++)
      {
        int tempocmd = -1, tempodata = 0, ch;
        for (ch = 0; ch < m.nch; ch++)
        {
          const FCELL *cell = m.pat[ch][m.orders[ch][seq[ei].order]] ? &m.pat[ch][m.orders[ch][seq[ei].order]][row] : NULL;
          CHST *cs = &st[ch];
          GTCMD best = { 0, 0, -1 };
          int e, newnote = 0, tonep = 0, explicitmove = 0, arpchanged = 0;
          unsigned char *out = rowbuf[ch][row];

          if (cell)
          {
            if (cell->note >= 0 && cell->note <= 182)
            {
              if (cell->note <= 179) { out[0] = gtnote(cell->note, &imp->warnings); cs->lastnote = out[0] - FIRSTNOTE; newnote = 1; }
              else out[0] = KEYOFF;
            }
            if (cell->vol >= 0) imp->warnings |= FSI_W_VOLUME;
            if (cell->ins >= 0 && cell->ins < FSI_MAXINSTR) out[1] = cell->ins + 1;
            else if (cell->ins >= FSI_MAXINSTR) imp->warnings |= FSI_W_INSTR;
            for (e = 0; e < 8; e++)
            {
              int fx = cell->fx[e], fv = cell->fv[e] < 0 ? 0 : cell->fv[e];
              GTCMD cand = { 0, 0, -1 };
              if (fx < 0) continue;
              if (cell->fv[e] < 0 && fx != 0xff && fx != 0x0d && fx != 0x0b && fx != 0x03) fv = 0;
              switch (fx)
              {
                case 0x09: s1 = fv; break;
                case 0x0f: s2 = fv; alt = 1; break;
                case 0x0b: case 0x0d: case 0xff: break;
                case 0x00: cs->arp = 0x100 | (fv & 0xff); arpchanged = 1; break;
                case 0xe0: cs->arpspeed = fv; break;
                case 0x01: case 0x02:
                if (fv) { cs->slide = fx; cs->param = fv; } else cs->slide = 0;
                explicitmove = 1;
                break;
                case 0x03:
                if (fv) { cs->slide = 3; cs->param = fv; tonep = 1; } else cs->slide = 0;
                explicitmove = 1;
                break;
                case 0x04:
                if ((fv >> 4) && (fv & 15)) { cs->slide = 4; cs->param = fv; } else cs->slide = 0;
                explicitmove = 1;
                break;
                case 0x20: cand.cmd = CMD_SETAD; cand.data = fv; cand.prio = 3; break;
                case 0x21: cand.cmd = CMD_SETSR; cand.data = fv; cand.prio = 3; break;
                case 0x10: if ((fv & 15) > 0 && (fv & 15) <= 8) { cand.cmd = CMD_SETWAVE; cand.data = ((fv & 15) << 4) | 1; cand.prio = 4; } break;
                case 0x13: cand.cmd = CMD_SETFILTERCTRL; cand.data = ((fv & 15) << 4) | 7; cand.prio = 2; break;
                default:
                if (fx >= 0x40 && fx <= 0x47) { cand.cmd = CMD_SETFILTERCUTOFF; cand.data = (((fx & 7) << 8) | fv) >> 3; cand.prio = 2; }
                else imp->warnings |= FSI_W_EFFECT;
                break;
              }
              if (cand.prio >= 0)
              {
                if (best.prio >= 0) imp->warnings |= FSI_W_ONECMD;
                if (cand.prio > best.prio) best = cand;
              }
            }
          }
          // an arpeggio goes on over notes and rows until it is stopped (0000)
          if ((arpchanged || (newnote && cs->arp)) && !tonep)
          {
            int x = (cs->arp >> 4) & 15, y = cs->arp & 15, row0 = 0;
            if (cs->arp & 0xff) row0 = arpprogram(imp, x, y, cs->arpspeed);
            else if (arpchanged && !newnote) row0 = arpprogram(imp, 0, 0, 1);
            if (row0)
            {
              if (best.prio >= 0) imp->warnings |= FSI_W_ONECMD;
              if (best.prio < 6) { best.cmd = CMD_SETWAVEPTR; best.data = row0; best.prio = 6; }
            }
          }
          // a new note ends a slide or vibrato, except tone portamento
          if (newnote && !explicitmove && !tonep) cs->slide = 0;
          if (newnote && tonep) cs->slide = 3;
          if (cs->slide)
          {
            int entryidx = 0;
            if (cs->slide == 1 || cs->slide == 2 || cs->slide == 3) entryidx = slideentry(imp, cs->param, cs->lastnote);
            else entryidx = vibratoentry(imp, cs->param >> 4, cs->param & 15, cs->lastnote);
            if (entryidx)
            {
              if (best.prio >= 0) imp->warnings |= FSI_W_ONECMD;
              else
              {
                best.cmd = cs->slide == 1 ? CMD_PORTAUP : cs->slide == 2 ? CMD_PORTADOWN : cs->slide == 3 ? CMD_TONEPORTA : CMD_VIBRATO;
                best.data = entryidx;
                best.prio = 1;
              }
            }
          }
          if (best.prio >= 0) { out[2] = best.cmd; out[3] = best.data; }
        }
        // the speeds: one command for all channels
        {
          if (s1 != lasts1 || (alt && s2 != lasts2) || alt != lastalt)
          {
            if (alt && s1 != s2)
            {
              int e = stblfind(imp, s1, s2);
              if (e) { tempocmd = CMD_FUNKTEMPO; tempodata = e; }
            }
            else
            {
              tempocmd = CMD_SETTEMPO;
              tempodata = s1;
              if (s1 < 3) { imp->warnings |= FSI_W_SPEED; tempodata = 3; }
              if (s1 > 0x7f) { imp->warnings |= FSI_W_SPEED; tempodata = 0x7f; }
            }
            lasts1 = s1; lasts2 = s2; lastalt = alt;
          }
        }
        if (tempocmd >= 0)
        {
          int placed = 0;
          for (ch = 0; ch < m.nch && !placed; ch++)
            if (!rowbuf[ch][row][2]) { rowbuf[ch][row][2] = tempocmd; rowbuf[ch][row][3] = tempodata; placed = 1; }
          if (!placed) imp->warnings |= FSI_W_ONECMD;
        }
      }
      // the rows of this entry into patterns of up to 128 rows
      for (c = 0; c < m.nch; c++)
      {
        for (start = 0; start < rows; start += MAX_PATTROWS)
        {
          int n = rows - start > MAX_PATTROWS ? MAX_PATTROWS : rows - start, p, r2;
          unsigned char buf[MAX_PATTROWS * 4 + 4];
          int lastins = 0;
          for (r2 = 0; r2 < n; r2++) memcpy(buf + r2 * 4, rowbuf[c][start + r2], 4);
          buf[n * 4] = ENDPATT; buf[n * 4 + 1] = buf[n * 4 + 2] = buf[n * 4 + 3] = 0;
          // the instrument is written on the first note of a pattern, and where it changes
          for (r2 = 0; r2 < n; r2++)
          {
            unsigned char *rw = buf + r2 * 4;
            if (rw[0] >= FIRSTNOTE && rw[0] <= LASTNOTE)
            {
              int ins = rw[1] ? rw[1] : st[c].instr;
              if (ins && ins == lastins && rw[1]) rw[1] = 0;
              else if (ins) rw[1] = ins;
              if (ins) lastins = ins;
              st[c].instr = ins;
            }
          }
          for (p = 0; p < imp->patterns; p++)
            if (imp->pattlen[p] == n && !memcmp(imp->pattern[p], buf, n * 4 + 4)) break;
          if (p == imp->patterns)
          {
            if (p >= MAX_PATT) { freemodule(&m); fsi_free(imp); return FSI_TOOBIG; }
            memcpy(imp->pattern[p], buf, n * 4 + 4);
            imp->pattlen[p] = n;
            imp->patterns++;
          }
          if (imp->songlen[c] < MAX_SONGLEN - 1)
          {
            if (start == 0) entry[c][ei] = imp->songlen[c];
            imp->order[c][imp->songlen[c]++] = p;
          }
          else imp->warnings |= FSI_W_ORDERS;
        }
      }
    }
    for (c = 0; c < m.nch; c++)
    {
      int len = imp->songlen[c];
      imp->order[c][len] = LOOPSONG;
      // a loop to the first pattern of the loop order; a song that ends loops to the end of the list, which stops it
      imp->order[c][len + 1] = (loopseq >= 0 && entry[c][loopseq] >= 0) ? entry[c][loopseq] : len;
    }
  }
  for (c = m.nch; c < MAX_CHN; c++) { imp->order[c][0] = LOOPSONG; imp->order[c][1] = 0; imp->songlen[c] = 0; }
  freemodule(&m);
  return FSI_OK;
}

int furnace_songinstrument(const FSONGIMP *imp, int slot, unsigned char *out, int outsize)
{
  int off, sz;
  if (slot < 1 || slot > FSI_MAXINSTR || !imp->module) return 0;
  off = imp->instroffset[slot]; sz = imp->instrsize[slot];
  if (!off || sz < 4 || sz + 4 > outsize) return 0;
  memcpy(out, "FINS", 4);
  memcpy(out + 4, imp->module + off, sz);
  return sz + 4;
}

int furnace_songbody(const FSONGIMP *imp, unsigned char *out, int outsize)
{
  int n = 0, c, p, t;
  int nch = imp->numsids * 3;
#define PUT(v) do { if (n >= outsize) return 0; out[n++] = (unsigned char)(v); } while (0)
  for (c = 0; c < 32; c++) PUT(c < (int)strlen(imp->name) && c < 31 ? imp->name[c] : 0);
  for (c = 0; c < 32; c++) PUT(c < (int)strlen(imp->author) && c < 31 ? imp->author[c] : 0);
  for (c = 0; c < 32; c++) PUT(0);
  PUT(1);
  for (c = 0; c < nch; c++)
  {
    int len = imp->songlen[c];
    PUT(len + 1);
    for (p = 0; p < len + 2; p++) PUT(imp->order[c][p]);
  }
  PUT(0);                                // no instruments
  for (t = 0; t < MAX_TABLES; t++)
  {
    int len = t == STBL ? imp->stbllen : t == WTBL ? imp->wtbllen : 0;
    const unsigned char *tl = t == STBL ? imp->stbl_l : imp->wtbl_l, *tr = t == STBL ? imp->stbl_r : imp->wtbl_r;
    PUT(len);
    for (p = 0; p < len; p++) PUT(tl[p]);
    for (p = 0; p < len; p++) PUT(tr[p]);
  }
  PUT(imp->patterns);
  for (p = 0; p < imp->patterns; p++)
  {
    int len = imp->pattlen[p], i;
    PUT(len + 1);
    for (i = 0; i < (len + 1) * 4; i++) PUT(imp->pattern[p][i]);
  }
#undef PUT
  return n;
}

void fsi_free(FSONGIMP *imp)
{
  free(imp->module);
  imp->module = NULL;
  imp->modulesize = 0;
}

void furnace_songimportwarningtext(char *buf, int size, int warnings)
{
  static const struct { int bit; const char *text; } list[] =
  {
    { FSI_W_VOLUME, "the volume column" },
    { FSI_W_EFFECT, "effects without a counterpart (pulse and cutoff slides, filter mode, note cut and delay ...)" },
    { FSI_W_ONECMD, "rows with more than one command (the most important was kept)" },
    { FSI_W_NOTERANGE, "notes outside C-0 to the top of the range (moved by octaves)" },
    { FSI_W_INSTR, "instruments above 63" },
    { FSI_W_TIMING, "a tick rate that is not 50 or 60 Hz times a whole number (approximated)" },
    { FSI_W_SPEED, "speeds below 3 or above 127 (changed)" },
    { FSI_W_JUMP, "jumps that are not the loop of the song, or a pattern break with a start row" },
    { FSI_W_SUBSONGS, "more than one subsong (only the first)" },
    { FSI_W_ORDERS, "an order list that is too long (cut)" },
    { FSI_W_SPEEDTABLE, "a full speedtable (some slides and vibratos dropped)" },
    { FSI_W_TUNING, "a tuning other than A-4 = 440 Hz" },
    { FSI_W_WAVETABLE, "a full wavetable (some arpeggios dropped)" },
  };
  int i, n = 0;
  if (size < 1) return;
  buf[0] = 0;
  for (i = 0; i < (int)(sizeof(list) / sizeof(list[0])); i++)
    if ((warnings & list[i].bit) && n < size - 1)
      n += snprintf(buf + n, size - n, "%s%s", n ? ", " : "", list[i].text);
}
