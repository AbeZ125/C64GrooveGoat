#ifndef GCONSOLE_H
#define GCONSOLE_H

// Screen size in character cells
#define MAX_COLUMNS 140
#define MAX_ROWS 50
// Base cell size in pixels (scale 1). Scale 2 is used on HiDPI displays.
#define CELLW 8
#define CELLH 16

#define HOLDDELAY 24
#define DOUBLECLICKDELAY 15

// Themes
#define THEME_NIGHT 0
#define THEME_DAY 1

// Extended palette entries (16-31). Entries 0-15 keep the semantic meaning
// of the original GoatTracker colors.
#define C_PANEL 16
#define C_HEADER 17
#define C_HEADERTEXT 18
#define C_BUTTON 19
#define C_BUTTONTEXT 20
#define C_BUTTONHI 21
#define C_RB1 22
#define C_RB2 23
#define C_RB3 24
#define C_RB4 25
#define C_RB5 26
#define C_LINE 27
#define C_BEATROW 28
#define C_PLAYROW 29
#define C_EDITROW 30
#define C_SELECT 31

// Build a cell color from 5-bit foreground and background indices
#define MKCOL(fg, bg) (0x8000 | (fg) | ((bg) << 5))

int initscreen(void);
void closescreen(void);
void clearscreen(void);
void fliptoscreen(void);
void forceredraw(void);
void settheme(int t);
void setfont(int f);              // the screen font: an index into res_fontnames (gres.h)
int getfont(void);
void printtext(int x, int y, int color, const char *text);
void printtextc(int y, int color, const char *text);
void printtextcp(int cp, int y, int color, const char *text);
void printblank(int x, int y, int length);
void printblankc(int x, int y, int color, int length);
void drawbox(int x, int y, int color, int sx, int sy);
void printbg(int x, int y, int color, int length);
void printbgx(int x, int y, int bg, int length);
void printcursor(int x, int y, int length);
void printfgx(int x, int y, int fg, int length);
void printglyph(int x, int y, int color, int glyph);
void fillarea(int x, int y, int w, int h, int color, int glyph);
void drawpanel(int x, int y, int w, int h, const char *title, int active);
void drawimage(int x, int y, const char *name, int cellsw, int cellsh);
void drawrainbow(int x, int y, int length);
#define MAX_CANVAS 11                 // 0-4: windows and panels, 5-10: the channel oscilloscopes
#define CANVAS_MAXCW 48
#define CANVAS_MAXCH 8
#define CANVAS_STRIDE (CANVAS_MAXCW * 16)
unsigned char *canvas_begin(int id, int cw, int ch);
void canvas_draw(int id, int x, int y, int color);
// Full colour canvas (one per screen, e.g. the piano keyboard): 0xRRGGBB
// pixels at 16x32 pixels per cell, scaled down at normal resolution
#define RGBCANVAS_MAXCW MAX_COLUMNS
#define RGBCANVAS_MAXCH 4
#define RGBCANVAS_STRIDE (RGBCANVAS_MAXCW * 16)
unsigned *rgbcanvas_begin(int cw, int ch);
void rgbcanvas_draw(int x, int y);
void getkey(void);
int savescreenshot(const char *filename);
extern char autoshotname[1024];
void autoscript_tick(void);
extern int autoshotframes;
unsigned getcell(int x, int y);

#ifndef GCONSOLE_C
extern unsigned kbdlayout;
extern int key, rawkey, symkey, shiftpressed, ctrlpressed, altpressed, metapressed, shiftonlypressed, cursorflashdelay;
extern int mouseb, prevmouseb;
extern int mouseheld;
extern int mousex, mousey;
extern int mousepx, mousepy;   // mouse position in 1/16 (x) and 1/32 (y) cell units
extern double mousefx, mousefy;                 // the same, in cells with the fraction (for drawing)
extern int mousewheel;
extern int theme;
extern unsigned bigwindow;
extern int gfxinitted;
#endif

#endif
