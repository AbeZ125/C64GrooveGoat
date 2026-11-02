// Tests of the live oscilloscope data: a steady wave stands still between two looks, silence is a flat line, the
// size of the wave follows the level.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../src/gscope.h"

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; if (failed < 10) printf("FAILED line %d: %s\n", __LINE__, #c); } } while (0)

static short wavebuf[5000];

static void feed(int voice, long *t, int n, double period, double amp, int saw)
{
  int i;
  for (i = 0; i < n; i++, (*t)++)
  {
    double ph = fmod((double)*t, period) / period;
    wavebuf[i] = (short)(saw ? (ph * 2 - 1) * amp : sin(ph * 6.283185307) * amp);
  }
  scope_add(voice, wavebuf, 1, n);
}

int main(void)
{
  int lo[100], hi[100], lo2[100], hi2[100], c, trial;
  long t = 0;

  scope_clear();
  // not enough data
  feed(0, &t, 500, 300, 8000, 0);
  CHECK(scope_render(0, 1024, 64, 64, 16000, lo, hi) == 0);
  for (c = 0; c < 64; c++) CHECK(lo[c] == 31 && hi[c] == 31);

  // a steady wave looks the same whenever it is looked at
  for (trial = 0; trial < 300; trial++)
  {
    int n = 50 + rand() % 900, maxdiff = 0;
    double period = 100 + rand() % 500;
    scope_clear();
    t = 0;
    feed(0, &t, 4000, period, 12000, trial & 1);
    CHECK(scope_render(0, 1024, 64, 64, 16000, lo, hi) == 1);
    feed(0, &t, n, period, 12000, trial & 1);
    CHECK(scope_render(0, 1024, 64, 64, 16000, lo2, hi2) == 1);
    for (c = 0; c < 64; c++)
    {
      CHECK(lo[c] <= hi[c]);
      if (abs(lo[c] - lo2[c]) > maxdiff) maxdiff = abs(lo[c] - lo2[c]);
      if (abs(hi[c] - hi2[c]) > maxdiff) maxdiff = abs(hi[c] - hi2[c]);
    }
    // the trigger is on a sample, so the picture can move by one sample: a few rows at most for a steep part
    if (maxdiff > 6 && failed < 10) printf("trial %d period %.0f saw %d n %d maxdiff %d\n", trial, period, trial & 1, n, maxdiff);
    CHECK(maxdiff <= 6);
  }

  // silence is a flat line in the middle; a quiet wave is small and a loud one fills the picture
  scope_clear();
  t = 0;
  feed(1, &t, 4000, 200, 0, 0);
  CHECK(scope_render(1, 1024, 64, 64, 16000, lo, hi) == 1);
  for (c = 0; c < 64; c++) CHECK(lo[c] == 31 && hi[c] == 31);
  scope_clear();
  t = 0;
  feed(1, &t, 5000, 200, 2000, 0);
  scope_render(1, 1024, 64, 64, 16000, lo, hi);
  {
    int top = 63, bot = 0;
    for (c = 0; c < 64; c++) { if (lo[c] < top) top = lo[c]; if (hi[c] > bot) bot = hi[c]; }
    CHECK(top >= 23 && bot <= 39 && top < 31 && bot > 31);
  }
  scope_clear();
  t = 0;
  feed(1, &t, 5000, 200, 16000, 0);
  scope_render(1, 1024, 64, 64, 16000, lo, hi);
  {
    int top = 63, bot = 0;
    for (c = 0; c < 64; c++) { if (lo[c] < top) top = lo[c]; if (hi[c] > bot) bot = hi[c]; }
    CHECK(top <= 1 && bot >= 61);
  }

  // voices are kept apart, and a bad voice is ignored
  scope_clear();
  t = 0;
  feed(2, &t, 4000, 200, 8000, 0);
  CHECK(scope_count(2) == 4000 && scope_count(3) == 0);
  CHECK(scope_render(3, 1024, 64, 64, 16000, lo, hi) == 0);
  CHECK(scope_render(99, 1024, 64, 64, 16000, lo, hi) == 0);

  printf("%d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
