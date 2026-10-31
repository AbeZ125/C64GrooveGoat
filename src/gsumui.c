//
// C64 GrooveGoat - table summaries on the screen: the one-line summary under the tables and the summary
// window (SHIFT+I in the table editor). The analysis itself is in gsummary.c.
//

#include "goattrk2.h"
#include "gsummary.h"
#include "gwsummary.h"
#include "gcmdsummary.h"
#include "gfsummary.h"
#include "gsumui.h"

static float shapeduty = 0.5f;    // pulse width as a fraction of the period (the output is low until then)
static void drawshapes(unsigned char *buf, int w, int h, int x0, int x1, int top, int bot, int shapes, int periods);
static const WAVESUM *wavesummary(int start);
#define WAVE_CANVAS 3             // the snapshots of the wave through the pulse (window)
#define SUM_CANVAS 1              // canvas 0 is the ADSR graph of the instrument panel, 1 is used by the modal windows

// Ticks per second the song runs at: 50 (PAL) or 60 (NTSC), times the speed multiplier (0 means half speed)
static double tickrate(void)
{
  double base = editorInfo.ntsc ? 60.0 : 50.0;
  return editorInfo.multiplier ? base * editorInfo.multiplier : base / 2;
}

// The voice that is playing the instrument being edited, or -1 when nothing is playing it. The screen reads the
// player's state while it runs: the values are single bytes or words, a torn read only shows one frame wrong.
static int livechannel(void)
{
  int c;
  if (!isplaying()) return -1;
  if ((chn[editorInfo.epchn].instr == editorInfo.einum) && (chn[editorInfo.epchn].gate || chn[editorInfo.epchn].wave & 1)) return editorInfo.epchn;
  for (c = 0; c < MAX_CHN; c++)
    if ((chn[c].instr == editorInfo.einum) && (!chn[c].mute) && (chn[c].wave & 1)) return c;
  return -1;
}

// The tick of a pulse replay that the playing voice is on (-1 if it cannot be told)
static int livepulsetick(const PULSESUM *s, int c)
{
  int row = chn[c].ptr[PTBL], first = -1, t;
  if (!row) return -1;
  if (ltable[PTBL][row - 1] == 0xff) row = rtable[PTBL][row - 1];
  if (!row) return -1;
  for (t = 0; t < s->ticks; t++)
    if (s->rowof[t] == row) { first = t; break; }
  if (first < 0) return -1;
  if (chn[c].pulsetime && ltable[PTBL][row - 1] < 0x80)
  {
    int elapsed = ltable[PTBL][row - 1] - chn[c].pulsetime;
    if (elapsed > 0) first += elapsed;
  }
  return first < s->ticks ? first : s->ticks - 1;
}

// The pulsetable program the cursor is in: it starts on the row after the last jump or stop above the cursor
static int programstart(int table, int pos)
{
  int k;
  for (k = pos - 1; k >= 0; k--)
    if (ltable[table][k] == 0xff) return k + 2;
  return 1;
}

// Which program to describe: the one the cursor is in while the pulsetable is being edited, otherwise the one
// the current instrument starts. Returns the start row (1 based, 0 = none).
static int currentprogram(int table, int *cursorrow)
{
  *cursorrow = -1;
  if ((editorInfo.editmode == EDIT_TABLES) && (editorInfo.etnum == table))
  {
    *cursorrow = editorInfo.etpos + 1;
    return programstart(table, editorInfo.etpos);
  }
  return instr[editorInfo.einum].ptr[table];
}

// The summaries are cached: replaying a program costs a little and the screen is redrawn many times a second
static PULSESUM cached;
static unsigned char cachel[MAX_TABLELEN], cacher[MAX_TABLELEN];
static int cachestart = -1;
static double cacherate;

static const PULSESUM *pulsesummary(int start)
{
  double rate = tickrate();
  if ((cachestart != start) || (cacherate != rate) || memcmp(cachel, ltable[PTBL], MAX_TABLELEN) || memcmp(cacher, rtable[PTBL], MAX_TABLELEN))
  {
    summarize_pulse(&cached, ltable[PTBL], rtable[PTBL], start, rate);
    memcpy(cachel, ltable[PTBL], MAX_TABLELEN);
    memcpy(cacher, rtable[PTBL], MAX_TABLELEN);
    cachestart = start;
    cacherate = rate;
  }
  return &cached;
}

// ---------------------------------------------------------------------------------------------------------
// The line under the tables
// ---------------------------------------------------------------------------------------------------------

static void drawwavesummaryline(int y);
static void drawfiltersummaryline(int y);
static int drawcommandline(int y);

void drawtablesummaryline(int y)
{
  char buf[80];
  int cursorrow;
  int start;
  if ((editorInfo.editmode == EDIT_TABLES) && (editorInfo.etnum == WTBL))
  {
    drawwavesummaryline(y);
    return;
  }
  if ((editorInfo.editmode == EDIT_TABLES) && (editorInfo.etnum == FTBL))
  {
    drawfiltersummaryline(y);
    return;
  }
  if (drawcommandline(y)) return;
  start = currentprogram(PTBL, &cursorrow);
  int width = RP_W - 2;
  fillarea(RP_X, y, RP_W, 1, MKCOL(8, C_PANEL), ' ');
  if ((cursorrow > 0) && (!ltable[PTBL][cursorrow - 1]) && (!rtable[PTBL][cursorrow - 1]))
    strcpy(buf, "PULSE: empty row");
  else
  {
    const PULSESUM *s = pulsesummary(start);
    if (start) snprintf(buf, sizeof buf, "PULSE %02X: %s", start, s->oneline);
    else snprintf(buf, sizeof buf, "PULSE: %s", s->oneline);
  }
  buf[width] = 0;
  printtext(RP_X + 1, y, MKCOL(editorInfo.editmode == EDIT_TABLES && editorInfo.etnum == PTBL ? 15 : 7, C_PANEL), buf);
}

// ---------------------------------------------------------------------------------------------------------
// The graph: the pulse width over time
// ---------------------------------------------------------------------------------------------------------

static inline void plot(unsigned char *buf, int w, int h, int x, int y, int a)
{
  unsigned char *p;
  if ((x < 0) || (y < 0) || (x >= w) || (y >= h) || (a <= 0)) return;
  if (a > 255) a = 255;
  p = buf + y * CANVAS_STRIDE + x;
  if (*p < a) *p = a;
}

// Draws the width of the replayed program over time, 0% at the bottom and 100% at the top, with the part that
// repeats marked by dotted lines. Returns the number of ticks the graph covers.
typedef struct
{
  const unsigned short *v;        // the value on each tick
  int ticks, maxv;                // number of ticks, the value at the top of the graph
  int looped, loopstart, looplen;
} CURVE;

static int drawcurve(int id, int x, int y, int cw, int ch, const CURVE *s, int color, int live)
{
  unsigned char *buf = canvas_begin(id, cw, ch);
  int w = cw * 16, h = ch * 32;
  int top = 6, base = h - 7;
  int span = s->ticks;
  int px;
  float prevy = (float)base;

  if (!buf) return 0;
  // Show two repetitions of a repeating program, so that the repetition is plain to see
  if (s->looped)
  {
    span = s->loopstart + 2 * s->looplen;
    if (span > s->ticks) span = s->ticks;
  }
  if (span < 1) span = 1;

  // 0%, 50% and 100% as faint guides
  for (px = 2; px < w - 2; px++)
  {
    plot(buf, w, h, px, base + 1, 70);
    if ((px & 7) < 3) plot(buf, w, h, px, top, 45);
    if ((px & 7) < 2) plot(buf, w, h, px, (top + base) / 2, 30);
  }
  // where the repeating part begins and ends
  if (s->looped)
  {
    int mark[2];
    int m;
    mark[0] = 4 + (w - 8) * s->loopstart / span;
    mark[1] = 4 + (w - 8) * (s->loopstart + s->looplen) / span;
    for (m = 0; m < 2; m++)
      for (px = top; px < base; px++)
        if ((px & 7) < 4) plot(buf, w, h, mark[m], px, 110);
  }

  for (px = 4; px < w - 4; px++)
  {
    int t0 = (px - 4) * span / (w - 8);
    int t1 = (px - 3) * span / (w - 8);
    int wmin = s->maxv, wmax = 0, k;
    float fy0, fy1;
    int y0, y1, py;
    if (t1 <= t0) t1 = t0 + 1;
    if (t1 > s->ticks) t1 = s->ticks;
    if (t0 >= t1) break;
    // the range the width covers in the ticks of this pixel column, joined to the previous column
    for (k = t0; k < t1; k++)
    {
      if (s->v[k] < wmin) wmin = s->v[k];
      if (s->v[k] > wmax) wmax = s->v[k];
    }
    fy0 = base - (wmax / (float)s->maxv) * (base - top);
    fy1 = base - (wmin / (float)s->maxv) * (base - top);
    y0 = (int)floorf(fy0 < prevy ? fy0 : prevy);
    y1 = (int)ceilf(fy1 > prevy ? fy1 : prevy);
    if (px == 4) { y0 = (int)floorf(fy0); y1 = (int)ceilf(fy1); }
    for (py = (int)fy0; py <= base; py++)
      plot(buf, w, h, px, py, 30 + 50 * (base - py) / (base - top + 1));
    for (py = y0 - 2; py <= y1 + 2; py++)
    {
      int dist = py < y0 ? y0 - py : (py > y1 ? py - y1 : 0);
      plot(buf, w, h, px, py, dist == 0 ? 255 : (dist == 1 ? 200 : 70));
    }
    prevy = (fy0 + fy1) / 2;
  }
  // where the playing voice is
  if ((live >= 0) && (live < span))
  {
    int lx = 4 + (w - 8) * live / span;
    int ly = (int)(base - (s->v[live] / (float)s->maxv) * (base - top));
    int dy, dx;
    for (dy = top - 2; dy <= base; dy++) plot(buf, w, h, lx, dy, 255);
    for (dy = -4; dy <= 4; dy++)
      for (dx = -4; dx <= 4; dx++)
        if (dx * dx + dy * dy <= 16) plot(buf, w, h, lx + dx, ly + dy, 255);
  }
  canvas_draw(id, x, y, color);
  return span;
}

static int drawpulsegraph(int id, int x, int y, int cw, int ch, const PULSESUM *p, int color, int live)
{
  CURVE c;
  c.v = p->width; c.ticks = p->ticks; c.maxv = 4095;
  c.looped = p->looped; c.loopstart = p->loopstart; c.looplen = p->looplen;
  return drawcurve(id, x, y, cw, ch, &c, color, live);
}

// ---------------------------------------------------------------------------------------------------------
// The summary window
// ---------------------------------------------------------------------------------------------------------

static void showfiltersummary(void);

static void showpulsesummary(void)
{
  int cursorrow;
  int start;
  int result = 0;

  start = currentprogram(PTBL, &cursorrow);
  key = 0;
  rawkey = 0;
  while (!result)
  {
    const PULSESUM *s = pulsesummary(start);
    int livec = livechannel();
    char buf[100], t[32], u[32];
    int w = 62;
    int listrows = s->nrows + (s->moreRows ? 1 : 0);
    int graphch = 5;
    int graphcw = 44;
    int h;
    int x, y, c, ly, spanticks;

    if (listrows < 3) listrows = 3;
    h = 2 + (start ? listrows + 1 : 2) + graphch + 1 + 5 + 3 + 5;
    x = (MAX_COLUMNS - w) / 2;
    y = (MAX_ROWS - h) / 2 - 1;

    printstatus();
    dlg_drawframe(x, y, w, h, "PULSE TABLE SUMMARY");
    ly = y + 2;
    if (!start)
    {
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "No program: the instrument's Pulsetable Pos is $00,");
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), "so the pulse width is left as it is.");
      ly += 3;
    }
    else
    {
      snprintf(buf, sizeof buf, "Program starting at row %02X", start);
      printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
      ly++;
      for (c = 0; c < s->nrows; c++)
      {
        int row = s->rows[c].row;
        int here = (row == cursorrow);
        snprintf(buf, sizeof buf, "%02X", row);
        printtext(x + 3, ly, MKCOL(3, C_PANEL), buf);
        printtext(x + 6, ly, MKCOL(here ? CEDIT : (s->rows[c].kind == PK_JUMP || s->rows[c].kind == PK_STOP ? 13 : (s->rows[c].kind == PK_SET ? 14 : 8)), C_PANEL), s->rows[c].text);
        if (here) printtext(x + 1, ly, MKCOL(CEDIT, C_PANEL), ">");
        if ((livec >= 0) && (row == chn[livec].ptr[PTBL])) printtext(x + 2, ly, MKCOL(11, C_PANEL), "*");
        ly++;
      }
      if (s->moreRows)
      {
        printtext(x + 6, ly, MKCOL(3, C_PANEL), "...");
        ly++;
      }
      ly += (listrows - s->nrows - (s->moreRows ? 1 : 0));
    }

    // the graph with its scale: 100% at the top, 0% at the bottom, ticks along the bottom
    ly++;
    printtext(x + 2, ly, MKCOL(3, C_PANEL), "100%");
    printtext(x + 2, ly + graphch - 1, MKCOL(3, C_PANEL), "  0%");
    spanticks = drawpulsegraph(SUM_CANVAS, x + 7, ly, graphcw, graphch, s, MKCOL(11, C_PANEL), livec >= 0 ? livepulsetick(s, livec) : -1);
    ly += graphch;
    sum_duration(t, sizeof t, spanticks, tickrate());
    snprintf(buf, sizeof buf, "%dt = %s", spanticks, t);
    printtext(x + 7 + graphcw - (int)strlen(buf), ly, MKCOL(3, C_PANEL), buf);
    printtext(x + 7, ly, MKCOL(3, C_PANEL), s->looped ? "dotted lines: the part that repeats" : "time");
    ly += 2;

    // the wave itself at six moments of the program: what the SID outputs while the width moves
    if (start)
    {
      unsigned char *cv = canvas_begin(WAVE_CANVAS, 42, 2);
      int span = s->looped ? s->loopstart + s->looplen : s->ticks;
      int shown = 0, lastwidth = -1, k;
      int shapes = 0x40;
      int wstart = instr[editorInfo.einum].ptr[WTBL];
      if (wstart && (wavesummary(wstart)->firstwave & 0x40)) shapes = wavesummary(wstart)->firstwave & 0xf0;
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Wave   ");
      printtext(x + 10, ly, MKCOL(3, C_PANEL), "the output at 6 moments, left to right");
      if (livec >= 0)
      {
        snprintf(buf, sizeof buf, "now %d%%", chn[livec].pulse * 100 / 4096);
        printtext(x + w - 12, ly, MKCOL(CEDIT, C_PANEL), buf);
      }
      if (cv && (span > 0))
      {
        for (k = 0; k < 6; k++)
        {
          int tk = span > 1 ? k * (span - 1) / 5 : 0;
          int wd = s->width[tk < s->ticks ? tk : s->ticks - 1];
          if ((wd == lastwidth) && (k > 0) && (span > 1)) continue;
          lastwidth = wd;
          shapeduty = wd / 4096.0f;
          drawshapes(cv, 42 * 16, 64, shown * 112 + 8, shown * 112 + 104, 8, 54, shapes, 2);
          snprintf(buf, sizeof buf, "%d%%", wd * 100 / 4096);
          printtext(x + 3 + shown * 7 + 3, ly + 3, MKCOL(15, C_PANEL), buf);
          shown++;
        }
        shapeduty = 0.5f;
        canvas_draw(WAVE_CANVAS, x + 3, ly + 1, MKCOL(11, C_PANEL));
      }
      if (wstart && !(wavesummary(wstart)->shapes & 0x40))
        printtext(x + 3, ly + 4, MKCOL(14, C_PANEL), "The instrument's waveform has no pulse: the width is not heard.");
      ly += 5;
    }

    // the outcome
    if (start)
    {
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Width ");
      printtext(x + 9, ly, MKCOL(15, C_PANEL), s->range);
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), "Shape ");
      printtext(x + 9, ly + 1, MKCOL(15, C_PANEL), s->shape);
      printtext(x + 3, ly + 2, MKCOL(7, C_PANEL), "Time  ");
      printtext(x + 9, ly + 2, MKCOL(15, C_PANEL), s->timing);
      if (s->note[0]) printtext(x + 3, ly + 3, MKCOL(14, C_PANEL), s->note);
    }
    snprintf(u, sizeof u, "%.0f", tickrate());
    snprintf(buf, sizeof buf, "t = tick (one frame, %s Hz here)    ESC / ENTER = close", u);
    printtext(x + 3, y + h - 2, MKCOL(3, C_PANEL), buf);
    fliptoscreen();
    getkey();
    if (win_quitted) result = 1;
    if ((rawkey == KEY_ESC) || (rawkey == KEY_ENTER) || (rawkey == KEY_SPACE)) result = 1;
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb)) result = 1;
  }
  key = 0;
  rawkey = 0;
  printmainscreen();
}

// ---------------------------------------------------------------------------------------------------------
// Waveform shapes (icon and lane of the graph)
// ---------------------------------------------------------------------------------------------------------

static float shapevalue(int shapes, float p, int px)
{
  float v = 1.0f, c;
  if (shapes & 0x10) { c = p < 0.5f ? 2 * p : 2 - 2 * p; if (c < v) v = c; }
  if (shapes & 0x20) { c = p; if (c < v) v = c; }
  if (shapes & 0x40) { c = p < shapeduty ? 0.0f : 1.0f; if (c < v) v = c; }
  if (shapes & 0x80)
  {
    unsigned h = (unsigned)(px >> 2) * 2654435761u;
    c = ((h >> 13) & 255) / 255.0f;
    if (c < v) v = c;
  }
  return v;
}

// Draws the one-period-or-two outline of the shapes in the mask (0x10 tri, 0x20 saw, 0x40 pulse, 0x80 noise) inside
// the box. With several shapes the thin lines are the shapes by themselves and the thick line is what comes out
// of the SID when they are combined (the output of a combination is roughly the lowest of them).
static void drawshapes(unsigned char *buf, int w, int h, int x0, int x1, int top, int bot, int shapes, int periods)
{
  int pass, k, px;
  int comb = 0;
  for (k = 0; k < 4; k++) if (shapes & (0x10 << k)) comb++;
  if (!comb) return;
  for (pass = 0; pass < (comb > 1 ? 2 : 1); pass++)
  {
    int li;
    for (li = 0; li < (comb > 1 && pass == 0 ? 4 : 1); li++)
    {
      int mask = shapes;
      float prevy = -1;
      if (comb > 1 && pass == 0)
      {
        mask = shapes & (0x10 << li);
        if (!mask) continue;
      }
      for (px = x0; px <= x1; px++)
      {
        float p = (float)(px - x0) * periods / (float)(x1 - x0 + 1);
        float v;
        float fy;
        int y0, y1, py;
        p -= floorf(p);
        v = shapevalue(mask, p, px - x0);
        fy = bot - v * (bot - top);
        if (prevy < 0) prevy = fy;
        y0 = (int)floorf(fy < prevy ? fy : prevy);
        y1 = (int)ceilf(fy > prevy ? fy : prevy);
        if (comb > 1 && pass == 0)
          for (py = y0; py <= y1; py++) plot(buf, w, h, px, py, 110);
        else
          for (py = y0 - 2; py <= y1 + 2; py++)
          {
            int dist = py < y0 ? y0 - py : (py > y1 ? py - y1 : 0);
            plot(buf, w, h, px, py, dist == 0 ? 255 : (dist == 1 ? 200 : 70));
          }
        prevy = fy;
      }
    }
  }
}

// "PUL+TRI RING" and so on, at most len characters
static void wavelabel(char *buf, int len, int wave)
{
  static const char *abbr[4] = {"TRI", "SAW", "PUL", "NSE"};
  char t[40];
  int k, n = 0;
  t[0] = 0;
  for (k = 0; k < 4; k++)
    if (wave & (0x10 << k))
    {
      if (n++) strcat(t, "+");
      strcat(t, abbr[k]);
    }
  if (!n) strcpy(t, "SILENT");
  if (strlen(t) + 5 <= (size_t)len && (wave & 4)) strcat(t, " RING");
  if (strlen(t) + 5 <= (size_t)len && (wave & 2)) strcat(t, " SYNC");
  if (strlen(t) + 5 <= (size_t)len && (wave & 8)) strcat(t, " TEST");
  snprintf(buf, len + 1, "%s", t);
}

// ---------------------------------------------------------------------------------------------------------
// Wavetable summary (cached like the pulse one)
// ---------------------------------------------------------------------------------------------------------

static WAVESUM wcached;
static unsigned char wcachel[MAX_TABLELEN], wcacher[MAX_TABLELEN];
static int wcachestart = -1;
static double wcacherate;

static const WAVESUM *wavesummary(int start)
{
  double rate = tickrate();
  if ((wcachestart != start) || (wcacherate != rate) || memcmp(wcachel, ltable[WTBL], MAX_TABLELEN) || memcmp(wcacher, rtable[WTBL], MAX_TABLELEN))
  {
    summarize_wave(&wcached, ltable[WTBL], rtable[WTBL], start, rate);
    memcpy(wcachel, ltable[WTBL], MAX_TABLELEN);
    memcpy(wcacher, rtable[WTBL], MAX_TABLELEN);
    wcachestart = start;
    wcacherate = rate;
  }
  return &wcached;
}

static void drawwavesummaryline(int y)
{
  char buf[80];
  int cursorrow;
  int start = currentprogram(WTBL, &cursorrow);
  const WAVESUM *s = wavesummary(start);
  fillarea(RP_X, y, RP_W, 1, MKCOL(8, C_PANEL), ' ');
  if (start) snprintf(buf, sizeof buf, "WAVE %02X: %s", start, s->oneline);
  else snprintf(buf, sizeof buf, "WAVE: %s", s->oneline);
  buf[RP_W - 2] = 0;
  printtext(RP_X + 1, y, MKCOL(editorInfo.editmode == EDIT_TABLES && editorInfo.etnum == WTBL ? 15 : 7, C_PANEL), buf);
}

// The waveform the cursor is on in the wavetable (or, when the wavetable is not being edited, the first one the
// instrument's wavetable program sets). Returns -1 if there is none; *inforce is set when the row itself does not
// set a waveform and the one in effect is shown.
static int panelwave(int *inforce)
{
  *inforce = 0;
  if ((editorInfo.editmode == EDIT_TABLES) && (editorInfo.etnum == WTBL))
  {
    int row = editorInfo.etpos, k;
    int l = ltable[WTBL][row], r = rtable[WTBL][row];
    if ((l > WAVELASTDELAY) && (l < WAVESILENT)) return l;
    if ((l >= WAVESILENT) && (l <= WAVELASTSILENT)) return l & 0xf;
    if ((l >= WAVECMD) && (l <= WAVELASTCMD) && ((l & 0xf) == CMD_SETWAVE)) return r;
    for (k = row - 1; k >= 0; k--)
    {
      int pl = ltable[WTBL][k], pr = rtable[WTBL][k];
      if (pl == 0xff) break;
      *inforce = 1;
      if ((pl > WAVELASTDELAY) && (pl < WAVESILENT)) return pl;
      if ((pl >= WAVESILENT) && (pl <= WAVELASTSILENT)) return pl & 0xf;
      if ((pl >= WAVECMD) && (pl <= WAVELASTCMD) && ((pl & 0xf) == CMD_SETWAVE)) return pr;
    }
    *inforce = 0;
    return -1;
  }
  {
    int start = instr[editorInfo.einum].ptr[WTBL];
    const WAVESUM *s;
    if (!start) return -1;
    s = wavesummary(start);
    return s->firstwave ? s->firstwave : -1;
  }
}

// The shapes the instrument's wave has in the pulse: the pulse itself, or a combination that includes it
static int pulseshapes(void)
{
  int start = instr[editorInfo.einum].ptr[WTBL];
  int fw = start ? wavesummary(start)->firstwave : 0;
  return (fw & 0x40) ? (fw & 0xf0) : 0x40;
}

// The pulse table row under the cursor: the wave with the width the row leaves. Returns 1 if it drew something.
static int drawpulseicon(int x, int y, unsigned char *cv, int cw, int ch)
{
  int cursorrow;
  int start = currentprogram(PTBL, &cursorrow);
  const PULSESUM *ps = pulsesummary(start);
  int first = -1, last = -1, t;
  char buf[24];
  for (t = 0; t < ps->ticks; t++)
    if (ps->rowof[t] == cursorrow) { if (first < 0) first = t; last = t; }
  if ((!start) || (first < 0))
  {
    printtext(x, y, MKCOL(3, C_PANEL), "PULSE");
    printtext(x, y + 1, MKCOL(3, C_PANEL), "not played");
    return 1;
  }
  shapeduty = ps->width[last] / 4096.0f;
  drawshapes(cv, cw * 16, ch * 32, 6, cw * 16 - 7, 8, ch * 32 - 9, pulseshapes(), 2);
  shapeduty = 0.5f;
  canvas_draw(2, x, y, MKCOL(11, C_PANEL));
  if (ps->width[first] != ps->width[last])
    snprintf(buf, sizeof buf, "%d%%>%d%%", ps->width[first] * 100 / 4096, ps->width[last] * 100 / 4096);
  else snprintf(buf, sizeof buf, "PUL %d%%", ps->width[last] * 100 / 4096);
  printtext(x, y + 2, MKCOL(15, C_PANEL), buf);
  return 1;
}

void drawwaveicon(int x, int y)
{
  const int cw = 11, ch = 2;
  char buf[24];
  int inforce;
  int wave = panelwave(&inforce);
  unsigned char *cv = canvas_begin(2, cw, ch);
  fillarea(x, y, cw, 3, MKCOL(8, C_PANEL), ' ');
  if (!cv) return;
  {
    int lc = livechannel();
    if (lc >= 0)
    {
      // playing: the wave as the voice has it right now, with its pulse width moving
      int wv = chn[lc].wave;
      shapeduty = (chn[lc].pulse & 0xfff) / 4096.0f;
      drawshapes(cv, cw * 16, ch * 32, 6, cw * 16 - 7, 8, ch * 32 - 9, wv & 0xf0, 2);
      shapeduty = 0.5f;
      canvas_draw(2, x, y, MKCOL(CEDIT, C_PANEL));
      wavelabel(buf, cw, wv);
      if (wv & 0x40)
      {
        char p[16];
        snprintf(p, sizeof p, " %d%%", (chn[lc].pulse & 0xfff) * 100 / 4096);
        if (strlen(buf) + strlen(p) <= (size_t)cw) strcat(buf, p);
      }
      printtext(x, y + 2, MKCOL(15, C_PANEL), buf);
      return;
    }
  }
  if ((editorInfo.editmode == EDIT_TABLES) && (editorInfo.etnum == PTBL) && drawpulseicon(x, y, cv, cw, ch)) return;
  if (wave < 0)
  {
    printtext(x, y, MKCOL(3, C_PANEL), "WAVEFORM");
    printtext(x, y + 1, MKCOL(3, C_PANEL), "none set");
    return;
  }
  drawshapes(cv, cw * 16, ch * 32, 6, cw * 16 - 7, 8, ch * 32 - 9, wave & 0xf0, 2);
  canvas_draw(2, x, y, MKCOL(inforce ? 7 : 11, C_PANEL));
  wavelabel(buf, cw, wave);
  printtext(x, y + 2, MKCOL((wave & 1) ? 15 : 3, C_PANEL), buf);
}

// ---------------------------------------------------------------------------------------------------------
// Wavetable summary window
// ---------------------------------------------------------------------------------------------------------

// tick i of the replay; a repeating program continues into its next repetition
static int wavetick(const WAVESUM *s, int i)
{
  if (i < s->ticks) return i;
  if (s->looped) return s->loopstart + (i - s->loopstart) % s->looplen;
  return s->ticks - 1;
}

static int drawwavegraph(int id, int x, int y, int cw, int ch, const WAVESUM *s, int color)
{
  unsigned char *buf = canvas_begin(id, cw, ch);
  int w = cw * 16, h = ch * 32;
  int lane = 64;                                  // pixel height of the waveform lane at the bottom
  int top = 8, base = h - lane - 8;
  int span = s->ticks, px, t;
  int lo = 100000, hi = -100000;
  float prevy = -1;
  int lastwave = -1, laststart = 0;

  if (!buf) return 0;
  if (s->looped)
  {
    span = s->loopstart + 2 * s->looplen;
    if (span > WSUM_MAXTICKS) span = WSUM_MAXTICKS;
  }
  if (!s->looped && s->stopped && span < s->ticks + 1) span = s->ticks + 1;   // one tick of "and then it just stays"
  if (span < 4) span = 4;
  for (t = 0; t < s->ticks; t++)
  {
    int v = s->pitch[t];
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (hi == lo) { lo -= 1; hi += 1; }

  for (px = 2; px < w - 2; px++)
  {
    plot(buf, w, h, px, base + 4, 70);
    if ((px & 7) < 2) plot(buf, w, h, px, (top + base) / 2, 30);
  }
  if (s->looped)
  {
    int mark[2], m;
    mark[0] = 4 + (w - 8) * s->loopstart / span;
    mark[1] = 4 + (w - 8) * (s->loopstart + s->looplen) / span;
    for (m = 0; m < 2; m++)
      for (px = top; px < h - 4; px++)
        if ((px & 7) < 4) plot(buf, w, h, mark[m], px, 110);
  }

  // the pitch over time, as a stepped line
  for (px = 4; px < w - 4; px++)
  {
    int i = wavetick(s, (px - 4) * span / (w - 8));
    float fy = base - (s->pitch[i] - lo) / (float)(hi - lo) * (base - top);
    int y0, y1, py;
    if (((px - 4) * span / (w - 8)) >= s->ticks && !s->looped) break;
    if (prevy < 0) prevy = fy;
    y0 = (int)floorf(fy < prevy ? fy : prevy);
    y1 = (int)ceilf(fy > prevy ? fy : prevy);
    for (py = y0 - 1; py <= y1 + 1; py++)
      plot(buf, w, h, px, py, (py < y0 || py > y1) ? 120 : 255);
    prevy = fy;
  }

  // the waveform lane: the shape of every waveform where it starts
  for (px = 4; px <= w - 4; px++)
  {
    int i = wavetick(s, (px - 4) * span / (w - 8));
    int wv = (px < w - 4 && !(((px - 4) * span / (w - 8)) >= s->ticks && !s->looped)) ? s->wave[i] : -1;
    if ((wv != lastwave) || (px == w - 4))
    {
      if ((lastwave > 0) && (px - laststart >= 6))
        drawshapes(buf, w, h, laststart + 2, px - 3, h - lane + 8, h - 10, lastwave & 0xf0, (px - laststart) > 40 ? 2 : 1);
      if ((lastwave > 0) && (laststart > 4))
        for (t = h - lane + 4; t < h - 6; t++) plot(buf, w, h, laststart, t, 60);
      lastwave = wv;
      laststart = px;
    }
  }
  canvas_draw(id, x, y, color);
  return span;
}

static void showwavesummary(void)
{
  int cursorrow;
  int start = currentprogram(WTBL, &cursorrow);
  int result = 0;

  key = 0;
  rawkey = 0;
  while (!result)
  {
    const WAVESUM *s = wavesummary(start);
    int tc = livechannel();
    char buf[100], t[32];
    int w = 62;
    int listrows = s->nrows + (s->moreRows ? 1 : 0);
    int graphch = 7, graphcw = 44;
    int h, x, y, c, ly, spanticks;

    if (listrows < 3) listrows = 3;
    h = 2 + (start ? listrows + 1 : 2) + graphch + 2 + 5 + 3;
    x = (MAX_COLUMNS - w) / 2;
    y = (MAX_ROWS - h) / 2 - 1;

    printstatus();
    dlg_drawframe(x, y, w, h, "WAVE TABLE SUMMARY");
    ly = y + 2;
    if (!start)
    {
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "No program: the instrument's Wavetable Pos is $00,");
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), "so no waveform or note is played from a table.");
      ly += 3;
    }
    else
    {
      snprintf(buf, sizeof buf, "Program starting at row %02X", start);
      printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
      ly++;
      for (c = 0; c < s->nrows; c++)
      {
        int here = (s->rows[c].row == cursorrow);
        snprintf(buf, sizeof buf, "%02X", s->rows[c].row);
        printtext(x + 3, ly, MKCOL(3, C_PANEL), buf);
        printtext(x + 6, ly, MKCOL(here ? CEDIT : (s->rows[c].kind == PK_JUMP || s->rows[c].kind == PK_STOP ? 13 : (s->rows[c].kind == PK_SET ? 14 : 8)), C_PANEL), s->rows[c].text);
        if (here) printtext(x + 1, ly, MKCOL(CEDIT, C_PANEL), ">");
        if ((tc >= 0) && (s->rows[c].row == chn[tc].ptr[WTBL])) printtext(x + 2, ly, MKCOL(11, C_PANEL), "*");
        ly++;
      }
      if (s->moreRows)
      {
        printtext(x + 6, ly, MKCOL(3, C_PANEL), "...");
        ly++;
      }
      ly += (listrows - s->nrows - (s->moreRows ? 1 : 0));
    }

    // the graph: note over time on top, the waveforms in a lane below
    ly++;
    printtext(x + 2, ly, MKCOL(3, C_PANEL), "note");
    printtext(x + 2, ly + 1, MKCOL(3, C_PANEL), "(+/-)");
    printtext(x + 2, ly + graphch - 2, MKCOL(3, C_PANEL), "wave");
    spanticks = drawwavegraph(SUM_CANVAS, x + 7, ly, graphcw, graphch, s, MKCOL(11, C_PANEL));
    ly += graphch;
    sum_duration(t, sizeof t, spanticks, tickrate());
    snprintf(buf, sizeof buf, "%dt = %s", spanticks, t);
    printtext(x + 7 + graphcw - (int)strlen(buf), ly, MKCOL(3, C_PANEL), buf);
    printtext(x + 7, ly, MKCOL(3, C_PANEL), s->looped ? "dotted lines: the part that repeats" : "time");
    ly += 2;

    if (start)
    {
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Waves ");
      printtext(x + 9, ly, MKCOL(15, C_PANEL), s->waves);
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), "Notes ");
      printtext(x + 9, ly + 1, MKCOL(15, C_PANEL), s->notes);
      printtext(x + 3, ly + 2, MKCOL(7, C_PANEL), "Time  ");
      printtext(x + 9, ly + 2, MKCOL(15, C_PANEL), s->timing);
      if (s->note[0]) printtext(x + 3, ly + 3, MKCOL(14, C_PANEL), s->note);
    }
    snprintf(buf, sizeof buf, "t = tick (one frame, %.0f Hz here)    ESC / ENTER = close", tickrate());
    printtext(x + 3, y + h - 2, MKCOL(3, C_PANEL), buf);
    fliptoscreen();
    getkey();
    if (win_quitted) result = 1;
    if ((rawkey == KEY_ESC) || (rawkey == KEY_ENTER) || (rawkey == KEY_SPACE)) result = 1;
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb)) result = 1;
  }
  key = 0;
  rawkey = 0;
  printmainscreen();
}

void showtablesummary(void)
{
  if (editorInfo.etnum == PTBL) showpulsesummary();
  else if (editorInfo.etnum == WTBL) showwavesummary();
  else if (editorInfo.etnum == FTBL) showfiltersummary();
  else
  {
    const char *lines[3] = {"Table summary", "", "The speedtable has an editor (SHIFT+E) instead of a summary."};
    dlg_message("Table summary", lines, 3);
  }
}

// ---------------------------------------------------------------------------------------------------------
// The side list of table codes: what every value of the table under the cursor means. It is drawn next to the
// table panel while the tables are edited, and the entry that matches the value under the cursor is lit.
// ---------------------------------------------------------------------------------------------------------

typedef struct
{
  const char *text;
  int side;                       // 0 = left column, 1 = right column, -1 = heading or free text
  int lo, hi;                     // values this entry covers
} CODELINE;

#define H(t) {t, -1, 0, 0}
static const CODELINE wavecodes[] = {
  H("LEFT SIDE"),
  {" 00     keep the waveform", 0, 0x00, 0x00},
  {" 01-0F  wait 1-15 frames", 0, 0x01, 0x0f},
  {" 10-DF  waveform register value:", 0, 0x10, 0xdf},
  H("        01 gate 02 sync 04 ring 08 test"),
  H("        10 tri 20 saw 40 pulse 80 noise"),
  {" E0-EF  silent waveform $0-$F", 0, 0xe0, 0xef},
  {" F0-FE  run pattern command 0-E (below)", 0, 0xf0, 0xfe},
  {" FF     jump to row (00 = stop)", 0, 0xff, 0xff},
  H("RIGHT SIDE"),
  {" 00-5F  note up, in semitones", 1, 0x00, 0x5f},
  {" 60-7F  note down", 1, 0x60, 0x7f},
  {" 80     keep the frequency", 1, 0x80, 0x80},
  {" 81-DF  absolute note C#0 - B-7", 1, 0x81, 0xdf},
  H("COMMANDS (F1-FD, right side = XY)"),
  {" F1  portamento up    F2  porta down", 0, 0xf1, 0xf2},
  {" F3  toneporta        F4  vibrato", 0, 0xf3, 0xf4},
  {" F5  set attack/decay F6  set sust/rel", 0, 0xf5, 0xf6},
  {" F7  set waveform", 0, 0xf7, 0xf7},
  {" F9  pulse pointer    FA  filter pointer", 0, 0xf9, 0xfa},
  {" FB  filter control   FC  filter cutoff", 0, 0xfb, 0xfc},
  {" FD  master volume", 0, 0xfd, 0xfd},
  H(" F0 F8 FE are not allowed here; F1-F4"),
  H(" take a speedtable index as parameter"),
};

static const CODELINE pulsecodes[] = {
  H("LEFT SIDE"),
  {" 00     empty row: the table stops", 0, 0x00, 0x00},
  {" 01-7F  modulation step: left = ticks,", 0, 0x01, 0x7f},
  H("        right = signed speed per tick"),
  {" 80-FE  set the width: left low nybble", 0, 0x80, 0xfe},
  H("        = high 4 bits, right = low 8"),
  H("        bits ($800 = 50%)"),
  {" FF     jump to row (00 = stop)", 0, 0xff, 0xff},
  H("RIGHT SIDE"),
  {" speed 01-7F up, 80-FF down (signed)", 1, 0x01, 0xff},
  H(""),
  H("The width wraps round at 0% / 100%."),
  H("Voice goes silent at 0% and 100%."),
};

static const CODELINE filtercodes[] = {
  H("LEFT SIDE"),
  {" 00     set the cutoff = right side", 0, 0x00, 0x00},
  {" 01-7F  modulation step: left = ticks,", 0, 0x01, 0x7f},
  H("        right = signed speed per tick"),
  {" 80-F0  set filter parameters:", 0, 0x80, 0xf0},
  H("        high nybble = passband:"),
  H("        $90 lowpass, $A0 bandpass,"),
  H("        $C0 highpass"),
  H("        right = resonance (high nybble)"),
  H("        and voice bitmask (low nybble)"),
  {" FF     jump to row (00 = stop)", 0, 0xff, 0xff},
  H(""),
  H("Parameters followed by a cutoff row"),
  H("run on the same frame. Each SID has"),
  H("its own filter."),
};

static const CODELINE speedcodes[] = {
  H("EACH ROW IS XX YY"),
  H(""),
  H(" Vibrato     XX = speed (time until"),
  H("             the direction changes)"),
  H("             YY = depth"),
  H(" Portamento  XXYY = 16-bit speed that"),
  H("             is added to the frequency"),
  H("             every tick"),
  H(" Funktempo   two tempos that alternate"),
  H("             on every row, XX first"),
  H(""),
  H("If XX has bit $80 set, vibrato depth"),
  H("and portamento speed follow the note"),
  H("and YY is the divisor (right shifts)."),
};

static int showcommands = 1;

void togglecommandpanel(void)
{
  showcommands ^= 1;
}

static void drawpatterncommandpanel(void);

void drawcommandpanel(void)
{
  static const char *titles[4] = {"WAVETABLE CODES", "PULSETABLE CODES", "FILTERTABLE CODES", "SPEEDTABLE VALUES"};
  const CODELINE *lines;
  int n, k, x, y, w = 44, h;
  int table = editorInfo.etnum;
  int val = -1, side = -1;
  if (!showcommands) return;
  if (editorInfo.editmode == EDIT_PATTERN)
  {
    drawpatterncommandpanel();
    return;
  }
  if (editorInfo.editmode != EDIT_TABLES) return;
  switch (table)
  {
    case WTBL: lines = wavecodes; n = sizeof wavecodes / sizeof wavecodes[0]; break;
    case PTBL: lines = pulsecodes; n = sizeof pulsecodes / sizeof pulsecodes[0]; break;
    case FTBL: lines = filtercodes; n = sizeof filtercodes / sizeof filtercodes[0]; break;
    default: lines = speedcodes; n = sizeof speedcodes / sizeof speedcodes[0]; break;
  }
  if (table != STBL)
  {
    side = editorInfo.etcolumn >= 2;
    val = side ? rtable[table][editorInfo.etpos] : ltable[table][editorInfo.etpos];
  }
  h = n + 4;
  x = RP_X - w - 1;
  y = TBL_Y - 1;
  if (y + h > MAX_ROWS - 3) y = MAX_ROWS - 3 - h;
  dlg_drawframe(x, y, w, h, titles[table]);
  for (k = 0; k < n; k++)
  {
    int lit = (lines[k].side >= 0) && (lines[k].side == side) && (val >= lines[k].lo) && (val <= lines[k].hi);
    int color = lines[k].side < 0 ? (lines[k].text[0] == ' ' || lines[k].text[0] == 0 ? 7 : 3) : (lit ? CEDIT : 15);
    printtext(x + 1, y + 2 + k, MKCOL(color, C_PANEL), lines[k].text);
  }
  printtext(x + 2, y + h - 2, MKCOL(3, C_PANEL), "SHIFT+H hides this list");
}

// ---------------------------------------------------------------------------------------------------------
// Pattern commands: the guide panel and the line under the tables. They show while the cursor is on the command
// or data column of the pattern editor.
// ---------------------------------------------------------------------------------------------------------

static const char *cmdlist[16] = {
  "0  do nothing, ends 1-4 (instr. vibrato)",
  "1  portamento up (speedtable XY)",
  "2  portamento down (speedtable XY)",
  "3  toneportamento (00 = tie note)",
  "4  vibrato (speedtable XY)",
  "5  set attack/decay",
  "6  set sustain/release",
  "7  set waveform",
  "8  wavetable pointer (00 = stop)",
  "9  pulsetable pointer (00 = stop)",
  "A  filtertable pointer (00 = stop)",
  "B  filter control (resonance, voices)",
  "C  filter cutoff",
  "D  master volume D0Y / timing mark",
  "E  funktempo (speedtable XY)",
  "F  tempo (03-7F all, 83-FF this voice)"};

static int patterncommand(int *cmd, int *data)
{
  int pn = editorUndoInfo.editorInfo[editorInfo.epchn].epnum;
  if ((editorInfo.editmode != EDIT_PATTERN) || (editorInfo.epcolumn < 3)) return 0;
  if (editorInfo.eppos >= pattlen[pn]) return 0;
  *cmd = pattern[pn][editorInfo.eppos * 4 + 2];
  *data = pattern[pn][editorInfo.eppos * 4 + 3];
  return 1;
}

static void commandtables(CMDTABLES *t)
{
  int k;
  for (k = 0; k < 4; k++)
  {
    t->l[k] = ltable[k];
    t->r[k] = rtable[k];
  }
}

static double sidclock(void)
{
  return editorInfo.ntsc ? 1022727.0 : 985248.0;
}

static CMDSUM cmdcache;

static const CMDSUM *commandsummary(int cmd, int data)
{
  // recomputed every frame: it is cheap, and it follows edits of the tables
  CMDTABLES t;
  commandtables(&t);
  summarize_command(&cmdcache, cmd, data, &t, tickrate(), sidclock());
  return &cmdcache;
}

static int drawcommandline(int y)
{
  int cmd, data;
  char buf[120];
  const CMDSUM *s;
  if (!patterncommand(&cmd, &data)) return 0;
  fillarea(RP_X, y, RP_W, 1, MKCOL(8, C_PANEL), ' ');
  if (!cmd && !data) snprintf(buf, sizeof buf, "No command on this row");
  else
  {
    s = commandsummary(cmd, data);
    snprintf(buf, sizeof buf, "%X%02X: %s", cmd, data, s->text);
  }
  buf[RP_W - 2] = 0;
  printtext(RP_X + 1, y, MKCOL(15, C_PANEL), buf);
  return 1;
}

// Prints text wrapped to the width, returns the number of lines used (at most maxlines)
int printwrapped(int x, int y, int width, int maxlines, int color, const char *text)
{
  int lines = 0;
  const char *p = text;
  while (*p && lines < maxlines)
  {
    char line[96];
    int len = (int)strlen(p), cut;
    if (len > width)
    {
      cut = width;
      while (cut > 0 && p[cut] != ' ') cut--;
      if (cut <= 0) cut = width;
    }
    else cut = len;
    memcpy(line, p, cut);
    line[cut] = 0;
    printtext(x, y + lines, color, line);
    lines++;
    p += cut;
    while (*p == ' ') p++;
  }
  return lines;
}

static void drawpatterncommandpanel(void)
{
  int cmd, data, k, x, y, w = 44, h = 30, ly;
  int cx;
  const CMDSUM *s;
  if (!patterncommand(&cmd, &data)) return;
  cmd &= 15;
  // on the side of the screen that the cursor is not on, so that the cursor stays visible
  cx = CHN_X(editorInfo.epchn - chnview);
  x = (cx >= PATT_W / 2) ? PATT_X + 1 : RP_X - w - 1;
  y = PATT_Y + 2;
  dlg_drawframe(x, y, w, h, "PATTERN COMMANDS");
  for (k = 0; k < 16; k++)
    printtext(x + 2, y + 2 + k, MKCOL(k == cmd ? CEDIT : 15, C_PANEL), cmdlist[k]);
  ly = y + 19;
  s = commandsummary(cmd, data);
  {
    char buf[60];
    snprintf(buf, sizeof buf, "UNDER THE CURSOR: %X%02X %s", cmd, data, s->name);
    buf[w - 4] = 0;
    printtext(x + 2, ly, MKCOL(3, C_PANEL), buf);
  }
  ly++;
  ly += printwrapped(x + 2, ly, w - 4, 3, MKCOL(14, C_PANEL), s->text);
  for (k = 0; k < s->ndetail && ly < y + h - 3; k++)
    ly += printwrapped(x + 2, ly, w - 4, y + h - 2 - ly > 3 ? 3 : y + h - 2 - ly, MKCOL(7, C_PANEL), s->detail[k]);
  printtext(x + 2, y + h - 2, MKCOL(3, C_PANEL), "SHIFT+G hides this guide");
}

// ---------------------------------------------------------------------------------------------------------
// Small exports for the step editor (gstepui.c)
// ---------------------------------------------------------------------------------------------------------

void sum_drawshapes(unsigned char *buf, int w, int h, int x0, int x1, int top, int bot, int shapes, int periods, float duty)
{
  shapeduty = duty;
  drawshapes(buf, w, h, x0, x1, top, bot, shapes, periods);
  shapeduty = 0.5f;
}

int sum_currentprogram(int table, int *cursorrow) { return currentprogram(table, cursorrow); }
int sum_pulseshapes(void) { return pulseshapes(); }
double sum_tickrate(void) { return tickrate(); }

// ---------------------------------------------------------------------------------------------------------
// The row of each table that the playing voice is on (highlighted in the table panel)
// ---------------------------------------------------------------------------------------------------------

// The voice whose tables are shown: the voice being edited if it plays the instrument being edited, otherwise the
// first voice that plays it and has a table program running. -1 when nothing plays.
static int tablechannel(void)
{
  int c;
  if (!isplaying()) return -1;
  if (chn[editorInfo.epchn].instr == editorInfo.einum) return editorInfo.epchn;
  for (c = 0; c < MAX_CHN; c++)
    if ((chn[c].instr == editorInfo.einum) && (!chn[c].mute) && (chn[c].ptr[WTBL] || chn[c].ptr[PTBL])) return c;
  return -1;
}

// Row (1 based) of the table that is being executed now, 0 if none. The wave and pulse pointers stay on a row
// while it waits or modulates; the filter pointer belongs to the SID of the voice; the speedtable row is the one
// that the voice's portamento or vibrato (also the instrument's own vibrato) is using.
int sum_playrow(int table)
{
  int c = tablechannel();
  if (c < 0) return 0;
  switch (table)
  {
    case WTBL: return chn[c].ptr[WTBL];
    case PTBL: return chn[c].ptr[PTBL];
    case FTBL: return sidfilt[c / 3].filterptr;
    case STBL:
    if (!chn[c].cmddata) return 0;
    if ((chn[c].command >= CMD_PORTAUP) && (chn[c].command <= CMD_VIBRATO)) return chn[c].cmddata;
    if ((chn[c].command == CMD_DONOTHING) && chn[c].vibdelay) return chn[c].cmddata;
    return 0;
  }
  return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Filtertable summary
// ---------------------------------------------------------------------------------------------------------

static FILTSUM fcached;
static unsigned char fcachel[MAX_TABLELEN], fcacher[MAX_TABLELEN];
static int fcachestart = -1;
static double fcacherate;

static const FILTSUM *filtersummary(int start)
{
  double rate = tickrate();
  if ((fcachestart != start) || (fcacherate != rate) || memcmp(fcachel, ltable[FTBL], MAX_TABLELEN) || memcmp(fcacher, rtable[FTBL], MAX_TABLELEN))
  {
    summarize_filter(&fcached, ltable[FTBL], rtable[FTBL], start, rate);
    memcpy(fcachel, ltable[FTBL], MAX_TABLELEN);
    memcpy(fcacher, rtable[FTBL], MAX_TABLELEN);
    fcachestart = start;
    fcacherate = rate;
  }
  return &fcached;
}

static void drawfiltersummaryline(int y)
{
  char buf[100];
  int cursorrow;
  int start = currentprogram(FTBL, &cursorrow);
  const FILTSUM *s = filtersummary(start);
  fillarea(RP_X, y, RP_W, 1, MKCOL(8, C_PANEL), ' ');
  if (start) snprintf(buf, sizeof buf, "FILTER %02X: %s", start, s->oneline);
  else snprintf(buf, sizeof buf, "FILTER: %s", s->oneline);
  buf[RP_W - 2] = 0;
  printtext(RP_X + 1, y, MKCOL(editorInfo.editmode == EDIT_TABLES && editorInfo.etnum == FTBL ? 15 : 7, C_PANEL), buf);
}

// The frequency response of the filter (a two pole state variable filter, as the SID's is): the level in dB over the
// frequency (30 Hz - 16 kHz, logarithmic), for the passband bits, the resonance and the cutoff. The shape is the
// real one, the exact cutoff frequency differs between a 6581, an 8580 and a given chip.
void sum_drawresponse(unsigned char *buf, int w, int h, int x0, int x1, int top, int bot, int type, int res, int cutoff, int bright)
{
  int px;
  double fc = filter_cutoffhz(cutoff), q = 0.7 + res * 0.45;
  float prevy = -1;
  if (!(type & 0x70))
  {
    for (px = x0; px <= x1; px++) plot(buf, w, h, px, bot, 70);
    return;
  }
  for (px = x0; px <= x1; px++)
  {
    double hz = 30.0 * pow(16000.0 / 30.0, (px - x0) / (double)(x1 - x0));
    double fn = hz / fc;
    double dr = 1.0 - fn * fn, di = fn / q;            // D = (1 - f^2) + j f/Q
    double den = dr * dr + di * di;
    double re = 0, im = 0, mag, db;
    float fy;
    int y0, y1, py;
    // the passband outputs divided by D: LP = 1, BP = j f/Q, HP = -f^2 (numerators)
    double nr = 0, ni = 0;
    if (type & 0x10) nr += 1;
    if (type & 0x20) ni += fn / q;
    if (type & 0x40) nr -= fn * fn;
    re = (nr * dr + ni * di) / den;
    im = (ni * dr - nr * di) / den;
    mag = sqrt(re * re + im * im);
    db = 20.0 * log10(mag + 1e-6);
    if (db < -36) db = -36;
    if (db > 18) db = 18;
    fy = (float)(bot - (db + 36.0) / 54.0 * (bot - top));
    if (prevy < 0) prevy = fy;
    y0 = (int)floorf(fy < prevy ? fy : prevy);
    y1 = (int)ceilf(fy > prevy ? fy : prevy);
    for (py = y0 - 1; py <= y1 + 1; py++)
      plot(buf, w, h, px, py, (py < y0 || py > y1) ? bright / 2 : bright);
    prevy = fy;
  }
  // the cutoff frequency as a dotted line
  {
    double t = log(fc / 30.0) / log(16000.0 / 30.0);
    int cx = x0 + (int)(t * (x1 - x0));
    for (px = top; px < bot; px++)
      if ((px & 7) < 3) plot(buf, w, h, cx, px, 90);
  }
}

// The tick of the filter replay that the playing voice's SID is on
static int livefiltertick(const FILTSUM *s, int c)
{
  int row = sidfilt[c / 3].filterptr, first = -1, t;
  if (!row) return -1;
  if (ltable[FTBL][row - 1] == 0xff) row = rtable[FTBL][row - 1];
  if (!row) return -1;
  for (t = 0; t < s->ticks; t++)
    if (s->rowof[t] == row) { first = t; break; }
  if (first < 0) return -1;
  if (sidfilt[c / 3].filtertime && ltable[FTBL][row - 1] < 0x80)
  {
    int elapsed = ltable[FTBL][row - 1] - sidfilt[c / 3].filtertime;
    if (elapsed > 0) first += elapsed;
  }
  return first < s->ticks ? first : s->ticks - 1;
}

static void showfiltersummary(void)
{
  int cursorrow;
  int start = currentprogram(FTBL, &cursorrow);
  int result = 0;

  key = 0;
  rawkey = 0;
  while (!result)
  {
    const FILTSUM *s = filtersummary(start);
    int livec = livechannel();
    char buf[100], t[32];
    int w = 62;
    int listrows = s->nrows + (s->moreRows ? 1 : 0);
    int graphch = 4, graphcw = 44;
    int h, x, y, c, ly, spanticks;
    CURVE cv;

    if (listrows < 3) listrows = 3;
    h = 2 + (start ? listrows + 1 : 2) + graphch + 1 + 5 + 5 + 3;
    x = (MAX_COLUMNS - w) / 2;
    y = (MAX_ROWS - h) / 2 - 1;

    printstatus();
    dlg_drawframe(x, y, w, h, "FILTER TABLE SUMMARY");
    ly = y + 2;
    if (!start)
    {
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "No program: the instrument's Filtertable Pos is $00,");
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), "so the filter is left as it is.");
      ly += 3;
    }
    else
    {
      snprintf(buf, sizeof buf, "Program starting at row %02X", start);
      printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
      ly++;
      for (c = 0; c < s->nrows; c++)
      {
        int row = s->rows[c].row;
        int here = (row == cursorrow);
        snprintf(buf, sizeof buf, "%02X", row);
        printtext(x + 3, ly, MKCOL(3, C_PANEL), buf);
        printtext(x + 6, ly, MKCOL(here ? CEDIT : (s->rows[c].kind == PK_JUMP || s->rows[c].kind == PK_STOP ? 13 : (s->rows[c].kind == PK_SET ? 14 : 8)), C_PANEL), s->rows[c].text);
        if (here) printtext(x + 1, ly, MKCOL(CEDIT, C_PANEL), ">");
        if ((livec >= 0) && (row == sidfilt[livec / 3].filterptr)) printtext(x + 2, ly, MKCOL(11, C_PANEL), "*");
        ly++;
      }
      if (s->moreRows)
      {
        printtext(x + 6, ly, MKCOL(3, C_PANEL), "...");
        ly++;
      }
      ly += (listrows - s->nrows - (s->moreRows ? 1 : 0));
    }

    // the cutoff over time
    ly++;
    printtext(x + 2, ly, MKCOL(3, C_PANEL), "100%");
    printtext(x + 2, ly + graphch - 1, MKCOL(3, C_PANEL), "  0%");
    cv.v = s->cutoff; cv.ticks = s->ticks; cv.maxv = 255;
    cv.looped = s->looped; cv.loopstart = s->loopstart; cv.looplen = s->looplen;
    spanticks = drawcurve(SUM_CANVAS, x + 7, ly, graphcw, graphch, &cv, MKCOL(11, C_PANEL), livec >= 0 ? livefiltertick(s, livec) : -1);
    ly += graphch;
    sum_duration(t, sizeof t, spanticks, tickrate());
    snprintf(buf, sizeof buf, "%dt = %s", spanticks, t);
    printtext(x + 7 + graphcw - (int)strlen(buf), ly, MKCOL(3, C_PANEL), buf);
    printtext(x + 7, ly, MKCOL(3, C_PANEL), s->looped ? "cutoff over time; dotted: the part that repeats" : "cutoff over time");
    ly += 2;

    // the shape of the filter at four moments of the program
    if (start)
    {
      unsigned char *cvb = canvas_begin(WAVE_CANVAS, 44, 3);
      int span = s->looped ? s->loopstart + s->looplen : s->ticks;
      int k, shown = 0, last = -1;
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Filter ");
      printtext(x + 10, ly, MKCOL(3, C_PANEL), "response at 4 moments (level over frequency)");
      if (cvb && span > 0 && s->ticks > 0)
      {
        for (k = 0; k < 4; k++)
        {
          int tk = span > 1 ? k * (span - 1) / 3 : 0, i2 = tk < s->ticks ? tk : s->ticks - 1;
          int cut = s->cutoff[i2], ty = s->type[i2], ct = s->ctrl[i2];
          if (!s->setsfilter) ty = 0x10, ct = 0;
          if ((cut == last) && (k > 0) && (span > 1)) continue;
          last = cut;
          sum_drawresponse(cvb, 44 * 16, 96, shown * 176 + 6, shown * 176 + 170, 6, 66, ty, ct >> 4, cut, 255);
          snprintf(buf, sizeof buf, "%d%%", cut * 100 / 255);
          printtext(x + 3 + shown * 11 + 4, ly + 4, MKCOL(15, C_PANEL), buf);
          shown++;
        }
        canvas_draw(WAVE_CANVAS, x + 3, ly + 1, MKCOL(11, C_PANEL));
      }
      ly += 5;
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Filter ");
      printtext(x + 10, ly, MKCOL(15, C_PANEL), s->filter);
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), "Cutoff ");
      printtext(x + 10, ly + 1, MKCOL(15, C_PANEL), s->range);
      printtext(x + 3, ly + 2, MKCOL(7, C_PANEL), "Shape  ");
      printtext(x + 10, ly + 2, MKCOL(15, C_PANEL), s->shape);
      printtext(x + 3, ly + 3, MKCOL(7, C_PANEL), "Time   ");
      printtext(x + 10, ly + 3, MKCOL(15, C_PANEL), s->timing);
      if (s->note[0]) printtext(x + 3, ly + 4, MKCOL(14, C_PANEL), s->note);
    }
    snprintf(buf, sizeof buf, "t = tick (one frame, %.0f Hz here)    ESC / ENTER = close", tickrate());
    printtext(x + 3, y + h - 2, MKCOL(3, C_PANEL), buf);
    fliptoscreen();
    getkey();
    if (win_quitted) result = 1;
    if ((rawkey == KEY_ESC) || (rawkey == KEY_ENTER) || (rawkey == KEY_SPACE)) result = 1;
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb)) result = 1;
  }
  key = 0;
  rawkey = 0;
  printmainscreen();
}
