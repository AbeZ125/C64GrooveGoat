//
// C64 GrooveGoat - the step editor: a wave or pulse program edited as one value per tick (SHIFT+E in the table
// editor). The model and the table rows it makes are in gstepedit.c.
//

#include <math.h>
#include "goattrk2.h"
#include "gsummary.h"
#include "gwsummary.h"
#include "gfsummary.h"
#include "gstepedit.h"
#include "gsumui.h"

#define STEP_CANVAS 1
#define TICKW 3                   // cells per tick in the strip
#define STRIP_X 8                 // strip offset in the window
#define PAD_CANVAS 4
#define PADH 6                    // draw pad height in cells
#define PAD_TOP 4                 // pad drawing area inside the canvas, in pixels
#define PAD_BOT (PADH * 32 - 5)
#define VIS 32                    // ticks on screen at once; longer programs scroll

void stepbutton(STEPBTN *list, int *nb, int x, int y, const char *label, int rk, int shift, int setwidth, int on, int enabled)
{
  char t[40];
  snprintf(t, sizeof t, " %s ", label);
  printtext(x, y, on ? MKCOL(C_HEADERTEXT, C_BUTTONHI) : MKCOL(enabled ? 15 : 3, C_LINE), t);
  if (*nb < MAXBTN && enabled)
  {
    list[*nb].x = x; list[*nb].y = y; list[*nb].w = (int)strlen(t);
    list[*nb].rk = rk; list[*nb].shift = shift; list[*nb].setwidth = setwidth;
    (*nb)++;
  }
}

static void plot(unsigned char *buf, int w, int h, int x, int y, int a)
{
  if ((x < 0) || (y < 0) || (x >= w) || (y >= h)) return;
  if (buf[y * CANVAS_STRIDE + x] < a) buf[y * CANVAS_STRIDE + x] = a;
}

// the note column as a person reads it: "+7", "-3", "C-4"
static void notestr(char *buf, int size, int b)
{
  if (b >= 0x81) wave_notename(buf, size, b & 0x7f);
  else if (b < 0x60) snprintf(buf, size, "%s%d", b ? "+" : "", b);
  else snprintf(buf, size, "-%d", 128 - b);
}

static int notevalue(int b)             // semitones for a relative note byte
{
  return b < 0x60 ? b : b - 128;
}

static int notebyte(int v)
{
  return v >= 0 ? v : v + 128;
}

static int addtick(STEPMODEL *m, int *sel)
{
  int k;
  if (m->n >= STEP_MAX) return 0;
  for (k = m->n; k > *sel + 1; k--) { m->wave[k] = m->wave[k - 1]; m->note[k] = m->note[k - 1]; m->width[k] = m->width[k - 1]; m->cutoff[k] = m->cutoff[k - 1]; }
  m->wave[*sel + 1] = m->wave[*sel]; m->note[*sel + 1] = m->note[*sel]; m->width[*sel + 1] = m->width[*sel]; m->cutoff[*sel + 1] = m->cutoff[*sel];
  if (m->loop > *sel + 1) m->loop++;
  m->n++;
  (*sel)++;
  return 1;
}

static int removetick(STEPMODEL *m, int *sel)
{
  int k;
  if (m->n <= 1) return 0;
  for (k = *sel; k < m->n - 1; k++) { m->wave[k] = m->wave[k + 1]; m->note[k] = m->note[k + 1]; m->width[k] = m->width[k + 1]; m->cutoff[k] = m->cutoff[k + 1]; }
  if (m->loop > *sel + 1) m->loop--;
  m->n--;
  if (m->loop > m->n) m->loop = m->n > 1 ? m->n : 0;
  if (*sel >= m->n) *sel = m->n - 1;
  return 1;
}

static void drawstrip(const STEPMODEL *m, int sel, int cw, int scroll)
{
  unsigned char *cv = canvas_begin(STEP_CANVAS, cw, 2);
  int w = cw * 16, h = 64, i, k;
  int shapes = sum_pulseshapes();
  int last = m->n < scroll + VIS ? m->n : scroll + VIS;
  if (!cv) return;
  for (i = scroll; i < last; i++)
  {
    int x0 = (i - scroll) * TICKW * 16;
    if (m->mode == STEP_FILTER)
    {
      // a bar as high as the cutoff
      int bh = m->cutoff[i] * (h - 14) / 255, bx, by;
      for (bx = x0 + 8; bx <= x0 + 40; bx++)
        for (by = h - 7 - bh; by <= h - 7; by++)
          plot(cv, w, h, bx, by, (by == h - 7 - bh || bx == x0 + 8 || bx == x0 + 40) ? 255 : 90);
    }
    else if (m->mode == STEP_WAVE) sum_drawshapes(cv, w, h, x0 + 5, x0 + 42, 9, 52, m->wave[i] & 0xf0, 1, 0.5f);
    else sum_drawshapes(cv, w, h, x0 + 5, x0 + 42, 9, 52, shapes, 1, m->width[i] / 4096.0f);
    // tick separators, and a frame round the selected tick
    for (k = 2; k < h - 2; k++) plot(cv, w, h, x0, k, i == sel ? 255 : 40);
    if (i == sel)
    {
      for (k = x0; k <= x0 + TICKW * 16; k++) { plot(cv, w, h, k, 2, 255); plot(cv, w, h, k, h - 3, 255); }
      for (k = 2; k < h - 2; k++) plot(cv, w, h, x0 + TICKW * 16, k, 255);
    }
  }
  for (k = 2; k < h - 2; k++) plot(cv, w, h, (last - scroll) * TICKW * 16, k, 40);
}

// The draw pad: one bar per tick, as high as the value. Dragging the mouse over it sets the values (see showstepeditor).
static void drawpad(const STEPMODEL *m, int sel, int cw, int scroll)
{
  unsigned char *cv = canvas_begin(PAD_CANVAS, cw, PADH);
  int w = cw * 16, h = PADH * 32, i, k, q;
  int base = m->mode == STEP_WAVE ? (PAD_TOP + PAD_BOT) / 2 : PAD_BOT;
  int last = m->n < scroll + VIS ? m->n : scroll + VIS;
  if (!cv) return;
  for (q = 0; q <= 4; q++)                         // guide lines at 0, 25, 50, 75, 100 %
  {
    int gy = PAD_BOT - q * (PAD_BOT - PAD_TOP) / 4;
    for (k = 0; k < (last - scroll) * TICKW * 16; k += (q == 2 ? 2 : 4)) plot(cv, w, h, k, gy, q == 2 ? 110 : 55);
  }
  for (i = scroll; i < last; i++)
  {
    int x0 = (i - scroll) * TICKW * 16;
    double g = step_padget(m, i);
    if (g < 0)                                     // an absolute note: not drawn here
    {
      for (k = x0 + 6; k < x0 + 42; k += 4) plot(cv, w, h, k, base, 160);
    }
    else
    {
      int vy = PAD_BOT - (int)(g * (PAD_BOT - PAD_TOP) + 0.5), y0 = vy < base ? vy : base, y1 = vy < base ? base : vy, bx, by;
      for (bx = x0 + 6; bx <= x0 + 42; bx++)
        for (by = y0; by <= y1; by++)
          plot(cv, w, h, bx, by, i == sel ? 150 : 90);
      for (bx = x0 + 6; bx <= x0 + 42; bx++) { plot(cv, w, h, bx, vy, 255); plot(cv, w, h, bx, vy - 1, 255); plot(cv, w, h, bx, vy + 1, 255); }
    }
    for (k = 2; k < h - 2; k++) plot(cv, w, h, x0, k, i == sel ? 255 : 40);
    if (i == sel)
    {
      for (k = x0; k <= x0 + TICKW * 16; k++) { plot(cv, w, h, k, 2, 255); plot(cv, w, h, k, h - 3, 255); }
      for (k = 2; k < h - 2; k++) plot(cv, w, h, x0 + TICKW * 16, k, 255);
    }
  }
  for (k = 2; k < h - 2; k++) plot(cv, w, h, (last - scroll) * TICKW * 16, k, 40);
}

void showstepeditor(void)
{
  int table = editorInfo.etnum;
  int cursorrow;
  int start;
  STEPMODEL m;
  unsigned char gl[300], gr[300], savel[MAX_TABLELEN], saver[MAX_TABLELEN];
  int nrows, cap, fits, room = 0;
  int sel = 0, scroll = 0, vis, live = 1, result = 0;           // result: 1 = keep, 2 = undo
  int status;
  const char *msg = "";
  int win_w = 64;
  int drawing = 0, lasttick = -1;              // drawing in the pad with the mouse
  double lastv = 0;
  int arpidx = -1, arphold = 1, arpping = 0;    // the arpeggio builder: preset (ARP_PRESETS = typed), ticks per note, up-down
  int carp[ARP_MAXNOTES], ncarp = 0;            // the typed intervals

  if ((table != WTBL) && (table != PTBL) && (table != FTBL))
  {
    const char *lines[3] = {"Step editor", "", "The step editor is for the wave, pulse and filter tables."};
    dlg_message("Step editor", lines, 3);
    return;
  }
  start = sum_currentprogram(table, &cursorrow);
  if (!start) start = 1;
  status = step_import(&m, table == WTBL ? STEP_WAVE : (table == PTBL ? STEP_PULSE : STEP_FILTER), ltable[table], rtable[table], start);
  if (status == STEP_COMMANDS)
  {
    const char *lines[3] = {"Step editor", "", "This program has pattern commands: edit it in the table."};
    dlg_message("Step editor", lines, 3);
    return;
  }
  if (status == STEP_COMPLEX)
  {
    const char *lines[3] = {"Step editor", "", "This filter program changes the passband or voices on the way: edit it in the table."};
    dlg_message("Step editor", lines, 3);
    return;
  }
  if (status == STEP_TOOLONG)
  {
    char text[80];
    const char *lines[3] = {"Step editor", "", text};
    snprintf(text, sizeof text, "This program is longer than %d ticks: edit it in the table.", STEP_MAX);
    dlg_message("Step editor", lines, 3);
    return;
  }
  memcpy(savel, ltable[table], MAX_TABLELEN);
  memcpy(saver, rtable[table], MAX_TABLELEN);
  key = 0;
  rawkey = 0;

  while (!result)
  {
    int shown, vis2;
    if (sel < scroll) scroll = sel;                // keep the selected tick on screen
    if (sel >= scroll + VIS) scroll = sel - VIS + 1;
    if (scroll > m.n - VIS) scroll = m.n - VIS;
    if (scroll < 0) scroll = 0;
    vis2 = m.n - scroll < VIS ? m.n - scroll : VIS;
    vis = vis2;
    shown = vis2 > 16 ? vis2 : 16;                // ticks the strip and pad have room for; the window widens for long programs
    int h = (table == FTBL ? 38 : (table == WTBL ? 35 : 31)) + PADH + 2, x, y = (MAX_ROWS - h) / 2 - 1;
    int x0, ly, i;
    char buf[100], t[24];
    int changed = 0;
    STEPBTN btn[MAXBTN];
    int nb = 0, shiftflag, padrow;

    win_w = TICKW * shown + 16;
    x = (MAX_COLUMNS - win_w) / 2;
    x0 = x + STRIP_X;

    // The rows this makes, and whether they fit in the table in place of the old program
    {
      unsigned char cl[MAX_TABLELEN], cr[MAX_TABLELEN];
      memcpy(cl, savel, MAX_TABLELEN);
      memcpy(cr, saver, MAX_TABLELEN);
      nrows = step_generate(&m, gl, gr, start);
      cap = step_capacity(cl, cr, start);
      fits = nrows <= cap;
      room = (nrows - cap) <= (255 - step_tablelen(cl, cr));     // rows could be made by shifting the later programs down
    }

    printstatus();
    dlg_drawframe(x, y, win_w, h, table == WTBL ? "WAVE STEP EDITOR" : (table == PTBL ? "PULSE STEP EDITOR" : "FILTER STEP EDITOR"));
    ly = y + 2;
    if (m.n > VIS) snprintf(buf, sizeof buf, "Row %02X, %d ticks (%d-%d)", start, m.n, scroll + 1, scroll + vis);
    else snprintf(buf, sizeof buf, "Program at row %02X, %d tick%s", start, m.n, m.n == 1 ? "" : "s");
    printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
    printtext(x + 38, ly, MKCOL(m.n > 1 ? 15 : 3, C_PANEL), "[ - ] remove tick");
    printtext(x + 38 + 18, ly, MKCOL(m.n < STEP_MAX ? 15 : 3, C_PANEL), "[ + ]");
    ly += 2;

    drawstrip(&m, sel, TICKW * shown, scroll);
    canvas_draw(STEP_CANVAS, x0, ly, MKCOL(11, C_PANEL));
    printtext(x + 2, ly, MKCOL(3, C_PANEL), table == WTBL ? "wave" : (table == PTBL ? "pulse" : "cutoff"));
    printtext(x + 2, ly + 1, MKCOL(3, C_PANEL), table == FTBL ? "bars" : "shape");
    ly += 2;

    for (i = scroll; i < scroll + vis; i++)
    {
      int cx = x0 + (i - scroll) * TICKW + 1, c = (i == sel) ? CEDIT : 15;
      snprintf(buf, sizeof buf, "%d", i + 1);
      printtext(cx, ly, MKCOL(i == sel ? CEDIT : 3, C_PANEL), buf);
      if (table == WTBL)
      {
        snprintf(buf, sizeof buf, "%02X", m.wave[i]);
        printtext(cx, ly + 1, MKCOL(c, C_PANEL), buf);
        notestr(t, sizeof t, m.note[i]);
        printtext(x0 + (i - scroll) * TICKW, ly + 2, MKCOL(c, C_PANEL), t);
      }
      else if (table == FTBL)
      {
        snprintf(buf, sizeof buf, "%d", m.cutoff[i] * 100 / 255);
        printtext(cx, ly + 1, MKCOL(c, C_PANEL), buf);
        snprintf(buf, sizeof buf, "%02X", m.cutoff[i]);
        printtext(x0 + (i - scroll) * TICKW + 1, ly + 2, MKCOL(c, C_PANEL), buf);
      }
      else
      {
        snprintf(buf, sizeof buf, "%d", m.width[i] * 100 / 4096);
        printtext(cx, ly + 1, MKCOL(c, C_PANEL), buf);
        snprintf(buf, sizeof buf, "%03X", m.width[i]);
        printtext(x0 + (i - scroll) * TICKW, ly + 2, MKCOL(c, C_PANEL), buf);
      }
      if (m.loop == i + 1) printtext(cx, ly + 3, MKCOL(14, C_PANEL), "L");
    }
    printtext(x + 2, ly, MKCOL(3, C_PANEL), "tick");
    printtext(x + 2, ly + 1, MKCOL(3, C_PANEL), table == WTBL ? "code" : (table == PTBL ? "wid %" : "cut %"));
    printtext(x + 2, ly + 2, MKCOL(3, C_PANEL), table == WTBL ? "note" : (table == PTBL ? "width" : "cutoff"));
    printtext(x + 2, ly + 3, MKCOL(3, C_PANEL), "loop");
    ly += 5;

    padrow = ly;
    drawpad(&m, sel, TICKW * shown, scroll);
    canvas_draw(PAD_CANVAS, x0, padrow, MKCOL(11, C_PANEL));
    printtext(x + 2, padrow, MKCOL(3, C_PANEL), table == WTBL ? "+24" : "100%");
    printtext(x + 2, padrow + PADH / 2, MKCOL(3, C_PANEL), table == WTBL ? "  0" : " 50%");
    printtext(x + 2, padrow + PADH - 1, MKCOL(3, C_PANEL), table == WTBL ? "-24" : "  0%");
    ly += PADH;
    printtext(x + 3, ly, MKCOL(7, C_PANEL), table == WTBL ? "Draw the pitch: click and drag in the pad (SHIFT: no snapping)" : "Draw it: click and drag in the pad (hold SHIFT for fine steps)");
    ly += 2;

    if (table == WTBL)
    {
      wave_name(t, sizeof t, m.wave[sel]);
      snprintf(buf, sizeof buf, "Tick %d: %s", sel + 1, t);
      printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
      {
        char n[40];
        notestr(n, sizeof n, m.note[sel]);
        snprintf(buf, sizeof buf, "note %s%s", n, m.note[sel] >= 0x81 ? " (absolute)" : " (from the played note)");
        printtext(x + 3 + 8 + (int)strlen(t), ly, MKCOL(7, C_PANEL), buf);
      }
    }
    else if (table == FTBL)
    {
      char v[16], hz[24];
      double f = filter_cutoffhz(m.cutoff[sel]);
      filter_voices(v, sizeof v, m.fvoices);
      if (f < 1000) snprintf(hz, sizeof hz, "%.0f Hz", f); else snprintf(hz, sizeof hz, "%.1f kHz", f / 1000);
      snprintf(buf, sizeof buf, "Tick %d: cutoff $%02X = %d%% (about %s)", sel + 1, m.cutoff[sel], m.cutoff[sel] * 100 / 255, hz);
      printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
      ly++;
      snprintf(buf, sizeof buf, "%s, resonance %X, voices %s", filter_passband(m.ftype), m.fres, v);
      printtext(x + 3, ly, MKCOL(7, C_PANEL), buf);
      {
        unsigned char *rv = canvas_begin(3, 44, 3);
        if (rv)
        {
          sum_drawresponse(rv, 44 * 16, 96, 8, 44 * 16 - 8, 8, 82, m.ftype, m.fres, m.cutoff[sel], 255);
          canvas_draw(3, x + 9, ly + 1, MKCOL(11, C_PANEL));
        }
        printtext(x + 2, ly + 1, MKCOL(3, C_PANEL), "level");
        printtext(x + 2, ly + 3, MKCOL(3, C_PANEL), "(this");
        printtext(x + 2, ly + 4, MKCOL(3, C_PANEL), "tick)");
        printtext(x + 9, ly + 4, MKCOL(3, C_PANEL), "30 Hz");
        printtext(x + 9 + 44 - 6, ly + 4, MKCOL(3, C_PANEL), "16 kHz");
        ly += 4;
      }
    }
    else
    {
      snprintf(buf, sizeof buf, "Tick %d: pulse width %d.%d%% ($%03X)", sel + 1, m.width[sel] * 100 / 4096, (m.width[sel] * 1000 / 4096) % 10, m.width[sel]);
      printtext(x + 3, ly, MKCOL(15, C_PANEL), buf);
    }
    ly++;
    if (m.loop) snprintf(buf, sizeof buf, "After tick %d it jumps back to tick %d and repeats.", m.n, m.loop);
    else snprintf(buf, sizeof buf, "After tick %d it ends and the last %s stays.", m.n, table == WTBL ? "waveform and note" : (table == PTBL ? "width" : "cutoff"));
    printtext(x + 3, ly, MKCOL(7, C_PANEL), buf);
    ly++;
    snprintf(buf, sizeof buf, "%d row%s needed, %d available%s", nrows, nrows == 1 ? "" : "s", cap, live ? "   (live: the table follows your edits)" : "");
    printtext(x + 3, ly, MKCOL(fits ? 7 : 14, C_PANEL), buf);
    if (!fits && room)
    {
      snprintf(buf, sizeof buf, "%d more row%s than fit: KEEP moves the programs after this one down to make room.", nrows - cap, nrows - cap == 1 ? "" : "s");
      printtext(x + 3, ly + 1, MKCOL(14, C_PANEL), buf);
    }
    else if (!fits) printtext(x + 3, ly + 1, MKCOL(14, C_PANEL), "The table is full: remove ticks, or free rows in this table first (SHIFT+O there).");
    else if (instr[editorInfo.einum].ptr[table] != start)
    {
      snprintf(buf, sizeof buf, "Note: instrument %02X uses row %02X of this table, not this program.", editorInfo.einum, instr[editorInfo.einum].ptr[table]);
      printtext(x + 3, ly + 1, MKCOL(7, C_PANEL), buf);
    }
    else if (msg[0]) printtext(x + 3, ly + 1, MKCOL(14, C_PANEL), msg);
    ly += 3;

    {
      int bx, w0 = m.wave[sel], k;
      if (table == WTBL)
      {
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Shape");
        bx = x + 10;
        stepbutton(btn, &nb, bx, ly, "TRI", KEY_T, 0, -1, (w0 & 0x10) != 0, 1); bx += 6;
        stepbutton(btn, &nb, bx, ly, "SAW", KEY_S, 0, -1, (w0 & 0x20) != 0, 1); bx += 6;
        stepbutton(btn, &nb, bx, ly, "PULSE", KEY_P, 0, -1, (w0 & 0x40) != 0, 1); bx += 8;
        stepbutton(btn, &nb, bx, ly, "NOISE", KEY_N, 0, -1, (w0 & 0x80) != 0, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "(T S P N)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Bits");
        bx = x + 10;
        stepbutton(btn, &nb, bx, ly, "GATE", KEY_G, 0, -1, (w0 & 0x01) != 0, 1); bx += 7;
        stepbutton(btn, &nb, bx, ly, "SYNC", KEY_Y, 0, -1, (w0 & 0x02) != 0, 1); bx += 7;
        stepbutton(btn, &nb, bx, ly, "RING", KEY_R, 0, -1, (w0 & 0x04) != 0, 1); bx += 7;
        stepbutton(btn, &nb, bx, ly, "TEST", KEY_X, 0, -1, (w0 & 0x08) != 0, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "(G Y R X)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Note");
        bx = x + 10;
        stepbutton(btn, &nb, bx, ly, "-12", KEY_DOWN, 1, -1, 0, 1); bx += 6;
        stepbutton(btn, &nb, bx, ly, "-1", KEY_DOWN, 0, -1, 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "+1", KEY_UP, 0, -1, 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "+12", KEY_UP, 1, -1, 0, 1); bx += 7;
        stepbutton(btn, &nb, bx, ly, m.note[sel] >= 0x81 ? "ABSOLUTE" : "RELATIVE", KEY_A, 0, -1, m.note[sel] >= 0x81, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "(UP DOWN, A)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Arp");
        bx = x + 10;
        for (k = 0; k < 6; k++)
        {
          stepbutton(btn, &nb, bx, ly, step_arp_name(k), 0, 0, 0x3000 | k, arpidx == k, 1);
          bx += (int)strlen(step_arp_name(k)) + 3;
        }
        snprintf(buf, sizeof buf, "note = %.0f ms", 1000.0 / sum_tickrate() * arphold);
        printtext(x + 50, ly, MKCOL(3, C_PANEL), buf);
        ly++;
        bx = x + 10;
        for (k = 6; k < ARP_PRESETS; k++)
        {
          stepbutton(btn, &nb, bx, ly, step_arp_name(k), 0, 0, 0x3000 | k, arpidx == k, 1);
          bx += (int)strlen(step_arp_name(k)) + 3;
        }
        stepbutton(btn, &nb, bx, ly, "TYPE...", 0, 0, 0x3300, arpidx == ARP_PRESETS, 1);
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Speed");
        bx = x + 10;
        {
          static const int holds[6] = {1, 2, 3, 4, 6, 8};
          for (k = 0; k < 6; k++)
          {
            snprintf(buf, sizeof buf, "%dT", holds[k]);
            stepbutton(btn, &nb, bx, ly, buf, 0, 0, 0x3100 | holds[k], arphold == holds[k], 1);
            bx += 5;
          }
        }
        stepbutton(btn, &nb, bx + 1, ly, arpping ? "UP-DOWN" : "UP", 0, 0, 0x3200, arpping, 1);
        ly++;
      }
      else if (table == FTBL)
      {
        static const int pct[5] = {10, 30, 50, 70, 90};
        int k;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Passband");
        bx = x + 13;
        stepbutton(btn, &nb, bx, ly, "LOW", KEY_1, 0, -1, (m.ftype & 0x10) != 0, 1); bx += 7;
        stepbutton(btn, &nb, bx, ly, "BAND", KEY_2, 0, -1, (m.ftype & 0x20) != 0, 1); bx += 8;
        stepbutton(btn, &nb, bx, ly, "HIGH", KEY_3, 0, -1, (m.ftype & 0x40) != 0, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "(1 2 3; several = notch ...)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Resonance");
        bx = x + 13;
        stepbutton(btn, &nb, bx, ly, "-", KEY_BRACKETL, 0, -1, 0, 1); bx += 5;
        snprintf(buf, sizeof buf, "%X", m.fres);
        printtext(bx, ly, MKCOL(15, C_PANEL), buf); bx += 3;
        stepbutton(btn, &nb, bx, ly, "+", KEY_BRACKETR, 0, -1, 0, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "([ and ])");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Voices");
        bx = x + 13;
        stepbutton(btn, &nb, bx, ly, "1", KEY_4, 0, -1, (m.fvoices & 1) != 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "2", KEY_5, 0, -1, (m.fvoices & 2) != 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "3", KEY_6, 0, -1, (m.fvoices & 4) != 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "EXT", KEY_7, 0, -1, (m.fvoices & 8) != 0, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "(4 5 6 7)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Cutoff");
        bx = x + 13;
        stepbutton(btn, &nb, bx, ly, "-32", KEY_DOWN, 1, -1, 0, 1); bx += 6;
        stepbutton(btn, &nb, bx, ly, "-8", KEY_DOWN, 0, -1, 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "+8", KEY_UP, 0, -1, 0, 1); bx += 5;
        stepbutton(btn, &nb, bx, ly, "+32", KEY_UP, 1, -1, 0, 1);
        printtext(x + 40, ly, MKCOL(3, C_PANEL), "(UP DOWN)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Set to");
        bx = x + 13;
        for (k = 0; k < 5; k++)
        {
          snprintf(buf, sizeof buf, "%d%%", pct[k]);
          stepbutton(btn, &nb, bx, ly, buf, 0, 0, 0x1000 | (pct[k] * 255 / 100), (m.cutoff[sel] * 100 / 255) == pct[k], 1);
          bx += (int)strlen(buf) + 3;
        }
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "All ticks");
        bx = x + 13;
        stepbutton(btn, &nb, bx, ly, "SWEEP UP", 0, 0, 0x2000, 0, m.n > 1); bx += 11;
        stepbutton(btn, &nb, bx, ly, "SWEEP DOWN", 0, 0, 0x2001, 0, m.n > 1); bx += 13;
        stepbutton(btn, &nb, bx, ly, "ALL = THIS", 0, 0, 0x2002, 0, 1);
        ly++;
      }
      else
      {
        static const int pct[5] = {5, 10, 25, 50, 75};
        int k;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Width");
        bx = x + 10;
        stepbutton(btn, &nb, bx, ly, "-12.5%", KEY_DOWN, 1, -1, 0, 1); bx += 9;
        stepbutton(btn, &nb, bx, ly, "-1.6%", KEY_DOWN, 0, -1, 0, 1); bx += 8;
        stepbutton(btn, &nb, bx, ly, "+1.6%", KEY_UP, 0, -1, 0, 1); bx += 8;
        stepbutton(btn, &nb, bx, ly, "+12.5%", KEY_UP, 1, -1, 0, 1);
        printtext(x + 50, ly, MKCOL(3, C_PANEL), "(UP DOWN)");
        ly++;
        printtext(x + 3, ly, MKCOL(7, C_PANEL), "Set to");
        bx = x + 10;
        for (k = 0; k < 5; k++)
        {
          snprintf(buf, sizeof buf, "%d%%", pct[k]);
          stepbutton(btn, &nb, bx, ly, buf, 0, 0, pct[k] * 4096 / 100, (m.width[sel] * 100 / 4096) == pct[k], 1);
          bx += (int)strlen(buf) + 3;
        }
        ly++;
      }
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Loop");
      bx = x + 10;
      stepbutton(btn, &nb, bx, ly, "LOOP FROM HERE", KEY_L, 0, -1, m.loop == sel + 1, 1);
      printtext(x + 28, ly, MKCOL(3, C_PANEL), "(L) repeat from this tick");
      ly++;
      printtext(x + 3, ly, MKCOL(7, C_PANEL), "Sound");
      bx = x + 10;
      stepbutton(btn, &nb, bx, ly, "PLAY", KEY_SPACE, 0, -1, 0, 1); bx += 8;
      stepbutton(btn, &nb, bx, ly, "RELEASE", KEY_SPACE, 1, -1, 0, 1); bx += 11;
      stepbutton(btn, &nb, bx, ly, live ? "LIVE: ON" : "LIVE: OFF", KEY_U, 0, -1, live, 1);
      printtext(x + 40, ly, MKCOL(3, C_PANEL), "(SPACE, U)");
      ly += 2;
      stepbutton(btn, &nb, x + 10, ly, "KEEP CHANGES", KEY_ENTER, 0, -1, 0, 1);
      stepbutton(btn, &nb, x + 27, ly, "UNDO ALL", KEY_ESC, 0, -1, 0, 1);
      printtext(x + 40, ly, MKCOL(3, C_PANEL), "(ENTER, ESC)");
      ly++;
      printtext(x + 3, ly, MKCOL(3, C_PANEL), "LEFT/RIGHT choose tick (PGUP PGDN HOME END jump)   + / - add or remove a tick");
    }
    fliptoscreen();
    getkey();
    shiftflag = shiftpressed;

    // drawing: press in the pad and drag; the ticks in between are filled in
    if (!(mouseb & MOUSEB_LEFT)) { drawing = 0; lasttick = -1; }
    else if ((!prevmouseb) && (mousefx >= x0) && (mousefx < x0 + vis * TICKW) && (mousefy >= padrow) && (mousefy < padrow + PADH)) drawing = 1;
    if (drawing)
    {
      int t = scroll + (int)floor((mousefx - x0) / TICKW);
      double v = (PAD_BOT - (mousefy - padrow) * 32.0) / (PAD_BOT - PAD_TOP);
      if (t < 0) t = 0;
      if (t > m.n - 1) t = m.n - 1;
      v = v < 0 ? 0 : (v > 1 ? 1 : v);
      if ((t != lasttick) || (fabs(v - lastv) > 1e-6))
      {
        if (lasttick >= 0) step_paint(&m, lasttick, lastv, t, v, shiftflag);
        else step_paint(&m, t, v, t, v, shiftflag);
        lasttick = t;
        lastv = v;
        sel = t;
        changed = 1;
        msg = "";
      }
    }

    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb))
    {
      int k;
      for (k = 0; k < nb; k++)
        if ((mousey == btn[k].y) && (mousex >= btn[k].x) && (mousex < btn[k].x + btn[k].w))
        {
          if ((btn[k].setwidth >= 0) && (table == WTBL))
          {
            int a = btn[k].setwidth, cnt = 0, sv[ARP_MAXNOTES];
            if ((a & 0xff00) == 0x3000) arpidx = a & 0xff;
            else if ((a & 0xff00) == 0x3100) arphold = a & 0xff;
            else if ((a & 0xff00) == 0x3200) arpping ^= 1;
            else if ((a & 0xff00) == 0x3300)
            {
              char text[64] = "";
              int j;
              for (j = 0; j < ncarp; j++) snprintf(text + strlen(text), sizeof text - strlen(text), "%s%d", j ? " " : "", carp[j]);
              if (!ncarp) snprintf(text, sizeof text, "0 4 7");
              if (dlg_edittext("ARPEGGIO", "Semitones above the note:", text, 40, 0))
              {
                int pc = step_arp_parse(text, sv, ARP_MAXNOTES);
                if (pc < 0) msg = "Type the semitones separated by spaces, like 0 4 7 (up to 32, -32 to 95).";
                else { ncarp = pc; memcpy(carp, sv, sizeof(int) * pc); arpidx = ARP_PRESETS; }
              }
              key = 0; rawkey = 0;
            }
            if (arpidx >= 0 && arpidx < ARP_PRESETS) cnt = step_arp_preset(arpidx, sv);
            else if (arpidx == ARP_PRESETS) { cnt = ncarp; memcpy(sv, carp, sizeof(int) * ncarp); }
            if (cnt > 0)
            {
              int wv = (m.wave[sel] & 0xf0) ? m.wave[sel] : 0x41;
              if (step_arp(&m, sv, cnt, arphold, arpping, wv)) { sel = 0; changed = 1; msg = ""; }
              else msg = "That is too many ticks: use a shorter speed, UP instead of UP-DOWN, or fewer notes.";
            }
            else if (arpidx < 0) msg = "Choose a chord first (MAJ, MIN, ...) or TYPE the semitones yourself.";
          }
          else if (btn[k].setwidth >= 0)
          {
            int a = btn[k].setwidth, j;
            if (table == FTBL && a >= 0x2000)
            {
              int mode = a & 0xf, from = m.cutoff[sel];
              for (j = 0; j < m.n; j++)
                m.cutoff[j] = mode == 0 ? (m.n > 1 ? j * 255 / (m.n - 1) : 0) : (mode == 1 ? (m.n > 1 ? 255 - j * 255 / (m.n - 1) : 255) : from);
            }
            else if (table == FTBL) m.cutoff[sel] = a & 0xff;
            else m.width[sel] = a;
            changed = 1;
          }
          else { rawkey = btn[k].rk; shiftflag = btn[k].shift; }
          break;
        }
    }

    // keys
    if (win_quitted) result = 2;
    if (rawkey == KEY_ESC) result = 2;
    if (rawkey == KEY_ENTER)
    {
      if (fits || room) result = 1;
      else msg = "Does not fit yet.";
    }
    if (rawkey == KEY_LEFT && sel > 0) sel--;
    if (rawkey == KEY_RIGHT && sel < m.n - 1) sel++;
    if (rawkey == KEY_PGUP) sel = sel > 16 ? sel - 16 : 0;
    if (rawkey == KEY_PGDN) sel = sel + 16 < m.n ? sel + 16 : m.n - 1;
    if (rawkey == KEY_HOME) sel = 0;
    if (rawkey == KEY_END) sel = m.n - 1;
    if ((key == '+') || (key == '=') || (rawkey == KEY_KPPLUS)) changed |= addtick(&m, &sel);
    else if ((key == '-') || (rawkey == KEY_KPMINUS)) changed |= removetick(&m, &sel);
    if (rawkey == KEY_L) { m.loop = (m.loop == sel + 1) ? 0 : sel + 1; changed = 1; }
    if (rawkey == KEY_U) live ^= 1;
    if (rawkey == KEY_SPACE)
    {
      if (!shiftflag) playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn);
      else releasenote(editorInfo.epchn);
    }
    if (table == WTBL)
    {
      int bit = 0;
      if (rawkey == KEY_T) bit = 0x10;
      if (rawkey == KEY_S) bit = 0x20;
      if (rawkey == KEY_P) bit = 0x40;
      if (rawkey == KEY_N) bit = 0x80;
      if (rawkey == KEY_G) bit = 0x01;
      if (rawkey == KEY_Y) bit = 0x02;
      if (rawkey == KEY_R) bit = 0x04;
      if (rawkey == KEY_X) bit = 0x08;
      if (bit)
      {
        int nw = m.wave[sel] ^ bit;
        if (step_wavevalid(nw)) { m.wave[sel] = nw; changed = 1; msg = ""; }
        else msg = "That combination can not be written in the wavetable.";
      }
      if (rawkey == KEY_A)
      {
        m.note[sel] = m.note[sel] >= 0x81 ? 0 : (0x80 | 48);
        changed = 1;
      }
      if ((rawkey == KEY_UP) || (rawkey == KEY_DOWN))
      {
        int step = shiftflag ? 12 : 1, dir = rawkey == KEY_UP ? 1 : -1;
        if (m.note[sel] >= 0x81)
        {
          int v = (m.note[sel] & 0x7f) + dir * step;
          if (v < 1) v = 1;
          if (v > 95) v = 95;
          m.note[sel] = 0x80 | v;
        }
        else
        {
          int v = notevalue(m.note[sel]) + dir * step;
          if (v < -32) v = -32;
          if (v > 95) v = 95;
          m.note[sel] = notebyte(v);
        }
        changed = 1;
      }
    }
    else if (table == FTBL)
    {
      int bit = 0;
      if (rawkey == KEY_1) bit = 0x10;
      if (rawkey == KEY_2) bit = 0x20;
      if (rawkey == KEY_3) bit = 0x40;
      if (bit) { m.ftype ^= bit; changed = 1; }
      if (rawkey == KEY_4) { m.fvoices ^= 1; changed = 1; }
      if (rawkey == KEY_5) { m.fvoices ^= 2; changed = 1; }
      if (rawkey == KEY_6) { m.fvoices ^= 4; changed = 1; }
      if (rawkey == KEY_7) { m.fvoices ^= 8; changed = 1; }
      if ((rawkey == KEY_BRACKETL) && (m.fres > 0)) { m.fres--; changed = 1; }
      if ((rawkey == KEY_BRACKETR) && (m.fres < 15)) { m.fres++; changed = 1; }
      if ((rawkey == KEY_UP) || (rawkey == KEY_DOWN))
      {
        int v = m.cutoff[sel] + (rawkey == KEY_UP ? 1 : -1) * (shiftflag ? 32 : 8);
        m.cutoff[sel] = v < 0 ? 0 : (v > 255 ? 255 : v);
        changed = 1;
      }
    }
    else if ((rawkey == KEY_UP) || (rawkey == KEY_DOWN))
    {
      int step = shiftflag ? 0x200 : 0x040, v = m.width[sel] + (rawkey == KEY_UP ? step : -step);
      if (v < 0) v = 0;
      if (v > 0xfff) v = 0xfff;
      m.width[sel] = v;
      changed = 1;
    }
    // mouse: a tick, or the + and - buttons
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb))
    {
      if ((mousey >= y + 4) && (mousey <= y + 8) && (mousex >= x0) && (mousex < x0 + vis * TICKW)) sel = scroll + (mousex - x0) / TICKW;
      if (mousey == y + 2)
      {
        if ((mousex >= x + 38) && (mousex < x + 38 + 17)) changed |= removetick(&m, &sel);
        if ((mousex >= x + 56) && (mousex < x + 61)) changed |= addtick(&m, &sel);
      }
    }
    // live: the table follows the edit, as long as the rows fit
    if (changed || (rawkey == KEY_U))
    {
      msg = "";
      if (!live)
      {
        memcpy(ltable[table], savel, MAX_TABLELEN);
        memcpy(rtable[table], saver, MAX_TABLELEN);
      }
      if (live && step_generate(&m, gl, gr, start) <= cap)
      {
        unsigned char cl[MAX_TABLELEN], cr[MAX_TABLELEN];
        memcpy(cl, savel, MAX_TABLELEN);
        memcpy(cr, saver, MAX_TABLELEN);
        step_write(cl, cr, start, gl, gr, step_generate(&m, gl, gr, start));
        memcpy(ltable[table], cl, MAX_TABLELEN);
        memcpy(rtable[table], cr, MAX_TABLELEN);
      }
    }
  }
  if (result == 1)
  {
    unsigned char cl[MAX_TABLELEN], cr[MAX_TABLELEN];
    memcpy(ltable[table], savel, MAX_TABLELEN);
    memcpy(rtable[table], saver, MAX_TABLELEN);
    nrows = step_generate(&m, gl, gr, start);
    cap = step_capacity(savel, saver, start);
    // Not enough rows in place: add empty rows after the program. inserttable moves everything behind them down and
    // updates every instrument, pattern command and jump that points there, so no other program is touched.
    table_makeroom(table, start - 1 + cap, nrows - cap);
    memcpy(cl, ltable[table], MAX_TABLELEN);
    memcpy(cr, rtable[table], MAX_TABLELEN);
    step_write(cl, cr, start, gl, gr, nrows);
    memcpy(ltable[table], cl, MAX_TABLELEN);
    memcpy(rtable[table], cr, MAX_TABLELEN);
  }
  else
  {
    memcpy(ltable[table], savel, MAX_TABLELEN);
    memcpy(rtable[table], saver, MAX_TABLELEN);
  }
  releasenote(editorInfo.epchn);
  key = 0;
  rawkey = 0;
  printmainscreen();
}
