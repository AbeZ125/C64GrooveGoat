//
// C64 GrooveGoat - plain language summaries of the instrument tables (see gsummary.h)
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gsummary.h"

void sum_percent(char *buf, int size, int width12)
{
  double p = (width12 & 0xfff) * 100.0 / 4096.0;
  double r = floor(p + 0.5);
  if (fabs(p - r) < 0.05) snprintf(buf, size, "%d%%", (int)r);
  else snprintf(buf, size, "%.1f%%", p);
}

void sum_duration(char *buf, int size, double ticks, double tickrate)
{
  double ms = tickrate > 0 ? ticks * 1000.0 / tickrate : 0;
  if (ms < 1000) snprintf(buf, size, "%d ms", (int)(ms + 0.5));
  else if (ms < 10000) snprintf(buf, size, "%.1f s", ms / 1000.0);
  else snprintf(buf, size, "%d s", (int)(ms / 1000.0 + 0.5));
}

// The change of a number of 12 bit width units as a signed percentage ("+50%", "-0.8%")
static void delta_percent(char *buf, int size, double units)
{
  double p = units * 100.0 / 4096.0;
  double r = floor(fabs(p) + 0.5);
  if (fabs(fabs(p) - r) < 0.05) snprintf(buf, size, "%c%d%%", p < 0 ? '-' : '+', (int)r);
  else snprintf(buf, size, "%c%.1f%%", p < 0 ? '-' : '+', fabs(p));
}

int pulse_rowtext(char *buf, int size, int row, int l, int r)
{
  char a[24], b[24];
  if (l == 0xff)
  {
    if (!r)
    {
      snprintf(buf, size, "stop the pulsetable");
      return PK_STOP;
    }
    if (r == row) snprintf(buf, size, "stay on this row (jump to %02X)", r);
    else if (r < row) snprintf(buf, size, "loop back to %02X", r);
    else snprintf(buf, size, "jump forward to %02X", r);
    return PK_JUMP;
  }
  if (l >= 0x80)
  {
    int w = ((l & 0xf) << 8) | r;
    sum_percent(a, sizeof a, w);
    snprintf(buf, size, "set width $%03X = %s", w, a);
    return PK_SET;
  }
  if (!l)
  {
    snprintf(buf, size, "empty row: the table stops here");
    return PK_EMPTY;
  }
  {
    int speed = (signed char)r;
    if (!speed)
    {
      snprintf(buf, size, "hold the width for %dt", l);
    }
    else
    {
      delta_percent(b, sizeof b, (double)speed * l);
      snprintf(buf, size, "%s %dt, %c$%02X/t = %s", speed > 0 ? "widen" : "narrow", l, speed > 0 ? '+' : '-', speed > 0 ? speed : -speed, b);
    }
  }
  return PK_MODULATE;
}

static int addrow(PULSESUM *s, unsigned char *rowseen, int row, const unsigned char *l, const unsigned char *r)
{
  if (rowseen[row]) return 0;
  rowseen[row] = 1;
  if (s->nrows < SUM_MAXROWS)
  {
    SUMROW *sr = &s->rows[s->nrows++];
    sr->row = row;
    sr->kind = pulse_rowtext(sr->text, sizeof sr->text, row, l[row - 1], r[row - 1]);
  }
  else s->moreRows = 1;
  return 1;
}

static unsigned char seenbits[256 * 4096 / 8];

void summarize_pulse(PULSESUM *s, const unsigned char *l, const unsigned char *r, int startrow, double tickrate)
{
  unsigned char ptr = (unsigned char)(startrow & 0xff);
  int pulsetime = 0;
  int pulse = 0x800;
  int tick;
  int firstevent = EV_NONE;
  int boundaries = 0;
  static int histtick[SUM_MAXTICKS];
  static unsigned char histptr[SUM_MAXTICKS];
  static unsigned short histpulse[SUM_MAXTICKS];
  unsigned char rowseen[256];
  int jumpjump = 0;
  int emptyrow = 0;
  int segstart, segend;
  int ups = 0, downs = 0, sets = 0, reversals = 0, lastdir = 0;
  int i;
  char a[24], b[24], t[40];

  memset(s, 0, sizeof *s);
  memset(rowseen, 0, sizeof rowseen);
  s->startrow = (startrow > 0 && startrow < 256) ? startrow : 0;
  s->startwidth = pulse;
  s->minwidth = s->maxwidth = pulse;

  if (!s->startrow)
  {
    strcpy(s->shape, "no pulse program");
    strcpy(s->timing, "the pulse width stays as it is");
    strcpy(s->oneline, "no pulse program: width unchanged");
    snprintf(s->range, sizeof s->range, "unchanged");
    return;
  }

  memset(seenbits, 0, sizeof seenbits);

  for (tick = 0; tick < SUM_MAXTICKS; tick++)
  {
    int event = EV_NONE;
    int before = pulse;
    int ran = 0;

    if (!ptr)
    {
      s->stopped = 1;
      s->stoptick = tick;
      break;
    }

    // One run of the pulsetable, exactly as the playroutine does it (gplay.c, PULSEEXEC)
    if (l[ptr - 1] == 0xff)
    {
      addrow(s, rowseen, ptr, l, r);
      ptr = r[ptr - 1];
      if (!ptr)
      {
        s->stopped = 1;
        s->stoptick = tick;
        break;
      }
      if (l[ptr - 1] == 0xff) jumpjump = ptr; // the row jumped to is a jump too: the player takes it as "set width"
    }
    if (!pulsetime)
    {
      // The state when a row starts (after any jump): if it has been seen before, the program repeats from here
      int key = (ptr << 12) | pulse;
      if (seenbits[key >> 3] & (1 << (key & 7)))
      {
        for (i = 0; i < boundaries; i++)
          if ((histptr[i] == ptr) && (histpulse[i] == pulse)) break;
        s->looped = 1;
        s->loopstart = (i < boundaries) ? histtick[i] : 0;
        s->looplen = tick - s->loopstart;
        break;
      }
      seenbits[key >> 3] |= (1 << (key & 7));
      if (boundaries < SUM_MAXTICKS)
      {
        histtick[boundaries] = tick;
        histptr[boundaries] = ptr;
        histpulse[boundaries] = (unsigned short)pulse;
        boundaries++;
      }
      addrow(s, rowseen, ptr, l, r);
      if (l[ptr - 1] >= 0x80)
      {
        pulse = ((l[ptr - 1] & 0xf) << 8) | r[ptr - 1];
        ran = ptr;
        ptr++;
        event = EV_SET;
      }
      else
      {
        pulsetime = l[ptr - 1];
        if (!pulsetime)
        {
          // An empty row: the pointer never moves on
          emptyrow = ptr;
          s->stopped = 1;
          s->stoptick = tick;
          break;
        }
      }
    }
    if ((pulsetime) && (event == EV_NONE))
    {
      int speed = (signed char)r[ptr - 1];
      int raw = pulse + speed;
      ran = ptr;
      if ((raw < 0) || (raw > 0xfff)) s->wraps = 1;
      pulse = raw & 0xfff;
      pulsetime--;
      if (!pulsetime) ptr++;
      event = EV_MODULATE;
    }

    s->event[tick] = (unsigned char)event;
    s->rowof[tick] = (unsigned char)ran;
    s->width[tick] = (unsigned short)pulse;
    s->ticks = tick + 1;
    if ((firstevent == EV_NONE) && (event != EV_NONE)) firstevent = event;
    if (pulse != before) s->moves = 1;
  }
  if ((tick >= SUM_MAXTICKS) && (!s->looped) && (!s->stopped)) s->capped = 1;
  s->startknown = (firstevent == EV_SET);

  // The part of the replay that describes the program: the repeating part, or all of it
  segstart = s->looped ? s->loopstart : 0;
  segend = s->looped ? s->loopstart + s->looplen : s->ticks;
  if (segend > s->ticks) segend = s->ticks;
  if (segend <= segstart) segend = segstart + 1 > s->ticks ? s->ticks : segstart + 1;

  s->minwidth = s->maxwidth = (s->ticks > 0 && segstart < s->ticks) ? s->width[segstart] : pulse;
  for (i = segstart; i < segend; i++)
  {
    int w = s->width[i];
    if (w < s->minwidth) s->minwidth = w;
    if (w > s->maxwidth) s->maxwidth = w;
  }
  if (s->startknown && (s->ticks > 0))
  {
    // include the width the program starts from when it is not part of the repeating part
    if (!s->looped)
    {
      for (i = 0; i < s->ticks; i++)
      {
        int w = s->width[i];
        if (w < s->minwidth) s->minwidth = w;
        if (w > s->maxwidth) s->maxwidth = w;
      }
    }
  }
  if ((!s->looped) && (!s->startknown))
  {
    if (s->startwidth < s->minwidth) s->minwidth = s->startwidth;
    if (s->startwidth > s->maxwidth) s->maxwidth = s->startwidth;
  }

  // Direction of the movement over that part
  {
    int prev = (segstart > 0) ? s->width[segstart - 1] : s->startwidth;
    for (i = segstart; i < segend; i++)
    {
      int d = s->width[i] - prev;
      if (s->event[i] == EV_SET) sets++;
      else if (s->event[i] == EV_MODULATE)
      {
        // a modulation step moves by its speed; take the shorter way round the 12 bit circle
        if (d > 2048) d -= 4096;
        if (d < -2048) d += 4096;
        if (d > 0) { ups++; if (lastdir < 0) reversals++; lastdir = 1; }
        if (d < 0) { downs++; if (lastdir > 0) reversals++; lastdir = -1; }
      }
      prev = s->width[i];
    }
    if (s->looped && (lastdir != 0))
    {
      // does the direction at the end of the cycle differ from the one at the start?
      int first = 0;
      for (i = segstart; i < segend; i++)
      {
        int d = s->width[i] - ((i > 0) ? s->width[i - 1] : s->startwidth);
        if (s->event[i] != EV_MODULATE) continue;
        if (d > 2048) d -= 4096;
        if (d < -2048) d += 4096;
        if (d) { first = d > 0 ? 1 : -1; break; }
      }
      if (first && (first != lastdir) && (sets == 0)) reversals++;
    }
  }

  // The words
  sum_percent(a, sizeof a, s->minwidth);
  sum_percent(b, sizeof b, s->maxwidth);
  if (s->minwidth == s->maxwidth) snprintf(s->range, sizeof s->range, "%s", a);
  else snprintf(s->range, sizeof s->range, "%s to %s", a, b);

  if (!s->moves)
  {
    char w[24];
    sum_percent(w, sizeof w, s->ticks ? s->width[segend - 1] : s->startwidth);
    if (s->startknown) snprintf(s->shape, sizeof s->shape, "sets the width to %s and holds it", w);
    else snprintf(s->shape, sizeof s->shape, "leaves the width alone");
  }
  else if (s->looped)
  {
    if (!sets)
    {
      if ((ups) && (downs))
      {
        if (reversals <= 2) snprintf(s->shape, sizeof s->shape, "sweeps up and down");
        else snprintf(s->shape, sizeof s->shape, "sweeps back and forth, %d turns", reversals);
      }
      else if (ups) snprintf(s->shape, sizeof s->shape, "keeps widening and wraps round");
      else snprintf(s->shape, sizeof s->shape, "keeps narrowing and wraps round");
    }
    else
    {
      if ((ups) && (!downs)) snprintf(s->shape, sizeof s->shape, "sweeps up, jumps back (sawtooth)");
      else if ((downs) && (!ups)) snprintf(s->shape, sizeof s->shape, "sweeps down, jumps back (sawtooth)");
      else if ((ups) || (downs)) snprintf(s->shape, sizeof s->shape, "sweeps and is set again each time");
      else snprintf(s->shape, sizeof s->shape, "steps between set widths");
    }
  }
  else
  {
    char w[24];
    sum_percent(w, sizeof w, s->width[s->ticks - 1]);
    if ((ups) && (downs)) snprintf(s->shape, sizeof s->shape, "sweeps up and down, stops at %s", w);
    else if (ups) snprintf(s->shape, sizeof s->shape, "sweeps up, stops at %s", w);
    else if (downs) snprintf(s->shape, sizeof s->shape, "sweeps down, stops at %s", w);
    else snprintf(s->shape, sizeof s->shape, "sets the width, stops at %s", w);
  }

  if (s->looped)
  {
    sum_duration(t, sizeof t, s->looplen, tickrate);
    if ((s->moves) && (tickrate > 0) && (s->looplen > 0))
    {
      double hz = tickrate / s->looplen;
      if (hz >= 10) snprintf(s->timing, sizeof s->timing, "repeats every %dt (%s, %.0f Hz)", s->looplen, t, hz);
      else snprintf(s->timing, sizeof s->timing, "repeats every %dt (%s, %.1f Hz)", s->looplen, t, hz);
    }
    else snprintf(s->timing, sizeof s->timing, "repeats every %dt (%s)", s->looplen, t);
  }
  else if (s->stopped)
  {
    int last = s->ticks;
    sum_duration(t, sizeof t, last, tickrate);
    snprintf(s->timing, sizeof s->timing, "ends after %dt (%s)", last, t);
  }
  else snprintf(s->timing, sizeof s->timing, "does not repeat within %d ticks", SUM_MAXTICKS);

  // One remark at most: the most important thing to know
  if (jumpjump) snprintf(s->note, sizeof s->note, "row %02X is a jump to a jump: plays as 'set width'", jumpjump);
  else if (emptyrow) snprintf(s->note, sizeof s->note, "row %02X is empty: the table stops there", emptyrow);
  else if ((s->minwidth == 0) || (s->maxwidth >= 0xfff)) snprintf(s->note, sizeof s->note, "silent at the extremes (0%% or 100%%)");
  else if (s->wraps) snprintf(s->note, sizeof s->note, "goes past 0%%/100%% and wraps round");
  else if ((!s->startknown) && (s->moves)) snprintf(s->note, sizeof s->note, "starts from the current width (shown from 50%%)");
  else s->note[0] = 0;

  // The whole thing in one line
  if (!s->moves)
  {
    if (s->startknown) snprintf(s->oneline, sizeof s->oneline, "holds %s", s->range);
    else snprintf(s->oneline, sizeof s->oneline, "width unchanged");
  }
  else if (s->looped)
  {
    char tt[24];
    sum_duration(tt, sizeof tt, s->looplen, tickrate);
    snprintf(s->oneline, sizeof s->oneline, "%s %s, %s loop", s->range, ((ups) && (downs)) ? "sweep" : (sets ? "saw" : "wrap"), tt);
  }
  else
  {
    snprintf(s->oneline, sizeof s->oneline, "%s, ends after %dt", s->range, s->ticks);
  }
}
