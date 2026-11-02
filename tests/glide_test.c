// Tests of the glide speed: replays the playroutine's tone portamento (gplay.c, toneporta) and checks the slide
// arrives when it should.
#include <stdio.h>
#include <stdlib.h>
#include "../src/gglide.h"

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
  CHECK(glide_speed(1000, 1000, 10) == 1);
  CHECK(glide_ticks(1000, 1000, 5) == 1);
  CHECK(glide_speed(0, 0xffff, 1) == GLIDE_MAXSPEED);
  CHECK(glide_speed(100, 200, 0) == 100);
  printf("%d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
