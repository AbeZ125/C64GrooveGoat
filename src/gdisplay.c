//
// C64 GrooveGoat screen display routines
// (based on the GoatTracker v2 display code)
//

#define GDISPLAY_C

#include "goattrk2.h"
#include "gordhint.h"
#include "ginsuse.h"
#include "gres.h"
#include "gpiano.h"

char *notename[] =
 {"C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0",
  "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
  "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
  "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
  "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
  "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5",
  "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6",
  "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "...", "---", "+++"};

char timechar[] = {':', ' '};

int timemin = 0;
int timesec = 0;
int timeframe = 0;
char statusmessage[256] = {0};
static char shownmessage[256] = {0};
static int messagetimer = 0;
int chnview = 0;
int ordchnview = 0;

UIBUTTON uibuttons[MAX_UIBUTTONS];
int numuibuttons = 0;

// Colors used for the pattern display
#define COL_ROWNUM MKCOL(3, 0)
#define COL_ROWNUMBEAT MKCOL(7, 0)

void ui_addbutton(int x, int y, int w, int id, int param)
{
  if (numuibuttons >= MAX_UIBUTTONS) return;
  uibuttons[numuibuttons].x = x;
  uibuttons[numuibuttons].y = y;
  uibuttons[numuibuttons].w = w;
  uibuttons[numuibuttons].id = id;
  uibuttons[numuibuttons].param = param;
  numuibuttons++;
}

int ui_hitbutton(int mx, int my, int *param)
{
  int c;
  for (c = numuibuttons - 1; c >= 0; c--)
  {
    if ((my == uibuttons[c].y) && (mx >= uibuttons[c].x) && (mx < uibuttons[c].x + uibuttons[c].w))
    {
      if (param) *param = uibuttons[c].param;
      return uibuttons[c].id;
    }
  }
  return BTN_NONE;
}

// Is there a < or > arrow glyph at this screen cell? Returns -1 for the
// left arrow (previous / decrease), 1 for the right arrow (next / increase)
// and 0 otherwise.
int ui_arrowat(int x, int y)
{
  unsigned c = getcell(x, y) & 0xffff;
  if (c == GL_PREV) return -1;
  if (c == GL_NEXT) return 1;
  return 0;
}

// A button: label drawn with button colors; "active" shows it highlighted
static int hoverbtn = 0;

// Short help for buttons, shown in the message line while the mouse is over them
static const char *buttonhint(int id)
{
  switch (id)
  {
    case BTN_NEW: return "New song (clears everything)";
    case BTN_OPEN: return "Load a song (.sng, .ggt) - F10";
    case BTN_SAVE: return "Save the song - F11 (a .bak copy of the previous version is kept)";
    case BTN_INSLOAD: return "Load an instrument (.ins)";
    case BTN_INSSAVE: return "Save the current instrument (.ins)";
    case BTN_EXPORTSID: return "Export a .sid file for SID players (PSID v2/v3/v5) - F9";
    case BTN_EXPORTPRG: return "Export a C64 program (.prg), optionally with a player screen";
    case BTN_EXPORTAUDIO: return "Export audio: WAV (also single voices) or MP3 - Ctrl+F9";
    case BTN_EXPORTVIDEO: return "Export an MP4 video (720p) with an oscilloscope for every voice";
    case BTN_THEME: return "Switch between the night (C64 blue) and the day theme";
    case BTN_KEYS: return "On-screen piano: click the keys to play and enter notes (also ALT+K)";
    case BTN_KEYOCT: return "Move the piano one octave down / up";
    case BTN_FONT:
    {
      static char fonttip[100];
      snprintf(fonttip, sizeof fonttip, "Screen font: %s - click for the next font", res_fontnames[getfont()]);
      return fonttip;
    }
    case BTN_HELP: return "Help: keys and commands - F12";
    case BTN_INFO: return "Version, author, credits and licenses";
    case BTN_UNDO: return "Undo the last change - Ctrl+Z";
    case BTN_REDO: return "Redo - Ctrl+Y or Ctrl+Shift+Z";
    case BTN_KBDLAYOUT: return "Keyboard layout for shortcuts (EN: QWERTY, DE: QWERTZ - Z and Y swapped)";
    case BTN_LIBRARY: return "Instrument library: search, favourites, preview and load";
    case BTN_RANDOMINS: return "Random instrument generator (bass, drums, leads, pads, strings ...)";
    case BTN_STEREO: return "Editor playback: Mono or Stereo with 100%, 90%, 80% or 70% width (right click: back)";
    case BTN_ADSRGRAPH: return "ADSR envelope of the instrument: click for the sliders (also SHIFT+E on Attack/Decay or Sustain/Release)";
  }
  return NULL;
}

void ui_drawbutton(int x, int y, const char *label, int id, int param, int active)
{
  int w = strlen(label);
  int hover = (mousey == y) && (mousex >= x) && (mousex < x + w);
  if (hover) hoverbtn = id;
  int bg = active ? C_BUTTONHI : C_BUTTON;
  if ((hover) && (!active)) bg = C_SELECT;
  printtext(x, y, MKCOL(C_BUTTONTEXT, bg), label);
  ui_addbutton(x, y, w, id, param);
}

void setstatusmessage(const char *msg)
{
  strncpy(statusmessage, msg, sizeof statusmessage - 1);
  statusmessage[sizeof statusmessage - 1] = 0;
}

void formatsongtime(int frames, char *buf)
{
  int fps;
  int secs;
  if (editorInfo.multiplier)
    fps = (editorInfo.ntsc ? NTSCFRAMERATE : PALFRAMERATE) * editorInfo.multiplier;
  else
    fps = (editorInfo.ntsc ? NTSCFRAMERATE : PALFRAMERATE) / 2;
  secs = frames / fps;
  sprintf(buf, "%02d:%02d", secs / 60, secs % 60);
}

void validatechnview(void)
{
  int group = VISIBLECHANNELS;
  if (numchannels <= VISIBLECHANNELS)
  {
    chnview = 0;
  }
  else
  {
    if (editorInfo.epchn < chnview) chnview = (editorInfo.epchn / group) * group;
    if (editorInfo.epchn >= chnview + group) chnview = (editorInfo.epchn / group) * group;
    if (chnview > numchannels - group) chnview = numchannels - group;
    if (chnview < 0) chnview = 0;
  }
  if (numchannels <= VISIBLEORDERCHNS)
    ordchnview = 0;
  else
  {
    if (editorInfo.eschn < ordchnview) ordchnview = editorInfo.eschn;
    if (editorInfo.eschn >= ordchnview + VISIBLEORDERCHNS) ordchnview = editorInfo.eschn - VISIBLEORDERCHNS + 1;
    if (ordchnview > numchannels - VISIBLEORDERCHNS) ordchnview = numchannels - VISIBLEORDERCHNS;
    if (ordchnview < 0) ordchnview = 0;
  }
}

void printmainscreen(void)
{
  clearscreen();
  printstatus();
  forceredraw();
  fliptoscreen();
}

static void drawtooltip(void);

void displayupdate(void)
{
  if (cursorflashdelay >= 6)
  {
    cursorflashdelay %= 6;
    cursorflash++;
    cursorflash &= 3;
  }
  printstatus();
  drawtooltip();   // only on the main screen, not behind dialogs
  fliptoscreen();
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

static void drawtoolbar(void)
{
  int x;
  char buf[80];
  int hc = MKCOL(C_HEADERTEXT, C_HEADER);

  printblankc(0, TOOLBAR_Y, hc, MAX_COLUMNS);
  drawimage(0, TOOLBAR_Y, "logo", 2, 1);
  printtext(3, TOOLBAR_Y, hc, PROGRAMNAME);
  printtext(3 + strlen(PROGRAMNAME) + 1, TOOLBAR_Y, MKCOL(C_RB3, C_HEADER), PROGRAMVERSION);
  // the C64 rainbow
  for (x = 0; x < 5; x++)
    printglyph(23 + x, TOOLBAR_Y, MKCOL(C_RB1 + x, C_HEADER), GL_RIGHTH);

  x = 29;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_NEWDOC " New ", BTN_NEW, 0, 0); x += 8;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_FOLDER " Open ", BTN_OPEN, 0, 0); x += 9;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_DISK " Save ", BTN_SAVE, 0, 0); x += 9;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_LOADINS " Load Ins ", BTN_INSLOAD, 0, 0); x += 13;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_SAVEINS " Save Ins ", BTN_INSSAVE, 0, 0); x += 13;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_CHIP " SID ", BTN_EXPORTSID, 0, 0); x += 8;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_PRG " PRG ", BTN_EXPORTPRG, 0, 0); x += 8;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_WAVE " Audio ", BTN_EXPORTAUDIO, 0, 0); x += 10;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_FILM " Video ", BTN_EXPORTVIDEO, 0, 0); x += 10;

  x = MAX_COLUMNS - 21;
  if (theme == THEME_NIGHT)
    ui_drawbutton(x, TOOLBAR_Y, " " GS_MOON " ", BTN_THEME, 0, 0);
  else
    ui_drawbutton(x, TOOLBAR_Y, " " GS_SUN " ", BTN_THEME, 0, 0);
  x += 4;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_HELP " Help ", BTN_HELP, 0, 0); x += 9;
  ui_drawbutton(x, TOOLBAR_Y, " " GS_INFO " Info ", BTN_INFO, 0, 0);

  (void)buf;
}

static void drawsettingsbar(void)
{
  int x = 0;
  int pc = MKCOL(7, C_PANEL);
  char buf[80];
  int c;

  printblankc(0, SETTINGS_Y, pc, MAX_COLUMNS);
  x = 1;
  ui_drawbutton(x, SETTINGS_Y, " " GS_PLAY " Play ", BTN_PLAY, 0, isplaying() && lastsonginit == PLAY_BEGINNING); x += 9;
  ui_drawbutton(x, SETTINGS_Y, " " GS_PLAYPOS " Pos ", BTN_PLAYPOS, 0, isplaying() && lastsonginit == PLAY_POS); x += 8;
  ui_drawbutton(x, SETTINGS_Y, " " GS_PLAYPAT " Patt ", BTN_PLAYPATT, 0, isplaying() && lastsonginit == PLAY_PATTERN); x += 9;
  ui_drawbutton(x, SETTINGS_Y, " " GS_STOP " Stop ", BTN_STOP, 0, 0); x += 9;
  ui_drawbutton(x, SETTINGS_Y, " Follow ", BTN_FOLLOW, 0, followplay); x += 9;

  printtext(x, SETTINGS_Y, MKCOL(15, C_PANEL), "SIDs");
  x += 5;
  for (c = 0; c < 4; c++)
  {
    static const int n[4] = {1, 2, 4, 8};
    sprintf(buf, " %d ", n[c]);
    ui_drawbutton(x, SETTINGS_Y, buf, BTN_SIDS1 + c, n[c], numsids == n[c]);
    x += 4;
  }
  x += 1;
  ui_drawbutton(x, SETTINGS_Y, editorInfo.sidmodel ? " 8580 " : " 6581 ", BTN_MODEL, 0, 0); x += 7;
  ui_drawbutton(x, SETTINGS_Y, editorInfo.ntsc ? " NTSC " : " PAL ", BTN_NTSC, 0, 0); x += editorInfo.ntsc ? 7 : 6;
  if (editorInfo.multiplier)
    sprintf(buf, " " GS_PREV "%2dx" GS_NEXT " ", editorInfo.multiplier);
  else
    sprintf(buf, " " GS_PREV "25Hz" GS_NEXT " ");
  ui_drawbutton(x, SETTINGS_Y, buf, BTN_SPEED, 0, 0); x += strlen(buf) + 1;
  sprintf(buf, " HR %04X ", editorInfo.adparam);
  ui_drawbutton(x, SETTINGS_Y, buf, BTN_HR, 0, eamode);
  if (eamode) printcursor(x + 4 + editorInfo.eacolumn, SETTINGS_Y, 1);
  x += 10;
  ui_drawbutton(x, SETTINGS_Y, " FV ", BTN_FV, 0, editorInfo.finevibrato); x += 5;
  ui_drawbutton(x, SETTINGS_Y, " PO ", BTN_PO, 0, editorInfo.optimizepulse); x += 5;
  ui_drawbutton(x, SETTINGS_Y, " RO ", BTN_RO, 0, editorInfo.optimizerealtime); x += 5;
  ui_drawbutton(x, SETTINGS_Y, " INT ", BTN_INT, 0, interpolate & 1); x += 6;
  ui_drawbutton(x, SETTINGS_Y, " FP ", BTN_FP, 0, (interpolate & 2) ? 1 : 0); x += 5;
  if (stereomode) sprintf(buf, " Stereo %d%% ", stereowidth());
  else strcpy(buf, " Mono ");
  ui_drawbutton(x, SETTINGS_Y, buf, BTN_STEREO, 0, 0);
}

// ---------------------------------------------------------------------------
// Pattern editor
// ---------------------------------------------------------------------------

static void drawlevel(int x, int y, int level, int bg)
{
  // 3 cells, 24 steps
  int steps = level * 24 / 255;
  int c;
  for (c = 0; c < 3; c++)
  {
    int v = steps - c * 8;
    int col = c == 0 ? C_RB4 : (c == 1 ? C_RB3 : C_RB1);
    if (v <= 0)
      printglyph(x + c, y, MKCOL(C_LINE, bg), GL_BAR1);
    else
    {
      if (v > 8) v = 8;
      printglyph(x + c, y, MKCOL(col, bg), GL_BAR1 + v - 1);
    }
  }
}

static int chnlevel(int c)
{
  int env;
  if (chn[c].mute) return 0;
  env = sid_getenv(c / 3, c % 3);
  if (!(sidreg[c / 3][4 + 7 * (c % 3)] & 0xf0)) env = env / 2;
  return env;
}

static void drawpatterns(void)
{
  int c, d, i;
  int cc = cursorcolortable[cursorflash];
  int vis = numchannels < VISIBLECHANNELS ? numchannels : VISIBLECHANNELS;
  char buf[80];

  (void)cc;
  validatechnview();

  // Panel header
  printblankc(PATT_X, PATT_Y, MKCOL(C_HEADERTEXT, editorInfo.editmode == EDIT_PATTERN ? C_BUTTONHI : C_HEADER), PATT_W);
  printtext(PATT_X + 1, PATT_Y, MKCOL(C_HEADERTEXT, editorInfo.editmode == EDIT_PATTERN ? C_BUTTONHI : C_HEADER), "PATTERNS");
  if (numsids > 2)
  {
    int x = PATT_X + 14;
    for (c = 0; c < numchannels; c += VISIBLECHANNELS)
    {
      sprintf(buf, " SID %d-%d ", c / 3 + 1, c / 3 + 2);
      ui_drawbutton(x, PATT_Y, buf, BTN_CHNVIEW, c, chnview == c);
      x += strlen(buf) + 1;
    }
  }
  sprintf(buf, "%d SID%s  %d channels", numsids, numsids > 1 ? "s" : "", numchannels);
  printtext(PATT_X + PATT_W - strlen(buf) - 1, PATT_Y, MKCOL(C_HEADERTEXT, editorInfo.editmode == EDIT_PATTERN ? C_BUTTONHI : C_HEADER), buf);
  ui_drawbutton(PATT_X + PATT_W - (int)strlen(buf) - 7, PATT_Y, " Aa ", BTN_FONT, 0, 0);
  ui_drawbutton(PATT_X + PATT_W - (int)strlen(buf) - 14, PATT_Y, " Keys ", BTN_KEYS, 0, showkeyboard);

  // Tool row: transpose, mark, select pattern, cut / copy / paste
  {
    int lc = MKCOL(7, C_PANEL);
    int x = PATT_X + 2;
    printblankc(PATT_X, PATT_TOOLS_Y, lc, PATT_W);
    printtext(x, PATT_TOOLS_Y, lc, "Transpose"); x += 10;
    ui_drawbutton(x, PATT_TOOLS_Y, " -1 ", BTN_TRANSDN, 0, 0); x += 5;
    ui_drawbutton(x, PATT_TOOLS_Y, " +1 ", BTN_TRANSUP, 0, 0); x += 5;
    ui_drawbutton(x, PATT_TOOLS_Y, " -12 ", BTN_OCTDN, 0, 0); x += 6;
    ui_drawbutton(x, PATT_TOOLS_Y, " +12 ", BTN_OCTUP, 0, 0); x += 7;
    printtext(x, PATT_TOOLS_Y, lc, "Mark"); x += 5;
    ui_drawbutton(x, PATT_TOOLS_Y, " " GS_UP " ", BTN_MARKUP, 0, 0); x += 4;
    ui_drawbutton(x, PATT_TOOLS_Y, " " GS_DOWN " ", BTN_MARKDN, 0, 0); x += 4;
    ui_drawbutton(x, PATT_TOOLS_Y, " All ", BTN_MARKALL, 0, editorInfo.epmarkchn != -1); x += 7;
    printtext(x, PATT_TOOLS_Y, lc, "Pattern"); x += 8;
    ui_drawbutton(x, PATT_TOOLS_Y, " " GS_PREV " ", BTN_PATTPREV, 0, 0); x += 4;
    ui_drawbutton(x, PATT_TOOLS_Y, " " GS_NEXT " ", BTN_PATTNEXT, 0, 0); x += 5;
    ui_drawbutton(x, PATT_TOOLS_Y, " Cut ", BTN_CUT, 0, 0); x += 6;
    ui_drawbutton(x, PATT_TOOLS_Y, " Copy ", BTN_COPY, 0, 0); x += 7;
    ui_drawbutton(x, PATT_TOOLS_Y, " Paste ", BTN_PASTE, 0, 0);
  }

  // Background of the pattern area
  fillarea(PATT_X, PATT_CHNHDR_Y, PATT_W, VISIBLEPATTROWS + 1, MKCOL(8, C_PANEL), ' ');

  if ((followplay) && (isplaying()))
  {
    for (c = 0; c < numchannels; c++)
    {
      int newpos = chn[c].pattptr / 4;
      if (chn[c].advance) editorUndoInfo.editorInfo[c].epnum = chn[c].pattnum;

      if (newpos > pattlen[editorUndoInfo.editorInfo[c].epnum]) newpos = pattlen[editorUndoInfo.editorInfo[c].epnum];

      if (c == editorInfo.epchn)
      {
        editorInfo.eppos = newpos;
        editorInfo.epview = newpos-VISIBLEPATTROWS/2;
      }

      newpos = chn[c].songptr;
      newpos--;
      if (newpos < 0) newpos = 0;
      if (newpos > songlen[editorInfo.esnum][c]) newpos = songlen[editorInfo.esnum][c];

      if ((c == editorInfo.eschn) && (chn[c].advance))
      {
        editorInfo.eseditpos = newpos;
        if (newpos - editorInfo.esview < 0)
          editorInfo.esview = newpos;
        if (newpos - editorInfo.esview >= VISIBLEORDERLIST)
          editorInfo.esview = newpos - VISIBLEORDERLIST + 1;
      }
    }
  }

  for (i = 0; i < vis; i++)
  {
    int x = CHN_X(i);
    int ch = chnview + i;
    int pn;
    int hdrbg = (ch == editorInfo.epchn) ? C_SELECT : C_PANEL;
    if (ch >= numchannels) break;
    pn = editorUndoInfo.editorInfo[ch].epnum;

    // Channel header
    printblankc(x - 1, PATT_CHNHDR_Y, MKCOL(15, hdrbg), CHN_W - 1);
    sprintf(buf, "%d.%d", ch / 3 + 1, ch % 3 + 1);
    printtext(x, PATT_CHNHDR_Y, MKCOL(chn[ch].mute ? CMUTE : 15, hdrbg), buf);
    sprintf(buf, "P%02X", pn);
    printtext(x + 4, PATT_CHNHDR_Y, MKCOL(14, hdrbg), buf);
    ui_addbutton(x + 4, PATT_CHNHDR_Y, 3, BTN_PATTNUM, ch);
    drawlevel(x + 8, PATT_CHNHDR_Y, chnlevel(ch), hdrbg);
    printtext(x + 11, PATT_CHNHDR_Y, chn[ch].mute ? MKCOL(C_HEADERTEXT, 4) : MKCOL(7, hdrbg), "M");
    ui_addbutton(x + 11, PATT_CHNHDR_Y, 1, BTN_MUTE, ch);
    {
      int solo = !chn[ch].mute;
      for (c = 0; c < numchannels; c++)
        if ((c != ch) && (!chn[c].mute)) solo = 0;
      printtext(x + 12, PATT_CHNHDR_Y, solo ? MKCOL(C_HEADERTEXT, 10) : MKCOL(7, hdrbg), "S");
    }
    ui_addbutton(x + 12, PATT_CHNHDR_Y, 1, BTN_SOLO, ch);

    for (d = 0; d < VISIBLEPATTROWS; d++)
    {
      int p = editorInfo.epview + d;
      int y = PATT_ROWS_Y + d;
      int rowbg = C_PANEL;
      int playing = 0;
      int muted = chn[ch].mute;
      int beat = (p >= 0) && ((p % stepsize) == 0);
      unsigned char *pp;

      if ((p < 0) || (p > pattlen[pn]))
        continue;

      if ((pn == chn[ch].pattnum) && (isplaying()))
      {
        int chnrow = chn[ch].pattptr / 4;
        if (chnrow > pattlen[chn[ch].pattnum]) chnrow = pattlen[chn[ch].pattnum];
        if (chnrow == p) playing = 1;
      }

      if (beat) rowbg = C_BEATROW;
      if (p == editorInfo.eppos) rowbg = C_EDITROW;
      if (playing) rowbg = C_PLAYROW;

      printblankc(x - 1, y, MKCOL(8, rowbg), CHN_W - 1);

      if (!(patterndispmode & 1))
      {
        if (p < 100) sprintf(buf, " %02d", p);
        else sprintf(buf, "%03d", p);
      }
      else
        sprintf(buf, " %02X", p);
      printtext(x - 1, y, MKCOL(beat ? 7 : 3, rowbg), buf);

      pp = &pattern[pn][p*4];
      if (pp[0] == ENDPATT)
      {
        printtext(x + 3, y, MKCOL(muted ? CMUTE : 7, rowbg), "-- END --");
      }
      else
      {
        int note = pp[0];
        int notecol = 15;
        if (note == REST) notecol = 3;
        else if ((note == KEYOFF) || (note == KEYON)) notecol = 6;
        if (muted) notecol = CMUTE;
        printtext(x + 3, y, MKCOL(notecol, rowbg), notename[note - FIRSTNOTE]);

        if ((!pp[1]) && (patterndispmode & 2))
          printtext(x + 7, y, MKCOL(3, rowbg), "..");
        else
        {
          sprintf(buf, "%02X", pp[1]);
          printtext(x + 7, y, MKCOL(muted ? CMUTE : (pp[1] ? 11 : 3), rowbg), buf);
        }
        if ((!pp[2]) && (patterndispmode & 2))
          printtext(x + 9, y, MKCOL(3, rowbg), "...");
        else
        {
          sprintf(buf, "%01X", pp[2]);
          printtext(x + 9, y, MKCOL(muted ? CMUTE : (pp[2] ? 14 : 3), rowbg), buf);
          sprintf(buf, "%02X", pp[3]);
          printtext(x + 10, y, MKCOL(muted ? CMUTE : (pp[2] ? 14 : 3), rowbg), buf);
        }
      }

      // Marked area
      if (ch == editorInfo.epmarkchn)
      {
        int lo = editorInfo.epmarkstart < editorInfo.epmarkend ? editorInfo.epmarkstart : editorInfo.epmarkend;
        int hi = editorInfo.epmarkstart < editorInfo.epmarkend ? editorInfo.epmarkend : editorInfo.epmarkstart;
        if ((p >= lo) && (p <= hi))
          printbgx(x + 3, y, C_SELECT, 9);
      }

      // Cursor
      if ((p == editorInfo.eppos) && (editorInfo.editmode == EDIT_PATTERN) && (editorInfo.epchn == ch) && (!eamode))
      {
        switch (editorInfo.epcolumn)
        {
          case 0:
          printcursor(x + 3, y, 3);
          break;

          default:
          printcursor(x + 6 + editorInfo.epcolumn, y, 1);
          break;
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Right panel: orderlist, instrument, tables, song info
// ---------------------------------------------------------------------------

static void panelheader(int y, const char *title, int active)
{
  int col = MKCOL(C_HEADERTEXT, active ? C_BUTTONHI : C_HEADER);
  printblankc(RP_X, y, col, RP_W);
  printtext(RP_X + 1, y, col, title);
}

static void draworderlist(void)
{
  int c, d;
  char buf[80];
  int active = editorInfo.editmode == EDIT_ORDERLIST;
  int hc = MKCOL(C_HEADERTEXT, active ? C_BUTTONHI : C_HEADER);

  panelheader(ORD_Y, "ORDERLIST", active);
  sprintf(buf, "SUBTUNE " GS_PREV "%02X" GS_NEXT, editorInfo.esnum);
  printtext(RP_X + 12, ORD_Y, hc, buf);
  ui_addbutton(RP_X + 20, ORD_Y, 4, BTN_SUBTUNE, 0);
  sprintf(buf, "POS %02X", editorInfo.eseditpos);
  printtext(RP_X + 27, ORD_Y, hc, buf);
  if (numchannels > VISIBLEORDERCHNS)
  {
    ui_drawbutton(RP_X + RP_W - 5, ORD_Y, " " GS_UP " ", BTN_ORDVIEW, -1, 0);
    ui_drawbutton(RP_X + RP_W - 3, ORD_Y, " " GS_DOWN " ", BTN_ORDVIEW, 1, 0);
  }

  fillarea(RP_X, ORD_ROWS_Y, RP_W, VISIBLEORDERCHNS, MKCOL(8, C_PANEL), ' ');

  for (c = ordchnview; (c < numchannels) && (c < ordchnview + VISIBLEORDERCHNS); c++)
  {
    int y = ORD_ROWS_Y + c - ordchnview;
    sprintf(buf, "%d.%d", c / 3 + 1, c % 3 + 1);
    printtext(RP_X + 1, y, MKCOL(c == editorInfo.eschn ? 15 : 7, C_PANEL), buf);

    for (d = 0; d < VISIBLEORDERLIST; d++)
    {
      int p = editorInfo.esview+d;
      int color = 8;
      int bg = C_PANEL;
      if (isplaying())
      {
        int chnpos = chn[c].songptr;
        chnpos--;
        if (chnpos < 0) chnpos = 0;
        if ((p == chnpos) && (chn[c].advance)) bg = C_PLAYROW;
      }
      if (p == editorUndoInfo.editorInfo[c].espos) color = CEDIT;
      if ((editorUndoInfo.editorInfo[c].esend) && (p == editorUndoInfo.editorInfo[c].esend)) color = CEDIT;

      if ((p < 0) || (p > (songlen[editorInfo.esnum][c]+1)) || (p > MAX_SONGLEN+1))
      {
        sprintf(buf, "   ");
      }
      else
      {
        unsigned char v = songorder[editorInfo.esnum][c][p];
        if (v < LOOPSONG)
        {
          if ((v < REPEAT) || (p >= songlen[editorInfo.esnum][c]))
          {
            sprintf(buf, "%02X ", v);
            if ((p >= songlen[editorInfo.esnum][c]) && (color == 8)) color = 6;
          }
          else
          {
            if (v >= TRANSUP)
            {
              sprintf(buf, "+%01X ", v&0xf);
              if (color == 8) color = 14;
            }
            else
            {
              if (v >= TRANSDOWN)
              {
                sprintf(buf, "-%01X ", 16-(v & 0x0f));
                if (color == 8) color = 14;
              }
              else
              {
                sprintf(buf, "R%01X ", (v+1) & 0x0f);
                if (color == 8) color = 13;
              }
            }
          }
        }
        if (v == LOOPSONG)
        {
          sprintf(buf, "RST");
          if (color == 8) color = 6;
        }
      }
      printtext(ORD_X + d*3, y, MKCOL(color, bg), buf);
      if (c == editorInfo.esmarkchn)
      {
        int lo = editorInfo.esmarkstart < editorInfo.esmarkend ? editorInfo.esmarkstart : editorInfo.esmarkend;
        int hi = editorInfo.esmarkstart < editorInfo.esmarkend ? editorInfo.esmarkend : editorInfo.esmarkstart;
        if ((p >= lo) && (p <= hi))
          printbgx(ORD_X + d*3, y, C_SELECT, p != hi ? 3 : 2);
      }
      if ((p == editorInfo.eseditpos) && (editorInfo.editmode == EDIT_ORDERLIST) && (editorInfo.eschn == c) && (!eamode))
        printcursor(ORD_X + d*3 + editorInfo.escolumn, y, 1);
    }
  }
}

static void drawinstrument(void)
{
  char buf[80];
  int active = (editorInfo.editmode == EDIT_INSTRUMENT);
  int hc = MKCOL(C_HEADERTEXT, active ? C_BUTTONHI : C_HEADER);
  int c;
  static const char *names[9] = {"Attack/Decay", "Sustain/Release", "Wavetable Pos", "Pulsetable Pos",
    "Filtertable Pos", "Vibrato Param", "Vibrato Delay", "HR/Gate Timer", "1stFrame Wave"};
  int vals[9];

  panelheader(INS_Y, "INSTRUMENT", active);
  sprintf(buf, GS_PREV "%02X" GS_NEXT, editorInfo.einum);
  printtext(RP_X + 12, INS_Y, hc, buf);
  ui_addbutton(RP_X + 12, INS_Y, 4, BTN_INSTRNUM, 0);
  sprintf(buf, "%-15s", instr[editorInfo.einum].name);
  printtext(RP_X + 17, INS_Y, MKCOL(C_RB3, active ? C_BUTTONHI : C_HEADER), buf);
  ui_drawbutton(RP_X + RP_W - 13, INS_Y, " " GS_DICE GS_DICER " ", BTN_RANDOMINS, 0, 0);
  ui_drawbutton(RP_X + RP_W - 9, INS_Y, " Library ", BTN_LIBRARY, 0, 0);

  fillarea(RP_X, INS_ROWS_Y, RP_W, 5, MKCOL(8, C_PANEL), ' ');
  vals[0] = instr[editorInfo.einum].ad;
  vals[1] = instr[editorInfo.einum].sr;
  vals[2] = instr[editorInfo.einum].ptr[WTBL];
  vals[3] = instr[editorInfo.einum].ptr[PTBL];
  vals[4] = instr[editorInfo.einum].ptr[FTBL];
  vals[5] = instr[editorInfo.einum].ptr[STBL];
  vals[6] = instr[editorInfo.einum].vibdelay;
  vals[7] = instr[editorInfo.einum].gatetimer;
  vals[8] = instr[editorInfo.einum].firstwave;
  for (c = 0; c < 9; c++)
  {
    int x = c < 5 ? INS_COL1_X : INS_COL2_X;
    int y = INS_ROWS_Y + (c % 5);
    int sel = (editorInfo.eipos == c);
    printtext(x, y, MKCOL(sel ? CEDIT : 7, C_PANEL), names[c]);
    sprintf(buf, "%02X", vals[c]);
    printtext(x + INS_VAL_OFS, y, MKCOL(sel ? CEDIT : 15, C_PANEL), buf);
  }

  // ADSR envelope graph
  {
    int ad = instr[editorInfo.einum].ad, sr = instr[editorInfo.einum].sr;
    char t[16];
    int tx = RP_X + ADSR_W + 2;
    fillarea(RP_X, ADSR_Y, RP_W, 3, MKCOL(8, C_PANEL), ' ');
    drawadsrgraph(0, RP_X + 1, ADSR_Y, ADSR_W, 3, ad, sr, MKCOL(11, C_PANEL));
    adsr_timestr(t, adsr_attackms(ad >> 4));
    sprintf(buf, "A %8s", t);
    printtext(tx, ADSR_Y, MKCOL(7, C_PANEL), buf);
    adsr_timestr(t, adsr_decayms(ad & 15));
    sprintf(buf, "D %8s", t);
    printtext(tx, ADSR_Y + 1, MKCOL(7, C_PANEL), buf);
    adsr_timestr(t, adsr_decayms(sr & 15));
    sprintf(buf, "S%X R%7s", sr >> 4, t);
    printtext(tx, ADSR_Y + 2, MKCOL(7, C_PANEL), buf);
    ui_addbutton(RP_X, ADSR_Y, ADSR_W + 12, BTN_ADSRGRAPH, 0);
    drawwaveicon(RP_X + ADSR_W + 12, ADSR_Y);
  }

  if ((editorInfo.editmode == EDIT_INSTRUMENT) && (!eamode))
  {
    if (editorInfo.eipos < 9)
      printcursor((editorInfo.eipos < 5 ? INS_COL1_X : INS_COL2_X) + INS_VAL_OFS + editorInfo.eicolumn, INS_ROWS_Y + (editorInfo.eipos % 5), 1);
    else
      printcursor(RP_X + 17 + strlen(instr[editorInfo.einum].name), INS_Y, 1);
  }
}

static INSUSE tableuse;                // see drawtables

static void drawtables(void)
{
  int c, d;
  char buf[80];
  int active = (editorInfo.editmode == EDIT_TABLES);
  static const char *tnames[4] = {"WAVE", "PULSE", "FILTER", "SPEED"};

  panelheader(TBL_Y, "", active);
  for (c = 0; c < MAX_TABLES; c++)
    printtext(TBL_X(c), TBL_Y, MKCOL(C_HEADERTEXT, active ? C_BUTTONHI : C_HEADER), tnames[c]);
  fillarea(RP_X, TBL_ROWS_Y, RP_W, VISIBLETABLEROWS, MKCOL(8, C_PANEL), ' ');

  {                                    // which instruments start at which row (marks at the right of the rows)
    static unsigned char insptrs[MAX_INSTR][INSUSE_TABLES];
    int i;
    for (i = 0; i < MAX_INSTR; i++)
      for (c = 0; c < INSUSE_TABLES; c++) insptrs[i][c] = instr[i].ptr[c];
    insuse_build(&tableuse, MAX_INSTR, insptrs, editorInfo.einum);
  }

  for (c = 0; c < MAX_TABLES; c++)
  {
    int playrow = sum_playrow(c);      // the row the playing voice is on
    for (d = 0; d < VISIBLETABLEROWS; d++)
    {
      int p = editorInfo.etview[c]+d;
      int color = 8;
      int bg = (p & 1) ? C_PANEL : C_BEATROW;
      int playing = (playrow == p + 1);

      if (p >= MAX_TABLELEN) continue;
      switch (c)
      {
        case WTBL:
        if (ltable[c][p] >= WAVECMD) color = 14;
        break;

        case PTBL:
        if (ltable[c][p] >= 0x80) color = 14;
        break;

        case FTBL:
        if ((ltable[c][p] >= 0x80) || ((!ltable[c][p]) && (rtable[c][p]))) color = 14;
        break;
      }
      if (ltable[c][p] == 0xff) color = 13;
      if ((!ltable[c][p]) && (!rtable[c][p])) color = 3;
      if ((p == editorInfo.etpos) && (editorInfo.etnum == c)) color = CEDIT;
      if (playing)
      {
        bg = C_BUTTONHI;
        if (color != CEDIT) color = 15;
        printtext(TBL_X(c) + 2, TBL_ROWS_Y + d, MKCOL(3, bg), " ");
      }
      sprintf(buf, "%02X", p+1);
      printtext(TBL_X(c), TBL_ROWS_Y + d, MKCOL(playing ? 15 : 3, bg), buf);
      sprintf(buf, "%02X %02X", ltable[c][p], rtable[c][p]);
      printtext(TBL_X(c) + 3, TBL_ROWS_Y + d, MKCOL(color, bg), buf);
      {
        char who[4];                   // "05" = instrument 05 starts here, "x3" = three instruments (yellow: the one being edited)
        int use = insuse_label(&tableuse, c, p, who);
        if (use) printtext(TBL_X(c) + 9, TBL_ROWS_Y + d, MKCOL(use == 2 ? 14 : 15, C_BUTTON), who);
      }

      if (editorInfo.etmarknum == c)
      {
        int lo = editorInfo.etmarkstart < editorInfo.etmarkend ? editorInfo.etmarkstart : editorInfo.etmarkend;
        int hi = editorInfo.etmarkstart < editorInfo.etmarkend ? editorInfo.etmarkend : editorInfo.etmarkstart;
        if ((p >= lo) && (p <= hi))
          printbgx(TBL_X(c) + 3, TBL_ROWS_Y + d, C_SELECT, 5);
      }
    }
  }

  if ((editorInfo.editmode == EDIT_TABLES) && (!eamode))
    printcursor(TBL_X(editorInfo.etnum) + 3 + (editorInfo.etcolumn & 1) + (editorInfo.etcolumn/2)*3, TBL_ROWS_Y + editorInfo.etpos - editorInfo.etview[editorInfo.etnum], 1);
}

static void drawnames(void)
{
  char buf[80];
  int active = (editorInfo.editmode == EDIT_NAMES);
  int c;
  static const char *labels[3] = {"Name", "Author", "Released"};
  char *values[3];

  values[0] = songname;
  values[1] = authorname;
  values[2] = copyrightname;

  panelheader(NAMES_Y, "SONG INFO", active);
  fillarea(RP_X, NAMES_ROWS_Y, RP_W, 3, MKCOL(8, C_PANEL), ' ');
  for (c = 0; c < 3; c++)
  {
    printtext(NAMES_X, NAMES_ROWS_Y + c, MKCOL(7, C_PANEL), labels[c]);
    sprintf(buf, "%-32s", values[c]);
    printtext(NAMES_VAL_X, NAMES_ROWS_Y + c, MKCOL(15, C_BEATROW), buf);
  }
  if ((editorInfo.editmode == EDIT_NAMES) && (!eamode))
    printcursor(NAMES_VAL_X + strlen(values[editorInfo.enpos]), NAMES_ROWS_Y + editorInfo.enpos, 1);
}

// ---------------------------------------------------------------------------
// Status area
// ---------------------------------------------------------------------------

static void drawstatusarea(void)
{
  char buf[200];
  int x, c;
  int sc = MKCOL(7, C_PANEL);

  fillarea(0, STATUS_Y - 1, MAX_COLUMNS, 1, MKCOL(8, 0), ' ');
  printblankc(0, STATUS_Y, sc, MAX_COLUMNS);
  printblankc(0, POSITIONS_Y, sc, MAX_COLUMNS);

  x = 1;
  sprintf(buf, " Octave " GS_PREV "%d" GS_NEXT " ", editorInfo.epoctave);
  ui_drawbutton(x, STATUS_Y, buf, BTN_OCTAVE, 0, 0);
  x += strlen(buf) + 1;
  ui_drawbutton(x, STATUS_Y, recordmode ? " " GS_DOT " EDIT MODE " : " JAM MODE  ", BTN_RECORD, 0, recordmode);
  x += 14;
  {
    static const char *aa[3] = {" Advance: all ", " Advance: notes ", " Advance: off "};
    ui_drawbutton(x, STATUS_Y, aa[autoadvance], BTN_AUTOADV, 0, 0);
    x += strlen(aa[autoadvance]) + 1;
  }
  {
    static const char *km[3] = {" Keys: Tracker ", " Keys: DMC ", " Keys: Janko "};
    ui_drawbutton(x, STATUS_Y, km[keypreset], BTN_KEYMODE, 0, 0);
    x += strlen(km[keypreset]) + 1;
  }
  ui_drawbutton(x, STATUS_Y, kbdlayout ? " Kbd: DE " : " Kbd: EN ", BTN_KBDLAYOUT, 0, 0);
  x += 10;
  sprintf(buf, "Step %d", stepsize);
  printtext(x, STATUS_Y, sc, buf);
  x += strlen(buf) + 2;
  ui_drawbutton(x, STATUS_Y, " " GS_UNDO " Undo ", BTN_UNDO, 0, 0);
  x += 9;
  ui_drawbutton(x, STATUS_Y, " " GS_REDO " Redo ", BTN_REDO, 0, 0);
  x += 9;
  ui_drawbutton(x, STATUS_Y, " " GS_NOTE " Piano ", BTN_PIANO, 0, showpiano);

  // Play status and time
  if (editorInfo.multiplier)
  {
    if (!editorInfo.ntsc)
      sprintf(buf, "%02d%c%02d", timemin, timechar[timeframe/(25* editorInfo.multiplier) & 1], timesec);
    else
      sprintf(buf, "%02d%c%02d", timemin, timechar[timeframe/(30* editorInfo.multiplier) & 1], timesec);
  }
  else
  {
    if (!editorInfo.ntsc)
      sprintf(buf, "%02d%c%02d", timemin, timechar[(timeframe/13) & 1], timesec);
    else
      sprintf(buf, "%02d%c%02d", timemin, timechar[(timeframe/15) & 1], timesec);
  }
  if (isplaying())
  {
    printglyph(MAX_COLUMNS - 22, STATUS_Y, MKCOL(10, C_PANEL), GL_PLAY);
    printtext(MAX_COLUMNS - 20, STATUS_Y, MKCOL(10, C_PANEL), "PLAYING");
  }
  else
  {
    printglyph(MAX_COLUMNS - 22, STATUS_Y, MKCOL(7, C_PANEL), GL_STOP);
    printtext(MAX_COLUMNS - 20, STATUS_Y, sc, "STOPPED");
  }
  printtext(MAX_COLUMNS - 11, STATUS_Y, MKCOL(15, C_PANEL), buf);
  if (songmodified) printtext(MAX_COLUMNS - 4, STATUS_Y, MKCOL(14, C_PANEL), "*");

  // Channel positions of visible channels
  x = 1;
  for (c = chnview; (c < numchannels) && (c < chnview + VISIBLECHANNELS); c++)
  {
    int chnpos = chn[c].songptr;
    int chnrow = chn[c].pattptr/4;
    chnpos--;
    if (chnpos < 0) chnpos = 0;
    if (chnrow > pattlen[chn[c].pattnum]) chnrow = pattlen[chn[c].pattnum];
    if (chnrow >= 100) chnrow -= 100;
    sprintf(buf, "%d.%d", c / 3 + 1, c % 3 + 1);
    printtext(x, POSITIONS_Y, MKCOL(chn[c].mute ? CMUTE : 7, C_PANEL), buf);
    sprintf(buf, "%03d/%02d", chnpos, chnrow);
    printtext(x + 4, POSITIONS_Y, MKCOL(15, C_PANEL), buf);
    ui_addbutton(x, POSITIONS_Y, 10, BTN_MUTE, c);
    x += 12;
  }
  // SID addresses
  x = MAX_COLUMNS - 1 - numsids * 6 - 5;
  printtext(x, POSITIONS_Y, sc, "SIDs");
  for (c = 0; c < numsids; c++)
  {
    sprintf(buf, "$%04X", sidaddr[c]);
    printtext(x + 5 + c * 6, POSITIONS_Y, MKCOL(c >= chnview / 3 && c < (chnview + VISIBLECHANNELS) / 3 ? 15 : 7, C_PANEL), buf);
  }
  ui_addbutton(x, POSITIONS_Y, 5 + numsids * 6, BTN_SIDADDR, 0);

  // Message line
  if (strcmp(statusmessage, shownmessage))
  {
    strcpy(shownmessage, statusmessage);
    messagetimer = 50 * 6;
  }
  printblankc(0, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), MAX_COLUMNS);
  if (buttonhint(hoverbtn))
  {
    printglyph(1, MESSAGE_Y, MKCOL(C_RB5, C_HEADER), GL_INFO);
    printtext(3, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), buttonhint(hoverbtn));
  }
  else if ((messagetimer > 0) && (shownmessage[0]))
  {
    messagetimer--;
    printglyph(1, MESSAGE_Y, MKCOL(C_RB3, C_HEADER), GL_BULLET);
    sprintf(buf, "%.*s", VOL_LABEL_X - 5, shownmessage);
    printtext(3, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), buf);
  }
  else
  {
    if (strlen(loadedsongfilename))
      snprintf(buf, sizeof buf, "%s", loadedsongfilename);
    else
      sprintf(buf, "Untitled song");
    printtext(1, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), buf);
    printtext(VOL_LABEL_X - 75, MESSAGE_Y, MKCOL(C_BUTTONHI == C_HEADER ? 7 : C_RB5, C_HEADER),
      "F1 Play  F4 Stop  F5-F8 Editors  F9 Export  F10 Load  F11 Save  F12 Help");
  }
  hoverbtn = 0;

  // Output volume slider
  {
    int filled = (mastervolume + 2) / 5;
    printglyph(VOL_LABEL_X, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), mastervolume ? GL_SPEAKER : GL_MUTE);
    printtext(VOL_LABEL_X + 1, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), " Vol");
    for (c = 0; c < VOL_W; c++)
    {
      int col = c < 12 ? C_RB4 : (c < 17 ? C_RB3 : C_RB1);
      if ((c == filled - 1) || ((!filled) && (!c)))
        printglyph(VOL_X + c, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), GL_SLIDERKNOB);
      else if (c < filled)
        printglyph(VOL_X + c, MESSAGE_Y, MKCOL(col, C_HEADER), GL_SLIDERON);
      else
        printglyph(VOL_X + c, MESSAGE_Y, MKCOL(C_PANEL, C_HEADER), GL_SLIDEROFF);
    }
    sprintf(buf, "%3d%%", mastervolume);
    printtext(VOL_X + VOL_W + 1, MESSAGE_Y, MKCOL(C_HEADERTEXT, C_HEADER), buf);
    ui_addbutton(VOL_LABEL_X, MESSAGE_Y, VOL_W + 6 + 5, BTN_VOLUME, 0);
  }
}

// ---------------------------------------------------------------------------
// Piano keyboard
// ---------------------------------------------------------------------------

static void drawpiano(void)
{
  unsigned keys[PIANO_KEYS];
  unsigned *buf = rgbcanvas_begin(MAX_COLUMNS, PIANO_H);
  PIANOFONT f = {res_font2, 16, 32};
  int r;
  fillarea(0, PIANO_SEP_Y, MAX_COLUMNS, 1, MKCOL(8, 0), ' ');
  piano_getstate(keys);
  piano_render(buf, RGBCANVAS_STRIDE, MAX_COLUMNS * 16, PIANO_H * 32, keys,
    theme == THEME_DAY ? &piano_day : &piano_night, &f);
  rgbcanvas_draw(0, PIANO_Y);
  for (r = 0; r < PIANO_H; r++) ui_addbutton(0, PIANO_Y + r, MAX_COLUMNS, BTN_PIANOKEY, 0);
}

// ---------------------------------------------------------------------------
// Tooltips: name and shortcut of the button under the mouse pointer, shown
// after the pointer rested on it for a second
// ---------------------------------------------------------------------------

static int tooltip(int id, const char **name, const char **keys)
{
  *keys = "";
  switch (id)
  {
    case BTN_NEW: *name = "New song"; break;
    case BTN_OPEN: *name = "Load song"; *keys = "F10"; break;
    case BTN_SAVE: case BTN_SAVEAS: *name = "Save song"; *keys = "F11"; break;
    case BTN_INSLOAD: *name = "Load instrument"; *keys = "F10 in the instrument editor"; break;
    case BTN_INSSAVE: *name = "Save instrument"; *keys = "F11 in the instrument editor"; break;
    case BTN_EXPORTSID: *name = "Export SID file"; *keys = "F9"; break;
    case BTN_EXPORTPRG: *name = "Export C64 program (.prg)"; break;
    case BTN_EXPORTAUDIO: *name = "Export audio (WAV / MP3)"; *keys = "CTRL+F9"; break;
    case BTN_EXPORTVIDEO: *name = "Export MP4 video"; *keys = "CTRL+SHIFT+F9"; break;
    case BTN_THEME: *name = "Night / day theme"; break;
    case BTN_HELP: *name = "Keyboard reference and manual"; *keys = "F12"; break;
    case BTN_INFO: *name = "Version, credits and licenses"; break;
    case BTN_PLAY: *name = "Play from the beginning"; *keys = "F1 (SHIFT+F1: follow)"; break;
    case BTN_PLAYPOS: *name = "Play from the orderlist position"; *keys = "F2 (SHIFT+F2: follow)"; break;
    case BTN_PLAYPATT: *name = "Play the current pattern"; *keys = "F3 (SHIFT+F3: follow)"; break;
    case BTN_STOP: *name = "Stop"; *keys = "F4"; break;
    case BTN_FOLLOW: *name = "Follow the play position"; *keys = "SHIFT+F1 / F2 / F3"; break;
    case BTN_SIDS1: case BTN_SIDS2: case BTN_SIDS4: case BTN_SIDS8: *name = "Number of SID chips"; break;
    case BTN_MODEL: *name = "SID model 6581 / 8580"; *keys = "SHIFT+F8"; break;
    case BTN_NTSC: *name = "PAL / NTSC timing"; break;
    case BTN_SPEED: *name = "Speed multiplier"; *keys = "SHIFT+F5 / SHIFT+F6"; break;
    case BTN_HR: *name = "Hard restart ADSR"; *keys = "SHIFT+F7"; break;
    case BTN_FV: *name = "Fine vibrato"; break;
    case BTN_PO: *name = "Pulse optimization"; break;
    case BTN_RO: *name = "Realtime command optimization"; break;
    case BTN_INT: *name = "reSID interpolation"; *keys = "SHIFT+F9"; break;
    case BTN_FP: *name = "reSID-fp (distortion emulation)"; break;
    case BTN_STEREO: *name = "Mono / stereo width of the editor"; *keys = "right click: back"; break;
    case BTN_OCTAVE: *name = "Octave for note entry"; *keys = "/ and *"; break;
    case BTN_RECORD: *name = "Edit mode / jam mode"; *keys = "SPACE"; break;
    case BTN_AUTOADV: *name = "Autoadvance mode"; *keys = "SHIFT+Z"; break;
    case BTN_KEYMODE: *name = "Note entry keys (Tracker, DMC, Janko)"; break;
    case BTN_KBDLAYOUT: *name = "Keyboard layout EN / DE"; break;
    case BTN_UNDO: *name = "Undo"; *keys = "CTRL+Z"; break;
    case BTN_REDO: *name = "Redo"; *keys = "CTRL+Y"; break;
    case BTN_PIANO: *name = "Piano keyboard on / off"; *keys = "CTRL+B"; break;
    case BTN_PIANOKEY: *name = "Click a key to play the note (current instrument and voice)"; break;
    case BTN_CHNVIEW: *name = "Show the voices of these SIDs"; break;
    case BTN_ORDVIEW: *name = "Scroll the orderlist voices"; break;
    case BTN_SUBTUNE: *name = "Subtune"; *keys = "< > in the orderlist"; break;
    case BTN_INSTRNUM: *name = "Instrument"; *keys = "- +"; break;
    case BTN_MUTE: *name = "Mute voice"; *keys = "SHIFT+F4"; break;
    case BTN_SOLO: *name = "Solo voice"; break;
    case BTN_PATTNUM: *name = "Pattern of this voice (right click: previous)"; *keys = "< >"; break;
    case BTN_SIDADDR: *name = "SID chips and addresses"; break;
    case BTN_LIBRARY: *name = "Instrument library"; *keys = "CTRL+L"; break;
    case BTN_RANDOMINS: *name = "Random instrument generator"; *keys = "CTRL+R"; break;
    case BTN_ADSRGRAPH: *name = "ADSR envelope"; break;
    case BTN_VOLUME: *name = "Output volume"; break;
    case BTN_TRANSUP: *name = "Transpose half step up"; *keys = "SHIFT+Q"; break;
    case BTN_TRANSDN: *name = "Transpose half step down"; *keys = "SHIFT+A"; break;
    case BTN_OCTUP: *name = "Transpose octave up"; *keys = "SHIFT+W"; break;
    case BTN_OCTDN: *name = "Transpose octave down"; *keys = "SHIFT+S"; break;
    case BTN_MARKUP: *name = "Mark pattern upwards"; *keys = "SHIFT+CRSR UP"; break;
    case BTN_MARKDN: *name = "Mark pattern downwards"; *keys = "SHIFT+CRSR DOWN"; break;
    case BTN_MARKALL: *name = "Mark / unmark entire pattern"; *keys = "SHIFT+L"; break;
    case BTN_PATTPREV: *name = "Select previous pattern"; *keys = "SHIFT+CRSR LEFT"; break;
    case BTN_PATTNEXT: *name = "Select next pattern"; *keys = "SHIFT+CRSR RIGHT"; break;
    case BTN_CUT: *name = "Cut pattern"; *keys = "SHIFT+X"; break;
    case BTN_COPY: *name = "Copy pattern"; *keys = "SHIFT+C"; break;
    case BTN_PASTE: *name = "Paste pattern"; *keys = "SHIFT+V"; break;
    default: return 0;
  }
  return 1;
}

#define TOOLTIPDELAY 1000      // milliseconds

static void drawtooltip(void)
{
  static int tipid = BTN_NONE, tipx = -1, tipy = -1, tipclicked = 0;
  static Uint32 tipstart = 0;
  int param = 0;
  int id = ui_hitbutton(mousex, mousey, &param);
  const char *name, *keys;
  char buf[160];
  int len, x, y;

  if ((id != tipid) || (mousex != tipx) || (mousey != tipy))
  {
    tipid = id;
    tipx = mousex;
    tipy = mousey;
    tipstart = SDL_GetTicks();
    tipclicked = 0;
    return;
  }
  if (mouseb) tipclicked = 1;
  if ((tipclicked) || (id == BTN_NONE)) return;
  if (SDL_GetTicks() - tipstart < TOOLTIPDELAY) return;
  if (!tooltip(id, &name, &keys)) return;

  if (keys[0]) snprintf(buf, sizeof buf, " %s   %s ", name, keys);
  else snprintf(buf, sizeof buf, " %s ", name);
  len = strlen(buf);
  x = mousex - 1;
  if (x + len > MAX_COLUMNS) x = MAX_COLUMNS - len;
  if (x < 0) x = 0;
  y = mousey + 1;
  if (y >= MAX_ROWS - 1) y = mousey - 1;
  printtext(x, y, MKCOL(C_HEADERTEXT, C_HEADER), buf);
  if (keys[0]) printtext(x + 1 + strlen(name) + 3, y, MKCOL(C_RB3, C_HEADER), keys);
}

void printstatus(void)
{
  menu = 0;
  numuibuttons = 0;

  drawtoolbar();
  drawsettingsbar();
  drawpatterns();
  drawkeyboard();
  printglyph(PATT_W, PATT_Y, MKCOL(8, 0), ' ');
  draworderlist();
  printblankc(RP_X, ORD_ROWS_Y + VISIBLEORDERCHNS, MKCOL(8, 0), RP_W);
  if (editorInfo.editmode == EDIT_ORDERLIST)      // what the entry under the cursor does
  {
    char hint[80];
    if (order_hint(songorder[editorInfo.esnum][editorInfo.eschn], songlen[editorInfo.esnum][editorInfo.eschn], editorInfo.eseditpos, hint, sizeof hint))
      printtext(RP_X + 1, ORD_ROWS_Y + VISIBLEORDERCHNS, MKCOL(14, 0), hint);
  }
  drawinstrument();
  printblankc(RP_X, ADSR_Y + 3, MKCOL(8, 0), RP_W);
  drawtables();
  drawtablesummaryline(TBL_ROWS_Y + VISIBLETABLEROWS);
  drawnames();
  drawcommandpanel();
  if (showpiano) drawpiano();
  drawstatusarea();
}

void resettime(void)
{
  timemin = 0;
  timesec = 0;
  timeframe = 0;
}

void incrementtime(void)
{
  timeframe++;
  if (!editorInfo.ntsc)
  {
    if (((editorInfo.multiplier) && (timeframe >= PALFRAMERATE* (int)editorInfo.multiplier))
        || ((!editorInfo.multiplier) && (timeframe >= PALFRAMERATE/2)))
    {
      timeframe = 0;
      timesec++;
    }
  }
  else
  {
    if (((editorInfo.multiplier) && (timeframe >= NTSCFRAMERATE* (int)editorInfo.multiplier))
        || ((!editorInfo.multiplier) && (timeframe >= NTSCFRAMERATE/2)))
    {
      timeframe = 0;
      timesec++;
    }
  }
  if (timesec == 60)
  {
    timesec = 0;
    timemin++;
    timemin %= 60;
  }
}
