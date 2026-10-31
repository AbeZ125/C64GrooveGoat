//
// C64 GrooveGoat - a one line description of the orderlist entry under the cursor
//

#include <stdio.h>
#include <string.h>
#include "gcommon.h"
#include "gordhint.h"

int order_hint(const unsigned char *list, int len, int pos, char *out, int outsize)
{
  out[0] = 0;
  if ((pos < 0) || (pos > len + 1)) return 0;

  if (pos == len)
  {
    snprintf(out, outsize, "End of list: loops back to position %02X", list[len + 1]);
  }
  else if (pos == len + 1)
  {
    int r = list[len + 1];
    if (r >= len) snprintf(out, outsize, "Restart position %02X is past the list", r);
    else if (r == 0) snprintf(out, outsize, "Restart position 00: whole list loops");
    else snprintf(out, outsize, "Restart position %02X: the loop jumps here", r);
  }
  else
  {
    int v = list[pos];
    if (v < REPEAT)
    {
      int n = 0;
      if ((pos > 0) && (list[pos - 1] >= REPEAT) && (list[pos - 1] < TRANSDOWN)) n = (list[pos - 1] + 1) & 0x0f;
      if ((pos > 0) && (list[pos - 1] >= REPEAT) && (list[pos - 1] < TRANSDOWN))
      {
        if (n == 0) n = 16;
        snprintf(out, outsize, "Plays pattern %02X, %d times in a row", v, n);
      }
      else snprintf(out, outsize, "Plays pattern %02X", v);
    }
    else if (v < TRANSDOWN)
    {
      int n = (v + 1) & 0x0f;
      if (n == 0) n = 16;
      snprintf(out, outsize, "R%X: the next pattern plays %d times", n & 0x0f, n);
    }
    else if (v < TRANSUP)
    {
      int n = 16 - (v & 0x0f);
      snprintf(out, outsize, "-%X: next patterns sound %d half step%s down", n, n, n == 1 ? "" : "s");
    }
    else if (v < LOOPSONG)
    {
      int n = v & 0x0f;
      snprintf(out, outsize, "+%X: next patterns sound %d half step%s up", n, n, n == 1 ? "" : "s");
    }
    else snprintf(out, outsize, "End of list: loops back to position %02X", list[len + 1]);
  }
  return (int)strlen(out);
}
