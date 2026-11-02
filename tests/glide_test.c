// Tests of the glide speed: replays the playroutine's tone portamento (gplay.c, toneporta) and checks the slide
// arrives when it should.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gglide.h"
#include "../src/gcommon.h"

static int checks = 0, failed = 0;
#define CHECK(c) do { checks++; if (!(c)) { failed++; if (failed < 10) printf("FAILED line %d: %s\n", __LINE__, #c); } } while (0)

// the ticks the playroutine takes: freq moves by speed each tick until it would pass the target (16 bit)
static int replay(int f1, int f2, int speed)
{
  unsigned short freq = (unsigned short)f1, target = (unsigned short)f2;
  int t = 0;
  if (freq == target) return 1;
  while (freq != target && t < 100000)
  {
    if (freq < target)
    {
      freq = (unsigned short)(freq + speed);
      if (freq > target) freq = target;
    }
    else if (freq > target)
    {
      freq = (unsigned short)(freq - speed);
      if (freq < target) freq = target;
    }
    t++;
  }
  return t;
}

int main(void)
{
  int i;
  for (i = 0; i < 200000; i++)
  {
    int f1 = rand() % 0x10000, f2 = rand() % 0x10000, ticks = 1 + rand() % 200;
    int dist = f1 > f2 ? f1 - f2 : f2 - f1;
    int s = glide_speed(f1, f2, ticks);
    CHECK(s >= 1 && s <= GLIDE_MAXSPEED);
    // never later than asked (unless the speed hit its limit), and as late as it can be: not faster than needed
    if (dist <= GLIDE_MAXSPEED * ticks)
    {
      CHECK(glide_ticks(f1, f2, s) <= ticks);
      if (s > 1) CHECK(glide_ticks(f1, f2, s - 1) > ticks);
    }
    // the model agrees with the playroutine (not for slides that would wrap past 16 bit, which cannot happen
    // between two notes)
    if ((f1 + s < 0x10000) && (f2 + s < 0x10000) && (f1 - s >= 0) && (f2 - s >= 0))
      CHECK(replay(f1, f2, s) == glide_ticks(f1, f2, s));
  }
  // the rows: a slide that is made to fit in the rows it has, is never cut off
  for (i = 0; i < 100000; i++)
  {
    unsigned char pat[64 * 4], before[64 * 4];
    int len = 4 + rand() % 60, r, row, tpr = 2 + rand() % 10, room, ticks, speed, rows, f1, f2, olddata = rand() % 4 ? 0 : 7;
    for (r = 0; r < 64; r++)
    {
      int k = rand() % 10;
      pat[r * 4] = k == 0 ? FIRSTNOTE + rand() % 40 : (k == 1 ? REST : (k == 2 ? KEYOFF : REST));
      pat[r * 4 + 1] = rand() % 4 ? 0 : 1 + rand() % 5;
      pat[r * 4 + 2] = (rand() % 12) ? 0 : 1 + rand() % 15;
      pat[r * 4 + 3] = pat[r * 4 + 2] ? rand() % 256 : 0;
    }
    row = rand() % len;
    pat[row * 4] = FIRSTNOTE + 12;
    pat[row * 4 + 2] = olddata ? CMD_TONEPORTA : 0;
    pat[row * 4 + 3] = olddata ? olddata : 0;
    if (olddata)                                              // an earlier fill of the same glide
    {
      int n = rand() % 4;
      for (r = row + 1; (n > 0) && (r < len) && (pat[r * 4] > LASTNOTE) && (!pat[r * 4 + 2]); r++, n--)
      { pat[r * 4 + 2] = CMD_TONEPORTA; pat[r * 4 + 3] = olddata; }
    }
    room = glide_room(pat, len, row, tpr, olddata);
    CHECK(room >= tpr);
    CHECK(room <= (len - row) * tpr);
    ticks = 1 + rand() % room;
    f1 = 1000 + rand() % 30000;
    f2 = 1000 + rand() % 30000;
    speed = glide_speed(f1, f2, ticks);
    if (glide_ticks(f1, f2, speed) > ticks) continue;           // speed at its limit
    memcpy(before, pat, sizeof pat);
    rows = glide_fill(pat, len, row, 9, tpr, ticks, olddata);
    CHECK(rows >= 1 && row + rows <= len);
    CHECK(rows == (ticks + tpr - 1) / tpr);
    CHECK(pat[row * 4 + 2] == CMD_TONEPORTA && pat[row * 4 + 3] == 9);
    // replay: a row that does not have the command ends the slide (the playroutine clears it at the start of the row)
    {
      unsigned short freq = (unsigned short)f1, target = (unsigned short)f2;
      int active = 1, rr, t;
      for (rr = row; (rr < len) && active; rr++)
      {
        if ((rr != row) && (pat[rr * 4 + 2] != CMD_TONEPORTA)) active = 0;
        if (!active) break;
        for (t = 0; t < tpr; t++)
        {
          if (freq < target) { freq = (unsigned short)(freq + speed); if (freq > target) freq = target; }
          else if (freq > target) { freq = (unsigned short)(freq - speed); if (freq < target) freq = target; }
        }
        if (freq == target) break;
      }
      CHECK(freq == target);
    }
    // nothing else was touched: only empty rows after the note row, and the old fill, changed
    for (r = 0; r < len; r++)
      if ((r < row) || (r > row + (rows > 1 ? rows - 1 : 0) + 4))
        CHECK(!memcmp(pat + r * 4, before + r * 4, 4));
    for (r = row + 1; r < len && r < row + rows; r++) CHECK(before[r * 4] > LASTNOTE && !(before[r * 4 + 2] && !(olddata && before[r * 4 + 2] == CMD_TONEPORTA && before[r * 4 + 3] == olddata)));
  }
  {
    unsigned char t[8 * 4];
    memset(t, 0, sizeof t);
    CHECK(glide_tempo(t, 7, 6) == 6);
    t[2 * 4 + 2] = CMD_SETTEMPO; t[2 * 4 + 3] = 4;
    CHECK(glide_tempo(t, 1, 6) == 6 && glide_tempo(t, 2, 6) == 4 && glide_tempo(t, 7, 6) == 4);
    t[5 * 4 + 2] = CMD_SETTEMPO; t[5 * 4 + 3] = 0x88;       // this voice only: 8
    CHECK(glide_tempo(t, 7, 6) == 8);
    t[6 * 4 + 2] = CMD_SETTEMPO; t[6 * 4 + 3] = 1;          // funktempo: not a number of ticks
    CHECK(glide_tempo(t, 7, 6) == 8);
  }
  CHECK(glide_speed(1000, 1000, 10) == 1);
  CHECK(glide_ticks(1000, 1000, 5) == 1);
  CHECK(glide_speed(0, 0xffff, 1) == GLIDE_MAXSPEED);
  CHECK(glide_speed(100, 200, 0) == 100);
  printf("%d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
