// Tests of the filtertable summary: the replay is compared tick by tick with a copy of filterexec() of the playroutine
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gfsummary.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 20) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

typedef struct { unsigned char filterctrl, filtertype, filtercutoff, filtertime, filterptr; } F;

// copy of filterexec() of gplay.c (ptr == 0 combine read is skipped where the original would read before the table)
static unsigned char L[255], R[255];
static int refstep(F *f)
{
  if (f->filterptr)
  {
    if (L[f->filterptr - 1] == 0xff)
    {
      f->filterptr = R[f->filterptr - 1];
      if (!f->filterptr) return 1;
    }
    if (!f->filtertime)
    {
      if (L[f->filterptr - 1] >= 0x80)
      {
        f->filtertype = L[f->filterptr - 1] & 0x70;
        f->filterctrl = R[f->filterptr - 1];
        f->filterptr++;
        if (f->filterptr && L[f->filterptr - 1] == 0x00)
        {
          f->filtercutoff = R[f->filterptr - 1];
          f->filterptr++;
        }
      }
      else
      {
        if (L[f->filterptr - 1]) f->filtertime = L[f->filterptr - 1];
        else
        {
          f->filtercutoff = R[f->filterptr - 1];
          f->filterptr++;
        }
      }
    }
    if (f->filtertime)
    {
      f->filtercutoff += R[f->filterptr - 1];
      f->filtertime--;
      if (!f->filtertime) f->filterptr++;
    }
  }
  return 0;
}

int main(void)
{
  static FILTSUM s;
  char b[96];
  int it, i, k;
  srand(11);
  for (it = 0; it < 30000; it++)
  {
    int start = 1 + rand() % 255, n = 20 + rand() % 235;
    F f;
    memset(L, 0, sizeof L); memset(R, 0, sizeof R);
    for (k = 0; k < n && k < 255; k++)
    {
      int m = rand() % 8;
      L[k] = m == 0 ? 0xff : (m < 3 ? 0x80 + (rand() % 8) * 16 : (m < 5 ? 0 : 1 + rand() % 40));
      R[k] = rand() % 256;
      if (L[k] == 0xff) R[k] = rand() % 3 ? 1 + rand() % 255 : 0;
    }
    summarize_filter(&s, L, R, start, 50);
    memset(&f, 0, sizeof f);
    f.filterptr = start; f.filtercutoff = 0x80;
    for (i = 0; i < s.ticks; i++)
    {
      refstep(&f);
      CHECK(s.cutoff[i] == f.filtercutoff);
      CHECK(s.type[i] == f.filtertype);
      CHECK(s.ctrl[i] == f.filterctrl);
    }
    if (s.looped) CHECK(s.looplen > 0);
  }
  // an example: low-pass resonance F voice 1, cutoff $40, then stop
  memset(L, 0, sizeof L); memset(R, 0, sizeof R);
  L[0] = 0x90; R[0] = 0xf1; L[1] = 0x00; R[1] = 0x40; L[2] = 0xff;
  summarize_filter(&s, L, R, 1, 50);
  CHECK(s.stopped && s.ticks == 1 && s.cutoff[0] == 0x40 && s.type[0] == 0x10 && s.ctrl[0] == 0xf1);
  CHECK(strstr(s.filter, "low-pass") && strstr(s.filter, "resonance F") && strstr(s.filter, "voices 1"));
  // a looping sweep
  memset(L, 0, sizeof L); memset(R, 0, sizeof R);
  L[0] = 0x90; R[0] = 0xf1; L[1] = 0x00; R[1] = 0x20; L[2] = 0x10; R[2] = 0x08; L[3] = 0x10; R[3] = 0xf8; L[4] = 0xff; R[4] = 3;
  summarize_filter(&s, L, R, 1, 50);
  CHECK(s.looped && s.looplen == 32 && strstr(s.shape, "up and down"));
  CHECK(filter_rowtext(b, sizeof b, 1, 0x00, 0x80) == PK_SET && strstr(b, "50%"));
  CHECK(strcmp(filter_passband(0x50), "notch (low+high)") == 0);
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
