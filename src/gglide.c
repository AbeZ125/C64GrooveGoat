//
// C64 GrooveGoat - the speed of a glide (see gglide.h)
//

#include "gglide.h"
#include "gcommon.h"

int glide_speed(int f1, int f2, int ticks)
{
  int dist = f1 > f2 ? f1 - f2 : f2 - f1, s;
  if (ticks < 1) ticks = 1;
  s = (dist + ticks - 1) / ticks;
  if (s < 1) s = 1;
  if (s > GLIDE_MAXSPEED) s = GLIDE_MAXSPEED;
  return s;
}

int glide_ticks(int f1, int f2, int speed)
{
  int dist = f1 > f2 ? f1 - f2 : f2 - f1;
  if (speed < 1) speed = 1;
  if (speed > GLIDE_MAXSPEED) speed = GLIDE_MAXSPEED;
  return dist ? (dist + speed - 1) / speed : 1;
}

int glide_tempo(const unsigned char *pat, int row, int deftempo)
{
  int r, t = deftempo;
  for (r = 0; r <= row; r++)
    if ((pat[r * 4 + 2] == CMD_SETTEMPO) && ((pat[r * 4 + 3] & 0x7f) >= 3)) t = pat[r * 4 + 3] & 0x7f;
  return t;
}

// a row that carries on the glide: no note and the command 3 with the data of the glide
static int isfill(const unsigned char *pat, int r, int olddata)
{
  return (olddata > 0) && (pat[r * 4] > LASTNOTE) && (pat[r * 4 + 2] == CMD_TONEPORTA) && (pat[r * 4 + 3] == olddata);
}

int glide_room(const unsigned char *pat, int len, int row, int tpr, int olddata)
{
  int r = row + 1;
  if (tpr < 1) tpr = 1;
  while ((r < len) && ((isfill(pat, r, olddata)) || ((pat[r * 4] > LASTNOTE) && (!pat[r * 4 + 2])))) r++;
  return (r - row) * tpr;
}

int glide_fill(unsigned char *pat, int len, int row, int data, int tpr, int ticks, int olddata)
{
  int r, rows;
  if (tpr < 1) tpr = 1;
  for (r = row + 1; (r < len) && ((isfill(pat, r, olddata)) || ((pat[r * 4] > LASTNOTE) && (!pat[r * 4 + 2]))); r++)
    if (isfill(pat, r, olddata))
    {
      pat[r * 4 + 2] = 0;
      pat[r * 4 + 3] = 0;
    }
  pat[row * 4 + 2] = CMD_TONEPORTA;
  pat[row * 4 + 3] = data;
  if (ticks < 1) return 1;
  rows = (ticks + tpr - 1) / tpr;
  for (r = row + 1; (r < row + rows) && (r < len) && (pat[r * 4] > LASTNOTE) && (!pat[r * 4 + 2]); r++)
  {
    pat[r * 4 + 2] = CMD_TONEPORTA;
    pat[r * 4 + 3] = data;
  }
  return r - row;
}
