#ifndef GDISPLAY_H
#define GDISPLAY_H

#define CNORMAL 8
#define CMUTE 3
#define CEDIT 10
#define CPLAYING 12
#define CCOMMAND 7
#define CTITLE 15

// Clickable UI elements (registered while drawing)
enum
{
  BTN_NONE = 0,
  BTN_NEW, BTN_OPEN, BTN_SAVE, BTN_SAVEAS, BTN_INSLOAD, BTN_INSSAVE,
  BTN_EXPORTSID, BTN_EXPORTPRG, BTN_THEME, BTN_FONT, BTN_HELP, BTN_MANUAL, BTN_INFO,
  BTN_PLAY, BTN_PLAYPOS, BTN_PLAYPATT, BTN_STOP, BTN_FOLLOW,
  BTN_SIDS1, BTN_SIDS2, BTN_SIDS4, BTN_SIDS8,
  BTN_MODEL, BTN_NTSC, BTN_SPEED, BTN_HR, BTN_FV, BTN_PO, BTN_RO, BTN_INT, BTN_FP,
  BTN_STEREO, BTN_OCTAVE, BTN_RECORD, BTN_AUTOADV, BTN_UNDO, BTN_KEYMODE,
  BTN_CHNVIEW, BTN_ORDVIEW, BTN_SUBTUNE, BTN_INSTRNUM,
  BTN_MUTE, BTN_SOLO, BTN_PATTNUM, BTN_SIDADDR,
  BTN_LIBRARY, BTN_QUIT, BTN_VOLUME, BTN_REDO, BTN_KBDLAYOUT,
  BTN_EXPORTAUDIO, BTN_EXPORTVIDEO, BTN_RANDOMINS, BTN_ADSRGRAPH, BTN_HISTORY,
  // pattern tool row
  BTN_TRANSUP, BTN_TRANSDN, BTN_OCTUP, BTN_OCTDN, BTN_MARKUP, BTN_MARKDN, BTN_MARKALL,
  BTN_PATTPREV, BTN_PATTNEXT, BTN_CUT, BTN_COPY, BTN_PASTE,
  // piano keyboard
  BTN_PIANO, BTN_PIANOKEY
};

typedef struct
{
  int x, y, w;
  int id;
  int param;
} UIBUTTON;

#define MAX_UIBUTTONS 128

void printmainscreen(void);
void displayupdate(void);
void printstatus(void);
void resettime(void);
void incrementtime(void);
void setstatusmessage(const char *msg);
int ui_hitbutton(int mx, int my, int *param);
int ui_arrowat(int x, int y);
void ui_addbutton(int x, int y, int w, int id, int param);
void ui_drawbutton(int x, int y, const char *label, int id, int param, int active);
void validatechnview(void);
void formatsongtime(int frames, char *buf);

extern UIBUTTON uibuttons[MAX_UIBUTTONS];
extern int numuibuttons;
extern int timemin, timesec, timeframe;
extern char *notename[];

#endif
