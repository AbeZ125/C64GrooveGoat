// Tests for the table usage marks
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/ginsuse.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void)
{
  static unsigned char ptr[64][INSUSE_TABLES];
  static INSUSE u;
  char lab[8];
  int out[80], i, t, r, s, n;

  // nothing uses anything
  insuse_build(&u, 64, ptr, 1);
  CHECK(insuse_label(&u, 0, 0, lab) == 0 && lab[0] == 0, "empty");

  // one instrument: its number; the current one is flagged
  ptr[5][0] = 3;                          // instrument 05 starts at wave row 03 (row index 2)
  insuse_build(&u, 64, ptr, 7);
  CHECK(insuse_label(&u, 0, 2, lab) == 1 && !strcmp(lab, "05"), "one user %s", lab);
  insuse_build(&u, 64, ptr, 5);
  CHECK(insuse_label(&u, 0, 2, lab) == 2 && !strcmp(lab, "05"), "current user");
  CHECK(insuse_label(&u, 1, 2, lab) == 0, "other table");

  // several: a count, then "x+" from ten
  for (i = 1; i <= 3; i++) ptr[i][1] = 10;
  insuse_build(&u, 64, ptr, 2);
  CHECK(insuse_label(&u, 1, 9, lab) == 2 && !strcmp(lab, "x3"), "three: %s", lab);
  for (i = 1; i <= 12; i++) ptr[i][2] = 20;
  insuse_build(&u, 64, ptr, 40);
  CHECK(insuse_label(&u, 2, 19, lab) == 1 && !strcmp(lab, "x+"), "twelve: %s", lab);
  ptr[0][3] = 5;                          // instrument 0 is not counted
  insuse_build(&u, 64, ptr, 1);
  CHECK(insuse_label(&u, 3, 4, lab) == 0, "instrument 0");

  // the highest row, and out of range
  ptr[9][0] = 255;
  insuse_build(&u, 64, ptr, 9);
  CHECK(insuse_label(&u, 0, 254, lab) == 2 && !strcmp(lab, "09"), "row 255");
  CHECK(insuse_label(&u, 0, 255, lab) == 0 && insuse_label(&u, 4, 0, lab) == 0 && insuse_label(&u, 0, -1, lab) == 0, "range");
  CHECK(insuse_list(64, ptr, 0, 254, out, 80) == 1 && out[0] == 9, "list");
  CHECK(insuse_list(64, ptr, 9, 0, out, 80) == 0, "list range");

  // random: the label agrees with the list
  srand(7);
  for (s = 0; s < 300; s++)
  {
    int cur = rand() % 64;
    memset(ptr, 0, sizeof ptr);
    for (i = 0; i < 64; i++) for (t = 0; t < INSUSE_TABLES; t++) if (rand() % 3) ptr[i][t] = rand() % 12;
    insuse_build(&u, 64, ptr, cur);
    for (t = 0; t < INSUSE_TABLES; t++)
      for (r = 0; r < 14; r++)
      {
        int k = insuse_label(&u, t, r, lab);
        n = insuse_list(64, ptr, t, r, out, 80);
        CHECK((n > 0) == (k > 0), "label/list disagree");
        if (n == 1) { char e[8]; snprintf(e, sizeof e, "%02X", out[0]); CHECK(!strcmp(lab, e), "label %s list %s", lab, e); }
        if (n > 1 && n < 10) { char e[8]; snprintf(e, sizeof e, "x%d", n); CHECK(!strcmp(lab, e), "count %s vs %s", lab, e); }
        if (n >= 10) CHECK(!strcmp(lab, "x+"), "many");
        { int has = 0; for (i = 0; i < n && i < 80; i++) if (out[i] == cur) has = 1; CHECK((k == 2) == (has != 0), "current flag"); }
      }
  }
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
