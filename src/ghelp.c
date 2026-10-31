//
// C64 GrooveGoat online help, manual viewer, info dialog and SID settings
// (keyboard reference based on the GoatTracker v2 online help)
//

#define GHELP_C

#include "goattrk2.h"
#include "gres.h"

#define HELP_HEADER MKCOL(14, C_PANEL)
#define HELP_NORMAL MKCOL(8, C_PANEL)
#define HELP_KEY MKCOL(15, C_PANEL)

static int printrows(int column, int row, int color, const char *strings[])
{
  int n = 0;
  while (strings[n])
  {
    // Keys (text before two spaces) are shown highlighted
    const char *s = strings[n];
    char de[200];
    // German keyboards: the lowest note row starts with Y
    if ((kbdlayout == 1) && (!strncmp(s, "Z-M, Q-P", 8)))
    {
      snprintf(de, sizeof de, "Y-M, Q-P%s", s + 8);
      s = de;
    }
    const char *sep = strstr(s, "  ");
    if ((sep) && (sep - s < 24))
    {
      char keybuf[32];
      int kl = sep - s;
      memcpy(keybuf, s, kl);
      keybuf[kl] = 0;
      printtext(column, row, color, s);
      printtext(column, row, HELP_KEY, keybuf);
    }
    else printtext(column, row, color, s);
    row++;
    n++;
  }
  return row;
}

static const char *genkeys[] = {
  "F1            Play from beginning",
  "F2            Play from current position",
  "F3            Play current pattern",
  "F4            Stop playing",
  "F5            Go to pattern editor",
  "F6            Go to orderlist editor",
  "F7            Instrument/table editor",
  "F8            Go to songname editor",
  "F9            Export SID/PRG (pack & relocate)",
  "CTRL+F9       Export WAV/MP3 (+SHIFT: video)",
  "F10           Load song/instrument",
  "F11           Save song/instrument",
  "F12           This screen",
  "SHIFT+F1-F3   Follow play begin/pos/patt.",
  "SHIFT+F4      Mute current channel",
  "SHIFT+F5-F6   Change speed multiplier",
  "SHIFT+F7      Change hardrestart ADSR",
  "SHIFT+F8      Switch 6581/8580 SID",
  "SHIFT+F9      Toggle reSID interpolation",
  "SHIFT+F10     Merge-load song",
  "SHIFT+, .     Move song startpos & restart",
  "TAB           Cycle between editing modes",
  "INS           Insert row (on endmark:",
  "DEL           Delete row  change length)",
  "CTRL/CMD+Z    Undo",
  "CTRL/CMD+Y    Redo (also CTRL+SHIFT+Z)",
  "CTRL/CMD+R    Random instrument generator",
  "CTRL/CMD+L    Instrument library",
  "CTRL/CMD+B    Piano keyboard on/off",
  "SHIFT+ESC     Clear/optimize musicdata",
  "ESC           Exit program",
  "ALT+ENTER     Toggle fullscreen",
  NULL
};

static const char *mackeys[] = {
  "FN+F1..F12    Function keys on Macs",
  "ALT+BACKSPACE Insert",
  "FN+SHIFT+BKSP Insert",
  "FN+BACKSPACE  Delete",
  "CTRL+CLICK    Right mouse button",
  NULL
};

static const char *mousekeys[] = {
  "Click         Move cursor / press button",
  "Right drag    Mark pattern/orderlist/table",
  "Wheel         Scroll the panel under mouse",
  "Double click  Play pattern from this row",
  "Click P00     Next pattern (right: prev.)",
  "Click M / S   Mute / solo a channel",
  "Drop a file   Load song or instrument",
  "Hover         Button help, after 1 s tooltip",
  "Click piano   Play a note (drag: glissando)",
  NULL
};

static const char *patternkeys[] = {
  "Z-M, Q-P      Enter notes like on a piano",
  "0-9 & A-F     Enter commands",
  "SPACE         Switch between jam/editmode",
  "BACKSPACE     Insert rest",
  "RETURN        Keyoff (/w SHIFT = Keyon)",
  "CAPSLOCK      Keyoff",
  "- +           Select instrument",
  "/ *           Select octave",
  "< >           Select pattern",
  "BACKQUOTE     Select channel (/w SHIFT back)",
  "SHIFT+SPACE   Play from cursor pos",
  "SHIFT+CRSR    Mark pattern",
  "SHIFT+Q,W     Transpose half/octave up",
  "SHIFT+A,S     Transpose half/octave down",
  "SHIFT+E,R     Copy,paste effects",
  "SHIFT+H       Make hifi vib/portaspeed",
  "SHIFT+I       Invert selection/pattern",
  "SHIFT+G       Show/hide the guide to the pattern commands",
  "SHIFT+J,K     Join/split pattern",
  "SHIFT+L       Mark/unmark whole pattern",
  "SHIFT+M,N     Choose highlighting step",
  "SHIFT+O,P     Shrink/expand pattern",
  "SHIFT+X,C,V   Cut,copy,paste pattern",
  "SHIFT+Z       Cycle autoadvance-mode",
  "SHIFT+1,2,3   Mute voice 1-3 of this SID",
  NULL
};

static const char *songkeys[] = {
  "0-9 & A-F     Enter pattern numbers",
  "SPACE         Set start position for F2",
  "BACKSPACE     Set end position for F2",
  "RETURN        Go to pattern (SHIFT=all)",
  "< >           Select subtune",
  "- +           Insert transpose down/up",
  "SHIFT+CRSR    Mark orderlist",
  "SHIFT+L       Mark/unmark whole orderlist",
  "SHIFT+R       Insert repeat command",
  "SHIFT+X,C,V   Cut,copy,paste orderlist",
  "SHIFT+1,2,3   Swap with voice 1-3 (same SID)",
  NULL
};

static const char *instkeys[] = {
  "0-9 & A-F     Enter parameters",
  "SPACE         Play test note",
  "SHIFT+SPACE   Silence test note",
  "RETURN        Go to table",
  "- +           Select instrument",
  "/ *           Select octave",
  "BACKQUOTE     Select table",
  "SHIFT+CRSR    Mark table",
  "SHIFT+Q,W     Speed half/octave up",
  "SHIFT+A,S     Speed half/octave down",
  "SHIFT+I       Summary of the pulse/wave/filter table",
  "SHIFT+H       Show/hide the list of table codes",
  "SHIFT+E       Step/speed editor (mouse)",
  "SHIFT+L       Convert pulse/filter limit",
  "SHIFT+N       Edit name/negate value",
  "SHIFT+O       Optimize table",
  "SHIFT+R       Convert abs./rel. note",
  "SHIFT+S       Smart instrument paste",
  "SHIFT+U       Unlock/lock table view",
  "SHIFT+X,C,V   Cut,copy,paste instr/table",
  "SHIFT+DEL     Delete instr.+tabledata",
  "SHIFT+RET     Convert vibrato parameter",
  NULL
};

static const char *multisid[] = {
  "SIDs 1/2/4/8  Number of SIDs (toolbar)",
  "              3, 6, 12 or 24 channels",
  "Channel n.m   SID n, voice m",
  "SID x-y tabs  Show channels of other SIDs",
  "BACKQUOTE     Next channel (all SIDs)",
  "Filters       Each SID has its own filter",
  "              & volume (cmds A,B,C,D)",
  "Tempo F/E     Affects all channels",
  ".sng          GoatTracker 2 (1 SID) and",
  "              GoatTracker Stereo (2 SIDs)",
  ".ggt          GrooveGoat format (4/8 SIDs)",
  NULL
};

static const char *newkeys[] = {
  "Kbd: EN/DE    Shortcut layout (DE: Z/Y swap)",
  "Library       Search by typing, TAB 6581/8580",
  "              SPACE preview, INS/* favourite",
  "Dice button   Random instrument: R generate,",
  "              SPACE preview, ENTER use it",
  "Audio/Video   Export WAV, MP3 or MP4 video",
  "Backups       .bak on save, autosave every",
  "              2 min., restore after a crash",
  "Piano         Played notes of all voices,",
  "              click a key to play (CTRL+B)",
  "Pattern tools Transpose, mark, cut/copy/paste",
  "              buttons above the pattern editor",
  NULL
};

static const char *pattcmds[] = {
  "0XY  Do nothing. Databyte will always be 00.",
  "1XY  Portamento up. XY is index to a 16-bit speed in speedtable.",
  "2XY  Portamento down. XY is index to a 16-bit speed in speedtable.",
  "3XY  Toneportamento. Raise or lower pitch until target note has been reached.",
  "     XY is index to a 16-bit speed or 00 for \"tie note\".",
  "4XY  Vibrato. XY is index to speedtable. Left side value determines how long",
  "     until the direction changes (speed), right side the pitch change (depth).",
  "5XY  Set attack/decay register to value XY.",
  "6XY  Set sustain/release register to value XY.",
  "7XY  Set waveform register to value XY. Ineffective while a wavetable changes it.",
  "8XY  Set wavetable pointer. 00 stops wavetable execution.",
  "9XY  Set pulsetable pointer. 00 stops pulsetable execution.",
  "AXY  Set filtertable pointer (filter of this channel's SID). 00 stops it.",
  "BXY  Set filter control. X is resonance and Y is the voice bitmask of this SID.",
  "     00 turns filter off and also stops filtertable execution.",
  "CXY  Set filter cutoff to XY (filter of this channel's SID).",
  "DXY  Set mastervolume of this channel's SID to Y, if X is 0. If X is not 0,",
  "     XY is copied to the timing mark location (playeraddress+$3F).",
  "EXY  Funktempo. XY is an index to speedtable. Alternates left and right side",
  "     tempo values on each pattern step.",
  "FXY  Set tempo. 03-7F set tempo on all channels, 83-FF only on current channel",
  "     (subtract 80 to get actual tempo). Tempos 00 and 01 recall the funktempos.",
  NULL
};

static const char *instparm[] = {
  "Attack/Decay     0 is fastest attack or decay, F is slowest",
  "Sustain/Release  Sustain 0 is silent and F the loudest. Release like A/D.",
  "Wavetable Pos    Wavetable startposition. 00 stops the wavetable.",
  "Pulsetable Pos   Pulsetable startposition. 00 leaves pulse untouched.",
  "Filtertable Pos  Filtertable startposition. 00 leaves filter untouched.",
  "Vibrato Param    Instrument vibrato: index to the speedtable (see 4XY).",
  "Vibrato Delay    Ticks until instrument vibrato starts. 00 = no vibrato.",
  "HR/Gate Timer    Ticks before note start for note fetch, gateoff and hard",
  "                 restart. At most tempo-1. Bit 80 disables hard restart,",
  "                 bit 40 disables gateoff.",
  "1stFrame Wave    Waveform on the init frame, usually 09 (gate + testbit).",
  "                 00 = unchanged, FE = gate off, FF = gate on.",
  NULL
};

static const char *tables[] = {
  "Wavetable left:  00 unchanged, 01-0F delay, 10-DF waveform, E0-EF inaudible",
  "                 waveform 00-0F, F0-FE command 0XY-EXY, FF jump (00 = stop)",
  "Wavetable right: 00-5F relative notes, 60-7F negative relative notes,",
  "                 80 keep frequency, 81-DF absolute notes C#0 - B-7",
  "Pulsetable:      01-7F modulation step (time / signed speed),",
  "                 8X-FX set pulse width (X = high bits, right = low bits), FF jump",
  "Filtertable:     00 set cutoff, 01-7F modulation step (time / signed speed),",
  "                 80-F0 set passband (90 lowpass, A0 bandpass ...) and",
  "                 resonance/voice mask, FF jump",
  "Speedtable:      vibrato XX speed YY depth / portamento 16-bit XXYY /",
  "                 funktempo XX YY. If XX has bit 80 set: note independent speed,",
  "                 YY is the divisor.",
  NULL
};

void onlinehelp(int standalone, int context)
{
  int hview = 0;
  int lastrow = 0;
  (void)standalone;
  (void)context;

  for (;;)
  {
    int left, right, third;
    int top = 2 - hview;

    fillarea(0, 0, MAX_COLUMNS, MAX_ROWS, MKCOL(8, C_PANEL), ' ');
    left = right = third = top;

    printtext(1, left++, HELP_HEADER, "GENERAL KEYS");
    left = printrows(1, left, HELP_NORMAL, genkeys);
    left++;
    printtext(1, left++, HELP_HEADER, "SONG (ORDERLIST) EDIT MODE");
    left = printrows(1, left, HELP_NORMAL, songkeys);
    left++;

    printtext(48, right++, HELP_HEADER, "PATTERN EDIT MODE");
    right = printrows(48, right, HELP_NORMAL, patternkeys);
    right++;
    printtext(48, right++, HELP_HEADER, "SONGNAME EDIT MODE");
    printtext(48, right++, HELP_NORMAL, "Use cursor UP/DOWN to change rows");
    right++;

    printtext(95, third++, HELP_HEADER, "INSTRUMENT / TABLE EDIT MODE");
    third = printrows(95, third, HELP_NORMAL, instkeys);
    third++;
    printtext(95, third++, HELP_HEADER, "MULTI-SID");
    third = printrows(95, third, HELP_NORMAL, multisid);
    third++;
    printtext(95, third++, HELP_HEADER, "NEW IN C64 GROOVEGOAT");
    third = printrows(95, third, HELP_NORMAL, newkeys);
    third++;

    printtext(48, right++, HELP_HEADER, "MOUSE");
    right = printrows(48, right, HELP_NORMAL, mousekeys);
    right++;
    printtext(48, right++, HELP_HEADER, "MAC KEYBOARDS");
    right = printrows(48, right, HELP_NORMAL, mackeys);
    right++;

    if (right > left) left = right;
    if (third > left) left = third;

    printtext(1, left++, HELP_HEADER, "PATTERN COMMANDS");
    left = printrows(1, left, HELP_NORMAL, pattcmds);
    left++;
    printtext(1, left++, HELP_HEADER, "INSTRUMENT PARAMETERS");
    left = printrows(1, left, HELP_NORMAL, instparm);
    left++;
    printtext(1, left++, HELP_HEADER, "TABLES");
    left = printrows(1, left, HELP_NORMAL, tables);
    left++;
    printtext(1, left++, MKCOL(7, C_PANEL), "The complete manual (all functions in detail, with search) opens with the \"Manual\" button above or the M key in this screen.");
    lastrow = left + hview;

    printblankc(0, 0, MKCOL(C_HEADERTEXT, C_HEADER), MAX_COLUMNS);
    drawimage(0, 0, "logo", 2, 1);
    printtext(3, 0, MKCOL(C_HEADERTEXT, C_HEADER), "C64 GrooveGoat " PROGRAMVERSION " - Keyboard reference");
    ui_drawbutton(MAX_COLUMNS - 43, 0, " " GS_PRG " History ", BTN_HISTORY, 0, 0);
    ui_drawbutton(MAX_COLUMNS - 30, 0, " " GS_NOTE " Manual ", BTN_MANUAL, 0, 0);
    numuibuttons = 0;
    ui_addbutton(MAX_COLUMNS - 43, 0, 11, BTN_HISTORY, 0);
    ui_addbutton(MAX_COLUMNS - 30, 0, 10, BTN_MANUAL, 0);
    printtext(MAX_COLUMNS - 19, 0, MKCOL(C_BUTTONTEXT, C_BUTTON), "  Close (ESC)  ");
    ui_addbutton(MAX_COLUMNS - 19, 0, 15, BTN_QUIT, 0);
    printblankc(0, MAX_ROWS - 1, MKCOL(C_HEADERTEXT, C_HEADER), MAX_COLUMNS);
    printtext(1, MAX_ROWS - 1, MKCOL(C_HEADERTEXT, C_HEADER), "Arrows / PgUp / PgDn / Home / End / mouse wheel scroll, M opens the manual, H the history, any other key closes");

    fliptoscreen();
    getkey();

    if (win_quitted)
    {
      win_quitted = 0;
      break;
    }

    if (mousewheel) hview -= mousewheel * 3;
    if ((mouseb) && (!prevmouseb))
    {
      int id = ui_hitbutton(mousex, mousey, NULL);
      if (id == BTN_MANUAL)
      {
        showmanual();
        continue;
      }
      if (id == BTN_HISTORY)
      {
        showhistory();
        continue;
      }
      if ((id == BTN_QUIT) || (mousey == 0)) break;
    }

    switch(rawkey)
    {
      case KEY_UP: hview--; break;
      case KEY_DOWN: hview++; break;
      case KEY_PGUP: hview -= MAX_ROWS - 4; break;
      case KEY_PGDN: hview += MAX_ROWS - 4; break;
      case KEY_HOME: hview = 0; break;
      case KEY_END: hview = lastrow - MAX_ROWS + 2; break;
      case KEY_M:
      showmanual();
      break;
      case KEY_H:
      showhistory();
      break;
      case 0: break;
      case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: break;
      default:
      goto EXITHELP;
    }
    if (hview > lastrow - MAX_ROWS + 2) hview = lastrow - MAX_ROWS + 2;
    if (hview < 0) hview = 0;
  }
  EXITHELP: ;
  printmainscreen();
  key = 0;
  rawkey = 0;
}

// ---------------------------------------------------------------------------
// Manual viewer
// ---------------------------------------------------------------------------

#define MAN_MAXLINES 8000
#define MAN_TOC_W 34
#define MAN_TEXT_X (MAN_TOC_W + 2)
#define MAN_TEXT_W (MAX_COLUMNS - MAN_TEXT_X - 2)

// Two documents use the same viewer: the manual and the history of C64 SID
// music. "doc" points to the one that is shown.
typedef struct
{
  const char *resource;
  const char *title;
  char *lines[MAN_MAXLINES];
  unsigned char style[MAN_MAXLINES];
  int num;
  int toc[256];
  int numtoc;
} DOCUMENT;

static DOCUMENT documents[2] = {{"manual.txt", "Manual"}, {"history.txt", "History of C64 SID music"}};
static DOCUMENT *doc = &documents[0];

#define manlines (doc->lines)
#define manstyle (doc->style)
#define nummanlines (doc->num)
#define mantoc (doc->toc)
#define numtoc (doc->numtoc)

static void addmanline(const char *s, int len, int style)
{
  if (nummanlines >= MAN_MAXLINES) return;
  manlines[nummanlines] = (char *)malloc(len + 1);
  memcpy(manlines[nummanlines], s, len);
  manlines[nummanlines][len] = 0;
  manstyle[nummanlines] = style;
  if ((style == 1) && (numtoc < 256)) mantoc[numtoc++] = nummanlines;
  if ((style == 2) && (numtoc < 256)) mantoc[numtoc++] = nummanlines;
  nummanlines++;
}

static void buildmanual(void)
{
  int len = 0;
  const char *text = (const char *)res_getfile(doc->resource, &len);
  const char *p, *end;
  if (nummanlines) return;
  if (!text) return;
  p = text;
  end = text + len;
  while (p < end)
  {
    const char *e = p;
    int style = 0;
    int l;
    while ((e < end) && (*e != '\n')) e++;
    l = e - p;
    if ((l > 0) && (p[l - 1] == '\r')) l--;
    if ((l >= 2) && (!strncmp(p, "# ", 2))) { style = 1; p += 2; l -= 2; }
    else if ((l >= 3) && (!strncmp(p, "## ", 3))) { style = 2; p += 3; l -= 3; }
    else if ((l >= 4) && (!strncmp(p, "### ", 4))) { style = 3; p += 4; l -= 4; }
    else if ((l >= 1) && (p[0] == '|')) { style = 4; p++; l--; }
    else if ((l >= 4) && (!strncmp(p, "    ", 4))) { style = 4; }

    // word wrap normal text
    if ((style == 0) && (l > MAN_TEXT_W))
    {
      const char *s = p;
      int remaining = l;
      int indent = 0;
      if ((l > 2) && (p[0] == '-') && (p[1] == ' ')) indent = 2;
      while (remaining > 0)
      {
        int take = remaining;
        int w = MAN_TEXT_W - ((s != p) ? indent : 0);
        if (take > w)
        {
          take = w;
          while ((take > 0) && (s[take] != ' ')) take--;
          if (take == 0) take = w;
        }
        if (s != p)
        {
          char tmp[512];
          memset(tmp, ' ', indent);
          memcpy(tmp + indent, s, take);
          addmanline(tmp, take + indent, 0);
        }
        else addmanline(s, take, 0);
        s += take;
        remaining -= take;
        while ((remaining > 0) && (*s == ' '))
        {
          s++;
          remaining--;
        }
      }
    }
    else addmanline(p, l > 400 ? 400 : l, style);
    p = e + 1;
  }
}

// Case-insensitive search for "what" from line "from" on (wraps around)
static int findmanline(const char *what, int from)
{
  int n, c;
  char low[64];
  if (!what[0]) return -1;
  for (c = 0; what[c] && c < 63; c++) low[c] = tolower((unsigned char)what[c]);
  low[c] = 0;
  for (n = 0; n < nummanlines; n++)
  {
    int l = (from + n) % nummanlines;
    char line[512];
    int i;
    for (i = 0; manlines[l][i] && i < 511; i++) line[i] = tolower((unsigned char)manlines[l][i]);
    line[i] = 0;
    if (strstr(line, low)) return l;
  }
  return -1;
}

static void showdocument(int which)
{
  int view = 0;
  int textrows = MAX_ROWS - 3;
  static char search[64];
  int hitline = -1;
  char title[100];
  doc = &documents[which];
  buildmanual();
  snprintf(title, sizeof title, "C64 GrooveGoat " PROGRAMVERSION " - %s", doc->title);

  for (;;)
  {
    int c;
    int cursection = 0;
    fillarea(0, 0, MAX_COLUMNS, MAX_ROWS, MKCOL(8, C_PANEL), ' ');
    printblankc(0, 0, MKCOL(C_HEADERTEXT, C_HEADER), MAX_COLUMNS);
    drawimage(0, 0, "logo", 2, 1);
    printtext(3, 0, MKCOL(C_HEADERTEXT, C_HEADER), title);
    numuibuttons = 0;
    // switch to the other document
    if (which == 0)
      ui_drawbutton(MAX_COLUMNS - 31, 0, " " GS_PRG " History ", BTN_HISTORY, 0, 0);
    else
      ui_drawbutton(MAX_COLUMNS - 31, 0, " " GS_NOTE " Manual  ", BTN_HISTORY, 0, 0);
    printtext(MAX_COLUMNS - 19, 0, MKCOL(C_BUTTONTEXT, C_BUTTON), "  Close (ESC)  ");
    ui_addbutton(MAX_COLUMNS - 19, 0, 15, BTN_QUIT, 0);

    for (c = 0; c < numtoc; c++)
      if (mantoc[c] <= view + 1) cursection = c;

    // table of contents
    fillarea(0, 1, MAN_TOC_W, MAX_ROWS - 2, MKCOL(8, C_BEATROW), ' ');
    printtext(1, 1, MKCOL(14, C_BEATROW), "CONTENTS");
    {
      int first = cursection - (MAX_ROWS - 6) / 2;
      if (first > numtoc - (MAX_ROWS - 5)) first = numtoc - (MAX_ROWS - 5);
      if (first < 0) first = 0;
      for (c = 0; (c < MAX_ROWS - 5) && (first + c < numtoc); c++)
      {
        char buf[64];
        int i = first + c;
        int st = manstyle[mantoc[i]];
        int bg = (i == cursection) ? C_SELECT : C_BEATROW;
        printblankc(0, 3 + c, MKCOL(8, bg), MAN_TOC_W);
        sprintf(buf, "%s%-.*s", st == 2 ? "  " : "", st == 2 ? MAN_TOC_W - 4 : MAN_TOC_W - 2, manlines[mantoc[i]]);
        printtext(1, 3 + c, MKCOL(st == 1 ? 15 : 7, bg), buf);
        ui_addbutton(0, 3 + c, MAN_TOC_W, BTN_MANUAL, mantoc[i]);
      }
    }

    for (c = 0; c < textrows; c++)
    {
      int l = view + c;
      int y = 2 + c;
      if (l >= nummanlines) break;
      switch (manstyle[l])
      {
        case 1:
        printblankc(MAN_TEXT_X - 1, y, MKCOL(C_HEADERTEXT, C_HEADER), MAN_TEXT_W + 2);
        printtext(MAN_TEXT_X, y, MKCOL(C_HEADERTEXT, C_HEADER), manlines[l]);
        break;
        case 2:
        printtext(MAN_TEXT_X, y, MKCOL(14, C_PANEL), manlines[l]);
        break;
        case 3:
        printtext(MAN_TEXT_X, y, MKCOL(11, C_PANEL), manlines[l]);
        break;
        case 4:
        printtext(MAN_TEXT_X, y, MKCOL(15, C_PANEL), manlines[l]);
        break;
        default:
        printtext(MAN_TEXT_X, y, MKCOL(8, C_PANEL), manlines[l]);
        break;
      }
      if (l == hitline) printbgx(MAN_TEXT_X - 1, y, C_SELECT, MAN_TEXT_W + 1);
    }
    // scrollbar
    if (nummanlines > textrows)
    {
      int barh = textrows * textrows / nummanlines;
      int bary = (textrows - barh) * view / (nummanlines - textrows);
      if (barh < 1) barh = 1;
      for (c = 0; c < textrows; c++)
        printglyph(MAX_COLUMNS - 1, 2 + c, MKCOL((c >= bary) && (c < bary + barh) ? 9 : C_LINE, C_PANEL), GL_FULL);
    }
    printblankc(0, MAX_ROWS - 1, MKCOL(C_HEADERTEXT, C_HEADER), MAX_COLUMNS);
    printtext(1, MAX_ROWS - 1, MKCOL(C_HEADERTEXT, C_HEADER), which ?
      "Arrows / PgUp / PgDn / Home / End / wheel scroll, LEFT/RIGHT chapter, F search, F3 or N next, M manual, ESC closes" :
      "Arrows / PgUp / PgDn / Home / End / wheel scroll, LEFT/RIGHT chapter, F search, F3 or N next, H history, ESC closes");
    if (search[0])
    {
      char sb[100];
      snprintf(sb, sizeof sb, "Search: %s%s", search, hitline < 0 ? " (not found)" : "");
      printtext(MAX_COLUMNS - 2 - (int)strlen(sb), MAX_ROWS - 1, MKCOL(C_RB3, C_HEADER), sb);
    }

    fliptoscreen();
    getkey();
    if (win_quitted)
    {
      win_quitted = 0;
      break;
    }
    if (mousewheel) view -= mousewheel * 3;
    if ((mouseb) && (!prevmouseb))
    {
      int param = 0;
      int id = ui_hitbutton(mousex, mousey, &param);
      if (id == BTN_QUIT) break;
      if (id == BTN_MANUAL) view = param;
      if (id == BTN_HISTORY)
      {
        showdocument(which ^ 1);
        doc = &documents[which];
        continue;
      }
    }
    switch (rawkey)
    {
      case KEY_UP: view--; break;
      case KEY_DOWN: view++; break;
      case KEY_PGUP: view -= textrows - 1; break;
      case KEY_PGDN:
      case KEY_SPACE: view += textrows - 1; break;
      case KEY_HOME: view = 0; break;
      case KEY_END: view = nummanlines - textrows; break;
      case KEY_LEFT:
      for (c = numtoc - 1; c >= 0; c--)
        if (mantoc[c] < view) { view = mantoc[c]; break; }
      break;
      case KEY_RIGHT:
      for (c = 0; c < numtoc; c++)
        if (mantoc[c] > view) { view = mantoc[c]; break; }
      break;
      case KEY_H:
      case KEY_M:
      if (((rawkey == KEY_H) && (which == 0)) || ((rawkey == KEY_M) && (which == 1)))
      {
        showdocument(which ^ 1);
        doc = &documents[which];
      }
      break;
      case KEY_F:
      if (dlg_edittext(which ? "Search the history" : "Search the manual", "Find:", search, 40, 0))
      {
        hitline = findmanline(search, view);
        if (hitline >= 0) view = hitline - 3;
      }
      break;
      case KEY_F3:
      case KEY_N:
      if (search[0])
      {
        hitline = findmanline(search, hitline >= 0 ? hitline + 1 : view);
        if (hitline >= 0) view = hitline - 3;
      }
      break;
      case KEY_ESC:
      if (hitline >= 0)
      {
        hitline = -1;
        search[0] = 0;
        break;
      }
      // fallthrough
      case KEY_F12:
      goto EXITMAN;
    }
    if (view > nummanlines - textrows) view = nummanlines - textrows;
    if (view < 0) view = 0;
  }
  EXITMAN:
  printmainscreen();
  key = 0;
  rawkey = 0;
}

void showmanual(void)
{
  showdocument(0);
}

void showhistory(void)
{
  showdocument(1);
}

// ---------------------------------------------------------------------------
// Info dialog
// ---------------------------------------------------------------------------

static const char *credits[] = {
  "C64 GrooveGoat is based on GoatTracker 2.76 and uses:",
  "",
  "GoatTracker 2 editor & playroutine   Lasse \xd6\xf6rni (Cadaver), Covert Bitops",
  "GoatTracker 2 \"Silver Fork\"          Joel Ricci  (github.com/joelricci/goattracker2)",
  "Undo system                          Jason Page (GoatTracker Ultra)",
  "reSID SID chip emulation             Dag Lem",
  "reSID-fp distortion / nonlinearity   Antti S. Lankila",
  "6510 cross assembler (Exomizer 2)    Magnus Lind",
  "HardSID 4U support                   T\xe9li S\xe1ndor",
  "Command quick reference              Simon Bennett",
  "GoatTracker patches                  S. A. Haubenthal, V. Cannone, R. M. Ekman,",
  "                                     T. Lindeman, H. Paulini, Groepaz",
  "Microtonal support                   Birgit Jauernig",
  "SDL 2 (Simple DirectMedia Layer)     libsdl.org - zlib license",
  "Fira Code font                       Nikita Prokopov - SIL Open Font License 1.1",
  "LAME MP3 encoder 3.100               The LAME project - GNU LGPL 2",
  "faac AAC encoder 2.2.0               M. Bakker, K. Nikiel, N. Schimmelmann - LGPL 2.1",
  "minih264 / minimp4 (H.264, MP4)      lieff (github.com/lieff) - CC0 public domain",
  "SIDGen instrument library            6581 & 8580 instrument sets",
  "SID file format (PSID v2/v3/v5)      High Voltage SID Collection",
  "",
  "License: GNU General Public License v2 or later. The C64 playroutine and",
  "the songs, SID, PRG, audio and video files you create are free to use for",
  "any purpose. The LGPL encoders are included as source code (src/codecs).",
  NULL
};

void showinfo(void)
{
  int w = 92;
  int h = 44;
  int x = (MAX_COLUMNS - w) / 2;
  int y = (MAX_ROWS - h) / 2 - 1;

  key = 0;
  rawkey = 0;
  for (;;)
  {
    int c;
    int ly;
    char buf[100];

    printstatus();
    dlg_drawframe(x, y, w, h, "About C64 GrooveGoat");
    drawimage(x + 3, y + 2, "logo", 16, 8);
    printtext(x + 22, y + 3, MKCOL(15, C_PANEL), "C64 GrooveGoat");
    sprintf(buf, "Version %s", PROGRAMVERSION);
    printtext(x + 22, y + 5, MKCOL(14, C_PANEL), buf);
    printtext(x + 22, y + 6, MKCOL(8, C_PANEL), "Author: " PROGRAMAUTHOR);
    printtext(x + 22, y + 8, MKCOL(7, C_PANEL), "Multi-SID music editor for the Commodore 64:");
    printtext(x + 22, y + 9, MKCOL(7, C_PANEL), "1, 2, 4 or 8 SID chips - up to 24 voices.");
    for (c = 0; c < 5; c++)
    {
      printglyph(x + 22 + c * 3, y + 11, MKCOL(C_RB1 + c, C_PANEL), GL_FULL);
      printglyph(x + 23 + c * 3, y + 11, MKCOL(C_RB1 + c, C_PANEL), GL_FULL);
      printglyph(x + 24 + c * 3, y + 11, MKCOL(C_RB1 + c, C_PANEL), GL_FULL);
    }

    ly = y + 14;
    {
      int license = 0;
      for (c = 0; credits[c]; c++)
      {
        int col = 8;
        if (c == 0) col = 15;
        // the license text after the credits list is shown in light grey
        if ((c >= 2) && (!credits[c][0])) license = 1;
        if (license) col = 7;
        printtext(x + 3, ly++, MKCOL(col, C_PANEL), credits[c]);
        // highlight the names column
        if ((c >= 2) && (!license) && (strlen(credits[c]) > 37))
          printfgx(x + 3 + 37, ly - 1, 11, strlen(credits[c]) - 37);
      }
    }
    printtext(x + w - 12, y + h - 3, MKCOL(C_BUTTONTEXT, C_BUTTONHI), "   OK   ");

    fliptoscreen();
    getkey();
    if (win_quitted)
    {
      win_quitted = 0;
      break;
    }
    if ((rawkey == KEY_ESC) || (rawkey == KEY_ENTER) || (rawkey == KEY_SPACE)) break;
    if ((mouseb) && (!prevmouseb))
    {
      if ((mousey == y + h - 3) && (mousex >= x + w - 12) && (mousex < x + w - 4)) break;
      if ((mousex < x) || (mousex >= x + w) || (mousey < y) || (mousey >= y + h)) break;
    }
  }
  printmainscreen();
  key = 0;
  rawkey = 0;
}

// ---------------------------------------------------------------------------
// Welcome window shown at startup
// ---------------------------------------------------------------------------

void showwelcomewindow(void)
{
  int w = 66;
  int h = 31;
  int x = (MAX_COLUMNS - w) / 2;
  int y = (MAX_ROWS - h) / 2 - 1;
  static const char *tips[] =
  {
    "Z-M, Q-P     enter notes (piano layout)",
    "SPACE        toggle edit / jam mode",
    "F1 / F4      play / stop",
    "TAB          next editor panel",
    "F12          keyboard help, Manual button: manual",
    "CTRL+Z / Y   undo / redo (Kbd: EN/DE in the status bar)",
    "",
    "More voices: choose 2, 4 or 8 SIDs (up to 24 voices).",
    "Instruments: Library button (search, favourites,",
    "preview) or the dice button for a random instrument.",
    "Share your song: Audio (WAV/MP3) and Video (MP4).",
    "Your work is backed up automatically every 2 minutes.",
    NULL
  };

  key = 0;
  rawkey = 0;
  for (;;)
  {
    int c, ly;
    int bx = x + w - 13;
    int by = y + h - 3;

    printstatus();
    dlg_drawframe(x, y, w, h, "Welcome");
    drawimage(x + 3, y + 2, "logo", 12, 6);
    printtext(x + 18, y + 3, MKCOL(15, C_PANEL), "C64 GrooveGoat " PROGRAMVERSION);
    printtext(x + 18, y + 4, MKCOL(8, C_PANEL), "by " PROGRAMAUTHOR);
    printtext(x + 18, y + 6, MKCOL(7, C_PANEL), "Multi-SID music editor for the C64");
    for (c = 0; c < 5; c++)
    {
      printglyph(x + 18 + c * 2, y + 7, MKCOL(C_RB1 + c, C_PANEL), GL_FULL);
      printglyph(x + 19 + c * 2, y + 7, MKCOL(C_RB1 + c, C_PANEL), GL_FULL);
    }
    ly = y + 10;
    printtext(x + 3, ly++, MKCOL(14, C_PANEL), "Quick start");
    ly++;
    for (c = 0; tips[c]; c++)
    {
      // German keyboards: the lowest note row starts with Y
      if ((kbdlayout == 1) && (c == 0))
        printtext(x + 3, ly++, MKCOL(7, C_PANEL), "Y-M, Q-P     enter notes (piano layout)");
      else
        printtext(x + 3, ly++, MKCOL(7, C_PANEL), tips[c]);
    }

    printtext(x + 3, by, MKCOL(showwelcome ? 15 : 8, C_PANEL), showwelcome ? "[x] Show at startup" : "[ ] Show at startup");
    printtext(bx, by, MKCOL(C_BUTTONTEXT, C_BUTTONHI), "   Close   ");

    fliptoscreen();
    getkey();
    if (win_quitted) break;
    if ((rawkey == KEY_ESC) || (rawkey == KEY_ENTER) || (rawkey == KEY_SPACE)) break;
    if ((rawkey == KEY_S) || (rawkey == KEY_X)) showwelcome ^= 1;
    if ((mouseb) && (!prevmouseb))
    {
      if ((mousey == by) && (mousex >= bx) && (mousex < bx + 11)) break;
      if ((mousey == by) && (mousex >= x + 3) && (mousex < x + 22)) showwelcome ^= 1;
      if ((mousex < x) || (mousex >= x + w) || (mousey < y) || (mousey >= y + h)) break;
    }
  }
  printmainscreen();
  key = 0;
  rawkey = 0;
}

// ---------------------------------------------------------------------------
// SID settings: addresses used for export and listening
// ---------------------------------------------------------------------------

void showsidinfo(void)
{
  int w = 72;
  int h = 20;
  int x = (MAX_COLUMNS - w) / 2;
  int y = (MAX_ROWS - h) / 2 - 2;
  int sel = 0;

  key = 0;
  rawkey = 0;
  for (;;)
  {
    int c;
    char buf[100];
    int hit = -1;
    int hitreset = 0, hitok = 0;

    printstatus();
    dlg_drawframe(x, y, w, h, "SID chips");
    sprintf(buf, "This song uses %d SID%s (%d voices). Model: %s, %s, %s output.", numsids, numsids > 1 ? "s" : "",
      numchannels, editorInfo.sidmodel ? "8580" : "6581", editorInfo.ntsc ? "NTSC" : "PAL", stereomode ? "stereo" : "mono");
    printtext(x + 3, y + 2, MKCOL(15, C_PANEL), buf);
    printtext(x + 3, y + 3, MKCOL(7, C_PANEL), "Default addresses for SID/PRG export (click or ENTER to edit):");
    for (c = 0; c < numsids; c++)
    {
      int ry = y + 5 + c;
      int bg = (c == sel) ? C_SELECT : C_PANEL;
      printblankc(x + 3, ry, MKCOL(8, bg), 40);
      sprintf(buf, "SID %d   voices %2d-%2d   $%04X   pan %+4d", c + 1, c * 3 + 1, c * 3 + 3, sidaddr[c], sidpan[c]);
      printtext(x + 4, ry, MKCOL(15, bg), buf);
      if ((mousey == ry) && (mousex >= x + 3) && (mousex < x + 43)) hit = c;
    }
    printtext(x + 3, y + h - 3, MKCOL(C_BUTTONTEXT, C_BUTTON), " Reset to $D400,$D420... ");
    if ((mousey == y + h - 3) && (mousex >= x + 3) && (mousex < x + 28)) hitreset = 1;
    printtext(x + w - 12, y + h - 3, MKCOL(C_BUTTONTEXT, C_BUTTONHI), "   OK   ");
    if ((mousey == y + h - 3) && (mousex >= x + w - 12) && (mousex < x + w - 4)) hitok = 1;

    fliptoscreen();
    getkey();
    if (win_quitted)
    {
      win_quitted = 0;
      break;
    }
    if ((mouseb) && (!prevmouseb))
    {
      if (hitok) break;
      if (hitreset)
      {
        for (c = 0; c < MAX_SIDS; c++) sidaddr[c] = 0xd400 + c * 0x20;
        songmodified = 1;
      }
      if (hit >= 0)
      {
        sel = hit;
        rawkey = KEY_ENTER;
      }
      else if ((mousex < x) || (mousex >= x + w) || (mousey < y) || (mousey >= y + h)) break;
    }
    switch (rawkey)
    {
      case KEY_UP: if (sel > 0) sel--; break;
      case KEY_DOWN: if (sel < numsids - 1) sel++; break;
      case KEY_ENTER:
      {
        unsigned v = sidaddr[sel];
        char title[40];
        sprintf(title, "SID %d address", sel + 1);
        if (dlg_edithex(title, "Address:", &v, 4))
        {
          if (((v >= 0xd400) && (v <= 0xd7e0)) || ((v >= 0xde00) && (v <= 0xdfe0)))
          {
            sidaddr[sel] = v & 0xffe0;
            songmodified = 1;
          }
          else
          {
            const char *lines[2] = {"SID addresses must be in $D400-$D7E0 or $DE00-$DFE0", "(in steps of $20)."};
            dlg_message("SID address", lines, 2);
          }
        }
      }
      break;
      case KEY_ESC: goto EXITSID;
    }
  }
  EXITSID:
  printmainscreen();
  key = 0;
  rawkey = 0;
}

// Used by the automatic build test: renders the help screen to a file
void helpscreenshot(const char *filename)
{
  int c;
  // draw one frame of the help and manual without waiting for keys
  fillarea(0, 0, MAX_COLUMNS, MAX_ROWS, MKCOL(8, C_PANEL), ' ');
  doc = &documents[0];
  buildmanual();
  for (c = 0; (c < MAX_ROWS - 2) && (c < nummanlines); c++)
    printtext(1, c + 1, MKCOL(manstyle[c] ? 14 : 8, C_PANEL), manlines[c]);
  savescreenshot(filename);
}
