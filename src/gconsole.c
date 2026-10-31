/*
 * C64 GrooveGoat "console" output routines
 *
 * Character cell screen like in GoatTracker, but rendered into a 32-bit
 * framebuffer with anti-aliased fonts, two color themes (night = C64 blue,
 * day = light) and HiDPI support. The framebuffer is scaled to the window.
 */

#define GCONSOLE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "goattrk2.h"
#include "gres.h"

int gfxinitted = 0;
unsigned *scrbuffer = NULL;
unsigned *prevscrbuffer = NULL;
int key = 0;
int rawkey = 0;
int symkey = 0;
int shiftpressed = 0;
int shiftonlypressed = 0;
int ctrlpressed = 0;
int altpressed = 0;
int metapressed = 0;
unsigned kbdlayout = 0;   // 0 = EN (QWERTY), 1 = DE (QWERTZ)
int cursorflashdelay = 0;
int mouseb = 0;
int prevmouseb = 0;
int mousex = 0;
int mousey = 0;
int mousepx = 0;
int mousepy = 0;
double mousefx = 0, mousefy = 0;
int mousewheel = 0;
int mouseheld = 0;
int theme = THEME_NIGHT;
unsigned bigwindow = 1;

extern int plat_dstx, plat_dsty, plat_dstw, plat_dsth, plat_fbw, plat_fbh;

static SDL_Texture *texture = NULL;
static Uint32 *fb = NULL;
static int scale = 0;
static int fbw = 0, fbh = 0;
static int fulldirty = 1;
static Uint32 palette[32];
static int cursorbg = 2;
static int cursorfg = 0;

// ---------------------------------------------------------------------------
// Themes
// ---------------------------------------------------------------------------

static const Uint32 nightpal[32] = {
  0x352879, // 0  background: original C64 dark blue
  0x6c5eb5, // 1  C64 light blue (title/mark background)
  0x9ad284, // 2  light green (cursor)
  0x7b71b8, // 3  muted channels
  0xd0574a, // 4  red
  0xa0782f, // 5  brown
  0xe09b48, // 6  orange
  0xbcb6ec, // 7  secondary text
  0x948adf, // 8  normal text (C64 light blue, a bit brighter)
  0xa9a0f5, // 9  light blue
  0xa8f0a0, // 10 edit highlight (light green)
  0x78d8e4, // 11 cyan
  0xff8f80, // 12 playing (light red)
  0xd37be0, // 13 magenta
  0xf2f07a, // 14 yellow
  0xffffff, // 15 white / titles
  0x2c2168, // 16 panel
  0x4a3c9e, // 17 header bar
  0xffffff, // 18 header text
  0x4b3ea3, // 19 button
  0xf4f2ff, // 20 button text
  0x7a6ad8, // 21 button highlight
  0xe2231a, // 22 rainbow red
  0xf7941d, // 23 rainbow orange
  0xffde17, // 24 rainbow yellow
  0x39b54a, // 25 rainbow green
  0x00aeef, // 26 rainbow blue
  0x5a4cb0, // 27 lines
  0x34287a, // 28 beat row
  0x6b3a78, // 29 playing row
  0x2a5a7c, // 30 edit row
  0x5d50b8, // 31 selection
};

static const Uint32 daypal[32] = {
  0xeceaf6, // 0  background
  0xc9c2f0, // 1  marks
  0x6cc46c, // 2  green
  0x9a96b4, // 3  muted
  0xc0392b, // 4  red
  0x8a6420, // 5  brown
  0xc8700f, // 6  orange
  0x5a5480, // 7  secondary text
  0x2e2a55, // 8  normal text
  0x3e31a2, // 9  C64 blue accent
  0x15803a, // 10 edit highlight
  0x0f7585, // 11 cyan
  0xc82828, // 12 playing
  0x8a2e9a, // 13 magenta
  0x7d6e00, // 14 yellow (dark)
  0x121030, // 15 titles (darkest)
  0xfbfaff, // 16 panel
  0x352879, // 17 header bar (C64 blue)
  0xffffff, // 18 header text
  0xdcd7f5, // 19 button
  0x1a1640, // 20 button text
  0xb4aaf0, // 21 button highlight
  0xe2231a, // 22 rainbow red
  0xf7941d, // 23 rainbow orange
  0xf2c500, // 24 rainbow yellow
  0x39b54a, // 25 rainbow green
  0x00aeef, // 26 rainbow blue
  0xc4bee6, // 27 lines
  0xf0eefa, // 28 beat row
  0xf8d7d0, // 29 playing row
  0xd3ecdc, // 30 edit row
  0xcfc8f5, // 31 selection
};

void settheme(int t)
{
  int c;
  const Uint32 *src;

  theme = t ? THEME_DAY : THEME_NIGHT;
  src = theme == THEME_DAY ? daypal : nightpal;
  for (c = 0; c < 32; c++) palette[c] = 0xff000000 | src[c];
  if (theme == THEME_DAY)
  {
    cursorbg = 9;
    cursorfg = 0;
  }
  else
  {
    cursorbg = 2;
    cursorfg = 16;
  }
  fulldirty = 1;
}

// ---------------------------------------------------------------------------
// Image tiles (glyph codes >= 256)
// ---------------------------------------------------------------------------

#define MAX_TILEIMAGES 16
#define FIRSTTILE 256

typedef struct
{
  char name[32];
  int cw, ch;          // size in cells
  int firstglyph;
  Uint32 *pixels;      // RGBA (A in top byte) at current scale
  int pw, ph;
} TILEIMAGE;

static TILEIMAGE tileimages[MAX_TILEIMAGES];
static int numtileimages = 0;
static int nexttile = FIRSTTILE;

// Render an image resource scaled (area average) into the tile pixel buffer
static void buildtileimage(TILEIMAGE *t)
{
  const RESIMAGE *src = NULL;
  const RESIMAGE *i;
  int w = t->cw * CELLW * scale;
  int h = t->ch * CELLH * scale;
  int side = w < h ? w : h;
  int ox = (w - side) / 2;
  int oy = (h - side) / 2;
  int x, y;
  char base[32];

  // Images are named like "logo" and stored in several sizes
  strcpy(base, t->name);
  for (i = res_images; i->name; i++)
  {
    if (!strncmp(i->name, base, strlen(base)))
    {
      if ((!src) || ((src->w < side) && (i->w > src->w)) || ((i->w >= side) && (i->w < src->w)))
        src = i;
    }
  }
  free(t->pixels);
  t->pixels = (Uint32 *)calloc(w * h, sizeof(Uint32));
  t->pw = w;
  t->ph = h;
  if ((!src) || (!t->pixels)) return;

  for (y = 0; y < side; y++)
  {
    for (x = 0; x < side; x++)
    {
      // box filter from source
      int sx0 = x * src->w / side;
      int sx1 = (x + 1) * src->w / side;
      int sy0 = y * src->h / side;
      int sy1 = (y + 1) * src->h / side;
      int sx, sy, n = 0;
      unsigned r = 0, g = 0, b = 0, a = 0;
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sy1 <= sy0) sy1 = sy0 + 1;
      for (sy = sy0; sy < sy1; sy++)
      {
        for (sx = sx0; sx < sx1; sx++)
        {
          const unsigned char *p = src->rgba + (sy * src->w + sx) * 4;
          r += p[0] * p[3];
          g += p[1] * p[3];
          b += p[2] * p[3];
          a += p[3];
          n++;
        }
      }
      if (a)
      {
        r /= a;
        g /= a;
        b /= a;
      }
      a /= n;
      t->pixels[(y + oy) * w + (x + ox)] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
}

static TILEIMAGE *gettileimage(const char *name, int cw, int ch)
{
  int c;
  for (c = 0; c < numtileimages; c++)
  {
    if ((!strcmp(tileimages[c].name, name)) && (tileimages[c].cw == cw) && (tileimages[c].ch == ch))
      return &tileimages[c];
  }
  if (numtileimages >= MAX_TILEIMAGES) return NULL;
  {
    TILEIMAGE *t = &tileimages[numtileimages++];
    memset(t, 0, sizeof(TILEIMAGE));
    strncpy(t->name, name, sizeof t->name - 1);
    t->cw = cw;
    t->ch = ch;
    t->firstglyph = nexttile;
    nexttile += cw * ch;
    if (scale) buildtileimage(t);
    return t;
  }
}

static TILEIMAGE *findtile(int glyph, int *tx, int *ty)
{
  int c;
  for (c = 0; c < numtileimages; c++)
  {
    TILEIMAGE *t = &tileimages[c];
    if ((glyph >= t->firstglyph) && (glyph < t->firstglyph + t->cw * t->ch))
    {
      int idx = glyph - t->firstglyph;
      *tx = idx % t->cw;
      *ty = idx / t->cw;
      return t;
    }
  }
  return NULL;
}

// ---------------------------------------------------------------------------
// Canvases: small pixel graphics (alpha masks at 16x32 pixels per cell) that
// are drawn with the colours of the cells they cover, e.g. the ADSR graph
// ---------------------------------------------------------------------------

#define CANVAS_BASE 0xe000

static inline void setcharcolor(unsigned *dptr, int ch, int color);
#define CANVAS_CELLS 512

static unsigned char canvasbuf[MAX_CANVAS][CANVAS_MAXCH * 32 * CANVAS_MAXCW * 16];
static unsigned char canvasprev[MAX_CANVAS][CANVAS_MAXCH * 32 * CANVAS_MAXCW * 16];
static int canvascw[MAX_CANVAS];
static int canvasch[MAX_CANVAS];

unsigned char *canvas_begin(int id, int cw, int ch)
{
  if ((id < 0) || (id >= MAX_CANVAS)) return NULL;
  if (cw > CANVAS_MAXCW) cw = CANVAS_MAXCW;
  if (ch > CANVAS_MAXCH) ch = CANVAS_MAXCH;
  canvascw[id] = cw;
  canvasch[id] = ch;
  memset(canvasbuf[id], 0, sizeof canvasbuf[id]);
  return canvasbuf[id];
}

void canvas_draw(int id, int x, int y, int color)
{
  int cx, cy;
  int changed;
  if ((id < 0) || (id >= MAX_CANVAS) || (!scrbuffer)) return;
  changed = memcmp(canvasbuf[id], canvasprev[id], sizeof canvasbuf[id]) != 0;
  if (changed) memcpy(canvasprev[id], canvasbuf[id], sizeof canvasbuf[id]);
  for (cy = 0; cy < canvasch[id]; cy++)
  {
    for (cx = 0; cx < canvascw[id]; cx++)
    {
      int idx;
      if ((x + cx < 0) || (x + cx >= MAX_COLUMNS) || (y + cy < 0) || (y + cy >= MAX_ROWS)) continue;
      idx = (x + cx) + (y + cy) * MAX_COLUMNS;
      setcharcolor(scrbuffer + idx, CANVAS_BASE + id * CANVAS_CELLS + cy * canvascw[id] + cx, color);
      if ((changed) && (prevscrbuffer)) prevscrbuffer[idx] = 0xffffffff;
    }
  }
}

// ---------------------------------------------------------------------------
// Full colour canvas
// ---------------------------------------------------------------------------

#define RGBCANVAS_BASE 0xd000

static unsigned rgbbuf[RGBCANVAS_MAXCH * 32 * RGBCANVAS_STRIDE];
static unsigned rgbprev[RGBCANVAS_MAXCH * 32 * RGBCANVAS_STRIDE];
static int rgbcw = 1, rgbch = 1;

unsigned *rgbcanvas_begin(int cw, int ch)
{
  if (cw > RGBCANVAS_MAXCW) cw = RGBCANVAS_MAXCW;
  if (ch > RGBCANVAS_MAXCH) ch = RGBCANVAS_MAXCH;
  if (cw < 1) cw = 1;
  if (ch < 1) ch = 1;
  rgbcw = cw;
  rgbch = ch;
  return rgbbuf;
}

void rgbcanvas_draw(int x, int y)
{
  int cx, cy;
  int changed;
  if (!scrbuffer) return;
  changed = memcmp(rgbbuf, rgbprev, rgbch * 32 * RGBCANVAS_STRIDE * sizeof(unsigned)) != 0;
  if (changed) memcpy(rgbprev, rgbbuf, rgbch * 32 * RGBCANVAS_STRIDE * sizeof(unsigned));
  for (cy = 0; cy < rgbch; cy++)
  {
    for (cx = 0; cx < rgbcw; cx++)
    {
      int idx;
      if ((x + cx < 0) || (x + cx >= MAX_COLUMNS) || (y + cy < 0) || (y + cy >= MAX_ROWS)) continue;
      idx = (x + cx) + (y + cy) * MAX_COLUMNS;
      scrbuffer[idx] = RGBCANVAS_BASE + cy * rgbcw + cx;
      if ((changed) && (prevscrbuffer)) prevscrbuffer[idx] = 0xffffffff;
    }
  }
}

void drawimage(int x, int y, const char *name, int cellsw, int cellsh)
{
  TILEIMAGE *t = gettileimage(name, cellsw, cellsh);
  int cx, cy;
  if ((!t) || (!scrbuffer)) return;
  for (cy = 0; cy < cellsh; cy++)
  {
    for (cx = 0; cx < cellsw; cx++)
    {
      unsigned *dptr;
      if ((x + cx < 0) || (x + cx >= MAX_COLUMNS) || (y + cy < 0) || (y + cy >= MAX_ROWS)) continue;
      dptr = scrbuffer + (x + cx) + (y + cy) * MAX_COLUMNS;
      *dptr = (*dptr & 0xffff0000) | (t->firstglyph + cy * cellsw + cx);
    }
  }
}

// ---------------------------------------------------------------------------
// Low level cell access
// ---------------------------------------------------------------------------

static inline unsigned decodecolor(int color)
{
  int fg, bg;
  if (color & 0x8000)
  {
    fg = color & 31;
    bg = (color >> 5) & 31;
  }
  else
  {
    fg = color & 15;
    bg = (color >> 4) & 15;
  }
  return (fg << 16) | (bg << 21);
}

static inline void setcharcolor(unsigned *dptr, int ch, int color)
{
  *dptr = (ch & 0xffff) | decodecolor(color);
}

unsigned getcell(int x, int y)
{
  if ((!scrbuffer) || (x < 0) || (y < 0) || (x >= MAX_COLUMNS) || (y >= MAX_ROWS)) return 0;
  return scrbuffer[x + y * MAX_COLUMNS];
}

// ---------------------------------------------------------------------------
// Init / shutdown
// ---------------------------------------------------------------------------

static void seticon(void)
{
  const RESIMAGE *img = res_getimage("logo64");
  SDL_Surface *s;
  if (!img) return;
  s = SDL_CreateRGBSurfaceFrom((void *)img->rgba, img->w, img->h, 32, img->w * 4,
    0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
  if (s)
  {
    plat_seticon(s);
    SDL_FreeSurface(s);
  }
}

static int setupscale(void)
{
  int newscale = plat_getscale();
  int c;

  if ((newscale == scale) && (texture) && (fb)) return 1;
  scale = newscale;
  fbw = MAX_COLUMNS * CELLW * scale;
  fbh = MAX_ROWS * CELLH * scale;
  plat_fbw = fbw;
  plat_fbh = fbh;
  if (texture) SDL_DestroyTexture(texture);
  free(fb);
  fb = (Uint32 *)malloc(fbw * fbh * sizeof(Uint32));
  texture = SDL_CreateTexture(plat_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, fbw, fbh);
  if ((!fb) || (!texture)) return 0;
  for (c = 0; c < numtileimages; c++) buildtileimage(&tileimages[c]);
  fulldirty = 1;
  return 1;
}

int initscreen(void)
{
  if (!plat_init("C64 GrooveGoat", MAX_COLUMNS * CELLW, MAX_ROWS * CELLH))
    return 0;
  seticon();

  scrbuffer = (unsigned *)malloc(MAX_COLUMNS * MAX_ROWS * sizeof(unsigned));
  prevscrbuffer = (unsigned *)malloc(MAX_COLUMNS * MAX_ROWS * sizeof(unsigned));
  if ((!scrbuffer) || (!prevscrbuffer)) return 0;

  settheme(theme);
  if (!setupscale()) return 0;

  gfxinitted = 1;
  clearscreen();
  atexit(closescreen);
  return 1;
}

void closescreen(void)
{
  int c;
  if (!gfxinitted) return;
  gfxinitted = 0;
  free(scrbuffer);
  free(prevscrbuffer);
  scrbuffer = prevscrbuffer = NULL;
  free(fb);
  fb = NULL;
  for (c = 0; c < numtileimages; c++)
  {
    free(tileimages[c].pixels);
    tileimages[c].pixels = NULL;
  }
  if (texture) SDL_DestroyTexture(texture);
  texture = NULL;
  plat_shutdown();
}

void forceredraw(void)
{
  fulldirty = 1;
}

// ---------------------------------------------------------------------------
// Text output (same API as GoatTracker)
// ---------------------------------------------------------------------------

void clearscreen(void)
{
  int c;
  unsigned *dptr = scrbuffer;
  if (!gfxinitted) return;
  for (c = 0; c < MAX_ROWS * MAX_COLUMNS; c++)
  {
    setcharcolor(dptr, 0x20, 0x7);
    dptr++;
  }
}

void printtext(int x, int y, int color, const char *text)
{
  unsigned *dptr;
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS)) return;
  dptr = scrbuffer + (x + y * MAX_COLUMNS);
  while (*text)
  {
    if ((x >= 0) && (x < MAX_COLUMNS)) setcharcolor(dptr, (unsigned char)*text, color);
    dptr++;
    x++;
    text++;
  }
}

void printtextc(int y, int color, const char *text)
{
  int x = (MAX_COLUMNS - (int)strlen(text)) / 2;
  printtext(x, y, color, text);
}

void printtextcp(int cp, int y, int color, const char *text)
{
  int x = cp - ((int)strlen(text) / 2);
  printtext(x, y, color, text);
}

void printblank(int x, int y, int length)
{
  printblankc(x, y, 0x7, length);
}

void printblankc(int x, int y, int color, int length)
{
  unsigned *dptr;
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS)) return;
  dptr = scrbuffer + (x + y * MAX_COLUMNS);
  while (length--)
  {
    if ((x >= 0) && (x < MAX_COLUMNS)) setcharcolor(dptr, 0x20, color);
    dptr++;
    x++;
  }
}

void printglyph(int x, int y, int color, int glyph)
{
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS) || (x < 0) || (x >= MAX_COLUMNS)) return;
  setcharcolor(scrbuffer + x + y * MAX_COLUMNS, glyph, color);
}

void fillarea(int x, int y, int w, int h, int color, int glyph)
{
  int cx, cy;
  for (cy = y; cy < y + h; cy++)
    for (cx = x; cx < x + w; cx++)
      printglyph(cx, cy, color, glyph);
}

void drawbox(int x, int y, int color, int sx, int sy)
{
  int c;
  if (!gfxinitted) return;
  if ((!sx) || (!sy)) return;
  for (c = 1; c < sx - 1; c++)
  {
    printglyph(x + c, y, color, GL_HLINE);
    printglyph(x + c, y + sy - 1, color, GL_HLINE);
  }
  for (c = 1; c < sy - 1; c++)
  {
    printglyph(x, y + c, color, GL_VLINE);
    printglyph(x + sx - 1, y + c, color, GL_VLINE);
  }
  printglyph(x, y, color, GL_TL);
  printglyph(x + sx - 1, y, color, GL_TR);
  printglyph(x, y + sy - 1, color, GL_BL);
  printglyph(x + sx - 1, y + sy - 1, color, GL_BR);
}

// Panel: a filled area with a header bar (title) on its first row
void drawpanel(int x, int y, int w, int h, const char *title, int active)
{
  fillarea(x, y, w, h, MKCOL(8, C_PANEL), ' ');
  if (title)
  {
    printblankc(x, y, MKCOL(C_HEADERTEXT, active ? C_BUTTONHI : C_HEADER), w);
    printtext(x + 1, y, MKCOL(C_HEADERTEXT, active ? C_BUTTONHI : C_HEADER), title);
  }
}

void drawrainbow(int x, int y, int length)
{
  int c;
  for (c = 0; c < length; c++)
    printglyph(x + c, y, MKCOL(C_RB1 + (c * 5 / (length > 0 ? length : 1)), 0), GL_FULL);
}

void printbg(int x, int y, int color, int length)
{
  unsigned *dptr;
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS)) return;
  dptr = scrbuffer + (x + y * MAX_COLUMNS);
  while (length--)
  {
    if ((x >= 0) && (x < MAX_COLUMNS))
    {
      if (color == 1)
        *dptr = (*dptr & 0xffff) | decodecolor(MKCOL(15, C_SELECT));
      else
        *dptr = (*dptr & 0xffff) | decodecolor(MKCOL(cursorfg, cursorbg));
    }
    dptr++;
    x++;
  }
}

void printcursor(int x, int y, int length)
{
  unsigned *dptr;
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS)) return;
  dptr = scrbuffer + (x + y * MAX_COLUMNS);
  while (length--)
  {
    if ((x >= 0) && (x < MAX_COLUMNS))
      *dptr = (*dptr & 0xffff) | decodecolor(MKCOL(cursorfg, cursorbg));
    dptr++;
    x++;
  }
}

void printbgx(int x, int y, int bg, int length)
{
  unsigned *dptr;
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS)) return;
  dptr = scrbuffer + (x + y * MAX_COLUMNS);
  while (length--)
  {
    if ((x >= 0) && (x < MAX_COLUMNS))
      *dptr = (*dptr & ~(31u << 21)) | ((unsigned)(bg & 31) << 21);
    dptr++;
    x++;
  }
}

void printfgx(int x, int y, int fg, int length)
{
  unsigned *dptr;
  if (!gfxinitted) return;
  if ((y < 0) || (y >= MAX_ROWS)) return;
  dptr = scrbuffer + (x + y * MAX_COLUMNS);
  while (length--)
  {
    if ((x >= 0) && (x < MAX_COLUMNS))
      *dptr = (*dptr & ~(31u << 16)) | ((unsigned)(fg & 31) << 16);
    dptr++;
    x++;
  }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

static inline Uint32 blend(Uint32 bg, Uint32 fg, unsigned a)
{
  unsigned ia = 255 - a;
  unsigned r = (((bg >> 16) & 0xff) * ia + ((fg >> 16) & 0xff) * a) / 255;
  unsigned g = (((bg >> 8) & 0xff) * ia + ((fg >> 8) & 0xff) * a) / 255;
  unsigned b = ((bg & 0xff) * ia + (fg & 0xff) * a) / 255;
  return 0xff000000 | (r << 16) | (g << 8) | b;
}

static void rendercell(int x, int y, unsigned cell)
{
  int glyph = cell & 0xffff;
  Uint32 fgc = palette[(cell >> 16) & 31];
  Uint32 bgc = palette[(cell >> 21) & 31];
  int cw = CELLW * scale;
  int ch = CELLH * scale;
  Uint32 *dst = fb + (y * ch) * fbw + x * cw;
  int px, py;

  if ((glyph >= RGBCANVAS_BASE) && (glyph < CANVAS_BASE))
  {
    int idx = glyph - RGBCANVAS_BASE;
    int tx = idx % rgbcw, ty = idx / rgbcw;
    const unsigned *a = rgbbuf + (ty * 32) * RGBCANVAS_STRIDE + tx * 16;
    for (py = 0; py < ch; py++)
    {
      for (px = 0; px < cw; px++)
      {
        if (scale == 1)
        {
          const unsigned *q = a + (py * 2) * RGBCANVAS_STRIDE + px * 2;
          unsigned r = ((q[0] >> 16) & 0xff) + ((q[1] >> 16) & 0xff) + ((q[RGBCANVAS_STRIDE] >> 16) & 0xff) + ((q[RGBCANVAS_STRIDE + 1] >> 16) & 0xff);
          unsigned g = ((q[0] >> 8) & 0xff) + ((q[1] >> 8) & 0xff) + ((q[RGBCANVAS_STRIDE] >> 8) & 0xff) + ((q[RGBCANVAS_STRIDE + 1] >> 8) & 0xff);
          unsigned b = (q[0] & 0xff) + (q[1] & 0xff) + (q[RGBCANVAS_STRIDE] & 0xff) + (q[RGBCANVAS_STRIDE + 1] & 0xff);
          dst[px] = 0xff000000 | (((r + 2) >> 2) << 16) | (((g + 2) >> 2) << 8) | ((b + 2) >> 2);
        }
        else dst[px] = 0xff000000 | a[py * RGBCANVAS_STRIDE + px];
      }
      dst += fbw;
    }
    return;
  }
  if (glyph >= CANVAS_BASE)
  {
    int id = (glyph - CANVAS_BASE) / CANVAS_CELLS;
    int idx = (glyph - CANVAS_BASE) % CANVAS_CELLS;
    int ccw = canvascw[id] ? canvascw[id] : 1;
    int tx = idx % ccw, ty = idx / ccw;
    const int stride = CANVAS_MAXCW * 16;
    const unsigned char *a = canvasbuf[id] + (ty * 32) * stride + tx * 16;
    for (py = 0; py < ch; py++)
    {
      for (px = 0; px < cw; px++)
      {
        unsigned v;
        if (scale == 1)
        {
          const unsigned char *q = a + (py * 2) * stride + px * 2;
          v = (q[0] + q[1] + q[stride] + q[stride + 1] + 2) >> 2;
        }
        else v = a[py * stride + px];
        if (!v) dst[px] = bgc;
        else if (v == 255) dst[px] = fgc;
        else dst[px] = blend(bgc, fgc, v);
      }
      dst += fbw;
    }
    return;
  }
  if (glyph < 256)
  {
    const unsigned char *a = (scale == 1 ? res_font1 : res_font2) + glyph * cw * ch;
    for (py = 0; py < ch; py++)
    {
      for (px = 0; px < cw; px++)
      {
        unsigned v = *a++;
        if (!v) dst[px] = bgc;
        else if (v == 255) dst[px] = fgc;
        else dst[px] = blend(bgc, fgc, v);
      }
      dst += fbw;
    }
  }
  else
  {
    int tx = 0, ty = 0;
    TILEIMAGE *t = findtile(glyph, &tx, &ty);
    if ((!t) || (!t->pixels))
    {
      for (py = 0; py < ch; py++, dst += fbw)
        for (px = 0; px < cw; px++) dst[px] = bgc;
      return;
    }
    for (py = 0; py < ch; py++)
    {
      const Uint32 *s = t->pixels + (ty * ch + py) * t->pw + tx * cw;
      for (px = 0; px < cw; px++)
      {
        Uint32 v = s[px];
        unsigned a = v >> 24;
        dst[px] = a ? blend(bgc, v, a) : bgc;
      }
      dst += fbw;
    }
  }
}

char autoshotname[1024];
int autoshotframes = 0;

void fliptoscreen(void)
{
  int x, y;
  int firstrow = MAX_ROWS, lastrow = -1;
  static int presented = 0;

  if (!gfxinitted) return;

  if (win_resized)
  {
    win_resized = 0;
    setupscale();
    fulldirty = 1;
  }
  if (win_redraw)
  {
    win_redraw = 0;
    presented = 0;
  }

  for (y = 0; y < MAX_ROWS; y++)
  {
    unsigned *sptr = scrbuffer + y * MAX_COLUMNS;
    unsigned *cmpptr = prevscrbuffer + y * MAX_COLUMNS;
    for (x = 0; x < MAX_COLUMNS; x++)
    {
      if ((fulldirty) || (sptr[x] != cmpptr[x]))
      {
        cmpptr[x] = sptr[x];
        rendercell(x, y, sptr[x]);
        if (y < firstrow) firstrow = y;
        if (y > lastrow) lastrow = y;
      }
    }
  }
  fulldirty = 0;

  if (lastrow >= firstrow)
  {
    SDL_Rect r;
    r.x = 0;
    r.y = firstrow * CELLH * scale;
    r.w = fbw;
    r.h = (lastrow - firstrow + 1) * CELLH * scale;
    SDL_UpdateTexture(texture, &r, fb + r.y * fbw, fbw * sizeof(Uint32));
    presented = 0;
  }

  if (!presented)
  {
    int outw = 1, outh = 1;
    SDL_Rect dst;
    double sx, sy, s;
    SDL_GetRendererOutputSize(plat_renderer, &outw, &outh);
    sx = (double)outw / fbw;
    sy = (double)outh / fbh;
    s = sx < sy ? sx : sy;
    dst.w = (int)(fbw * s);
    dst.h = (int)(fbh * s);
    dst.x = (outw - dst.w) / 2;
    dst.y = (outh - dst.h) / 2;
    plat_dstx = dst.x;
    plat_dsty = dst.y;
    plat_dstw = dst.w;
    plat_dsth = dst.h;
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(texture, ((dst.w == fbw) && (dst.h == fbh)) ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
#endif
    SDL_SetRenderDrawColor(plat_renderer, (palette[0] >> 16) & 0xff, (palette[0] >> 8) & 0xff, palette[0] & 0xff, 255);
    SDL_RenderClear(plat_renderer);
    SDL_RenderCopy(plat_renderer, texture, NULL, &dst);
    SDL_RenderPresent(plat_renderer);
    presented = 1;
  }

  // Automatic screenshot of whatever is shown (build tests)
  if ((autoshotname[0]) && (--autoshotframes <= 0))
  {
    char name[1024];
    strcpy(name, autoshotname);
    autoshotname[0] = 0;
    savescreenshot(name);
    exit(0);
  }
}

int savescreenshot(const char *filename)
{
  SDL_Surface *s;
  int result;
  if (!fb) return 0;
  fulldirty = 1;
  fliptoscreen();
  s = SDL_CreateRGBSurfaceFrom(fb, fbw, fbh, 32, fbw * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0);
  if (!s) return 0;
  result = SDL_SaveBMP(s, filename);
  SDL_FreeSurface(s);
  return result == 0;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void getkey(void)
{
  int c;
  autoscript_tick();
  win_asciikey = 0;
  win_symkey = 0;
  cursorflashdelay += win_getspeed(50);

  prevmouseb = mouseb;
  mouseb = win_mousebuttons;
  mousex = win_mousex / (CELLW * (scale ? scale : 1));
  mousey = win_mousey / (CELLH * (scale ? scale : 1));
  mousepx = win_mousex * 16 / (CELLW * (scale ? scale : 1));
  mousepy = win_mousey * 32 / (CELLH * (scale ? scale : 1));
  mousefx = (double)win_mousex / (CELLW * (scale ? scale : 1));
  mousefy = (double)win_mousey / (CELLH * (scale ? scale : 1));
  if (mousex >= MAX_COLUMNS) mousex = MAX_COLUMNS - 1;
  if (mousey >= MAX_ROWS) mousey = MAX_ROWS - 1;
  mousewheel = win_mousewheel;
  win_mousewheel = 0;

  if (mouseb) mouseheld++;
  else mouseheld = 0;

  key = win_asciikey;
  symkey = win_symkey;
  rawkey = 0;
  for (c = 0; c < MAX_KEYS; c++)
  {
    if (win_keytable[c])
    {
      if ((c != KEY_LEFTSHIFT) && (c != KEY_RIGHTSHIFT) &&
          (c != KEY_CTRL) && (c != KEY_RIGHTCTRL) &&
          (c != KEY_ALT) && (c != KEY_RIGHTALT) &&
          (c != KEY_LEFTMETA) && (c != KEY_RIGHTMETA))
      {
        rawkey = c;
        win_keytable[c] = 0;
        break;
      }
      win_keytable[c] = 0;
    }
  }

  shiftpressed = 0;
  ctrlpressed = 0;
  altpressed = 0;
  metapressed = 0;
  shiftonlypressed = 0;

  if ((win_keystate[KEY_LEFTSHIFT]) || (win_keystate[KEY_RIGHTSHIFT]) ||
      (win_keystate[KEY_CTRL]) || (win_keystate[KEY_RIGHTCTRL]))
    shiftpressed = 1;
  if (((win_keystate[KEY_LEFTSHIFT]) || (win_keystate[KEY_RIGHTSHIFT])) &&
      (!win_keystate[KEY_CTRL]) && (!win_keystate[KEY_RIGHTCTRL]) &&
      (!win_keystate[KEY_LEFTMETA]) && (!win_keystate[KEY_RIGHTMETA]))
    shiftonlypressed = 1;
  if ((win_keystate[KEY_CTRL]) || (win_keystate[KEY_RIGHTCTRL]))
    ctrlpressed = 1;
  if ((win_keystate[KEY_ALT]) || (win_keystate[KEY_RIGHTALT]))
    altpressed = 1;
  if ((win_keystate[KEY_LEFTMETA]) || (win_keystate[KEY_RIGHTMETA]))
    metapressed = 1;

  // Keyboard layout DE (QWERTZ): commands with SHIFT / CTRL / CMD use the
  // key labels, so the Y and Z keys are swapped for them (CTRL+Z = undo on
  // the key labelled Z). Notes are always entered by key position.
  if ((kbdlayout == 1) && ((shiftpressed) || (metapressed)))
  {
    if (rawkey == KEY_Z) rawkey = KEY_Y;
    else if (rawkey == KEY_Y) rawkey = KEY_Z;
  }

  // While CTRL / CMD / ALT is held, no text characters are wanted
  if ((ctrlpressed) || (metapressed))
  {
    if ((key >= 32) && (key < 256)) key = 0;
  }

  if (rawkey == KEY_KPENTER)
  {
    key = KEY_ENTER;
    rawkey = KEY_ENTER;
  }

  if ((rawkey >= KEY_KP0) && (rawkey <= KEY_KP9) && (!key))
    key = '0' + rawkey - KEY_KP0;

  // Macbook keyboard: insert using ALT+BACKSPACE or FN+SHIFT+BACKSPACE
  if (((rawkey == KEY_BACKSPACE) && (altpressed)) ||
      ((rawkey == KEY_DEL) && (shiftonlypressed)))
  {
    rawkey = KEY_INS;
    key = 0;
  }
}
