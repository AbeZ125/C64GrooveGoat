// Tests of the whole-song Furnace export (.fur): synthetic songs with known notes, repeats, transposes, tempo
// commands, loops that need lining up, and the structure of the file. A small reader of the .fur layout (the one
// Furnace 0.6 writes, see gfurnacesong.c) reads the file back. Run with sanitizers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gfurnacesong.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 30) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned char pat[MAX_PATT][MAX_PATTROWS*4+4];
static int pattlen[MAX_PATT];
static unsigned char ord[MAX_CHN][MAX_SONGLEN+2];
static int songlen[MAX_CHN];
static INSTR instr[MAX_INSTR];
static unsigned char ltab[MAX_TABLES * MAX_TABLELEN], rtab[MAX_TABLES * MAX_TABLELEN];
static unsigned char file[FSONG_MAXFILE];

static int u16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned u32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

typedef struct { int note, ins, fx[3], fv[3]; } RC;

// Reads pattern number idx of channel ch: rows[0..plen)
static int readpattern(const unsigned char *d, int size, int npat, const unsigned *patptr, int ch, int idx, RC *rows, int plen)
{
  int k, r;
  for (k = 0; k < npat; k++)
  {
    const unsigned char *p = d + patptr[k];
    if (memcmp(p, "PATN", 4) || p[9] != ch || u16(p + 10) != idx) continue;
    {
      const unsigned char *q = p + 12, *end = p + 8 + u32(p + 4);
      for (r = 0; r < plen; r++) { rows[r].note = rows[r].ins = -1; rows[r].fx[0] = rows[r].fx[1] = rows[r].fx[2] = -1; }
      q++;                               // name
      r = 0;
      while (q < end)
      {
        int m = *q++;
        if (m == 0xff) break;
        if (m & 0x80) { r += (m & 0x7f) + 2; continue; }
        if (!m) { r++; continue; }
        {
          int bits = 0, e;
          if (m & 32) bits = *q++;
          else { if (m & 8) bits |= 1; if (m & 16) bits |= 2; }
          if (r >= plen) return 0;
          if (m & 1) rows[r].note = *q++;
          if (m & 2) rows[r].ins = *q++;
          if (m & 4) q++;
          for (e = 0; e < 4; e++)
          {
            if (bits & (1 << (2 * e))) rows[r].fx[e] = *q++;
            if (bits & (2 << (2 * e))) rows[r].fv[e] = *q++;
          }
        }
        r++;
      }
      return q == end || (q == end + 0);
    }
  }
  (void)size;
  return 0;
}

typedef struct
{
  int plen, olen, ni, npat, nch, speed;
  float hz;
  unsigned patptr[4096];
  int orders[MAX_CHN][256];
} FUR;

static int parse(const unsigned char *d, int size, FUR *f)
{
  const unsigned char *p;
  int i, c, q;
  if (size < 64 || memcmp(d, "-Furnace module-", 16)) return 0;
  p = d + u32(d + 20);
  if (memcmp(p, "INFO", 4)) return 0;
  f->speed = p[9];
  memcpy(&f->hz, p + 12, 4);
  f->plen = u16(p + 16); f->olen = u16(p + 18);
  f->ni = u16(p + 22); f->npat = u32(p + 28);
  f->nch = 0;
  for (i = 0; i < 32; i++) if (p[32 + i]) f->nch += 3;
  q = 32 + 32 * 3 + 128;                 // the song name starts here (relative to the block start)
  while (p[q]) q++; q++;                 // song name
  while (p[q]) q++; q++;                 // author
  q += 4 + 20;
  for (i = 0; i < f->ni; i++) q += 4;
  for (i = 0; i < f->npat; i++) f->patptr[i] = u32(p + q + 4 * i);
  q += 4 * f->npat;
  for (c = 0; c < f->nch; c++)
    for (i = 0; i < f->olen; i++) f->orders[c][i] = p[q + c * f->olen + i];
  return 1;
}

static void note(int p, int row, int n, int ins, int cmd, int data)
{
  pat[p][row * 4] = n; pat[p][row * 4 + 1] = ins; pat[p][row * 4 + 2] = cmd; pat[p][row * 4 + 3] = data;
}

static void newpattern(int p, int len)
{
  int r;
  pattlen[p] = len;
  memset(pat[p], 0, sizeof pat[p]);
  for (r = 0; r < len; r++) pat[p][r * 4] = REST;
  pat[p][len * 4] = ENDPATT;
}

static void clear(void)
{
  memset(pat, 0, sizeof pat); memset(pattlen, 0, sizeof pattlen); memset(ord, 0, sizeof ord);
  memset(songlen, 0, sizeof songlen); memset(instr, 0, sizeof instr); memset(ltab, 0, sizeof ltab); memset(rtab, 0, sizeof rtab);
}

static void setup(FSONGIN *in)
{
  int c;
  memset(in, 0, sizeof *in);
  for (c = 0; c < MAX_CHN; c++) { in->order[c] = ord[c]; in->songlen[c] = songlen[c]; }
  in->pattern = pat; in->pattlen = pattlen; in->numsids = 1; in->multiplier = 1; in->instr = instr;
  in->ltable = ltab; in->rtable = rtab; in->name = "test"; in->author = "x";
}

static int rowat(const unsigned char *d, int size, const FUR *f, int ch, int row, RC *out)
{
  static RC rows[256];
  int o = row / f->plen;
  if (o >= f->olen) return 0;
  if (!readpattern(d, size, f->npat, f->patptr, ch, f->orders[ch][o], rows, f->plen)) return 0;
  *out = rows[row % f->plen];
  return 1;
}

int main(void)
{
  FSONGIN in;
  FSONGINFO info;
  FUR f;
  RC r;
  int n, i;

  // 1. one channel, two patterns of 16 rows, repeat and transpose, the song ends
  clear();
  newpattern(1, 16); newpattern(2, 16);
  note(1, 0, 0x60 + 24, 1, 0, 0);        // C-2
  note(1, 4, 0x60 + 28, 0, 0, 0);        // E-2, same instrument
  note(1, 8, KEYOFF, 0, 0, 0);
  note(2, 0, 0x60 + 12, 2, CMD_SETTEMPO, 4);
  ord[0][0] = REPEAT + 1; ord[0][1] = 1;  // pattern 1 twice
  ord[0][2] = TRANSUP + 2; ord[0][3] = 1; // up two semitones
  ord[0][4] = 2;
  ord[0][5] = LOOPSONG; ord[0][6] = 99;   // loop target beyond the list: treated as the end
  songlen[0] = 5; songlen[1] = 0; songlen[2] = 0;
  instr[1].ad = 0x09; instr[2].ad = 0x08;
  setup(&in);
  n = furnace_song(file, sizeof file, &in, &info);
  CHECK(n > 0);
  CHECK(parse(file, n, &f));
  CHECK(f.plen == 64);                   // 16 + 16 + 16 + 16 rows
  CHECK(f.nch == 3);
  CHECK(f.olen == 1);
  CHECK(f.ni == 2);
  CHECK(f.speed == 6);
  CHECK(rowat(file, n, &f, 0, 0, &r) && r.note == 60 + 24 && r.ins == 0);
  CHECK(rowat(file, n, &f, 0, 4, &r) && r.note == 60 + 28 && r.ins == 0);
  CHECK(rowat(file, n, &f, 0, 8, &r) && r.note == 180);
  CHECK(rowat(file, n, &f, 0, 16, &r) && r.note == 60 + 24);       // the repeat
  CHECK(rowat(file, n, &f, 0, 32, &r) && r.note == 60 + 26);       // transposed
  CHECK(rowat(file, n, &f, 0, 36, &r) && r.note == 60 + 30);
  CHECK(rowat(file, n, &f, 0, 48, &r) && r.note == 60 + 14 && r.ins == 1 && r.fx[0] == 0x09 && r.fv[0] == 4);   // still transposed
  CHECK(rowat(file, n, &f, 0, 63, &r) && r.fx[0] == 0xff);        // the song stops after the last row
  CHECK(rowat(file, n, &f, 1, 0, &r) && r.note == -1);            // an empty channel
  CHECK(!(info.warnings & FSONG_W_LOOPS));

  // 2. two channels that loop with periods of 2 and 3 patterns: lined up at 6 patterns, jump to the loop start
  clear();
  newpattern(1, 8); newpattern(2, 8); newpattern(3, 8);
  note(1, 0, 0x60 + 12, 1, 0, 0);
  note(2, 0, 0x60 + 14, 1, 0, 0);
  note(3, 0, 0x60 + 16, 1, 0, 0);
  ord[0][0] = 1; ord[0][1] = 2; ord[0][2] = LOOPSONG; ord[0][3] = 0; songlen[0] = 2;
  ord[1][0] = 1; ord[1][1] = 2; ord[1][2] = 3; ord[1][3] = LOOPSONG; ord[1][4] = 0; songlen[1] = 3;
  setup(&in);
  n = furnace_song(file, sizeof file, &in, &info);
  CHECK(n > 0 && parse(file, n, &f));
  CHECK(f.plen == 48);                   // lcm(16, 24) rows, the largest multiple of 8 up to 64 that divides it
  CHECK(f.olen == 1);
  CHECK(rowat(file, n, &f, 0, 16, &r) && r.note == 60 + 12);    // channel 0 around again
  CHECK(rowat(file, n, &f, 1, 8 * 5, &r) && r.note == 60 + 16);
  CHECK(rowat(file, n, &f, 1, 8 * 3, &r) && r.note == 60 + 12);
  CHECK(rowat(file, n, &f, 0, 47, &r) && r.fx[0] == 0x0b && r.fv[0] == 0);   // jump to order 0
  CHECK(!(info.warnings & FSONG_W_LOOPS));

  // 3. loops that start at different rows are lined up and reported
  clear();
  newpattern(1, 8); newpattern(2, 8);
  note(1, 0, 0x60 + 12, 1, 0, 0);
  note(2, 0, 0x60 + 14, 1, 0, 0);
  ord[0][0] = 1; ord[0][1] = 2; ord[0][2] = LOOPSONG; ord[0][3] = 0; songlen[0] = 2;
  ord[1][0] = 1; ord[1][1] = 2; ord[1][2] = LOOPSONG; ord[1][3] = 1; songlen[1] = 2;
  setup(&in);
  n = furnace_song(file, sizeof file, &in, &info);
  CHECK(n > 0);
  printf("w=%x\n", info.warnings);
  CHECK(info.warnings & FSONG_W_LOOPS);

  // 4. funktempo, one channel tempo, different pattern lengths (gcd 8 -> 64 rows), 2 SIDs, a dangerous order list
  clear();
  newpattern(1, 24); newpattern(2, 40);
  ltab[3 * MAX_TABLELEN] = 7; rtab[3 * MAX_TABLELEN] = 5;
  note(1, 0, 0x60 + 12, 1, CMD_FUNKTEMPO, 1);
  note(1, 2, 0x60 + 13, 1, CMD_SETTEMPO, 0x86);
  note(2, 0, 0x60 + 14, 1, CMD_SETTEMPO, 6);
  ord[0][0] = 1; ord[0][1] = 2; ord[0][2] = LOOPSONG; ord[0][3] = 0; songlen[0] = 2;
  ord[3][0] = 2; ord[3][1] = 1; ord[3][2] = LOOPSONG; ord[3][3] = 0; songlen[3] = 2;
  setup(&in);
  in.numsids = 2;
  n = furnace_song(file, sizeof file, &in, &info);
  CHECK(n > 0 && parse(file, n, &f));
  CHECK(f.nch == 6);
  CHECK(f.plen == 64);
  CHECK(info.warnings & FSONG_W_CHANTEMPO);
  CHECK(rowat(file, n, &f, 0, 0, &r) && r.fx[0] == 0x09 && r.fv[0] == 7 && r.fx[1] == 0x0f && r.fv[1] == 5);
  CHECK(rowat(file, n, &f, 0, 24, &r) && r.note == 60 + 14 && r.fx[0] == 0x09 && r.fv[0] == 6 && r.fx[1] == 0x0f && r.fv[1] == 6);

  // 5. an order list that reads past its end and patterns that do not exist must not crash
  clear();
  ord[0][0] = 200; ord[0][1] = 255; ord[0][2] = LOOPSONG; ord[0][3] = 250; songlen[0] = 2;
  setup(&in);
  n = furnace_song(file, sizeof file, &in, &info);
  CHECK(n >= 0);
  for (i = 0; i < MAX_CHN; i++) songlen[i] = 0;
  setup(&in);
  n = furnace_song(file, sizeof file, &in, &info);
  CHECK(n > 0 && parse(file, n, &f) && f.olen == 1);

  // 6. too small an output buffer fails cleanly
  clear();
  newpattern(1, 16); note(1, 0, 0x60 + 12, 1, 0, 0);
  ord[0][0] = 1; songlen[0] = 1;
  setup(&in);
  CHECK(furnace_song(file, 100, &in, &info) == 0);

  printf("furnacesong: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
