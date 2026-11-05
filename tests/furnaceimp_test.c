// Tests of the Furnace instrument import: hand made files (delay, speed, loops, relative and absolute values,
// damaged and foreign files), and a round trip of every instrument of the library (FURNACE_LIB=<folder>, optional):
// export it, import the file, rebuild the tables with the step editor's row generator, export again. The two files
// must be the same bytes. Run with sanitizers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gfurnace.h"
#include "../src/gfurnaceimp.h"
#include "../src/gsummary.h"
#include "../src/gwsummary.h"
#include "../src/gfsummary.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 30) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

typedef struct { unsigned char b[2048]; int n; } FILEBUF;

static void p8(FILEBUF *f, int v) { f->b[f->n++] = (unsigned char)v; }
static void p16(FILEBUF *f, int v) { p8(f, v & 255); p8(f, v >> 8); }

static void header(FILEBUF *f, int type)
{
  f->n = 0;
  memcpy(f->b, "FINS", 4); f->n = 4;
  p16(f, 199); p16(f, type);
}

// the 64 feature: flags1, flags2, ad, sr, duty, cutoff word
static void c64(FILEBUF *f, int fl1, int fl2, int ad, int sr, int duty, int cutword)
{
  p8(f, '6'); p8(f, '4'); p16(f, 9);
  p8(f, fl1); p8(f, fl2); p8(f, ad); p8(f, sr); p16(f, duty); p16(f, cutword); p8(f, 0);
}

// one macro, 8 bit unsigned values
static void macro(FILEBUF *f, int code, int loop, int delay, int speed, const int *v, int n, int wordsize)
{
  int i;
  p8(f, code); p8(f, n); p8(f, loop); p8(f, 255); p8(f, 0); p8(f, wordsize); p8(f, delay); p8(f, speed);
  for (i = 0; i < n; i++)
  {
    if (wordsize == 192) { p16(f, v[i] & 0xffff); p16(f, (int)(((unsigned)v[i] >> 16) & 0xffff)); }
    else if (wordsize == 128) p16(f, v[i] & 0xffff);
    else p8(f, v[i] & 255);
  }
}

static void ma(FILEBUF *f, int *lenpos)
{
  p8(f, 'M'); p8(f, 'A');
  *lenpos = f->n;
  p16(f, 0);
  p16(f, 8);
}

static void maend(FILEBUF *f, int lenpos)
{
  p8(f, 255);
  f->b[lenpos] = (unsigned char)((f->n - lenpos - 2) & 255);
  f->b[lenpos + 1] = (unsigned char)((f->n - lenpos - 2) >> 8);
}

static FURNACEIMPORT imp;

static void handmade(void)
{
  FILEBUF f;
  int lp, r;
  int a[8];

  // not Furnace, not C64, damaged
  CHECK(furnace_import((const unsigned char *)"hello world", 11, &imp) == FIMP_NOTFUI);
  header(&f, 4); CHECK(furnace_import(f.b, f.n, &imp) == FIMP_NOTC64);
  header(&f, 3); p8(&f, '6'); p8(&f, '4'); p16(&f, 9); p8(&f, 1);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_BAD);
  header(&f, 3); CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK);   // an empty file is a plain instrument
  CHECK(!imp.haswave && !imp.haspulse && !imp.hasfilter);

  // plain pulse instrument: ADSR, no programs
  header(&f, 3);
  p8(&f, 'N'); p8(&f, 'A'); p16(&f, 5); memcpy(f.b + f.n, "Lead", 5); f.n += 5;
  c64(&f, 0x80 | 4, 0, 0x2a, 0xf4, 0x800, 0);
  r = furnace_import(f.b, f.n, &imp);
  CHECK(r == FIMP_OK && !strcmp(imp.name, "Lead") && imp.ad == 0x2a && imp.sr == 0xf4);
  CHECK(imp.firstwave == 0x41 && !imp.haswave && !imp.haspulse && !imp.hasfilter && imp.warnings == 0);

  // ring and sync from the settings
  header(&f, 3); c64(&f, 0x80 | 2, 0xc0, 0x00, 0xf0, 0x800, 0);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.firstwave == (0x20 | 1 | 4 | 2));

  // wave macro: noise for 2 ticks, then pulse, looping nothing: tick 0 is the first waveform, tick 1 step 0
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0x00, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 8; a[1] = 8; a[2] = 4;
  macro(&f, 3, 255, 0, 1, a, 3, 0);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK);
  CHECK(imp.firstwave == 0x81);
  CHECK(imp.haswave && imp.wave.n == 2 && imp.wave.loop == 0);
  CHECK(imp.wave.wave[0] == 0x81 && imp.wave.wave[1] == 0x41 && imp.wave.note[0] == 0);

  // arpeggio 0 +4 +7 looping from the start, one tick each, plus an absolute note: C-4 = 48
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0x00, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 0; a[1] = 4; a[2] = 7;
  macro(&f, 1, 0, 0, 1, a, 3, 64);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.haswave);
  CHECK(imp.wave.n == 3 && imp.wave.loop == 1);      // tick 0 is 0, steps are ticks 1.. : 4 7 0 loops
  CHECK(imp.wave.note[0] == 4 && imp.wave.note[1] == 7 && imp.wave.note[2] == 0);
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0x00, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 0; a[1] = (0x40000000 | 48);
  macro(&f, 1, 255, 0, 1, a, 2, 192);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.haswave);
  CHECK(imp.wave.n == 1 && imp.wave.note[0] == (0x80 | 48) && imp.wave.loop == 0);

  // delay and speed: a macro 1 2 3 with delay 2 and speed 2 is none none 1 1 2 2 3 3 3 ...
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0x00, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 0x100; a[1] = 0x200; a[2] = 0x300;
  macro(&f, 2, 255, 2, 2, a, 3, 128);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.haspulse);
  // duty[t]: 0x800,0x800,0x100,0x100,0x200,0x200,0x300...; step k = duty[k+1]
  CHECK(imp.pulse.n == 6 && imp.pulse.width[0] == 0x800 && imp.pulse.width[1] == 0x100 && imp.pulse.width[2] == 0x100);
  CHECK(imp.pulse.width[3] == 0x200 && imp.pulse.width[4] == 0x200 && imp.pulse.width[5] == 0x300 && imp.pulse.loop == 0);

  // relative duty (not "absolute"): the value is taken from the duty of the tick before
  header(&f, 3); c64(&f, 4, 0, 0x00, 0xf0, 0x400, 0); ma(&f, &lp);
  a[0] = 0; a[1] = 0x10;
  macro(&f, 2, 1, 0, 1, a, 2, 0);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.haspulse);
  CHECK(imp.pulse.width[0] == 0x400 - 0x10);
  CHECK(imp.pulse.width[1] == 0x400 - 0x20);

  // filter: low pass, resonance 8, cutoff 0x400 (8 bit 0x80), routed, a cutoff macro with a loop
  header(&f, 3); c64(&f, 0x80 | 4 | 0x40 | 0x10, 0x10 | 1, 0x00, 0xf0, 0x800, 0x400 | (8 << 12)); ma(&f, &lp);
  a[0] = 0x400; a[1] = 0x600;
  macro(&f, 8, 0, 0, 1, a, 2, 128);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.hasfilter);
  CHECK(imp.filter.ftype == 0x10 && imp.filter.fres == 8 && imp.filter.fvoices == 7 && (imp.warnings & FIMP_W_VOICES));
  CHECK(imp.filter.n == 2 && imp.filter.loop == 1 && imp.filter.cutoff[0] == 0x80 && imp.filter.cutoff[1] == 0xc0);

  // a release point and an ADSR macro are noticed
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 4;
  p8(&f, 3); p8(&f, 1); p8(&f, 255); p8(&f, 0); p8(&f, 0); p8(&f, 0); p8(&f, 0); p8(&f, 1); p8(&f, 4);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && (imp.warnings & FIMP_W_RELEASE));
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0, 0xf0, 0x800, 0); ma(&f, &lp);
  p8(&f, 3); p8(&f, 1); p8(&f, 255); p8(&f, 255); p8(&f, 1); p8(&f, 0); p8(&f, 0); p8(&f, 1); p8(&f, 4);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && (imp.warnings & FIMP_W_MODE));
  // a macro of another kind (volume, code 0)
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 15; macro(&f, 0, 255, 0, 1, a, 1, 0);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && (imp.warnings & FIMP_W_OTHER));

  // a pitch step outside the range is clamped and noticed
  header(&f, 3); c64(&f, 0x80 | 4, 0, 0, 0xf0, 0x800, 0); ma(&f, &lp);
  a[0] = 0; a[1] = 100; macro(&f, 1, 255, 0, 1, a, 2, 64);
  maend(&f, lp);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && (imp.warnings & FIMP_W_NOTERANGE) && imp.wave.note[0] == 95);

  // unknown features are skipped
  header(&f, 3); p8(&f, 'X'); p8(&f, 'Y'); p16(&f, 3); p8(&f, 1); p8(&f, 2); p8(&f, 3);
  c64(&f, 0x80 | 4, 0, 0x12, 0x34, 0x800, 0);
  p8(&f, 'E'); p8(&f, 'N'); p16(&f, 0);
  CHECK(furnace_import(f.b, f.n, &imp) == FIMP_OK && imp.ad == 0x12 && imp.sr == 0x34);
}

// ---- the round trip ---------------------------------------------------------------------------------------------

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

// where the pulse width of the 64 feature is in a file image (-1 if there is none)
static int duty_offset(const unsigned char *f, int n)
{
  int pos = 8;
  while (pos + 4 <= n)
  {
    int len = f[pos + 2] | (f[pos + 3] << 8);
    if (f[pos] == '6' && f[pos + 1] == '4') return pos + 4 + 4;
    pos += 4 + len;
  }
  return -1;
}

static void library(const char *dir)
{
  static unsigned char L[4][255], R[4][255], L2[4][255], R2[4][255], f1[FURNACE_MAXFILE], f2[FURNACE_MAXFILE];
  unsigned char *l[4], *r[4];
  char cmd[1024], path[1024];
  FILE *p;
  int count = 0, same = 0, skipped = 0, c, shown = 0;

  for (c = 0; c < 4; c++) { l[c] = L[c]; r[c] = R[c]; }
  snprintf(cmd, sizeof cmd, "find '%s' -name '*.ins'", dir);
  p = popen(cmd, "r");
  if (!p) return;
  while (fgets(path, sizeof path, p))
  {
    INSTR in, in2;
    FURNACEINFO info, info2;
    int n1, n2, ok;
    unsigned char gl[300], gr[300];
    path[strcspn(path, "\n")] = 0;
    if (!loadins(path, &in, l, r)) continue;
    count++;
    n1 = furnace_instrument(f1, sizeof f1, &in, l[WTBL], r[WTBL], l[PTBL], r[PTBL], l[FTBL], r[FTBL], &info);
    CHECK(n1 > 0);
    if (!n1) continue;
    if (info.warnings & FURNACE_W_TRUNCATED) { skipped++; continue; }
    ok = furnace_import(f1, n1, &imp);
    CHECK(ok == FIMP_OK);
    if (ok != FIMP_OK) continue;

    memset(&in2, 0, sizeof in2);
    memset(L2, 0, sizeof L2); memset(R2, 0, sizeof R2);
    in2.ad = imp.ad; in2.sr = imp.sr; in2.firstwave = imp.firstwave; in2.gatetimer = 2;
    memcpy(in2.name, imp.name, sizeof imp.name > sizeof in2.name ? sizeof in2.name : sizeof imp.name);
    in2.name[MAX_INSTRNAMELEN - 1] = 0;
    if (imp.haswave)
    {
      int rows = step_generate(&imp.wave, gl, gr, 1);
      memcpy(L2[WTBL], gl, rows); memcpy(R2[WTBL], gr, rows);
      in2.ptr[WTBL] = 1;
    }
    if (imp.haspulse)
    {
      int rows = step_generate(&imp.pulse, gl, gr, 1);
      memcpy(L2[PTBL], gl, rows); memcpy(R2[PTBL], gr, rows);
      in2.ptr[PTBL] = 1;
    }
    if (imp.hasfilter)
    {
      int rows = furnace_filterrows(&imp, gl, gr, 1);
      memcpy(L2[FTBL], gl, rows); memcpy(R2[FTBL], gr, rows);
      in2.ptr[FTBL] = 1;
    }
    n2 = furnace_instrument(f2, sizeof f2, &in2, L2[WTBL], R2[WTBL], L2[PTBL], R2[PTBL], L2[FTBL], R2[FTBL], &info2);
    if (!imp.haspulse)
    {
      // no pulse program: the stored pulse width is dropped on purpose (the instrument does not play a pulse)
      int d1 = duty_offset(f1, n1), d2 = duty_offset(f2, n2);
      if (d1 >= 0) f1[d1] = f1[d1 + 1] = 0;
      if (d2 >= 0) f2[d2] = f2[d2 + 1] = 0;
    }
    if (n1 == n2 && !memcmp(f1, f2, n1)) same++;
    else
    {
      CHECK(0);
      if (shown++ < 5)
      {
        int k;
        for (k = 0; k < n1 && k < n2 && f1[k] == f2[k]; k++)
          ;
        { int q; printf("  f1:"); for (q = 0; q < 80 && q < n1; q++) printf(" %02x", f1[q]); printf("\n  f2:"); for (q = 0; q < 80 && q < n2; q++) printf(" %02x", f2[q]); printf("\n"); }
        printf("  differs: %s (warnings %02x, import %02x, sizes %d %d) first difference at byte %d: %02x vs %02x\n", path, info.warnings, imp.warnings, n1, n2, k, f1[k], f2[k]);
      }
    }
  }
  pclose(p);
  printf("library: %d instruments, %d identical after the round trip, %d skipped (macro cut)\n", count, same, skipped);
}

int main(void)
{
  const char *lib = getenv("FURNACE_LIB");
  handmade();
  if (lib && *lib) library(lib);
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
