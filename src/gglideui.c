//
// C64 GrooveGoat - the glide window (SHIFT+T on a note in the pattern editor): a slider for how long the pitch takes
// to slide from the note before into this one. The slide is written (pattern_glide, gpattern.c) when you keep it.
//

#include "goattrk2.h"
#include "gsumui.h"
#include "gglide.h"

#define SLIDER_W 80                   // cells
#define SLIDER_MAX 120                // ticks at the right end of the slider (the room of the slide can be less)

void showglideeditor(const GLIDEINFO *g)
{
  int maxv = g->room < SLIDER_MAX ? g->room : SLIDER_MAX;
  int ticks = 0, drag = 0, result = 0, shown = 0;
  double rate = sum_tickrate();
  int speed = 1;

  // start at the slide that was made before, or at 0.2 s
  if (g->olddata > 0)
  {
    int s = (ltable[STBL][g->olddata - 1] << 8) | rtable[STBL][g->olddata - 1];
    if ((s > 0) && (s < 0x8000)) ticks = glide_ticks(g->f1, g->f2, s);
  }
  else ticks = (int)(0.2 * rate + 0.5);
  if (ticks > maxv) ticks = maxv;
  if (ticks < 1) ticks = 1;
  key = 0;
  rawkey = 0;

  while (!result)
  {
    int w = 92, h = 24, x = (MAX_COLUMNS - w) / 2, y = (MAX_ROWS - h) / 2 - 1;
    int tx = x + 3, ly, k, nb = 0, shiftflag, rows, actual;
    STEPBTN btn[MAXBTN];
    char buf[160], a[16], b[16];
    double semis = 12 * log((double)(g->f2 > g->f1 ? g->f2 : g->f1) / (g->f2 > g->f1 ? g->f1 : g->f2)) / log(2.0);

    printstatus();
    dlg_drawframe(x, y, w, h, "GLIDE INTO THIS NOTE");
    snprintf(buf, sizeof buf, "Slide from %s (row %d) to %s (row %d): %.0f semitones %s",
      notename[g->prevnote - FIRSTNOTE], g->prevrow, notename[g->note - FIRSTNOTE], g->row, semis, g->f2 >= g->f1 ? "up" : "down");
    printtext(x + 3, y + 2, MKCOL(15, C_PANEL), buf);

    if (ticks > 0)
    {
      speed = glide_speed(g->f1, g->f2, ticks);
      actual = glide_ticks(g->f1, g->f2, speed);
    }
    else actual = 0;
    rows = actual ? (actual + g->tpr - 1) / g->tpr : 1;

    printtext(x + 3, y + 4, MKCOL(CEDIT, C_PANEL), "TIME");
    if (ticks > 0) snprintf(buf, sizeof buf, "%.2f s (%d ticks)", actual / rate, actual);
    else snprintf(buf, sizeof buf, "Instant");
    printtext(x + 10, y + 4, MKCOL(15, C_PANEL), buf);
    for (k = 0; k < 2; k++)
    {
      int fillw = maxv ? (ticks * SLIDER_W / maxv) : 0;
      fillarea(tx, y + 5 + k, SLIDER_W, 1, MKCOL(8, C_BEATROW), ' ');
      printbgx(tx, y + 5 + k, C_BUTTONHI, fillw);
      printbgx(tx + (fillw > 0 ? fillw - 1 : 0), y + 5 + k, C_SELECT, 1);
    }
    snprintf(a, sizeof a, "0");
    snprintf(b, sizeof b, "%.1f s", maxv / rate);
    printtext(tx, y + 7, MKCOL(3, C_PANEL), a);
    printtext(tx + SLIDER_W - (int)strlen(b), y + 7, MKCOL(3, C_PANEL), b);

    // quick choices
    ly = y + 9;
    printtext(x + 3, ly, MKCOL(7, C_PANEL), "Quick");
    {
      static const double secs[5] = {0.1, 0.2, 0.4, 0.8, 1.6};
      int bx = x + 11;
      stepbutton(btn, &nb, bx, ly, "INSTANT", 0, 0, 1000, ticks == 0, 1);
      bx += 11;
      for (k = 0; k < 5; k++)
      {
        int t = (int)(secs[k] * rate + 0.5);
        snprintf(buf, sizeof buf, "%.1f s", secs[k]);
        stepbutton(btn, &nb, bx, ly, buf, 0, 0, 1000 + t, ticks == t, t <= g->room);
        bx += (int)strlen(buf) + 3;
      }
      stepbutton(btn, &nb, bx, ly, "LONGEST", 0, 0, 1000 + g->room, ticks == g->room, 1);
    }

    // what it makes
    ly = y + 11;
    if (ticks > 0)
    {
      snprintf(buf, sizeof buf, "It writes 3XY on this row and the next %d empty row%s (every row of the slide needs it).", rows - 1, rows == 2 ? "" : "s");
      if (rows == 1) snprintf(buf, sizeof buf, "It fits in this row: it writes 3XY on the note row only.");
      printtext(x + 3, ly++, MKCOL(7, C_PANEL), buf);
      snprintf(buf, sizeof buf, "Speed $%04X per tick, %d ticks to arrive.", speed, actual);
      printtext(x + 3, ly++, MKCOL(7, C_PANEL), buf);
    }
    else
    {
      printtext(x + 3, ly++, MKCOL(7, C_PANEL), "It writes 300 (a tie note): the pitch jumps to the note at once, with no new attack.");
      ly++;
    }
    snprintf(buf, sizeof buf, "Room: %d ticks (%.2f s) before the next note, command or pattern end; %d ticks per row%s.",
      g->room, g->room / rate, g->tpr, g->tempofound ? "" : " (assumed)");
    printtext(x + 3, ly++, MKCOL(g->room > ticks ? 3 : 14, C_PANEL), buf);
    if (g->room < SLIDER_MAX * 1 && maxv == g->room) printtext(x + 3, ly, MKCOL(3, C_PANEL), "A slide is never longer than that, so it cannot be cut off.");
    else printtext(x + 3, ly, MKCOL(3, C_PANEL), "The slider goes up to 120 ticks; LONGEST uses all the room.");
    ly += 2;
    printtext(x + 3, ly, MKCOL(3, C_PANEL), "Play it from a few rows before the first note (SHIFT+SPACE) to hear it.");

    stepbutton(btn, &nb, x + 3, y + h - 3, "KEEP", KEY_ENTER, 0, -1, 0, 1);
    stepbutton(btn, &nb, x + 12, y + h - 3, "CANCEL", KEY_ESC, 0, -1, 0, 1);
    printtext(x + 24, y + h - 3, MKCOL(3, C_PANEL), "(ENTER, ESC)  LEFT/RIGHT 1 tick, SHIFT: 1 row, HOME/END");
    (void)shown;
    fliptoscreen();
    getkey();

    shiftflag = shiftpressed;
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb))
    {
      for (k = 0; k < nb; k++)
        if ((mousey == btn[k].y) && (mousex >= btn[k].x) && (mousex < btn[k].x + btn[k].w))
        {
          if (btn[k].setwidth >= 1000)
          {
            ticks = btn[k].setwidth - 1000;
            if (ticks > g->room) ticks = g->room;
          }
          else { rawkey = btn[k].rk; shiftflag = btn[k].shift; }
          break;
        }
      if ((k >= nb) && (mousey >= y + 5) && (mousey <= y + 6) && (mousex >= tx - 1) && (mousex <= tx + SLIDER_W)) drag = 1;
    }
    if (drag)
    {
      if (mouseb & MOUSEB_LEFT)
      {
        int v = maxv * (mousex - tx + 1) / SLIDER_W;
        ticks = v < 0 ? 0 : (v > maxv ? maxv : v);
      }
      else drag = 0;
    }
    if (win_quitted || rawkey == KEY_ESC) result = 2;
    if (rawkey == KEY_ENTER) result = 1;
    if (rawkey == KEY_LEFT) ticks -= shiftflag ? g->tpr : 1;
    if (rawkey == KEY_RIGHT) ticks += shiftflag ? g->tpr : 1;
    if (rawkey == KEY_HOME) ticks = 0;
    if (rawkey == KEY_END) ticks = maxv;
    if (ticks < 0) ticks = 0;
    if (ticks > g->room) ticks = g->room;
  }
  key = 0;
  rawkey = 0;
  printmainscreen();
  if (result == 1) pattern_glide(g->row, ticks);
}
