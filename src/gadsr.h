#ifndef GADSR_H
#define GADSR_H

float adsr_attackms(int a);
float adsr_decayms(int d);
void adsr_timestr(char *buf, float ms);
// Draw the envelope of the AD/SR register values into canvas "id" at cell x,y
void showadsreditor(void);                       // the window with the ADSR sliders (SHIFT+E on Attack/Decay or Sustain/Release)
void drawadsrgraph(int id, int x, int y, int cw, int ch, int ad, int sr, int color);

#endif
