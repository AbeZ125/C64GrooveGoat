//
// C64 GrooveGoat - the ADSR sliders (SHIFT+E on the Attack/Decay or Sustain/Release value of the instrument, or a click
// on the envelope graph). Four sliders for attack, decay, sustain and release, drag them with the mouse. The values go
// to the instrument at once; ESC puts the old values back.
//

#include "goattrk2.h"
#include "gsumui.h"
#include "gadsr.h"

#define ADSR_CANVAS 1
#define SLIDER_STEP 3                 // cells per value
#define SLIDER_W (16 * SLIDER_STEP)

static int getval(int s)
{
  const INSTR *in = &instr[editorInfo.einum];
  switch (s)
  {
    case 0: return in->ad >> 4;
    case 1: return in->ad & 15;
    case 2: return in->sr >> 4;
  }
  return in->sr & 15;
}

static void setval(int s, int v)
{
  INSTR *in = &instr[editorInfo.einum];
  if (v < 0) v = 0;
  if (v > 15) v = 15;
  switch (s)
  {
    case 0: in->ad = (in->ad & 0x0f) | (v << 4); break;
    case 1: in->ad = (in->ad & 0xf0) | v; break;
    case 2: in->sr = (in->sr & 0x0f) | (v << 4); break;
    default: in->sr = (in->sr & 0xf0) | v; break;
  }
}

static void valuetext(char *buf, int size, int s, int v)
{
  char t[16];
  if (s == 2) snprintf(buf, size, "%X %d%%", v, v * 100 / 15);
  else
  {
    adsr_timestr(t, s == 0 ? adsr_attackms(v) : adsr_decayms(v));
    snprintf(buf, size, "%X %s", v, t);
  }
}

void showadsreditor(void)
{
  static const char *names[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
  static const char *hint[4] = {
    "How long the sound takes to reach full volume.",
    "How long it takes to fall from full volume to the sustain level.",
    "The volume it stays at while the key is held (F = full, 0 = silent).",
    "How long the sound takes to die away after the key is let go."};
  int oldad, oldsr, sel, drag = -1, autoplay = 1, result = 0, i;

  if (!editorInfo.einum)
  {
    setstatusmessage("Choose an instrument first (the instrument number is in the header)");
    return;
  }
  oldad = instr[editorInfo.einum].ad;
  oldsr = instr[editorInfo.einum].sr;
  sel = (editorInfo.eipos == 1) ? 2 : 0;
  key = 0;
  rawkey = 0;

  while (!result)
  {
    int w = 64, h = 29, x = (MAX_COLUMNS - w) / 2, y = (MAX_ROWS - h) / 2 - 1;
    int tx = x + 11, k, nb = 0, shiftflag;
    STEPBTN btn[MAXBTN];
    char buf[120];

    printstatus();
    dlg_drawframe(x, y, w, h, "ADSR SLIDERS");
    snprintf(buf, sizeof buf, "Instrument %02X  %s", editorInfo.einum, instr[editorInfo.einum].name);
    printtext(x + 3, y + 2, MKCOL(15, C_PANEL), buf);
    snprintf(buf, sizeof buf, "AD $%02X  SR $%02X", instr[editorInfo.einum].ad, instr[editorInfo.einum].sr);
    printtext(x + w - 20, y + 2, MKCOL(3, C_PANEL), buf);
    for (i = 0; i < 16; i++)
    {
      snprintf(buf, sizeof buf, "%X", i);
      printtext(tx + i * SLIDER_STEP + 1, y + 4, MKCOL(3, C_PANEL), buf);
    }
    for (i = 0; i < 4; i++)
    {
      int sy = y + 5 + i * 3, v = getval(i), r;
      printtext(x + 3, sy, MKCOL(sel == i ? CEDIT : 7, C_PANEL), names[i]);
      valuetext(buf, sizeof buf, i, v);
      printtext(x + 3, sy + 1, MKCOL(15, C_PANEL), buf);
      for (r = 0; r < 2; r++)
      {
        fillarea(tx, sy + r, SLIDER_W, 1, MKCOL(8, C_BEATROW), ' ');
        printbgx(tx, sy + r, C_BUTTONHI, (v + 1) * SLIDER_STEP);
        printbgx(tx + v * SLIDER_STEP, sy + r, sel == i ? C_SELECT : C_BUTTON, SLIDER_STEP);
      }
    }
    printtext(x + 3, y + 17, MKCOL(7, C_PANEL), "ENVELOPE");
    drawadsrgraph(ADSR_CANVAS, tx, y + 17, 48, 5, instr[editorInfo.einum].ad, instr[editorInfo.einum].sr, MKCOL(11, C_PANEL));
    printtext(x + 3, y + 23, MKCOL(3, C_PANEL), hint[sel]);
    stepbutton(btn, &nb, x + 3, y + h - 4, "PLAY", KEY_SPACE, 0, -1, 0, 1);
    stepbutton(btn, &nb, x + 12, y + h - 4, "RELEASE", KEY_SPACE, 1, -1, 0, 1);
    stepbutton(btn, &nb, x + 24, y + h - 4, autoplay ? "PLAY WHEN LET GO: ON" : "PLAY WHEN LET GO: OFF", 0, 0, 1, autoplay, 1);
    stepbutton(btn, &nb, x + 3, y + h - 2, "KEEP CHANGES", KEY_ENTER, 0, -1, 0, 1);
    stepbutton(btn, &nb, x + 20, y + h - 2, "UNDO ALL", KEY_ESC, 0, -1, 0, 1);
    printtext(x + 32, y + h - 2, MKCOL(3, C_PANEL), "(ENTER, ESC)  UP/DOWN slider, LEFT/RIGHT value");
    fliptoscreen();
    getkey();
    converthex();

    shiftflag = shiftpressed;
    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb))
    {
      for (k = 0; k < nb; k++)
        if ((mousey == btn[k].y) && (mousex >= btn[k].x) && (mousex < btn[k].x + btn[k].w))
        {
          if (btn[k].setwidth >= 0) autoplay ^= 1;
          else { rawkey = btn[k].rk; shiftflag = btn[k].shift; }
          break;
        }
      if (k >= nb)
        for (i = 0; i < 4; i++)                       // a click on a slider starts a drag
        {
          int sy = y + 5 + i * 3;
          if ((mousey >= sy) && (mousey <= sy + 1) && (mousex >= tx - 1) && (mousex <= tx + SLIDER_W))
          {
            drag = i;
            sel = i;
            break;
          }
        }
    }
    if (drag >= 0)
    {
      if (mouseb & MOUSEB_LEFT) setval(drag, (mousex - tx) / SLIDER_STEP < 0 ? 0 : (mousex - tx) / SLIDER_STEP);
      else
      {
        drag = -1;
        if (autoplay) playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn);
      }
    }
    if (hexnybble >= 0) setval(sel, hexnybble);
    if (win_quitted || rawkey == KEY_ESC) result = 2;
    if (rawkey == KEY_ENTER) result = 1;
    if (rawkey == KEY_UP) sel = (sel + 3) % 4;
    if (rawkey == KEY_DOWN) sel = (sel + 1) % 4;
    if (rawkey == KEY_LEFT) setval(sel, getval(sel) - (shiftflag ? 4 : 1));
    if (rawkey == KEY_RIGHT) setval(sel, getval(sel) + (shiftflag ? 4 : 1));
    if (rawkey == KEY_SPACE)
    {
      if (shiftflag) releasenote(editorInfo.epchn);
      else playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn);
    }
  }
  if (result == 2)
  {
    instr[editorInfo.einum].ad = oldad;
    instr[editorInfo.einum].sr = oldsr;
  }
  releasenote(editorInfo.epchn);
  hexnybble = -1;
  key = 0;
  rawkey = 0;
  printmainscreen();
}
