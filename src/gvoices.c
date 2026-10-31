//
// C64 GrooveGoat - which voice (channel) plays which held key
//

#include "gvoices.h"

void voices_init(VOICES *v)
{
  v->n = 0;
  v->clock = 0;
}

static int find(const VOICES *v, int key)
{
  int i;
  for (i = 0; i < v->n; i++)
    if (v->key[i] == key) return i;
  return -1;
}

static void removeat(VOICES *v, int i)
{
  v->n--;
  v->key[i] = v->key[v->n];
  v->chn[i] = v->chn[v->n];
  v->age[i] = v->age[v->n];
}

int voices_on(VOICES *v, int key, int home, int nchan, int span)
{
  int i, k, ch = -1, old;

  if (nchan < 1) nchan = 1;
  if (span < 1) span = 1;
  if (span > nchan) span = nchan;
  home = ((home % nchan) + nchan) % nchan;

  i = find(v, key);
  if (i >= 0)
  {
    v->age[i] = v->clock++;
    return v->chn[i];
  }

  for (k = 0; k < span; k++)
  {
    int c = (home + k) % nchan, used = 0;
    for (i = 0; i < v->n; i++)
      if (v->chn[i] == c) used = 1;
    if (!used) { ch = c; break; }
  }
  if (ch < 0)                                   // every usable channel is taken: steal the oldest key's
  {
    old = -1;
    for (i = 0; i < v->n; i++)
    {
      int inspan = 0;
      for (k = 0; k < span; k++)
        if ((home + k) % nchan == v->chn[i]) inspan = 1;
      if ((inspan) && ((old < 0) || (v->age[i] < v->age[old]))) old = i;
    }
    if (old >= 0) { ch = v->chn[old]; removeat(v, old); }
    else ch = home;
  }
  if (v->n >= VOICES_MAX) removeat(v, 0);
  v->key[v->n] = key;
  v->chn[v->n] = ch;
  v->age[v->n] = v->clock++;
  v->n++;
  return ch;
}

int voices_off(VOICES *v, int key)
{
  int i = find(v, key), ch;
  if (i < 0) return -1;
  ch = v->chn[i];
  removeat(v, i);
  return ch;
}

int voices_count(const VOICES *v) { return v->n; }
int voices_key(const VOICES *v, int i) { return v->key[i]; }
