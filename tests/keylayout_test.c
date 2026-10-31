// Tests of the piano layout: every cell belongs to at most one key, black keys are on top, every key can be hit
#include <stdio.h>
#include "../src/gkeylayout.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 20) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
  int k, cx, cy, blacks = 0, whites = 0;

  for (k = 0; k < KEYLAY_KEYS; k++) { if (keylay_isblack(k)) blacks++; else whites++; }
  CHECK(blacks == 13 && whites == 19 && keylay_whites() == 19 && keylay_width() == 75);
  CHECK(keylay_isblack(1) && !keylay_isblack(0) && !keylay_isblack(4) && !keylay_isblack(5) && keylay_isblack(30) && !keylay_isblack(31));

  // brute force: the last key that covers the cell, black keys win
  for (cy = -1; cy <= KEYLAY_ROWS; cy++)
    for (cx = -1; cx <= keylay_width(); cx++)
    {
      int expect = -1;
      for (k = 0; k < KEYLAY_KEYS; k++)
      {
        int x, w, h;
        keylay_keyrect(k, &x, &w, &h);
        if ((cx >= x) && (cx < x + w) && (cy >= 0) && (cy < h))
        {
          if (keylay_isblack(k)) { expect = k; break; }
          if (!keylay_isblack(k)) expect = k;
        }
      }
      if ((cx < 0) || (cy < 0) || (cy >= KEYLAY_ROWS) || (cx >= keylay_width())) expect = -1;
      CHECK(keylay_hit(cx, cy) == expect);
    }

  // every key can be hit in its middle, and the keys keep their order from left to right
  for (k = 0; k < KEYLAY_KEYS; k++)
  {
    int x, w, h;
    keylay_keyrect(k, &x, &w, &h);
    CHECK(keylay_hit(x + 1, 0) == k);
    CHECK(x >= 0 && x + w <= keylay_width() + 1);
    if (!keylay_isblack(k)) CHECK(keylay_hit(x + 1, KEYLAY_ROWS - 1) == k);
    else CHECK(keylay_hit(x + 1, KEYLAY_ROWS - 1) != k);
    if (k > 0)
    {
      int px, pw, ph;
      keylay_keyrect(k - 1, &px, &pw, &ph);
      CHECK(x > px);
    }
  }
  // a black key sits between its two white neighbours
  for (k = 1; k < KEYLAY_KEYS - 1; k++)
    if (keylay_isblack(k))
    {
      int bx, bw, bh, lx, lw, lh, rx, rw, rh;
      keylay_keyrect(k, &bx, &bw, &bh);
      keylay_keyrect(k - 1, &lx, &lw, &lh);
      keylay_keyrect(k + 1, &rx, &rw, &rh);
      CHECK(!keylay_isblack(k - 1) && !keylay_isblack(k + 1));
      CHECK(bx == lx + 2 && bx + bw - 1 == rx);
    }
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
