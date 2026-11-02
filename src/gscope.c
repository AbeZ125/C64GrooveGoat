//
// C64 GrooveGoat - live oscilloscope data (see gscope.h)
//

#include <string.h>
#include "gscope.h"

static short hist[SCOPE_VOICES][SCOPE_HIST];
static volatile unsigned written[SCOPE_VOICES];

void scope_clear(void)
{
  memset(hist, 0, sizeof hist);
  memset((void *)written, 0, sizeof written);
}

void scope_add(int voice, const short *src, int stride, int n)
{
  unsigned pos;
  int i;
  if ((voice < 0) || (voice >= SCOPE_VOICES)) return;
  pos = written[voice];
  for (i = 0; i < n; i++)
    hist[voice][(pos + i) & (SCOPE_HIST - 1)] = src[i * stride];
  written[voice] = pos + n;
}

unsigned scope_count(int voice)
{
  if ((voice < 0) || (voice >= SCOPE_VOICES)) return 0;
  return written[voice];
}

static int row(int sample, int h, int full)
{
  int r = (h - 1) / 2 - (int)((long)sample * ((h - 1) / 2) / full);
  return r < 0 ? 0 : (r > h - 1 ? h - 1 : r);
}

int scope_render(int voice, int span, int cols, int h, int full, int *lo, int *hi)
{
  unsigned end;
  int c, i, first, best;
  int wmin = 32767, wmax = -32768;
  const short *v;

  for (c = 0; c < cols; c++) lo[c] = hi[c] = (h - 1) / 2;
  if ((voice < 0) || (voice >= SCOPE_VOICES) || (span < cols) || (cols < 1) || (full < 1)) return 0;
  if (span + SCOPE_SEARCH + 1 > SCOPE_HIST) return 0;
  end = written[voice];
  if (end < (unsigned)(span + SCOPE_SEARCH + 1)) return 0;
  v = hist[voice];

  // the middle of the newest window is the middle of the wave (the average moves with the part of a period that is cut off)
  for (i = 0; i < span; i++)
  {
    int s = v[(end - span + i) & (SCOPE_HIST - 1)];
    if (s < wmin) wmin = s;
    if (s > wmax) wmax = s;
  }
  {
    int mean = (wmin + wmax) / 2;
    // the newest point where the wave crosses its middle going up; looks back from the newest window
    best = (int)(end - span);
    for (i = 0; i < SCOPE_SEARCH; i++)
    {
      unsigned p = end - span - i;
      if ((v[(p - 1) & (SCOPE_HIST - 1)] < mean) && (v[p & (SCOPE_HIST - 1)] >= mean)) { best = (int)p; break; }
    }
    first = best;
    for (c = 0; c < cols; c++)
    {
      int a = c * span / cols, b = (c + 1) * span / cols, mn = 32767, mx = -32768;
      if (b <= a) b = a + 1;
      for (i = a; i < b; i++)
      {
        int s = v[(unsigned)(first + i) & (SCOPE_HIST - 1)] - mean;
        if (s < mn) mn = s;
        if (s > mx) mx = s;
      }
      hi[c] = row(mn, h, full);      // the lowest sample is the lowest row on the screen (largest row number)
      lo[c] = row(mx, h, full);
    }
  }
  return 1;
}
