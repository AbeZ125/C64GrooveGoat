// Tests of the table summaries (src/gsummary.c). Build and run with:  tests/summary_test.sh
// The analysis is compared, tick by tick, with a copy of the playroutine's pulsetable code (gplay.c,
// PULSEEXEC) on thousands of random tables, and the known examples of the manual are checked.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gsummary.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; if (failures < 25) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

// ---- the playroutine's pulsetable code, copied from gplay.c (PULSEEXEC) -----------------------------
typedef struct { unsigned char ptr; unsigned char pulsetime; unsigned short pulse; } REFCHN;

static void refstep(REFCHN *cptr, const unsigned char *ltab, const unsigned char *rtab)
{
PULSEEXEC:
  if (cptr->ptr)
  {
    if (ltab[cptr->ptr - 1] == 0xff)
    {
      cptr->ptr = rtab[cptr->ptr - 1];
      if (!cptr->ptr) goto PULSEEXEC;
    }
    if (!cptr->pulsetime)
    {
      if (ltab[cptr->ptr - 1] >= 0x80)
      {
        cptr->pulse = (ltab[cptr->ptr - 1] & 0xf) << 8;
        cptr->pulse |= rtab[cptr->ptr - 1];
        cptr->ptr++;
      }
      else cptr->pulsetime = ltab[cptr->ptr - 1];
    }
    if (cptr->pulsetime)
    {
      unsigned char speed = rtab[cptr->ptr - 1];
      if (speed < 0x80) { cptr->pulse += speed; cptr->pulse &= 0xfff; }
      else { cptr->pulse += speed; cptr->pulse -= 0x100; cptr->pulse &= 0xfff; }
      cptr->pulsetime--;
      if (!cptr->pulsetime) cptr->ptr++;
    }
  }
}

static unsigned seed = 12345;
static unsigned rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

static void randomtable(unsigned char *l, unsigned char *r)
{
  int n = 1 + rnd() % 12;       // rows in use
  int i;
  memset(l, 0, 255);
  memset(r, 0, 255);
  for (i = 0; i < n; i++)
  {
    int kind = rnd() % 10;
    if (kind < 5) { l[i] = 1 + rnd() % (rnd() % 2 ? 8 : 127); r[i] = (rnd() % 3) ? (rnd() % 0x30) : (0x100 - rnd() % 0x30); if (!(rnd() % 6)) r[i] = 0; }
    else if (kind < 7) { l[i] = 0x80 + rnd() % 0x70; r[i] = rnd() % 256; }
    else if (kind < 9) { l[i] = 0xff; r[i] = rnd() % (n + 2); }
    else { l[i] = 0; r[i] = 0; }
  }
  if (rnd() % 2) { l[n] = 0xff; r[n] = 1 + rnd() % n; }
}

static PULSESUM S;

static void test_manual_example(void)
{
  // The pulsetable example of the manual:  01: 80 10   02: 20 40   03: 40 E0   04: 40 20   05: FF 03
  unsigned char l[255] = {0x80, 0x20, 0x40, 0x40, 0xff}, r[255] = {0x10, 0x40, 0xe0, 0x20, 0x03};
  summarize_pulse(&S, l, r, 1, 50.0);
  CHECK(S.looped, "the example repeats");
  CHECK(S.loopstart == 33, "loop starts at tick 33, was %d", S.loopstart);
  CHECK(S.looplen == 128, "loop is 128 ticks, was %d", S.looplen);
  CHECK(S.minwidth == 0x010 && S.maxwidth == 0x810, "range $%03X-$%03X", S.minwidth, S.maxwidth);
  CHECK(S.startknown, "starts by setting the width");
  CHECK(!S.wraps && !S.stopped && !S.capped, "no wrap, no stop");
  CHECK(strstr(S.shape, "up and down") != NULL, "shape '%s'", S.shape);
  CHECK(strstr(S.timing, "128t") != NULL && strstr(S.timing, "2.6 s") != NULL, "timing '%s'", S.timing);
  CHECK(S.nrows == 5, "five rows described, %d", S.nrows);
  CHECK(strstr(S.rows[0].text, "set width $010 = 0.4%") != NULL, "row 1 '%s'", S.rows[0].text);
  CHECK(strstr(S.rows[1].text, "widen 32t, +$40/t = +50%") != NULL, "row 2 '%s'", S.rows[1].text);
  CHECK(strstr(S.rows[2].text, "narrow 64t, -$20/t = -50%") != NULL, "row 3 '%s'", S.rows[2].text);
  CHECK(strstr(S.rows[4].text, "loop back to 03") != NULL, "row 5 '%s'", S.rows[4].text);
  printf("  manual example: %s | %s | %s\n", S.range, S.shape, S.timing);
}

static void test_simple_cases(void)
{
  unsigned char l[255], r[255];
  memset(l, 0, 255); memset(r, 0, 255);
  // The tip of the manual: "80 00 / FF 00" stops a pulse modulation: the width is set to 0 and the table stops
  l[0] = 0x88; r[0] = 0x00; l[1] = 0xff; r[1] = 0x00;
  summarize_pulse(&S, l, r, 1, 50.0);
  CHECK(S.stopped && !S.looped, "set then stop");
  CHECK(S.minwidth == 0x800 && S.maxwidth == 0x800, "$800 = 50%%, got $%03X-$%03X", S.minwidth, S.maxwidth);
  CHECK(strstr(S.shape, "sets the width to 50%") != NULL, "shape '%s'", S.shape);
  // No program
  summarize_pulse(&S, l, r, 0, 50.0);
  CHECK(S.startrow == 0 && strstr(S.oneline, "unchanged") != NULL, "no program: '%s'", S.oneline);
  // A one-way sweep with a stop
  memset(l, 0, 255); memset(r, 0, 255);
  l[0] = 0x80; r[0] = 0x00; l[1] = 0x40; r[1] = 0x10; l[2] = 0xff; r[2] = 0x00;
  summarize_pulse(&S, l, r, 1, 50.0);
  CHECK(S.stopped && S.moves && S.maxwidth == 0x400, "sweep to $400, max $%03X", S.maxwidth);
  CHECK(strstr(S.shape, "sweeps up, stops at 25%") != NULL, "shape '%s'", S.shape);
  // A modulation that goes right round the width (64 ticks of +$40 is a whole lap of $1000) wraps
  memset(l, 0, 255); memset(r, 0, 255);
  l[0] = 0x40; r[0] = 0x40; l[1] = 0xff; r[1] = 0x01;
  summarize_pulse(&S, l, r, 1, 50.0);
  CHECK(S.looped && S.wraps, "wraps round");
  CHECK(strstr(S.shape, "widening") != NULL, "shape '%s'", S.shape);
  // The empty row stops the table
  memset(l, 0, 255); memset(r, 0, 255);
  l[0] = 0x84; r[0] = 0x00;
  summarize_pulse(&S, l, r, 1, 50.0);
  CHECK(S.stopped && strstr(S.note, "empty") != NULL, "empty row: '%s'", S.note);
  // A jump to a jump row plays as 'set width' (a quirk of the player)
  memset(l, 0, 255); memset(r, 0, 255);
  l[0] = 0xff; r[0] = 0x02; l[1] = 0xff; r[1] = 0x01;
  summarize_pulse(&S, l, r, 1, 50.0);
  CHECK(S.looped || S.stopped || S.capped, "terminates");
  printf("  jump to a jump: %s | note: %s\n", S.oneline, S.note);
}

static void test_against_the_player(void)
{
  int t, loops = 0, stops = 0, caps = 0, wraps = 0, compared = 0;
  for (t = 0; t < 40000; t++)
  {
    unsigned char l[255], r[255];
    int start = 1 + rnd() % 8;
    REFCHN c;
    int k;
    randomtable(l, r);
    summarize_pulse(&S, l, r, start, 50.0);
    c.ptr = start; c.pulsetime = 0; c.pulse = 0x800;
    // Every tick the analysis replayed is the player's tick
    for (k = 0; k < S.ticks; k++)
    {
      refstep(&c, l, r);
      CHECK(c.pulse == S.width[k], "table %d tick %d: player $%03X, summary $%03X", t, k, c.pulse, S.width[k]);
      if (c.pulse != S.width[k]) break;
      compared++;
    }
    if (S.looped)
    {
      // The player repeats: the width a period later is the same, for several periods
      int p;
      loops++;
      for (p = 0; p < 3 * S.looplen && p < 3000; p++)
      {
        int idx = S.loopstart + (p % S.looplen);
        CHECK(idx < S.ticks, "loop index in range");
        refstep(&c, l, r);
        if (S.loopstart + S.looplen + p < S.ticks) continue; // already compared above
        CHECK(c.pulse == S.width[idx], "table %d loop tick %d: player $%03X, summary $%03X", t, p, c.pulse, S.width[idx]);
        if (c.pulse != S.width[idx]) break;
      }
    }
    if (S.stopped)
    {
      int p;
      stops++;
      for (p = 0; p < 50; p++)
      {
        unsigned short before = c.pulse;
        refstep(&c, l, r);
        CHECK(c.pulse == before, "table %d: still changing after the summary says it stopped", t);
      }
    }
    if (S.capped) caps++;
    if (S.wraps) wraps++;
    CHECK(S.minwidth <= S.maxwidth, "min <= max");
    CHECK(S.oneline[0] != 0 && S.shape[0] != 0 && S.range[0] != 0, "words for table %d", t);
    CHECK(S.nrows >= 1 || !S.startrow, "rows listed");
  }
  printf("  %d random tables, %d ticks compared with the player: %d repeat, %d stop, %d wrap, %d not decided in %d ticks\n", 40000, compared, loops, stops, wraps, caps, SUM_MAXTICKS);
  CHECK(loops > 3000 && stops > 3000 && wraps > 500, "the random tables cover both kinds");
}

static void test_formats(void)
{
  char b[32];
  sum_percent(b, sizeof b, 0x800); CHECK(!strcmp(b, "50%"), "'%s'", b);
  sum_percent(b, sizeof b, 0x010); CHECK(!strcmp(b, "0.4%"), "'%s'", b);
  sum_percent(b, sizeof b, 0xfff); CHECK(!strcmp(b, "100%"), "'%s'", b);
  sum_percent(b, sizeof b, 0); CHECK(!strcmp(b, "0%"), "'%s'", b);
  sum_duration(b, sizeof b, 25, 50.0); CHECK(!strcmp(b, "500 ms"), "'%s'", b);
  sum_duration(b, sizeof b, 128, 50.0); CHECK(!strcmp(b, "2.6 s"), "'%s'", b);
  sum_duration(b, sizeof b, 1000, 60.0); CHECK(!strcmp(b, "17 s"), "'%s'", b);
  CHECK(pulse_rowtext(b, sizeof b, 3, 0xff, 0) == PK_STOP, "stop");
  CHECK(pulse_rowtext(b, sizeof b, 3, 0xff, 5) == PK_JUMP && strstr(b, "forward") != NULL, "forward jump '%s'", b);
  CHECK(pulse_rowtext(b, sizeof b, 3, 0x05, 0) == PK_MODULATE && strstr(b, "hold the width for 5t") != NULL, "hold '%s'", b);
  CHECK(pulse_rowtext(b, sizeof b, 3, 0x00, 0) == PK_EMPTY, "empty");
}

int main(void)
{
  printf("table summary tests\n");
  test_formats();
  test_manual_example();
  test_simple_cases();
  test_against_the_player();
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
