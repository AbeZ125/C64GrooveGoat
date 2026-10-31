//
// C64 GrooveGoat - the on-screen piano: key layout in character cells (pure, no drawing)
//
// 32 chromatic keys from a C: 19 white keys, 13 black keys. A white key is 3 cells wide with a 1 cell gap,
// a black key is 3 cells wide, sits over the gap and covers the upper KEYLAY_BLACKROWS rows.
//

#ifndef GKEYLAYOUT_H
#define GKEYLAYOUT_H

#define KEYLAY_KEYS 32
#define KEYLAY_WHITEW 4                  // cells from one white key to the next
#define KEYLAY_ROWS 4                    // rows of the keys
#define KEYLAY_BLACKROWS 2

int keylay_isblack(int key);             // key 0 is a C
int keylay_whites(void);                 // number of white keys (19)
int keylay_width(void);                  // width in cells
int keylay_whiteindex(int key);          // for a white key: its number among the white keys; for a black key: the white key on its left
// The cells a key covers: columns x .. x + w - 1, rows 0 .. h - 1
void keylay_keyrect(int key, int *x, int *w, int *h);
// The key under a cell (cx, cy are relative to the top left of the keys), or -1 if none
int keylay_hit(int cx, int cy);

#endif
