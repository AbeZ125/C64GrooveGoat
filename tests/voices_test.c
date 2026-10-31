// Tests for the voice bookkeeping of held keys
#include <stdio.h>
#include <stdlib.h>
#include "../src/gvoices.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static int distinct(const VOICES *v)
{
  int i, j;
  for (i = 0; i < v->n; i++)
    for (j = i + 1; j < v->n; j++)
      if ((v->chn[i] == v->chn[j]) || (v->key[i] == v->key[j])) return 0;
  return 1;
}

int main(void)
{
  VOICES v;
  int i, s, a, b, c;

  // a chord uses three channels, the first key the home channel, the others the ones after it
  voices_init(&v);
  CHECK(voices_on(&v, 60, 1, 3, 3) == 1, "first key on home");
  CHECK(voices_on(&v, 64, 1, 3, 3) == 2, "second key next channel");
  CHECK(voices_on(&v, 67, 1, 3, 3) == 0, "third key wraps round");
  CHECK(voices_count(&v) == 3, "three held");
  // a fourth key steals the oldest (60 on channel 1)
  CHECK(voices_on(&v, 72, 1, 3, 3) == 1, "fourth key steals the oldest channel");
  CHECK(voices_off(&v, 60) == -1, "the key that lost its channel releases nothing");
  CHECK(voices_off(&v, 72) == 1, "the new key releases it");
  CHECK(voices_off(&v, 64) == 2, "release returns the channel");
  CHECK(voices_off(&v, 64) == -1, "released twice");
  CHECK(voices_count(&v) == 1, "one held");
  // a freed home channel is used again
  CHECK(voices_on(&v, 50, 1, 3, 3) == 1, "home is free again");

  // the same key struck again keeps its channel
  voices_init(&v);
  a = voices_on(&v, 60, 2, 6, 6);
  CHECK(voices_on(&v, 60, 2, 6, 6) == a && voices_count(&v) == 1, "retrigger");

  // while a song plays only the home channel is used: a new key takes it over
  voices_init(&v);
  CHECK(voices_on(&v, 60, 0, 3, 1) == 0, "mono first");
  CHECK(voices_on(&v, 64, 0, 3, 1) == 0, "mono second takes the channel");
  CHECK(voices_off(&v, 60) == -1, "the first key lost it");
  CHECK(voices_off(&v, 64) == 0, "the second releases it");

  // more channels than keys: 2 SIDs
  voices_init(&v);
  for (i = 0; i < 6; i++) CHECK(voices_on(&v, 40 + i, 4, 6, 6) == (4 + i) % 6, "six channels in order");

  // random play: the held keys never share a channel, and every release matches the channel of the last note on
  srand(12345);
  for (s = 0; s < 2000; s++)
  {
    int nchan = 1 + rand() % 12, home = rand() % 14 - 1, span = rand() % 14;
    int owner[16];
    for (i = 0; i < 16; i++) owner[i] = -999;
    voices_init(&v);
    for (i = 0; i < 300; i++)
    {
      int key = rand() % 20 - 1;
      if (rand() % 3)
      {
        int ch = voices_on(&v, key, home, nchan, span);
        CHECK((ch >= 0) && (ch < nchan), "channel in range %d of %d", ch, nchan);
        for (c = 0; c < nchan; c++) if (owner[c] == key) owner[c] = -999;
        owner[ch] = key;
      }
      else
      {
        int ch = voices_off(&v, key);
        if (ch >= 0) { CHECK(owner[ch] == key, "released a channel that is not the key's"); owner[ch] = -999; }
        else for (c = 0; c < nchan; c++) CHECK(owner[c] != key, "key %d owns channel %d but releases none", key, c);
      }
      CHECK(distinct(&v), "keys or channels shared");
      CHECK(voices_count(&v) <= nchan, "more held keys than channels");
      for (b = 0; b < v.n; b++) CHECK(owner[v.chn[b]] == v.key[b], "bookkeeping differs");
    }
  }
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
