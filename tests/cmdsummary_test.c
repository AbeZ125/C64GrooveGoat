// Tests of the pattern command summary
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gcmdsummary.h"
#include "../src/gcommon.h"

float adsr_attackms(int a) { static const float t[16] = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000}; return t[a & 15]; }
float adsr_decayms(int d) { return adsr_attackms(d) * 3; }

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
  static unsigned char l[4][255], r[4][255];
  CMDTABLES t;
  CMDSUM s;
  int k, i, c, d, period;
  double cents;

  for (k = 0; k < 4; k++) { t.l[k] = l[k]; t.r[k] = r[k]; }
  l[STBL][0] = 0x08; r[STBL][0] = 0x10;       // row 1: vibrato speed 8 depth 16
  l[STBL][1] = 0x01; r[STBL][1] = 0x00;       // row 2: portamento $0100
  l[STBL][2] = 0x82; r[STBL][2] = 0x01;       // row 3: note independent
  l[STBL][3] = 0x04; r[STBL][3] = 0x02;       // row 4: funktempo 4/2

  summarize_command(&s, 4, 1, &t, 50, 985248);
  CHECK(strstr(s.text, "vibrato") && strstr(s.text, "Hz"));
  sum_vibrato(8, 16, 985248, &period, &cents);
  CHECK(period > 0 && cents > 0);
  summarize_command(&s, 1, 2, &t, 50, 985248);
  CHECK(strstr(s.detail[0], "$0100"));
  summarize_command(&s, 2, 3, &t, 50, 985248);
  CHECK(strstr(s.detail[0], "1/2"));
  summarize_command(&s, 3, 0, &t, 50, 985248);
  CHECK(strstr(s.text, "tie note"));
  summarize_command(&s, 0xe, 4, &t, 50, 985248);
  CHECK(strstr(s.text, "tempo 4 and 2"));
  summarize_command(&s, 0xf, 6, &t, 50, 985248);
  CHECK(strstr(s.text, "6 frames per row") && strstr(s.text, "120 ms") && strstr(s.text, "all voices"));
  summarize_command(&s, 0xf, 0x86, &t, 50, 985248);
  CHECK(strstr(s.text, "this voice only"));
  summarize_command(&s, 5, 0x09, &t, 50, 985248);
  CHECK(strstr(s.text, "attack 0 (2 ms)") && strstr(s.text, "decay 9"));
  summarize_command(&s, 0xb, 0xf1, &t, 50, 985248);
  CHECK(strstr(s.text, "resonance F") && strstr(s.text, "voices 1"));
  summarize_command(&s, 0xd, 0x0f, &t, 50, 985248);
  CHECK(strstr(s.text, "100%"));
  summarize_command(&s, 0x9, 0, &t, 50, 985248);
  CHECK(strstr(s.text, "stop the pulsetable"));

  // the vibrato curve against a copy of vibrato() of the playroutine (gplay.c) with its 16-bit frequency
  for (k = 0; k < 400; k++)
  {
    int xx = rand() % 0x80, yy = rand() % 256, n;
    double curve[300];
    unsigned short freq = 0x4000;
    unsigned char vibtime = 0;
    sum_vibrato_curve(xx, yy, 985248, curve, 300);
    for (n = 0; n < 300; n++)
    {
      unsigned short speed = (xx || yy) ? yy : 0;
      if ((vibtime < 0x80) && (vibtime > xx)) vibtime ^= 0xff;
      vibtime += 0x02;
      if (vibtime & 0x01) freq -= speed; else freq += speed;
      CHECK((unsigned short)(0x4000 + (long)curve[n]) == freq);
    }
  }

  // every command with every data byte on random tables: no crash (run with sanitizers)
  srand(3);
  for (i = 0; i < 300; i++)
  {
    for (k = 0; k < 4; k++) for (c = 0; c < 255; c++) { l[k][c] = rand(); r[k][c] = rand(); }
    for (c = 0; c < 16; c++)
      for (d = 0; d < 256; d += 1 + rand() % 17)
      {
        summarize_command(&s, c, d, &t, 50, 985248);
        CHECK(s.text[0] != 0);
      }
  }
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
