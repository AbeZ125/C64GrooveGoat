//
// C64 GrooveGoat - the on-screen piano: key layout
//

#include "gkeylayout.h"

int keylay_isblack(int key)
{
  int n = key % 12;
  return (n == 1) || (n == 3) || (n == 6) || (n == 8) || (n == 10);
}

int keylay_whiteindex(int key)
{
  int i, w = -1;
  for (i = 0; i <= key; i++)
    if (!keylay_isblack(i)) w++;
  return w;
}

int keylay_whites(void)
{
  return keylay_whiteindex(KEYLAY_KEYS - 1) + 1;
}

int keylay_width(void)
{
  return keylay_whites() * KEYLAY_WHITEW - 1;
}

void keylay_keyrect(int key, int *x, int *w, int *h)
{
  int wi = keylay_whiteindex(key);
  *w = 3;
  if (keylay_isblack(key))
  {
    *x = wi * KEYLAY_WHITEW + 2;
    *h = KEYLAY_BLACKROWS;
  }
  else
  {
    *x = wi * KEYLAY_WHITEW;
    *h = KEYLAY_ROWS;
  }
}

int keylay_hit(int cx, int cy)
{
  int k, white = -1;
  if ((cx < 0) || (cy < 0) || (cy >= KEYLAY_ROWS) || (cx >= keylay_width())) return -1;
  for (k = 0; k < KEYLAY_KEYS; k++)
  {
    int x, w, h;
    keylay_keyrect(k, &x, &w, &h);
    if ((cx >= x) && (cx < x + w) && (cy < h))
    {
      if (keylay_isblack(k)) return k;       // a black key is on top
      white = k;
    }
  }
  return white;
}
