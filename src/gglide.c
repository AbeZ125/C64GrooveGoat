//
// C64 GrooveGoat - the speed of a glide (see gglide.h)
//

#include "gglide.h"

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
