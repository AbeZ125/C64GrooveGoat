// Tests of the wavetable summary: the manual's examples, and random tables (run with sanitizers) for crashes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gwsummary.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void load(unsigned char *l, unsigned char *r, const unsigned char *rows, int n)
{
  int i;
  memset(l, 0, 255); memset(r, 0, 255);
  for (i = 0; i < n; i++) { l[i] = rows[i * 2]; r[i] = rows[i * 2 + 1]; }
}

int main(void)
{
  static WAVESUM s;
  unsigned char l[255], r[255];
  char b[64];
  int i;

  // flute: pulse on the first frame, then triangle
  { const unsigned char t[] = {0x41,0x00, 0x11,0x00, 0xff,0x00}; load(l, r, t, 3); }
  summarize_wave(&s, l, r, 1, 50);
  CHECK(s.stopped && !s.looped && s.stoptick == 2);
  CHECK(s.wave[0] == 0x41 && s.wave[1] == 0x11);
  CHECK(strcmp(s.waves, "pulse, then tri") == 0);

  // snare with absolute notes
  { const unsigned char t[] = {0x81,0xd0, 0x41,0xaa, 0x41,0xa4, 0x80,0xd4, 0x80,0xd1, 0xff,0x00}; load(l, r, t, 6); }
  summarize_wave(&s, l, r, 1, 50);
  CHECK(s.stopped && s.stoptick == 5);
  CHECK(s.wave[0] == 0x81 && s.pitchabs[0] && s.pitch[0] == 0x50);
  CHECK(s.pitch[1] == 0x2a && s.pitch[2] == 0x24 && s.pitch[3] == 0x54 && s.pitch[4] == 0x51);

  // looping 4 note arpeggio
  { const unsigned char t[] = {0x41,0x00, 0x00,0x04, 0x00,0x07, 0x00,0x0c, 0x00,0x00, 0xff,0x02}; load(l, r, t, 6); }
  summarize_wave(&s, l, r, 1, 50);
  CHECK(s.looped && s.looplen == 4 && s.loopstart == 1);
  CHECK(strncmp(s.notes, "arpeggio", 8) == 0);
  CHECK(s.pitch[1] == 4 && s.pitch[2] == 7 && s.pitch[3] == 12 && s.pitch[4] == 0);

  // delay: 41, wait 3, 11
  { const unsigned char t[] = {0x41,0x00, 0x03,0x00, 0x11,0x00, 0xff,0x00}; load(l, r, t, 4); }
  summarize_wave(&s, l, r, 1, 50);
  CHECK(s.wave[0] == 0x41 && s.wave[4] == 0x41 && s.wave[5] == 0x11);

  // relative note down
  CHECK(wave_rowtext(b, sizeof b, 1, 0x41, 0x7f) == PK_SET && strstr(b, "note -1"));
  CHECK(wave_rowtext(b, sizeof b, 1, 0x21, 0x81) == PK_SET && strstr(b, "note C#0"));
  CHECK(wave_rowtext(b, sizeof b, 1, 0xff, 0x00) == PK_STOP);
  CHECK(wave_rowtext(b, sizeof b, 1, 0xf7, 0x41) == PK_MODULATE && strstr(b, "set waveform"));

  // E9 = test bit + gate
  wave_name(b, sizeof b, 0x09); CHECK(strstr(b, "test") != NULL);
  wave_name(b, sizeof b, 0x41); CHECK(strcmp(b, "pulse") == 0);
  wave_name(b, sizeof b, 0x30); CHECK(strstr(b, "tri+saw") && strstr(b, "gate off"));

  // empty program, and a row without any end
  summarize_wave(&s, l, r, 0, 50); CHECK(!s.startrow);
  memset(l, 0x41, 255); memset(r, 0, 255);
  summarize_wave(&s, l, r, 1, 50); CHECK(s.stopped || s.looped);

  // random tables: must not crash (use -fsanitize=address,undefined)
  srand(1);
  for (i = 0; i < 20000; i++)
  {
    int k;
    for (k = 0; k < 255; k++) { l[k] = rand() % 4 ? (rand() % 256) : 0xff; r[k] = rand() % 256; if (rand() % 3 == 0) l[k] = rand() % 16; }
    summarize_wave(&s, l, r, 1 + rand() % 255, 50);
    CHECK(s.ticks <= WSUM_MAXTICKS);
  }
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
