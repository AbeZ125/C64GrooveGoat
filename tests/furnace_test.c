// Tests of the Furnace instrument export: a reader of the .fui format written from the spec (not from the writer),
// hand made instruments with known tables, and every instrument of the library (FURNACE_LIB=<folder>, optional) for
// the structure of the file. Run with sanitizers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gfurnace.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

typedef struct
{
  int ok;
  int version, type;
  char name[64];
  int have64, haveMA;
  int flags1, flags2, ad, sr, duty, cutoff, res;
  int nmacros;
  struct { int code, len, loop, rel, mode, wordsize, delay, speed; int val[256]; } m[24];
} FUI;

static unsigned rd(const unsigned char *b, int n, int *pos, int bytes, int *bad)
{
  unsigned v = 0;
  int i;
  if (*pos + bytes > n) { *bad = 1; return 0; }
  for (i = 0; i < bytes; i++) v |= (unsigned)b[*pos + i] << (8 * i);
  *pos += bytes;
  return v;
}

// The format as written in papers/newIns.md and instrument.cpp of Furnace
static void parse(FUI *f, const unsigned char *b, int n)
{
  int pos = 0, bad = 0;
  memset(f, 0, sizeof *f);
  if (n < 8 || memcmp(b, "FINS", 4)) return;
  pos = 4;
  f->version = rd(b, n, &pos, 2, &bad);
  f->type = rd(b, n, &pos, 1, &bad);
  rd(b, n, &pos, 1, &bad);
  while (pos < n && !bad)
  {
    char code[3] = {0, 0, 0};
    int len, end;
    code[0] = b[pos]; code[1] = b[pos + 1]; pos += 2;
    if (!strcmp(code, "EN")) break;
    len = rd(b, n, &pos, 2, &bad);
    end = pos + len;
    if (end > n) { bad = 1; break; }
    if (!strcmp(code, "NA"))
    {
      int i = 0;
      while (pos < end && b[pos] && i < 63) f->name[i++] = b[pos++];
      if (pos >= end || b[pos]) bad = 1;       // zero terminated, inside the block
      else pos++;
    }
    else if (!strcmp(code, "64"))
    {
      f->have64 = 1;
      if (len != 9) bad = 1;
      f->flags1 = rd(b, n, &pos, 1, &bad);
      f->flags2 = rd(b, n, &pos, 1, &bad);
      f->ad = rd(b, n, &pos, 1, &bad);
      f->sr = rd(b, n, &pos, 1, &bad);
      f->duty = rd(b, n, &pos, 2, &bad);
      { int w = rd(b, n, &pos, 2, &bad); f->cutoff = w & 0x7ff; f->res = (w >> 12) & 15; }
      rd(b, n, &pos, 1, &bad);
    }
    else if (!strcmp(code, "MA"))
    {
      int hl = rd(b, n, &pos, 2, &bad);
      f->haveMA = 1;
      if (hl != 8) bad = 1;
      for (;;)
      {
        int code2 = rd(b, n, &pos, 1, &bad), i, k;
        if (bad || code2 == 255) break;
        if (f->nmacros >= 24) { bad = 1; break; }
        k = f->nmacros++;
        f->m[k].code = code2;
        f->m[k].len = rd(b, n, &pos, 1, &bad);
        f->m[k].loop = rd(b, n, &pos, 1, &bad);
        f->m[k].rel = rd(b, n, &pos, 1, &bad);
        f->m[k].mode = rd(b, n, &pos, 1, &bad);
        f->m[k].wordsize = rd(b, n, &pos, 1, &bad) >> 6;
        f->m[k].delay = rd(b, n, &pos, 1, &bad);
        f->m[k].speed = rd(b, n, &pos, 1, &bad);
        for (i = 0; i < f->m[k].len && !bad; i++)
        {
          switch (f->m[k].wordsize)
          {
            case 0: f->m[k].val[i] = rd(b, n, &pos, 1, &bad); break;
            case 1: f->m[k].val[i] = (signed char)rd(b, n, &pos, 1, &bad); break;
            case 2: f->m[k].val[i] = (short)rd(b, n, &pos, 2, &bad); break;
            default: f->m[k].val[i] = (int)rd(b, n, &pos, 4, &bad); break;
          }
        }
      }
    }
    if (!bad && pos != end) bad = 1;           // every block is used up exactly
    pos = end;
  }
  f->ok = !bad;
}

// value of macro "code" at tick t the way Furnace plays it (loop, or the last value held)
static int macro_at(const FUI *f, int code, int t, int def)
{
  int k;
  for (k = 0; k < f->nmacros; k++)
    if (f->m[k].code == code)
    {
      int len = f->m[k].len, loop = f->m[k].loop;
      if (t < len) return f->m[k].val[t];
      if (loop >= 0 && loop < len) return f->m[k].val[loop + (t - loop) % (len - loop)];
      return f->m[k].val[len - 1];
    }
  return def;
}

static void tables(unsigned char *wl, unsigned char *wr, unsigned char *pl, unsigned char *pr,
                   unsigned char *fl, unsigned char *fr)
{
  memset(wl, 0, 255); memset(wr, 0, 255); memset(pl, 0, 255); memset(pr, 0, 255); memset(fl, 0, 255); memset(fr, 0, 255);
}

static int build(FUI *f, const INSTR *in, const unsigned char *wl, const unsigned char *wr, const unsigned char *pl,
                 const unsigned char *pr, const unsigned char *fl, const unsigned char *fr, FURNACEINFO *info)
{
  static unsigned char buf[FURNACE_MAXFILE];
  int n = furnace_instrument(buf, sizeof buf, in, wl, wr, pl, pr, fl, fr, info);
  if (n) parse(f, buf, n);
  else memset(f, 0, sizeof *f);
  return n;
}

static void handmade(void)
{
  static FUI f;
  FURNACEINFO info;
  INSTR in;
  unsigned char wl[255], wr[255], pl[255], pr[255], fl[255], fr[255];
  int t;

  // A plain pulse with an ADSR and no tables at all
  memset(&in, 0, sizeof in);
  tables(wl, wr, pl, pr, fl, fr);
  in.ad = 0x2a; in.sr = 0xb7; in.firstwave = 0x41;
  strcpy(in.name, "Plain");
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok && f.type == 3 && f.version == FURNACE_FILEVERSION);
  CHECK(!strcmp(f.name, "Plain"));
  CHECK(f.have64 && f.ad == 0x2a && f.sr == 0xb7);
  CHECK((f.flags1 & 15) == 4);              // pulse only
  CHECK(f.flags1 & 0x80);                   // absolute duty
  CHECK(!(f.flags1 & 0x40));                // no filter
  CHECK(f.duty == 0x800);
  CHECK(info.warnings == FURNACE_W_NOWAVETABLE);

  // A wavetable: pulse, then triangle, ends (FF 00). The first tick is the first waveform, then the table runs.
  tables(wl, wr, pl, pr, fl, fr);
  in.ptr[WTBL] = 1;
  wl[0] = 0x41; wr[0] = 0x00; wl[1] = 0x11; wr[1] = 0x00; wl[2] = 0xff; wr[2] = 0x00;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok);
  CHECK(macro_at(&f, 3, 0, -1) == 4);       // tick 0: first waveform 41 -> pulse
  CHECK(macro_at(&f, 3, 1, -1) == 4);       // tick 1: row 1 (pulse)
  CHECK(macro_at(&f, 3, 2, -1) == 1);       // tick 2: row 2 (triangle)
  for (t = 3; t < 400; t++) CHECK(macro_at(&f, 3, t, -1) == 1);   // and it stays
  CHECK(info.warnings == 0);

  // A looping arpeggio 0 +4 +7 +12: relative steps
  tables(wl, wr, pl, pr, fl, fr);
  in.firstwave = 0x41;
  wl[0] = 0x41; wr[0] = 0x00; wl[1] = 0x00; wr[1] = 0x04; wl[2] = 0x00; wr[2] = 0x07; wl[3] = 0x00; wr[3] = 0x0c;
  wl[4] = 0xff; wr[4] = 0x02;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok);
  {
    CHECK(macro_at(&f, 1, 0, -99) == 0);
    CHECK(macro_at(&f, 1, 1, -99) == 0);
    CHECK(macro_at(&f, 1, 2, -99) == 4);
    CHECK(macro_at(&f, 1, 3, -99) == 7);
    CHECK(macro_at(&f, 1, 4, -99) == 12);
    CHECK(macro_at(&f, 1, 5, -99) == 4);
    CHECK(macro_at(&f, 1, 6, -99) == 7);
    CHECK(macro_at(&f, 1, 7, -99) == 12);
    CHECK(macro_at(&f, 1, 101, -99) == 4);    // (101-2) % 3 == 0: the loop starts again
  }

  // Absolute notes (a snare): the flag 0x40000000 and the note number
  tables(wl, wr, pl, pr, fl, fr);
  wl[0] = 0x81; wr[0] = 0xd0; wl[1] = 0xff; wr[1] = 0x00;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok);
  CHECK(macro_at(&f, 1, 1, 0) == (0x40000000 | 0x50));
  CHECK(macro_at(&f, 3, 1, 0) == 8);        // noise

  // Pulse: set 0x400 then widen by 0x10 for 8 ticks, then it stays
  tables(wl, wr, pl, pr, fl, fr);
  in.ptr[WTBL] = 0;
  in.firstwave = 0x41;
  in.ptr[PTBL] = 1;
  pl[0] = 0x84; pr[0] = 0x00; pl[1] = 0x08; pr[1] = 0x10; pl[2] = 0xff; pr[2] = 0x00;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok);
  CHECK(f.duty == 0x400);
  CHECK(macro_at(&f, 2, 1, -1) == 0x400);
  CHECK(macro_at(&f, 2, 2, -1) == 0x410);
  CHECK(macro_at(&f, 2, 9, -1) == 0x480);
  CHECK(macro_at(&f, 2, 10, -1) == 0x480);
  CHECK(macro_at(&f, 2, 300, -1) == 0x480);

  // Filter: low-pass, resonance 5, voice 1, cutoff set to 0x20 then raised by 4 for 10 ticks
  tables(wl, wr, pl, pr, fl, fr);
  in.ptr[PTBL] = 0;
  in.ptr[FTBL] = 1;
  fl[0] = 0x90; fr[0] = 0x51; fl[1] = 0x00; fr[1] = 0x20; fl[2] = 0x0a; fr[2] = 0x04; fl[3] = 0xff; fr[3] = 0x00;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok);
  CHECK(info.filter);
  CHECK(f.flags1 & 0x40);                   // initial filter
  CHECK(f.flags1 & 0x10);                   // to filter
  CHECK(f.flags2 & 0x10);                   // absolute cutoff
  CHECK(f.flags2 & 1);                      // low-pass
  CHECK(!(f.flags2 & 6));
  CHECK(f.res == 5);
  CHECK(f.cutoff == (0x20 << 3));
  CHECK(macro_at(&f, 8, 0, -1) == (0x20 << 3));
  CHECK(macro_at(&f, 8, 1, -1) == ((0x20 + 4) << 3));
  CHECK(macro_at(&f, 8, 10, -1) == ((0x20 + 40) << 3));
  CHECK(macro_at(&f, 8, 11, -1) == ((0x20 + 40) << 3));
  CHECK(macro_at(&f, 8, 500, -1) == ((0x20 + 40) << 3));
  CHECK(info.warnings & FURNACE_W_NOWAVETABLE);
  CHECK(info.warnings & FURNACE_W_FILTERVOICES);   // the table filters voice 1 only

  // Ring and sync on all ticks are flags; on some ticks only they are a warning
  tables(wl, wr, pl, pr, fl, fr);
  in.ptr[FTBL] = 0;
  in.ptr[WTBL] = 1;
  in.firstwave = 0x15;
  wl[0] = 0x15; wr[0] = 0; wl[1] = 0xff; wr[1] = 0;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok && (f.flags2 & 0x40) && !(f.flags2 & 0x80) && !(info.warnings & FURNACE_W_RINGSYNC));
  wl[0] = 0x15; wl[1] = 0x11; wr[1] = 0; wl[2] = 0xff; wr[2] = 0;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok && (info.warnings & FURNACE_W_RINGSYNC));

  // Vibrato is a warning
  in.ptr[STBL] = 1; in.vibdelay = 4;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(info.warnings & FURNACE_W_VIBRATO);

  // A pulse that sweeps for ever without repeating inside 255 ticks is cut, with a warning
  tables(wl, wr, pl, pr, fl, fr);
  memset(&in, 0, sizeof in);
  in.firstwave = 0x41; in.ptr[PTBL] = 1;
  pl[0] = 0x80; pr[0] = 0x00; pl[1] = 0x7f; pr[1] = 0x01; pl[2] = 0xff; pr[2] = 0x02;
  CHECK(build(&f, &in, wl, wr, pl, pr, fl, fr, &info) > 0);
  CHECK(f.ok);
  CHECK(info.dutylen > 0 && info.dutylen <= 255);

  // Buffer too small: nothing is written outside it and 0 comes back
  {
    unsigned char small[20];
    memset(&in, 0, sizeof in); in.firstwave = 0x41;
    CHECK(furnace_instrument(small, sizeof small, &in, wl, wr, pl, pr, fl, fr, &info) == 0);
  }

  // Names for files
  {
    char b[64];
    memset(&in, 0, sizeof in);
    strcpy(in.name, "Pad Flt 6-1149");
    furnace_filename(b, sizeof b, &in, 5); CHECK(!strcmp(b, "Pad Flt 6-1149.fui"));
    strcpy(in.name, "a/b:c*?");
    furnace_filename(b, sizeof b, &in, 5); CHECK(!strcmp(b, "abc.fui"));
    strcpy(in.name, "///");
    furnace_filename(b, sizeof b, &in, 0x1a); CHECK(!strcmp(b, "instrument 1A.fui"));
    strcpy(in.name, "  x ");
    furnace_filename(b, sizeof b, &in, 1); CHECK(!strcmp(b, "x.fui"));
  }
}

// One instrument file of the library (GTI5): the instrument and the table parts that belong to it
static int loadins(const char *path, INSTR *in, unsigned char *l[4], unsigned char *r[4])
{
  unsigned char h[4 + 9 + MAX_INSTRNAMELEN];
  FILE *fp = fopen(path, "rb");
  int c;
  if (!fp) return 0;
  if (fread(h, sizeof h, 1, fp) != 1 || memcmp(h, "GTI5", 4)) { fclose(fp); return 0; }
  memset(in, 0, sizeof *in);
  in->ad = h[4]; in->sr = h[5];
  in->ptr[WTBL] = h[6]; in->ptr[PTBL] = h[7]; in->ptr[FTBL] = h[8]; in->ptr[STBL] = h[9];
  in->vibdelay = h[10]; in->gatetimer = h[11]; in->firstwave = h[12];
  memcpy(in->name, h + 13, MAX_INSTRNAMELEN);
  in->name[MAX_INSTRNAMELEN - 1] = 0;
  for (c = 0; c < MAX_TABLES; c++)
  {
    memset(l[c], 0, 255); memset(r[c], 0, 255);
    if (in->ptr[c])
    {
      int len = fgetc(fp), pos = in->ptr[c] - 1;
      if (len < 0 || pos + len > 255) { fclose(fp); return 0; }
      if (fread(l[c] + pos, len, 1, fp) != 1 || fread(r[c] + pos, len, 1, fp) != 1) { fclose(fp); return 0; }
    }
  }
  fclose(fp);
  return 1;
}

static void library(const char *dir)
{
  static FUI f;
  static unsigned char L[4][255], R[4][255];
  unsigned char *l[4], *r[4];
  char cmd[1024], path[1024];
  FILE *p;
  int count = 0, wavem = 0, filt = 0, c, warned = 0, trunc = 0;

  for (c = 0; c < 4; c++) { l[c] = L[c]; r[c] = R[c]; }
  snprintf(cmd, sizeof cmd, "find '%s' -name '*.ins'", dir);
  p = popen(cmd, "r");
  if (!p) return;
  while (fgets(path, sizeof path, p))
  {
    INSTR in;
    FURNACEINFO info;
    int n, k, t;
    path[strcspn(path, "\n")] = 0;
    if (!loadins(path, &in, l, r)) continue;
    count++;
    n = build(&f, &in, l[WTBL], r[WTBL], l[PTBL], r[PTBL], l[FTBL], r[FTBL], &info);
    CHECK(n > 0);
    CHECK(f.ok);
    CHECK(f.type == 3 && f.have64);
    CHECK(f.ad == in.ad && f.sr == in.sr);
    CHECK(f.duty >= 0 && f.duty <= 4095);
    CHECK(f.nmacros == info.macros);
    CHECK((f.flags1 & 15) != 0 || !in.ptr[WTBL] || info.wavelen > 0 || in.firstwave == 0 || 1);
    if (info.warnings) warned++;
    if (info.warnings & FURNACE_W_TRUNCATED) trunc++;
    if (info.wavelen) wavem++;
    if (info.filter) filt++;
    for (k = 0; k < f.nmacros; k++)
    {
      int code = f.m[k].code;
      CHECK(f.m[k].len >= 1 && f.m[k].len <= 255);
      CHECK(f.m[k].loop == 255 || f.m[k].loop < f.m[k].len);
      CHECK(f.m[k].rel == 255 && f.m[k].mode == 0 && f.m[k].speed == 1 && f.m[k].delay == 0);
      for (t = 0; t < f.m[k].len; t++)
      {
        int v = f.m[k].val[t];
        if (code == 3) CHECK(v >= 0 && v <= 15);
        if (code == 2) CHECK(v >= 0 && v <= 4095);
        if (code == 8) CHECK(v >= 0 && v <= 2047 && (v & 7) == 0);
        if (code == 5) CHECK(v >= 0 && v <= 7);
        if (code == 6) CHECK(v >= 0 && v <= 15);
        if (code == 7) CHECK(v == 0 || v == 1);
        if (code == 1) CHECK((v >= 0 && (v & 0x40000000)) ? ((v & ~0x40000000) < 128) : (v >= -128 && v < 128));
      }
    }
    // the macros come in the order of their codes
    for (k = 1; k < f.nmacros; k++) CHECK(f.m[k].code > f.m[k - 1].code);
  }
  pclose(p);
  printf("library: %d instruments (%d with a wave macro, %d with a filter, %d with notes, %d cut)\n",
         count, wavem, filt, warned, trunc);
}

int main(void)
{
  const char *lib = getenv("FURNACE_LIB");
  handmade();
  if (lib && *lib) library(lib);
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
