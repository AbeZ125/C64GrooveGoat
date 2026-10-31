#ifndef GSUMUI_H
#define GSUMUI_H

// The one-line summary of the table program under the cursor (or of the current instrument), drawn on row y of the right panel
void drawtablesummaryline(int y);

// The summary window of the table the cursor is in (SHIFT+I in the table editor)
void showtablesummary(void);
void drawwaveicon(int x, int y);
void drawcommandpanel(void);
void showstepeditor(void);
void showspeededitor(void);
int printwrapped(int x, int y, int width, int maxlines, int color, const char *text);

typedef struct
{
  int x, y, w;
  int rk;                         // the key the button stands for
  int shift;                      // pressed as if SHIFT was held
  int setwidth;                   // >= 0: sets the pulse width of the tick to this value
} STEPBTN;

#define MAXBTN 40
void stepbutton(STEPBTN *list, int *nb, int x, int y, const char *label, int rk, int shift, int setwidth, int on, int enabled);

// For the step editor (gstepui.c)
void sum_drawshapes(unsigned char *buf, int w, int h, int x0, int x1, int top, int bot, int shapes, int periods, float duty);
int sum_currentprogram(int table, int *cursorrow);
int sum_pulseshapes(void);
int sum_playrow(int table);
void sum_drawresponse(unsigned char *buf, int w, int h, int x0, int x1, int top, int bot, int type, int res, int cutoff, int bright);
double sum_tickrate(void);
void togglecommandpanel(void);

#endif
