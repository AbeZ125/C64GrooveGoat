//
// C64 GrooveGoat - the speedtable editor (SHIFT+E in the speedtable). A row is edited as a vibrato, a portamento
// or a funktempo, with the values in words and a picture of what it does. The edits go to the table at once;
// ESC puts the old row values back.
//

#include "goattrk2.h"
#include "gsummary.h"
#include "gwsummary.h"
#include "gcmdsummary.h"
#include "gsumui.h"

#define SPEED_CANVAS 1
#define T_VIB 0
#define T_PORTA 1
#define T_FUNK 2

#define ACT_PRESET 0x40000000

static void plot(unsigned char *buf, int w, int h, int x, int y, int a)
{
  if ((x < 0) || (y < 0) || (x >= w) || (y >= h)) return;
  if (buf[y * CANVAS_STRIDE + x] < a) buf[y * CANVAS_STRIDE + x] = a;
}

static int clampi(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

// the three kinds of rows by the byte values they are made of
static int indep(int row) { return ltable[STBL][row] >= 0x80; }

// ---------------------------------------------------------------------------------------------------------
// Where the row is used
// ---------------------------------------------------------------------------------------------------------

typedef struct
{
  int instr[MAX_INSTR];           // instruments whose Vibrato Param is the row
  int ninstr;
  int cmd[16];                    // pattern commands 1XY-4XY and EXY that use the row, by command
  int wave[16];                   // wavetable rows F1-F4 and FE that use the row, by command
} USAGE;

static void findusage(USAGE *u, int row)
{
  int i, p, r, v = row + 1;
  memset(u, 0, sizeof *u);
  for (i = 1; i < MAX_INSTR; i++)
    if (instr[i].ptr[STBL] == v) u->instr[u->ninstr++] = i;
  for (p = 0; p < MAX_PATT; p++)
    for (r = 0; r < pattlen[p]; r++)
    {
      int c = pattern[p][r * 4 + 2];
      if (((c >= 1 && c <= 4) || c == 0xe) && (pattern[p][r * 4 + 3] == v)) u->cmd[c]++;
    }
  for (r = 0; r < MAX_TABLELEN; r++)
  {
    int c = ltable[WTBL][r] & 0xf;
    if ((ltable[WTBL][r] >= WAVECMD) && (ltable[WTBL][r] <= WAVELASTCMD) && ((c >= 1 && c <= 4) || c == 0xe) && (rtable[WTBL][r] == v)) u->wave[c]++;
  }
}

static int guesstype(const USAGE *u)
{
  if (u->ninstr || u->cmd[4] || u->wave[4]) return T_VIB;
  if (u->cmd[0xe] || u->wave[0xe]) return T_FUNK;
  if (u->cmd[1] || u->cmd[2] || u->cmd[3] || u->wave[1] || u->wave[2] || u->wave[3]) return T_PORTA;
  return T_VIB;
}

// ---------------------------------------------------------------------------------------------------------
// Pictures
// ---------------------------------------------------------------------------------------------------------

static void line(unsigned char *buf, int w, int h, int x, float fy, float *prev, int thick)
{
  int y0, y1, py;
  if (*prev < -1e8f) *prev = fy;
  y0 = (int)floorf(fy < *prev ? fy : *prev);
  y1 = (int)ceilf(fy > *prev ? fy : *prev);
  for (py = y0 - thick; py <= y1 + thick; py++)
  {
    int dist = py < y0 ? y0 - py : (py > y1 ? py - y1 : 0);
    plot(buf, w, h, x, py, dist == 0 ? 255 : (dist == 1 ? 200 : 80));
  }
  *prev = fy;
}

// Draws the picture for the row; returns the full scale of the vertical axis in the unit of the type (cents or
// semitones), and the ticks the horizontal axis covers through *span.
static double drawpicture(int type, int row, int cw, int ch, double clock, double rate, int *span)
{
  unsigned char *buf = canvas_begin(SPEED_CANVAS, cw, ch);
  int w = cw * 16, h = ch * 32, px, mid = h / 2;
  int xx = ltable[STBL][row], yy = rtable[STBL][row];
  double full = 0;
  float prev = -1e9f;
  if (!buf) return 0;
  *span = 0;
  if (type == T_VIB)
  {
    static double off[400];
    int period, n, i;
    double cents, reg = 440.0 * 16777216.0 / clock, peak = 1;
    sum_vibrato(xx, yy, clock, &period, &cents);
    n = period ? period * 3 : 100;
    n = clampi(n, 48, 400);
    sum_vibrato_curve(xx, yy, clock, off, n);
    for (i = 0; i < n; i++)
    {
      double c = 1200.0 * log((reg + off[i]) / reg) / log(2.0);
      if (fabs(c) > peak) peak = fabs(c);
    }
    full = peak < 5 ? 5 : peak;
    for (px = 2; px < w - 2; px++) plot(buf, w, h, px, mid, ((px & 7) < 3) ? 70 : 20);
    for (px = 6; px < w - 6; px++)
    {
      int tick = (px - 6) * n / (w - 12);
      double c = 1200.0 * log((reg + off[tick]) / reg) / log(2.0);
      line(buf, w, h, px, (float)(mid - c / full * (mid - 10)), &prev, 1);
    }
    *span = n;
  }
  else if (type == T_PORTA)
  {
    double st;                    // semitones per second
    int s = (xx << 8) | yy;
    if (s >= 0x8000) st = rate / (double)(1 << (yy > 15 ? 15 : yy));
    else st = s * clock / 16777216.0 * rate / (440.0 * (pow(2.0, 1.0 / 12) - 1));
    full = st < 1 ? 1 : (st > 24 ? 24 : st);
    for (px = 2; px < w - 2; px++) plot(buf, w, h, px, h - 8, 70);
    // one dotted line per semitone while that does not get crowded
    if (full <= 12)
    {
      int k;
      for (k = 1; k <= (int)full; k++)
        for (px = 2; px < w - 2; px += 4) plot(buf, w, h, px, h - 8 - (int)(k / full * (h - 18)), 40);
    }
    for (px = 6; px < w - 6; px++)
    {
      double t = (px - 6) / (double)(w - 12);
      double v = st * t;
      if (v > full * 1.02) break;
      line(buf, w, h, px, (float)(h - 8 - v / full * (h - 18)), &prev, 1);
    }
    *span = (int)rate;
  }
  else
  {
    // eight rows of the funktempo: the two tempos alternate, a block is as wide as its row lasts
    int total = 0, i, x = 6;
    int t[2] = {xx ? xx : 1, yy ? yy : 1};
    for (i = 0; i < 8; i++) total += t[i & 1];
    for (i = 0; i < 8; i++)
    {
      int bw = (w - 12) * t[i & 1] / total, k, top = (i & 1) ? 22 : 38;
      for (k = 0; k < bw - 2; k++)
      {
        int py;
        for (py = top; py < h - 14; py++) plot(buf, w, h, x + k, py, (py == top || k == 0 || k == bw - 3) ? 255 : 90);
      }
      x += bw;
    }
    *span = total;
  }
  return full;
}

// ---------------------------------------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------------------------------------

typedef struct { const char *label; int xx, yy; } PRESET;

static const PRESET vibpresets[4] = {{"gentle", 0x0a, 0x06}, {"normal", 0x0c, 0x0b}, {"wide", 0x10, 0x14}, {"fast", 0x05, 0x0c}};
static const PRESET portapresets[4] = {{"slow", 0x00, 0x20}, {"medium", 0x00, 0x80}, {"fast", 0x02, 0x00}, {"very fast", 0x08, 0x00}};
static const PRESET funkpresets[4] = {{"even 6/6", 0x06, 0x06}, {"swing 7/5", 0x07, 0x05}, {"shuffle 8/4", 0x08, 0x04}, {"fast 4/2", 0x04, 0x02}};

void showspeededitor(void)
{
  int row = editorInfo.etpos;
  unsigned char savel[MAX_TABLELEN], saver[MAX_TABLELEN];
  USAGE u;
  int type, field = 0, result = 0;
  double clock = editorInfo.ntsc ? 1022727.0 : 985248.0;

  memcpy(savel, ltable[STBL], MAX_TABLELEN);
  memcpy(saver, rtable[STBL], MAX_TABLELEN);
  findusage(&u, row);
  type = guesstype(&u);
  key = 0;
  rawkey = 0;

  while (!result)
  {
    int w = 64, h = 33, x = (MAX_COLUMNS - w) / 2, y = (MAX_ROWS - h) / 2 - 1;
    int ly, bx, nb = 0, shiftflag, span = 0;
    STEPBTN btn[MAXBTN];
    char buf[120];
    int xx = ltable[STBL][row], yy = rtable[STBL][row];
    int ind = indep(row), s16 = (xx << 8) | yy;
    int fieldA = (type == T_PORTA) ? !ind : 1;        // is the first field used
    int fieldB = (type == T_PORTA) ? ind : 1;
    double full;
    const PRESET *pre = type == T_VIB ? vibpresets : (type == T_PORTA ? portapresets : funkpresets);
    CMDTABLES ct;
    CMDSUM cs;
    int k;

    for (k = 0; k < 4; k++) { ct.l[k] = ltable[k]; ct.r[k] = rtable[k]; }
    if (!fieldA && field == 0) field = 1;
    if (!fieldB && field == 1) field = 0;

    printstatus();
    dlg_drawframe(x, y, w, h, "SPEED TABLE EDITOR");
    ly = y + 2;
    snprintf(buf, sizeof buf, "Speedtable row %02X:  %02X %02X", row + 1, xx, yy);
    printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
    stepbutton(btn, &nb, x + 33, ly, "< PREV ROW", KEY_PGUP, 0, -1, 0, row > 0);
    stepbutton(btn, &nb, x + 46, ly, "NEXT ROW >", KEY_PGDN, 0, -1, 0, row < MAX_TABLELEN - 2);
    ly += 2;
    printtext(x + 3, ly, MKCOL(7, C_PANEL), "Used as");
    bx = x + 11;
    stepbutton(btn, &nb, bx, ly, "VIBRATO", KEY_1, 0, -1, type == T_VIB, 1); bx += 10;
    stepbutton(btn, &nb, bx, ly, "PORTAMENTO", KEY_2, 0, -1, type == T_PORTA, 1); bx += 13;
    stepbutton(btn, &nb, bx, ly, "FUNKTEMPO", KEY_3, 0, -1, type == T_FUNK, 1);
    printtext(x + 50, ly, MKCOL(3, C_PANEL), "(1 2 3)");
    ly += 2;

    // the values
    {
      static const int steps16[6] = {-256, -16, -1, 1, 16, 256};
      static const int steps[4] = {-16, -1, 1, 16};
      static const char *lab16[6] = {"-256", "-16", "-1", "+1", "+16", "+256"};
      static const char *lab[4] = {"-16", "-1", "+1", "+16"};
      int f;
      for (f = 0; f < 2; f++)
      {
        const char *name;
        int used = f == 0 ? fieldA : fieldB;
        int n = 4, wide = (type == T_PORTA && !ind && f == 0);
        if (!used) continue;
        if (type == T_VIB) name = f == 0 ? "Speed (XX)" : (ind ? "Depth shift (YY)" : "Depth (YY)");
        else if (type == T_PORTA) name = f == 0 ? "Speed (XXYY)" : "Shift (YY)";
        else name = f == 0 ? "Tempo A (XX)" : "Tempo B (YY)";
        printtext(x + 3, ly, MKCOL(field == f ? CEDIT : 7, C_PANEL), field == f ? ">" : " ");
        printtext(x + 4, ly, MKCOL(field == f ? CEDIT : 7, C_PANEL), name);
        if (wide) snprintf(buf, sizeof buf, "$%04X", s16);
        else if (type == T_VIB && f == 0) snprintf(buf, sizeof buf, "$%02X", xx & 0x7f);
        else snprintf(buf, sizeof buf, "$%02X", f == 0 ? xx : yy);
        printtext(x + 22, ly, MKCOL(15, C_PANEL), buf);
        bx = x + 29;
        if (wide) n = 6;
        for (k = 0; k < n; k++)
        {
          int d = wide ? steps16[k] : steps[k];
          stepbutton(btn, &nb, bx, ly, wide ? lab16[k] : lab[k], 0, 0, (f << 20) | (d + 0x80000), 0, 1);
          bx += (int)strlen(wide ? lab16[k] : lab[k]) + 3;
        }
        ly++;
      }
      if (ly < y + 8) ly = y + 8;
    }
    if (type != T_FUNK)
    {
      stepbutton(btn, &nb, x + 4, ly, ind ? "NOTE INDEPENDENT: ON" : "NOTE INDEPENDENT: OFF", KEY_I, 0, -1, ind, 1);
      printtext(x + 28, ly, MKCOL(3, C_PANEL), "(I) the effect follows the note's size");
    }
    ly += 1;
    printtext(x + 3, ly, MKCOL(7, C_PANEL), "Presets");
    bx = x + 11;
    for (k = 0; k < 4; k++)
    {
      stepbutton(btn, &nb, bx, ly, pre[k].label, 0, 0, ACT_PRESET | (pre[k].xx << 8) | pre[k].yy, 0, 1);
      bx += (int)strlen(pre[k].label) + 3;
    }
    ly += 2;

    // the picture
    full = drawpicture(type, row, 44, 5, clock, sum_tickrate(), &span);
    canvas_draw(SPEED_CANVAS, x + 9, ly, MKCOL(11, C_PANEL));
    if (type == T_VIB)
    {
      snprintf(buf, sizeof buf, "+%.0f c", full);
      printtext(x + 2, ly, MKCOL(3, C_PANEL), buf);
      snprintf(buf, sizeof buf, "-%.0f c", full);
      printtext(x + 2, ly + 4, MKCOL(3, C_PANEL), buf);
      snprintf(buf, sizeof buf, "%d ticks", span);
      printtext(x + 9, ly + 5, MKCOL(3, C_PANEL), "pitch over time (cents at A-4)");
      printtext(x + 9 + 44 - (int)strlen(buf), ly + 5, MKCOL(3, C_PANEL), buf);
    }
    else if (type == T_PORTA)
    {
      snprintf(buf, sizeof buf, "+%.0f st", full);
      printtext(x + 2, ly, MKCOL(3, C_PANEL), buf);
      printtext(x + 2, ly + 4, MKCOL(3, C_PANEL), "0");
      printtext(x + 9, ly + 5, MKCOL(3, C_PANEL), "pitch over the first second (semitones)");
    }
    else
    {
      printtext(x + 2, ly + 1, MKCOL(3, C_PANEL), "row A");
      printtext(x + 2, ly + 2, MKCOL(3, C_PANEL), "row B");
      printtext(x + 9, ly + 5, MKCOL(3, C_PANEL), "eight rows: block width = how long the row lasts");
    }
    ly += 7;

    // what it means, in words
    summarize_command(&cs, type == T_VIB ? CMD_VIBRATO : (type == T_PORTA ? CMD_PORTAUP : CMD_FUNKTEMPO), row + 1, &ct, sum_tickrate(), clock);
    ly += printwrapped(x + 3, ly, w - 6, 2, MKCOL(14, C_PANEL), cs.text);
    for (k = 0; k < cs.ndetail && k < 2; k++)
      ly += printwrapped(x + 3, ly, w - 6, 2, MKCOL(7, C_PANEL), cs.detail[k]);
    {
      char who[100];
      int i;
      who[0] = 0;
      for (i = 0; i < u.ninstr && i < 3; i++)
      {
        char t[16];
        snprintf(t, sizeof t, "%sinstrument %02X", who[0] ? ", " : "", u.instr[i]);
        strncat(who, t, sizeof who - strlen(who) - 1);
      }
      for (i = 1; i < 16; i++)
        if (u.cmd[i] || u.wave[i])
        {
          char t[40];
          snprintf(t, sizeof t, "%s%dx %X-cmd%s", who[0] ? ", " : "", u.cmd[i] + u.wave[i], i, u.wave[i] ? " (wave)" : "");
          strncat(who, t, sizeof who - strlen(who) - 1);
        }
      if (who[0]) snprintf(buf, sizeof buf, "Used by: %s", who);
      else snprintf(buf, sizeof buf, "Not used yet: use %X%02X in a pattern or the instrument's Vibrato Param.", type == T_VIB ? 4 : (type == T_PORTA ? 1 : 0xe), row + 1);
      buf[w - 6] = 0;
      printtext(x + 3, y + h - 4, MKCOL(3, C_PANEL), buf);
    }
    stepbutton(btn, &nb, x + 3, y + h - 3, "KEEP CHANGES", KEY_ENTER, 0, -1, 0, 1);
    stepbutton(btn, &nb, x + 20, y + h - 3, "UNDO ALL", KEY_ESC, 0, -1, 0, 1);
    printtext(x + 32, y + h - 3, MKCOL(3, C_PANEL), "(ENTER, ESC)  LEFT/RIGHT field, UP/DOWN value");
    fliptoscreen();
    getkey();

    shiftflag = shiftpressed;
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb))
    {
      for (k = 0; k < nb; k++)
        if ((mousey == btn[k].y) && (mousex >= btn[k].x) && (mousex < btn[k].x + btn[k].w))
        {
          int a = btn[k].setwidth;
          if (a >= ACT_PRESET)
          {
            ltable[STBL][row] = (a >> 8) & 0xff;
            rtable[STBL][row] = a & 0xff;
          }
          else if (a >= 0)
          {
            int f = a >> 20, d = (a & 0xfffff) - 0x80000;
            field = f;
            {
              int nx = xx, ny = yy;
              if (type == T_PORTA && !ind && f == 0)
              {
                int v = clampi(s16 + d, 0, 0x7fff);
                nx = v >> 8; ny = v & 255;
              }
              else if (type == T_VIB && f == 0) nx = (ind ? 0x80 : 0) | clampi((xx & 0x7f) + d, 0, 0x7f);
              else if (f == 0) nx = clampi(xx + d, 0, 255);
              else ny = clampi(yy + d, 0, (ind && type != T_FUNK) ? 15 : 255);
              ltable[STBL][row] = nx;
              rtable[STBL][row] = ny;
            }
          }
          else { rawkey = btn[k].rk; shiftflag = btn[k].shift; }
          break;
        }
    }
    if (win_quitted || rawkey == KEY_ESC) result = 2;
    if (rawkey == KEY_ENTER) result = 1;
    if (rawkey == KEY_1) type = T_VIB;
    if (rawkey == KEY_2) type = T_PORTA;
    if (rawkey == KEY_3) type = T_FUNK;
    if (rawkey == KEY_LEFT) field = 0;
    if (rawkey == KEY_RIGHT) field = 1;
    if ((rawkey == KEY_PGUP) && (row > 0)) { row--; findusage(&u, row); type = guesstype(&u); }
    if ((rawkey == KEY_PGDN) && (row < MAX_TABLELEN - 2)) { row++; findusage(&u, row); type = guesstype(&u); }
    if (rawkey == KEY_I && type != T_FUNK)
    {
      if (ind) ltable[STBL][row] &= 0x7f; else ltable[STBL][row] |= 0x80;
      if (!ind && rtable[STBL][row] > 15 && type == T_PORTA) rtable[STBL][row] = 2;
      if (!ind && type == T_VIB && rtable[STBL][row] > 15) rtable[STBL][row] = 2;
    }
    if ((rawkey == KEY_UP) || (rawkey == KEY_DOWN))
    {
      int d = (rawkey == KEY_UP ? 1 : -1) * (shiftflag ? (type == T_PORTA && !ind && field == 0 ? 256 : 16) : 1);
      int nx = xx, ny = yy;
      if (type == T_PORTA && !ind && field == 0)
      {
        int v = clampi(s16 + d, 0, 0x7fff);
        nx = v >> 8; ny = v & 255;
      }
      else if (type == T_VIB && field == 0) nx = (ind ? 0x80 : 0) | clampi((xx & 0x7f) + d, 0, 0x7f);
      else if (field == 0) nx = clampi(xx + d, 0, 255);
      else ny = clampi(yy + d, 0, (ind && type != T_FUNK) ? 15 : 255);
      ltable[STBL][row] = nx;
      rtable[STBL][row] = ny;
    }
  }
  if (result == 2)
  {
    memcpy(ltable[STBL], savel, MAX_TABLELEN);
    memcpy(rtable[STBL], saver, MAX_TABLELEN);
  }
  key = 0;
  rawkey = 0;
  printmainscreen();
}
