//
// C64 GrooveGoat - plain language summary of the filtertable
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gfsummary.h"

static const char *bandnames[8] = {"no passband", "low-pass", "band-pass", "low+band-pass", "high-pass", "notch (low+high)", "band+high-pass", "low+band+high"};

const char *filter_passband(int type)
{
  return bandnames[(type >> 4) & 7];
}

void filter_voices(char *buf, int size, int ctrl)
{
  char t[16];
  t[0] = 0;
  if (ctrl & 1) strcat(t, "1 ");
  if (ctrl & 2) strcat(t, "2 ");
  if (ctrl & 4) strcat(t, "3 ");
  if (ctrl & 8) strcat(t, "ext ");
  if (!t[0]) strcpy(t, "none");
  else t[strlen(t) - 1] = 0;
  snprintf(buf, size, "%s", t);
}

double filter_cutoffhz(int cutoff)
{
  return 30.0 + (cutoff & 0xff) * 8 * (12000.0 - 30.0) / 2047.0;
}

static void hztext(char *buf, int size, int cutoff)
{
  double hz = filter_cutoffhz(cutoff);
  if (hz < 1000) snprintf(buf, size, "%.0f Hz", hz);
  else snprintf(buf, size, "%.1f kHz", hz / 1000);
}

static void cutoffpct(char *buf, int size, int cutoff)
{
  snprintf(buf, size, "%d%%", (cutoff & 0xff) * 100 / 255);
}

int filter_rowtext(char *buf, int size, int row, int l, int r)
{
  char a[32], b[32], v[16];
  if (l == 0xff)
  {
    if (!r) { snprintf(buf, size, "stop the filtertable"); return PK_STOP; }
    if (r == row) snprintf(buf, size, "stay on this row (jump to %02X)", r);
    else if (r < row) snprintf(buf, size, "loop back to %02X", r);
    else snprintf(buf, size, "jump forward to %02X", r);
    return PK_JUMP;
  }
  if (l >= 0x80)
  {
    filter_voices(v, sizeof v, r);
    snprintf(buf, size, "%s, resonance %X, voices %s", filter_passband(l & 0x70), r >> 4, v);
    return PK_SET;
  }
  if (!l)
  {
    cutoffpct(a, sizeof a, r);
    hztext(b, sizeof b, r);
    snprintf(buf, size, "set cutoff $%02X = %s (%s)", r, a, b);
    return PK_SET;
  }
  {
    int sp = (signed char)r;
    double total = sp * (double)l * 100.0 / 255.0;
    if (!sp) snprintf(buf, size, "hold the cutoff for %dt", l);
    else snprintf(buf, size, "cutoff %s %dt, %+d/t = %+.0f%%", sp > 0 ? "up" : "down", l, sp, total);
    return PK_MODULATE;
  }
}

// --- the replay: one run of filterexec() per tick

#define HASHSIZE 16384

void summarize_filter(FILTSUM *s, const unsigned char *l, const unsigned char *r, int startrow, double tickrate)
{
  unsigned char ptr = (unsigned char)(startrow & 0xff);
  int time = 0, cutoff = 0x80, type = 0, ctrl = 0;
  int tick, i, k;
  static int hash[HASHSIZE];                    // boundary index + 1
  static int bticks[FSUM_MAXTICKS];
  static unsigned char bptr[FSUM_MAXTICKS], btype[FSUM_MAXTICKS], bctrl[FSUM_MAXTICKS], bcut[FSUM_MAXTICKS];
  int nb = 0;
  unsigned char rowseen[256];
  int jumpjump = 0, startcut = cutoff;
  int firstcut = -1;

  memset(s, 0, sizeof *s);
  s->startrow = (startrow > 0 && startrow <= 255) ? startrow : 0;
  if (!s->startrow)
  {
    strcpy(s->filter, "no filter program");
    strcpy(s->shape, "the filter stays as it is");
    strcpy(s->timing, "");
    snprintf(s->oneline, sizeof s->oneline, "no filtertable program");
    return;
  }
  memset(rowseen, 0, sizeof rowseen);
  memset(hash, 0, sizeof hash);

  // the rows, from the start to the first jump or stop
  for (k = s->startrow; k <= 255; k++)
  {
    if (s->nrows >= SUM_MAXROWS) { s->moreRows = 1; break; }
    s->rows[s->nrows].row = k;
    s->rows[s->nrows].kind = filter_rowtext(s->rows[s->nrows].text, sizeof s->rows[0].text, k, l[k - 1], r[k - 1]);
    s->nrows++;
    if (l[k - 1] == 0xff) break;
  }

  for (tick = 0; tick < FSUM_MAXTICKS; tick++)
  {
    int ran = 0;
    if (!ptr)
    {
      s->stopped = 1;
      s->stoptick = tick;
      break;
    }
    if (l[ptr - 1] == 0xff)
    {
      ptr = r[ptr - 1];
      if (!ptr)
      {
        s->stopped = 1;
        s->stoptick = tick;
        break;
      }
      if (l[ptr - 1] == 0xff) jumpjump = ptr;
    }
    if (!time)
    {
      // a row starts: if this state was seen at the start of a row before, the program repeats from there
      unsigned h = ((unsigned)ptr * 2654435761u) ^ ((unsigned)cutoff * 40503u) ^ ((unsigned)type * 97u) ^ ((unsigned)ctrl * 131071u);
      int slot = (int)(h % HASHSIZE), found = -1;
      while (hash[slot])
      {
        int b = hash[slot] - 1;
        if (bptr[b] == ptr && bcut[b] == cutoff && btype[b] == type && bctrl[b] == ctrl) { found = b; break; }
        slot = (slot + 1) % HASHSIZE;
      }
      if (found >= 0)
      {
        s->looped = 1;
        s->loopstart = bticks[found];
        s->looplen = tick - s->loopstart;
        break;
      }
      if (nb < FSUM_MAXTICKS)
      {
        hash[slot] = nb + 1;
        bticks[nb] = tick; bptr[nb] = ptr; bcut[nb] = (unsigned char)cutoff; btype[nb] = (unsigned char)type; bctrl[nb] = (unsigned char)ctrl;
        nb++;
      }
      ran = ptr;
      rowseen[ptr] = 1;
      if (l[ptr - 1] >= 0x80)
      {
        type = l[ptr - 1] & 0x70;
        ctrl = r[ptr - 1];
        ptr++;
        s->setsfilter = 1;
        if (ptr && l[ptr - 1] == 0x00)
        {
          cutoff = r[ptr - 1];
          ptr++;
          s->setscutoff = 1;
          if (firstcut < 0) firstcut = cutoff;
        }
      }
      else
      {
        if (l[ptr - 1]) time = l[ptr - 1];
        else
        {
          cutoff = r[ptr - 1];
          ptr++;
          s->setscutoff = 1;
          if (firstcut < 0) firstcut = cutoff;
        }
      }
    }
    if (time)
    {
      int raw = cutoff + (signed char)r[ptr - 1];
      if (!ran) ran = ptr;
      if (raw < 0 || raw > 255) s->wraps = 1;
      cutoff = (cutoff + r[ptr - 1]) & 0xff;
      time--;
      if (!time) ptr++;
    }
    s->cutoff[tick] = (unsigned short)cutoff;
    s->type[tick] = (unsigned char)type;
    s->ctrl[tick] = (unsigned char)ctrl;
    s->rowof[tick] = (unsigned char)ran;
    s->ticks = tick + 1;
  }
  if (tick >= FSUM_MAXTICKS) s->capped = 1;
  (void)i;

  // the cutoff range (of the repeating part when it repeats)
  {
    int from = s->looped ? s->loopstart : 0, lo = 255, hi = 0;
    for (i = from; i < s->ticks; i++)
    {
      if (s->cutoff[i] < lo) lo = s->cutoff[i];
      if (s->cutoff[i] > hi) hi = s->cutoff[i];
      if (i && s->cutoff[i] != s->cutoff[i - 1]) s->moves = 1;
    }
    if (s->ticks && s->cutoff[0] != startcut) s->moves = 1;
    if (s->ticks == 0) lo = hi = cutoff;
    s->mincut = lo;
    s->maxcut = hi;
  }

  // --- the words
  {
    char v[16], a[24], b[24];
    int last = s->ticks ? s->ticks - 1 : 0;
    int ty = s->ticks ? s->type[last] : type, ct = s->ticks ? s->ctrl[last] : ctrl;
    filter_voices(v, sizeof v, ct);
    if (s->setsfilter) snprintf(s->filter, sizeof s->filter, "%s, resonance %X, voices %s", filter_passband(ty), ct >> 4, v);
    else strcpy(s->filter, "passband, resonance and voices are not set here");

    cutoffpct(a, sizeof a, s->mincut);
    cutoffpct(b, sizeof b, s->maxcut);
    if (s->mincut == s->maxcut) snprintf(s->range, sizeof s->range, "cutoff %s", a);
    else snprintf(s->range, sizeof s->range, "cutoff %s to %s", a, b);

    // the shape: the direction of the moves over the part that matters
    {
      int up = 0, down = 0, rev = 0, dir = 0, from = s->looped ? s->loopstart : 0;
      for (i = from; i < s->ticks; i++)
      {
        int d = (int)s->cutoff[i] - (i ? (int)s->cutoff[i - 1] : startcut);
        if (d > 128) d -= 256;
        if (d < -128) d += 256;
        if (d > 0) { up++; if (dir < 0) rev++; dir = 1; }
        if (d < 0) { down++; if (dir > 0) rev++; dir = -1; }
      }
      if (!s->moves) strcpy(s->shape, "the cutoff stays put");
      else if (s->looped && up && down && rev >= 1) strcpy(s->shape, "sweeps up and down (wah wah)");
      else if (s->looped && up && !down) strcpy(s->shape, s->wraps ? "keeps rising and wraps round" : "sweeps up, jumps back (sawtooth)");
      else if (s->looped && down && !up) strcpy(s->shape, s->wraps ? "keeps falling and wraps round" : "sweeps down, jumps back");
      else if (up && down) strcpy(s->shape, "moves up and down, then stops");
      else if (up) { cutoffpct(b, sizeof b, s->cutoff[last]); snprintf(s->shape, sizeof s->shape, "sweeps up, ends at %s", b); }
      else { cutoffpct(b, sizeof b, s->cutoff[last]); snprintf(s->shape, sizeof s->shape, "sweeps down, ends at %s", b); }
    }

    if (s->looped)
    {
      sum_duration(a, sizeof a, s->looplen, tickrate);
      if (s->loopstart) snprintf(s->timing, sizeof s->timing, "repeats from tick %d, every %dt (%s)", s->loopstart, s->looplen, a);
      else snprintf(s->timing, sizeof s->timing, "repeats every %dt (%s)", s->looplen, a);
    }
    else if (s->capped) snprintf(s->timing, sizeof s->timing, "runs for more than %d ticks", FSUM_MAXTICKS);
    else
    {
      sum_duration(a, sizeof a, s->stoptick, tickrate);
      snprintf(s->timing, sizeof s->timing, "ends after %dt (%s), then holds", s->stoptick, a);
    }

    // one remark
    s->note[0] = 0;
    if (jumpjump) strcpy(s->note, "jump to a jump row: played as a set row (low-pass+band+high)");
    else if (s->setsfilter && !(ty & 0x70)) strcpy(s->note, "no passband: voices sent through the filter are silent");
    else if (s->setsfilter && !(ct & 0xf)) strcpy(s->note, "no voice is sent through the filter");
    else if (!s->setscutoff && s->moves) strcpy(s->note, "starts from the current cutoff (shown from 50%)");
    else if (s->wraps && !s->looped) strcpy(s->note, "the cutoff wraps round at 0% / 100%");
    (void)firstcut;

    // the one line
    if (!s->setsfilter && !s->moves) snprintf(s->oneline, sizeof s->oneline, "%s", s->shape);
    else
    {
      char band[32];
      snprintf(band, sizeof band, "%s", s->setsfilter ? filter_passband(ty) : "filter");
      if (s->looped) snprintf(s->oneline, sizeof s->oneline, "%s, %s, %s", band, s->range, s->looplen ? "loops" : "");
      else snprintf(s->oneline, sizeof s->oneline, "%s, %s, ends after %dt", band, s->range, s->stoptick);
    }
  }
}
