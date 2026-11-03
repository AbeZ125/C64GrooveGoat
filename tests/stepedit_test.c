// Tests of the step editor model: the rows generated from a model, replayed with the table summaries (which follow
// the playroutine), must give back the model tick by tick
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gstepedit.h"
#include "../src/gsummary.h"
#include "../src/gwsummary.h"
#include "../src/gfsummary.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 20) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int pitchbyte(const WAVESUM *s, int t)
{
  int p = s->pitch[t];
  if (s->pitchabs[t]) return 0x80 | p;
  return p >= 0 ? p : p + 128;
}

int main(void)
{
  static unsigned char l[255], r[255], gl[300], gr[300];
  static STEPMODEL m, m2;
  static WAVESUM ws;
  static PULSESUM ps;
  static FILTSUM fs;
  int it, i, start, nrows;

  srand(7);
  for (it = 0; it < 30000; it++)
  {
    int mode = it % 3;
    step_default(&m, mode);
    m.n = 1 + rand() % STEP_MAX;
    for (i = 0; i < m.n; i++)
    {
      int shapes = (rand() % 15 + 1) << 4;
      if (shapes >= 0xe0) shapes = 0x40;
      m.wave[i] = shapes | (rand() % 16);
      if (it % 3 == 0 && i) { m.wave[i] = m.wave[i - 1]; }
      m.note[i] = rand() % 3 ? (rand() % 0x5f) : (0x80 | (1 + rand() % 90));
      if (it % 3 == 0 && i && rand() % 2) m.note[i] = m.note[i - 1];
      m.cutoff[i] = rand() % 3 ? rand() % 256 : (i ? (m.cutoff[i - 1] + 4 * (rand() % 5)) & 0xff : 0x40);
      if (it % 5 == 0 && i > 1) m.cutoff[i] = (2 * m.cutoff[i - 1] - m.cutoff[i - 2]) & 0xff;
      m.width[i] = rand() % 4 ? (rand() % 4096) : (i ? (m.width[i - 1] + 8 * (rand() % 5)) & 0xfff : 0x100);
      if (it % 4 == 0 && i > 1) m.width[i] = (2 * m.width[i - 1] - m.width[i - 2]) & 0xfff;   // linear runs
    }
    m.loop = rand() % 3 ? 0 : 1 + rand() % m.n;
    m.ftype = (rand() % 7 + 1) << 4;
    m.fres = rand() % 16;
    m.fvoices = rand() % 16;
    start = 1 + rand() % 40;
    memset(l, 0, sizeof l); memset(r, 0, sizeof r);
    nrows = step_generate(&m, gl, gr, start);
    CHECK(nrows >= 1 && nrows <= 2 * STEP_MAX + 2);
    if (nrows > 256 - start) { CHECK(!step_write(l, r, start, gl, gr, nrows)); continue; }   // too many rows for the table: refused, nothing changed
    CHECK(step_write(l, r, start, gl, gr, nrows));
    if (mode == STEP_WAVE)
    {
      summarize_wave(&ws, l, r, start, 50);
      for (i = 0; i < ws.ticks; i++)
      {
        int idx = i < m.n ? i : (m.loop ? (m.loop - 1) + (i - (m.loop - 1)) % (m.n - (m.loop - 1)) : m.n - 1);
        CHECK(ws.wave[i] == m.wave[idx] || ((m.wave[idx] & 0xf0) == 0 && ws.wave[i] == (m.wave[idx] & 0xf)));
        CHECK(pitchbyte(&ws, i) == m.note[idx]);
      }
      CHECK(ws.ticks >= (m.loop ? 1 : m.n));
      CHECK(m.loop ? ws.looped : (ws.stopped && ws.stoptick == m.n));
    }
    else if (mode == STEP_FILTER)
    {
      summarize_filter(&fs, l, r, start, 50);
      for (i = 0; i < fs.ticks; i++)
      {
        int idx = i < m.n ? i : (m.loop ? (m.loop - 1) + (i - (m.loop - 1)) % (m.n - (m.loop - 1)) : m.n - 1);
        CHECK(fs.cutoff[i] == m.cutoff[idx]);
        CHECK(fs.type[i] == (m.ftype & 0x70));
        CHECK(fs.ctrl[i] == ((m.fres << 4) | m.fvoices));
      }
      CHECK(m.loop ? fs.looped : (fs.stopped && fs.stoptick == m.n));
    }
    else
    {
      summarize_pulse(&ps, l, r, start, 50);
      for (i = 0; i < ps.ticks; i++)
      {
        int idx = i < m.n ? i : (m.loop ? (m.loop - 1) + (i - (m.loop - 1)) % (m.n - (m.loop - 1)) : m.n - 1);
        CHECK(ps.width[i] == m.width[idx]);
      }
      CHECK(m.loop ? ps.looped : (ps.stopped && ps.stoptick == m.n));
    }
    // and back: importing what was written gives the same ticks
    if (step_import(&m2, mode, l, r, start) == STEP_OK)
    {
      CHECK(m2.n == m.n || m.loop);
      for (i = 0; i < m2.n && i < m.n; i++)
        if (mode == STEP_PULSE) CHECK(m2.width[i] == m.width[i]);
        else if (mode == STEP_FILTER) CHECK(m2.cutoff[i] == m.cutoff[i] && m2.ftype == (m.ftype & 0x70) && m2.fres == m.fres && m2.fvoices == m.fvoices);
    }
  }

  // capacity: a program followed by another program does not grow into it
  memset(l, 0, sizeof l); memset(r, 0, sizeof r);
  l[0] = 0x41; l[1] = 0xff; r[1] = 0; l[2] = 0x11; l[3] = 0xff;
  CHECK(step_capacity(l, r, 1) == 2);
  CHECK(step_capacity(l, r, 5) == 251);           // empty rows to the end of the table
  step_default(&m, STEP_WAVE); m.n = 3; m.wave[1] = 0x11; m.wave[2] = 0x81;
  nrows = step_generate(&m, gl, gr, 1);
  CHECK(nrows > 2 && !step_write(l, r, 1, gl, gr, nrows));
  CHECK(l[2] == 0x11);                            // nothing was changed

  // drawing: random strokes always give a program that reads back as drawn, and snapping holds
  for (it = 0; it < 30000; it++)
  {
    int mode = it % 3, a, b, fine = rand() & 1;
    double va = (rand() % 1001) / 1000.0, vb = (rand() % 1001) / 1000.0;
    step_default(&m, mode);
    m.n = 1 + rand() % STEP_MAX;
    for (i = 0; i < m.n; i++)
    {
      m.wave[i] = 0x41; m.note[i] = rand() % 3 ? rand() % 0x30 : 0x80 | (1 + rand() % 90);
      m.width[i] = rand() % 4096; m.cutoff[i] = rand() % 256;
    }
    a = rand() % m.n; b = rand() % m.n;
    step_paint(&m, a, va, b, vb, fine);
    for (i = 0; i < m.n; i++)
    {
      double g = step_padget(&m, i);
      CHECK(mode == STEP_WAVE && m.note[i] >= 0x81 ? g < 0 : (g >= 0 && g <= 1));
      CHECK(m.width[i] >= 0 && m.width[i] <= 4095 && m.cutoff[i] >= 0 && m.cutoff[i] <= 255);
      if ((mode == STEP_WAVE) && (i >= (a < b ? a : b)) && (i <= (a > b ? a : b)) && (m.note[i] < 0x81))
        CHECK(m.note[i] <= 24 || (m.note[i] >= 128 - 24 && m.note[i] < 128));
    }
    // the end points of the stroke are what was asked for (to the step)
    {
      double e = step_padget(&m, b);
      double tol = mode == STEP_WAVE ? 0.011 : (fine ? 0.01 : 0.04);
      if (e >= 0) CHECK(e - vb < tol && vb - e < tol);
    }
    nrows = step_generate(&m, gl, gr, 1);
    memset(l, 0, sizeof l); memset(r, 0, sizeof r);
    if (nrows > 255) continue;                      // does not fit in a table (the editor says so)
    CHECK(step_write(l, r, 1, gl, gr, nrows));
    CHECK(step_import(&m2, mode, l, r, 1) == STEP_OK);
    if (mode == STEP_PULSE) for (i = 0; i < m.n; i++) CHECK(m2.width[i] == m.width[i]);
    if (mode == STEP_FILTER) for (i = 0; i < m.n; i++) CHECK(m2.cutoff[i] == m.cutoff[i]);
  }
  // snapping, exactly
  step_default(&m, STEP_PULSE); m.n = 2;
  step_padset(&m, 0, 0.5, 0); CHECK(m.width[0] == 2048);
  step_padset(&m, 0, 1.0, 0); CHECK(m.width[0] == 4095);
  step_padset(&m, 0, 0.0, 0); CHECK(m.width[0] == 0);
  step_padset(&m, 1, 7.0, 1); CHECK(m.width[1] == 4095);
  step_default(&m, STEP_WAVE); m.n = 2; m.note[1] = 0x80 | 48;
  step_padset(&m, 0, 0.5, 0); CHECK(m.note[0] == 0);
  step_padset(&m, 0, 1.0, 0); CHECK(m.note[0] == 24);
  step_padset(&m, 0, 0.0, 0); CHECK(m.note[0] == 128 - 24);
  step_padset(&m, 1, 0.0, 0); CHECK(m.note[1] == (0x80 | 48));   // absolute notes are left alone
  CHECK(step_padget(&m, 1) < 0);
  step_default(&m, STEP_FILTER); m.n = 1;
  step_padset(&m, 0, 1.0, 0); CHECK(m.cutoff[0] == 255);
  step_padset(&m, 0, 0.5, 0); CHECK(m.cutoff[0] == 128);

  // arpeggios: made of presets or typed text, they play back as the intervals in the right order, and loop
  for (it = 0; it < 20000; it++)
  {
    int semis[ARP_MAXNOTES], cnt, hold = 1 + rand() % 6, ping = rand() & 1, ok, ticks, wv = 0x41 + (rand() % 4) * 0x10, want[STEP_MAX], nw2 = 0, k, j;
    if (it % 2) cnt = step_arp_preset(rand() % ARP_PRESETS, semis);
    else { cnt = 1 + rand() % 8; for (i = 0; i < cnt; i++) semis[i] = rand() % 60 - 20; }
    step_default(&m, STEP_WAVE);
    ticks = step_arp_ticks(cnt, hold, ping);
    ok = step_arp(&m, semis, cnt, hold, ping, wv);
    CHECK(ok == (ticks <= STEP_MAX && step_wavevalid(wv)));
    if (!ok) { CHECK(m.n == 1 && m.loop == 0); continue; }
    CHECK(m.n == ticks && m.loop == 1);
    for (i = 0; i < cnt; i++) for (k = 0; k < hold; k++) want[nw2++] = semis[i];
    if (ping && cnt > 2) for (i = cnt - 2; i >= 1; i--) for (k = 0; k < hold; k++) want[nw2++] = semis[i];
    CHECK(nw2 == m.n);
    memset(l, 0, sizeof l); memset(r, 0, sizeof r);
    start = 1 + rand() % 30;
    nrows = step_generate(&m, gl, gr, start);
    CHECK(step_write(l, r, start, gl, gr, nrows));
    summarize_wave(&ws, l, r, start, 50);
    CHECK(ws.looped);
    for (j = 0; j < 3 * nw2 && j < 150; j++)       // three rounds of the loop
    {
      int pb = pitchbyte(&ws, j < ws.ticks ? j : ((j - ws.loopstart) % ws.looplen) + ws.loopstart);
      int sv = want[j % nw2];
      CHECK(pb == (sv >= 0 ? sv : sv + 128));
    }
  }
  {
    int s[8];
    CHECK(step_arp_parse("0 4 7", s, 8) == 3 && s[0] == 0 && s[1] == 4 && s[2] == 7);
    CHECK(step_arp_parse(" 0,3, 7 ,12 ", s, 8) == 4 && s[3] == 12);
    CHECK(step_arp_parse("0 -5 4", s, 8) == 3 && s[1] == -5);
    CHECK(step_arp_parse("0-4-7", s, 8) == -1);
    CHECK(step_arp_parse("", s, 8) == -1 && step_arp_parse("a", s, 8) == -1 && step_arp_parse("0 4 x", s, 8) == -1);
    CHECK(step_arp_parse("0 4 7 9 11 12", s, 5) == -1);
    CHECK(step_arp_parse("0 200", s, 8) == -1 && step_arp_parse("-40", s, 8) == -1);
    CHECK(step_arp_preset(0, s) == 3 && s[1] == 4 && s[2] == 7);   // MAJ
    CHECK(step_arp_preset(1, s) == 3 && s[1] == 3 && s[2] == 7);   // MIN
    CHECK(step_arp_ticks(3, 1, 0) == 3 && step_arp_ticks(3, 2, 1) == 8 && step_arp_ticks(2, 3, 1) == 6);
  }

  // programs longer than 32 ticks: they import, stay compact and play back tick by tick
  {
    int t;
    // a 200 tick pulse sweep is one set row, one modulation row and the jump
    step_default(&m, STEP_PULSE); m.n = 200;
    for (t = 0; t < 200; t++) m.width[t] = 0x100 + t * 8;
    nrows = step_generate(&m, gl, gr, 1);
    CHECK(nrows <= 6);
    memset(l, 0, sizeof l); memset(r, 0, sizeof r);
    CHECK(step_write(l, r, 1, gl, gr, nrows));
    CHECK(step_import(&m2, STEP_PULSE, l, r, 1) == STEP_OK && m2.n >= 100);
    // a 150 tick wave program: 8 ticks of noise, then a long held pulse
    step_default(&m, STEP_WAVE); m.n = 150;
    for (t = 0; t < 150; t++) { m.wave[t] = t < 8 ? 0x81 : 0x41; m.note[t] = 0; }
    nrows = step_generate(&m, gl, gr, 1);
    CHECK(nrows <= 2 * 9 + 2);
    memset(l, 0, sizeof l); memset(r, 0, sizeof r);
    CHECK(step_write(l, r, 1, gl, gr, nrows));
    CHECK(step_import(&m2, STEP_WAVE, l, r, 1) == STEP_OK);
    CHECK(m2.n == 150);
    for (t = 0; t < 150; t++) CHECK(m2.wave[t] == m.wave[t] && m2.note[t] == m.note[t]);
    // an arpeggio of 4 notes up and down, 8 ticks per note: 48 ticks
    {
      int semis[4] = {0, 4, 7, 12};
      step_default(&m, STEP_WAVE);
      CHECK(step_arp(&m, semis, 4, 8, 1, 0x41) && m.n == 48);
    }
    CHECK(STEP_MAX >= 200);
  }

  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
