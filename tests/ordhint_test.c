// Tests for the orderlist hint text
#include <stdio.h>
#include <string.h>
#include "../src/gordhint.h"
#include "../src/gcommon.h"

static int fails = 0, checks = 0;
static void expect(const unsigned char *l, int len, int pos, const char *want)
{
  char buf[100];
  order_hint(l, len, pos, buf, sizeof buf);
  checks++;
  if (strcmp(buf, want)) { printf("FAIL pos %d: got \"%s\" want \"%s\"\n", pos, buf, want); fails++; }
}

int main(void)
{
  // 0F 10 RST 01 : the intro example
  unsigned char a[8] = { 0x0f, 0x10, LOOPSONG, 0x01 };
  unsigned char b[12] = { 0x00, REPEAT + 2, 0x05, TRANSUP + 2, TRANSDOWN + 0x0d, REPEAT + 0x0f, 0x06, LOOPSONG, 0x00 };
  unsigned char c[6] = { 0x00, LOOPSONG, 0x07 };
  unsigned char d[6] = { TRANSUP + 1, TRANSDOWN + 0x0f, 0x00, LOOPSONG, 0x00 };
  char buf[100];
  int i, n;

  expect(a, 2, 0, "Plays pattern 0F");
  expect(a, 2, 2, "End of list: loops back to position 01");
  expect(a, 2, 3, "Restart position 01: the loop jumps here");
  expect(a, 2, 4, "");
  expect(a, 2, -1, "");
  expect(b, 7, 1, "R3: the next pattern plays 3 times");
  expect(b, 7, 2, "Plays pattern 05, 3 times in a row");
  expect(b, 7, 3, "+2: next patterns sound 2 half steps up");
  expect(b, 7, 4, "-3: next patterns sound 3 half steps down");
  expect(b, 7, 5, "R0: the next pattern plays 16 times");
  expect(b, 7, 6, "Plays pattern 06, 16 times in a row");
  expect(b, 7, 7, "End of list: loops back to position 00");
  expect(b, 7, 8, "Restart position 00: whole list loops");
  expect(c, 1, 2, "Restart position 07 is past the list");
  expect(d, 3, 0, "+1: next patterns sound 1 half step up");
  expect(d, 3, 1, "-1: next patterns sound 1 half step down");

  // never longer than the 43 characters the display has, never overruns a small buffer
  for (n = 0; n < 6; n++)
    for (i = 0; i < 256; i++)
    {
      unsigned char l[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
      int len = n, p, r;
      l[0] = (unsigned char)i; l[1] = REPEAT + 1; l[len] = LOOPSONG; l[len + 1] = (unsigned char)(i & 7);
      for (p = -1; p <= len + 2; p++)
      {
        r = order_hint(l, len, p, buf, sizeof buf);
        checks++;
        if (r > 43 || r != (int)strlen(buf)) { printf("FAIL length %d for entry %02X pos %d\n", r, i, p); fails++; }
        r = order_hint(l, len, p, buf, 10);
        checks++;
        if (strlen(buf) > 9) { printf("FAIL small buffer\n"); fails++; }
      }
    }
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
