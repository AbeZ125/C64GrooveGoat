//
// C64 GrooveGoat - which instruments use which table program
//

#include <stdio.h>
#include <string.h>
#include "ginsuse.h"

void insuse_build(INSUSE *u, int ninstr, const unsigned char ptr[][INSUSE_TABLES], int current)
{
  int i, t;
  memset(u, 0, sizeof *u);
  for (i = 1; i < ninstr; i++)
    for (t = 0; t < INSUSE_TABLES; t++)
    {
      int p = ptr[i][t];
      if (!p) continue;
      p--;
      if (!u->count[t][p]) u->first[t][p] = (unsigned char)i;
      if (u->count[t][p] < 255) u->count[t][p]++;
      if (i == current) u->mine[t][p] = 1;
    }
}

int insuse_label(const INSUSE *u, int table, int row, char *out)
{
  int n;
  out[0] = 0;
  if ((table < 0) || (table >= INSUSE_TABLES) || (row < 0) || (row >= INSUSE_ROWS)) return 0;
  n = u->count[table][row];
  if (!n) return 0;
  if (n == 1) snprintf(out, 3, "%02X", u->first[table][row]);
  else if (n < 10) snprintf(out, 3, "x%d", n);
  else snprintf(out, 3, "x+");
  return u->mine[table][row] ? 2 : 1;
}

int insuse_list(int ninstr, const unsigned char ptr[][INSUSE_TABLES], int table, int row, int *out, int max)
{
  int i, n = 0;
  if ((table < 0) || (table >= INSUSE_TABLES) || (row < 0) || (row >= INSUSE_ROWS)) return 0;
  for (i = 1; i < ninstr; i++)
    if (ptr[i][table] == row + 1)
    {
      if (n < max) out[n] = i;
      n++;
    }
  return n;
}
