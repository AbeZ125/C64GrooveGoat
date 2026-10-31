//
// C64 GrooveGoat - the on-screen piano: drawing and mouse. It sits over the lower rows of the pattern panel.
//
// Keys light up orange while they are held with the mouse or a MIDI keyboard, and green while a voice is playing
// the note (the song, or a test note). The 32 keys start at the C of the current octave.
//

#include "goattrk2.h"
#include "gkeylayout.h"
#include "gvoices.h"

#define KB_ROWS (1 + KEYLAY_ROWS)
#define KB_Y (PATT_ROWS_Y + VISIBLEPATTROWS - KB_ROWS)       // the header row
#define KB_X (PATT_X + (PATT_W - keylay_width()) / 2)

int showkeyboard = 0;
static int mousekey = -1;               // the key held with the mouse

static int keyofnote(int note)          // tracker note value (0 = C-0) to a key of the piano, or -1
{
  int k = note - editorInfo.epoctave * 12;
  return (k >= 0 && k < KEYLAY_KEYS) ? k : -1;
}

void drawkeyboard(void)
{
  int state[KEYLAY_KEYS], below = 0, above = 0, c, k, w, held[VOICES_MAX], nheld;
  char buf[80];
  int x;

  if (!showkeyboard) return;
  for (k = 0; k < KEYLAY_KEYS; k++) state[k] = 0;
  for (c = 0; c < numchannels; c++)
  {
    int n = chn[c].note;
    if (chn[c].gate != 0xff) continue;
    if (n < editorInfo.epoctave * 12) below = 1;
    else if (keyofnote(n) >= 0) state[keyofnote(n)] = 1;
    else above = 1;
  }
  nheld = editor_heldnotes(held, VOICES_MAX);
  for (k = 0; k < nheld; k++)
    if (keyofnote(held[k] - FIRSTNOTE) >= 0) state[keyofnote(held[k] - FIRSTNOTE)] = 2;
  if (mousekey >= 0) state[mousekey] = 2;

  // header
  printblankc(PATT_X, KB_Y, MKCOL(C_HEADERTEXT, C_HEADER), PATT_W);
  snprintf(buf, sizeof buf, " KEYBOARD   octave %d: C-%d to G-%d", editorInfo.epoctave, editorInfo.epoctave, editorInfo.epoctave + 2);
  printtext(PATT_X, KB_Y, MKCOL(C_HEADERTEXT, C_HEADER), buf);
  if (below) printtext(PATT_X + 46, KB_Y, MKCOL(12, C_HEADER), "< lower");
  if (above) printtext(PATT_X + 55, KB_Y, MKCOL(12, C_HEADER), "higher >");
  ui_drawbutton(PATT_X + PATT_W - 15, KB_Y, " - ", BTN_KEYOCT, -1, 0);
  ui_drawbutton(PATT_X + PATT_W - 11, KB_Y, " + ", BTN_KEYOCT, 1, 0);
  ui_drawbutton(PATT_X + PATT_W - 7, KB_Y, " X ", BTN_KEYS, 0, 0);

  fillarea(PATT_X, KB_Y + 1, PATT_W, KEYLAY_ROWS, MKCOL(8, C_PANEL), ' ');

  // white keys first, black keys over them
  for (w = 0; w < 2; w++)
    for (k = 0; k < KEYLAY_KEYS; k++)
    {
      int kx, kw, kh, row, bg, fg;
      if (keylay_isblack(k) != w) continue;
      keylay_keyrect(k, &kx, &kw, &kh);
      bg = state[k] == 2 ? 6 : (state[k] == 1 ? 25 : (w ? 17 : 18));
      fg = w ? 18 : 17;
      if (state[k]) fg = 0;
      for (row = 0; row < kh; row++) printblankc(KB_X + kx, KB_Y + 1 + row, MKCOL(fg, bg), kw);
      if ((!w) && ((k % 12) == 0))
      {
        snprintf(buf, sizeof buf, "C-%d", editorInfo.epoctave + k / 12);
        printtext(KB_X + kx, KB_Y + KEYLAY_ROWS, MKCOL(state[k] ? 0 : 17, bg), buf);
      }
      if (FIRSTNOTE + editorInfo.epoctave * 12 + k > LASTNOTE)       // keys the tracker does not have
        for (row = 0; row < kh; row++) printblankc(KB_X + kx, KB_Y + 1 + row, MKCOL(3, 28), kw);
    }
  (void)x;
}

int keyboard_busy(void)
{
  return mousekey >= 0;
}

// Returns 1 when the mouse is busy with the keys (so the pattern editor under them must not see it)
int keyboardmouse(void)
{
  int down = (mouseb & MOUSEB_LEFT) != 0, hit = -1;

  if (!showkeyboard) { mousekey = -1; return 0; }
  if ((mousex >= KB_X) && (mousex < KB_X + keylay_width()) && (mousey > KB_Y) && (mousey < KB_Y + KB_ROWS))
    hit = keylay_hit(mousex - KB_X, mousey - KB_Y - 1);

  if (!down)
  {
    if (mousekey >= 0) { editor_noteoff(); mousekey = -1; }
    return 0;
  }
  if (!(prevmouseb & MOUSEB_LEFT))
  {
    if ((mousex >= PATT_X) && (mousex < PATT_X + PATT_W) && (mousey > KB_Y) && (mousey < KB_Y + KB_ROWS))
    {
      if (hit >= 0)
      {
        int n = FIRSTNOTE + editorInfo.epoctave * 12 + hit;
        if (n <= LASTNOTE) { editor_noteon(n, 1); mousekey = hit; }
      }
      return 1;
    }
    return 0;
  }
  if (mousekey >= 0)                    // sliding over the keys: plays them, but enters only the first one
  {
    if ((hit >= 0) && (hit != mousekey))
    {
      int n = FIRSTNOTE + editorInfo.epoctave * 12 + hit;
      if (n <= LASTNOTE) { editor_noteon(n, 0); mousekey = hit; }
    }
    return 1;
  }
  return 0;
}
