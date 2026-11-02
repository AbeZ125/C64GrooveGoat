//
// C64 GrooveGoat - live oscilloscope data: the recent output of every voice, and a stable window of it to draw
//
// The sound thread adds the samples of each voice (scope_add); the screen asks for a window (scope_render) that starts
// where the wave crosses its middle going up, so a steady note stands still instead of crawling across the screen.
// No drawing in here.
//

#ifndef GSCOPE_H
#define GSCOPE_H

#define SCOPE_VOICES 24                // 8 SIDs with 3 voices
#define SCOPE_HIST 16384               // samples kept per voice (a power of 2)
#define SCOPE_SEARCH 2048              // how far back a trigger point is looked for

#ifdef __cplusplus
extern "C" {
#endif

void scope_clear(void);

// Adds n samples of a voice: src[0], src[stride], src[2 * stride] ...
void scope_add(int voice, const short *src, int stride, int n);

// Total number of samples added to the voice so far.
unsigned scope_count(int voice);

// Takes a window of "span" samples that ends at or before the last sample and fills, for each of "cols" columns,
// the top (lo) and bottom (hi) pixel row of the wave in a picture "h" rows high (lo <= hi): row 0 is the top, a
// sample of +full above the middle of the wave is at row 0 and -full at row h-1. Returns 0 if there is not enough data yet (the arrays are then a flat line).
int scope_render(int voice, int span, int cols, int h, int full, int *lo, int *hi);

#ifdef __cplusplus
}
#endif

#endif
