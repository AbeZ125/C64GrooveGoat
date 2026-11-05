// Tests of the Furnace song import (.fur): the decompressor, round trips of synthetic songs through the exporter
// (notes, instruments, tempo, loops, slides), refused files, and damaged files (truncated and with flipped bytes) that
// must never crash. FURNACE_FURLIB=<folder> also reads every .fur of the folder (every C64 module must import).
// Run with sanitizers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "../src/gfurnacesong.h"
#include "../src/gfurnacesongimp.h"
#include "furnacesongimp_zdata.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 30) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned char pat[MAX_PATT][MAX_PATTROWS*4+4];
static int pattlen[MAX_PATT];
static unsigned char ord[MAX_CHN][MAX_SONGLEN+2];
static int songlen[MAX_CHN];
static INSTR instr[MAX_INSTR];
static unsigned char ltab[MAX_TABLES * MAX_TABLELEN], rtab[MAX_TABLES * MAX_TABLELEN];
static unsigned char file[FSONG_MAXFILE];
static FSONGIMP imp;

static void clear(void)
{
  memset(pat, 0, sizeof pat); memset(pattlen, 0, sizeof pattlen); memset(ord, 0, sizeof ord);
  memset(songlen, 0, sizeof songlen); memset(instr, 0, sizeof instr); memset(ltab, 0, sizeof ltab); memset(rtab, 0, sizeof rtab);
}

static void newpattern(int p, int len)
{
  int r;
  pattlen[p] = len;
  memset(pat[p], 0, sizeof pat[p]);
  for (r = 0; r < len; r++) pat[p][r * 4] = REST;
  pat[p][len * 4] = ENDPATT;
}

static void note(int p, int row, int n, int ins, int cmd, int data)
{
  pat[p][row * 4] = n; pat[p][row * 4 + 1] = ins; pat[p][row * 4 + 2] = cmd; pat[p][row * 4 + 3] = data;
}

static int exportsong(int numsids)
{
  FSONGIN in;
  FSONGINFO info;
  int c;
  memset(&in, 0, sizeof in);
  for (c = 0; c < MAX_CHN; c++) { in.order[c] = ord[c]; in.songlen[c] = songlen[c]; }
  in.pattern = pat; in.pattlen = pattlen; in.numsids = numsids; in.multiplier = 1; in.instr = instr;
  in.ltable = ltab; in.rtable = rtab; in.name = "round trip"; in.author = "tester";
  return furnace_song(file, sizeof file, &in, &info);
}

// the note byte of channel c at the row (in the order list played out, the loop not followed)
static int noteat(int c, int row)
{
  int p, r = row;
  for (p = 0; p < imp.songlen[c]; p++)
  {
    int pn = imp.order[c][p];
    if (pn >= MAX_PATT) return -1;
    if (r < imp.pattlen[pn]) return imp.pattern[pn][r * 4];
    r -= imp.pattlen[pn];
  }
  return -1;
}
static int cmdat(int c, int row, int *data)
{
  int p, r = row;
  for (p = 0; p < imp.songlen[c]; p++)
  {
    int pn = imp.order[c][p];
    if (r < imp.pattlen[pn]) { if (data) *data = imp.pattern[pn][r * 4 + 3]; return imp.pattern[pn][r * 4 + 2]; }
    r -= imp.pattlen[pn];
  }
  return -1;
}
static int insat(int c, int row)
{
  int p, r = row;
  for (p = 0; p < imp.songlen[c]; p++)
  {
    int pn = imp.order[c][p];
    if (r < imp.pattlen[pn]) return imp.pattern[pn][r * 4 + 1];
    r -= imp.pattlen[pn];
  }
  return -1;
}

static void testinflate(void)
{
  unsigned char *out;
  unsigned char plain2[1380];
  int i, n;
  for (i = 0; i < 900; i++) plain2[i] = (i * i + 3 * i) % 16;
  for (i = 0; i < 40; i++) memcpy(plain2 + 900 + i * 12, "GoatTracker ", 12);
  n = fsi_inflate(z_stored, sizeof z_stored, &out, 0);
  CHECK(n == 1380 && !memcmp(out, plain2, 1380)); free(out);
  n = fsi_inflate(z_fixed, sizeof z_fixed, &out, 0);
  CHECK(n == 29 && !memcmp(out, "hello, hello, hello, Furnace!", 29)); free(out);
  n = fsi_inflate(z_dynamic, sizeof z_dynamic, &out, 0);
  CHECK(n == 1380 && !memcmp(out, plain2, 1380)); free(out);
  // damaged: every truncation fails or gives less, none crashes
  for (i = 0; i < (int)sizeof z_dynamic; i++)
  {
    n = fsi_inflate(z_dynamic, i, &out, 0);
    if (n >= 0) free(out);
  }
  {
    unsigned char bad[sizeof z_dynamic];
    for (i = 2; i < (int)sizeof z_dynamic; i++)
    {
      memcpy(bad, z_dynamic, sizeof bad);
      bad[i] ^= 0x5a;
      n = fsi_inflate(bad, sizeof bad, &out, 0);
      if (n >= 0) free(out);
    }
  }
}

static void testroundtrip(void)
{
  int n, d = 0;

  // one channel: repeats, a transpose, a tempo change, a slide, the loop
  clear();
  newpattern(1, 16); newpattern(2, 16);
  ltab[3 * MAX_TABLELEN] = 0x00; rtab[3 * MAX_TABLELEN] = 0x85;          // speedtable 1: half a semitone at C-4
  note(1, 0, 0x60 + 24, 1, 0, 0);
  note(1, 4, 0x60 + 28, 0, CMD_PORTAUP, 1);
  note(1, 8, KEYOFF, 0, 0, 0);
  note(2, 0, 0x60 + 12, 2, CMD_SETTEMPO, 4);
  note(2, 2, 0x60 + 19, 2, 0, 0);
  ord[0][0] = REPEAT + 1; ord[0][1] = 1;
  ord[0][2] = TRANSUP + 2; ord[0][3] = 2;
  ord[0][4] = LOOPSONG; ord[0][5] = 0;
  songlen[0] = 4;
  instr[1].ad = 0x09; instr[1].sr = 0xf0; instr[1].ptr[WTBL] = 0; instr[1].firstwave = 0x41;
  instr[2].ad = 0x08; instr[2].sr = 0xa0; instr[2].firstwave = 0x21;
  n = exportsong(1);
  CHECK(n > 0);
  CHECK(furnace_songimport(file, n, &imp) == FSI_OK);
  CHECK(imp.numsids == 1);
  CHECK(imp.instruments == 2);
  CHECK(noteat(0, 0) == 0x60 + 24 && insat(0, 0) == 1);
  CHECK(noteat(0, 4) == 0x60 + 28);
  CHECK(noteat(0, 8) == KEYOFF);
  CHECK(noteat(0, 16) == 0x60 + 24);                                      // the repeat is played out
  CHECK(noteat(0, 32) == 0x60 + 14 && insat(0, 32) == 2);                 // so is the transpose (12 + 2)
  CHECK(noteat(0, 34) == 0x60 + 21);
  CHECK(cmdat(0, 32, &d) == CMD_SETTEMPO && d == 4);
  CHECK(cmdat(0, 4, &d) == CMD_PORTAUP);
  CHECK(imp.stbllen >= 1);
  {
    int e = d, speed;
    CHECK(e >= 1 && e <= imp.stbllen);
    speed = (imp.stbl_l[e - 1] << 8) | imp.stbl_r[e - 1];
    CHECK(speed > 0x70 && speed < 0x9a);                                  // 0x85 +/- a few percent
  }
  CHECK(imp.songlen[0] > 0 && imp.order[0][imp.songlen[0]] == LOOPSONG);
  CHECK(imp.order[0][imp.songlen[0] + 1] == 0);                           // loops to the start
  CHECK(imp.ntsc == 0 && imp.sidmodel == 0 && imp.multiplier == 1);
  CHECK(!strcmp(imp.name, "round trip") && !strcmp(imp.author, "tester"));
  {
    unsigned char fui[FURNACE_MAXFILE];
    CHECK(furnace_songinstrument(&imp, 1, fui, sizeof fui) > 12);
    CHECK(!furnace_songinstrument(&imp, 3, fui, sizeof fui));
  }
  {
    static unsigned char body[400000];
    int bs = furnace_songbody(&imp, body, sizeof body);
    CHECK(bs > 100);
    CHECK(furnace_songbody(&imp, body, 50) == 0);
  }
  fsi_free(&imp);

  // a song that ends does not loop (the loop target is past the list)
  clear();
  newpattern(1, 8);
  note(1, 0, 0x60 + 12, 1, 0, 0);
  ord[0][0] = 1; ord[0][1] = LOOPSONG; ord[0][2] = 99; songlen[0] = 1;
  n = exportsong(1);
  CHECK(n > 0 && furnace_songimport(file, n, &imp) == FSI_OK);
  CHECK(imp.songlen[0] >= 1 && imp.order[0][imp.songlen[0] + 1] >= imp.songlen[0]);
  fsi_free(&imp);

  // two SIDs, an 8580, funktempo: the speedtable gets the pair
  clear();
  newpattern(1, 32);
  ltab[3 * MAX_TABLELEN] = 7; rtab[3 * MAX_TABLELEN] = 5;
  note(1, 0, 0x60 + 12, 1, CMD_FUNKTEMPO, 1);
  ord[0][0] = 1; ord[0][1] = LOOPSONG; ord[0][2] = 0; songlen[0] = 1;
  ord[3][0] = 1; ord[3][1] = LOOPSONG; ord[3][2] = 0; songlen[3] = 1;
  n = exportsong(2);
  CHECK(n > 0 && furnace_songimport(file, n, &imp) == FSI_OK);
  CHECK(imp.numsids == 2);
  CHECK(cmdat(0, 0, &d) == CMD_FUNKTEMPO);
  CHECK(imp.stbl_l[d - 1] == 7 && imp.stbl_r[d - 1] == 5);
  CHECK(noteat(3, 0) == 0x60 + 12);
  fsi_free(&imp);
}

static void testrefusals(void)
{
  int n, i;
  static unsigned char copy[FSONG_MAXFILE];
  clear();
  newpattern(1, 8);
  note(1, 0, 0x60 + 12, 1, 0, 0);
  ord[0][0] = 1; ord[0][1] = LOOPSONG; ord[0][2] = 0; songlen[0] = 1;
  n = exportsong(1);
  CHECK(n > 0);
  // not a module
  CHECK(furnace_songimport((const unsigned char *)"nothing at all, not a module of any kind, nothing at all.......", 64, &imp) == FSI_NOTFUR);
  CHECK(furnace_songimport(file, 10, &imp) == FSI_NOTFUR);
  // another chip
  memcpy(copy, file, n);
  {
    unsigned info = copy[20] | (copy[21] << 8);
    copy[info + 8 + 24] = 0x83;          // chip id of the first chip: not a C64
  }
  CHECK(furnace_songimport(copy, n, &imp) == FSI_NOTC64);
  // a version this reader does not know
  memcpy(copy, file, n);
  copy[16] = 100; copy[17] = 0;
  CHECK(furnace_songimport(copy, n, &imp) == FSI_TOONEW);
  memcpy(copy, file, n);
  copy[16] = 0xf0; copy[17] = 0;
  CHECK(furnace_songimport(copy, n, &imp) == FSI_TOONEW);
  // the file as it is
  CHECK(furnace_songimport(file, n, &imp) == FSI_OK);
  fsi_free(&imp);
  // every truncation, and every byte changed: no crash
  for (i = 0; i < n; i += 3)
  {
    int r = furnace_songimport(file, i, &imp);
    if (r == FSI_OK) fsi_free(&imp);
  }
  for (i = 16; i < n; i++)
  {
    int r;
    memcpy(copy, file, n);
    copy[i] ^= 0xa5;
    r = furnace_songimport(copy, n, &imp);
    if (r == FSI_OK) fsi_free(&imp);
    copy[i] = file[i] ^ 0xff;
    r = furnace_songimport(copy, n, &imp);
    if (r == FSI_OK) fsi_free(&imp);
  }
}

static void testlibrary(const char *dir)
{
  DIR *d = opendir(dir);
  struct dirent *e;
  int files = 0, ok = 0, notc64 = 0, other = 0;
  if (!d) return;
  while ((e = readdir(d)))
  {
    int len = (int)strlen(e->d_name);
    char path[1024];
    FILE *f;
    long sz;
    unsigned char *buf;
    int r;
    if (len < 5 || strcmp(e->d_name + len - 4, ".fur")) continue;
    snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
    f = fopen(path, "rb");
    if (!f) continue;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc(sz);
    if (buf && fread(buf, 1, sz, f) == (size_t)sz)
    {
      static unsigned char body[400000];
      files++;
      r = furnace_songimport(buf, (int)sz, &imp);
      if (r == FSI_OK)
      {
        ok++;
        CHECK(furnace_songbody(&imp, body, sizeof body) > 0);
        fsi_free(&imp);
      }
      else if (r == FSI_NOTC64 || r == FSI_TOONEW) notc64++;
      else other++;
    }
    free(buf);
    fclose(f);
  }
  closedir(d);
  printf("library: %d modules, %d imported, %d refused (other chips or versions), %d failed\n", files, ok, notc64, other);
  CHECK(other == 0);
}

int main(void)
{
  const char *lib = getenv("FURNACE_FURLIB");
  testinflate();
  testroundtrip();
  testrefusals();
  if (lib && lib[0]) testlibrary(lib);
  printf("furnacesongimp: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
