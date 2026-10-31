//
// C64 GrooveGoat - plain language summary of the wavetable
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gwsummary.h"
#include "gcommon.h"

static const char *shapename[4] = {"tri", "saw", "pulse", "noise"};
static const char *notenames[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

static const char *cmdname[16] = {
  "(not allowed here)", "portamento up", "portamento down", "tone portamento", "vibrato", "set attack/decay",
  "set sustain/release", "set waveform", "(not allowed here)", "set pulse pointer", "set filter pointer",
  "set filter control", "set filter cutoff", "set master volume", "(not allowed here)", "(no effect here)"};

void wave_notename(char *buf, int size, int note)
{
  note &= 0x7f;
  snprintf(buf, size, "%s%d", notenames[note % 12], note / 12);
}

void wave_name(char *buf, int size, int wave)
{
  char t[48];
  int k, n = 0;
  t[0] = 0;
  for (k = 0; k < 4; k++)
    if (wave & (0x10 << k))
    {
      if (n++) strcat(t, "+");
      strcat(t, shapename[k]);
    }
  if (!n) strcpy(t, "no waveform");
  if (wave & 0x02) strcat(t, "+sync");
  if (wave & 0x04) strcat(t, "+ring");
  if (wave & 0x08) strcat(t, "+test");
  if ((wave & 0xf0) && !(wave & 1)) strcat(t, " (gate off)");
  snprintf(buf, size, "%s", t);
}

// the pitch part of a row: right = the note column
static void notetext(char *buf, int size, int r)
{
  if (r == 0x80) snprintf(buf, size, "pitch unchanged");
  else if (r == 0) snprintf(buf, size, "note as played");
  else if (r < 0x60) snprintf(buf, size, "note +%d", r);
  else if (r < 0x80) snprintf(buf, size, "note -%d", 128 - r);
  else
  {
    char n[8];
    double hz = 440.0 * pow(2.0, ((r & 0x7f) - 57) / 12.0);
    wave_notename(n, sizeof n, r);
    if (hz < 1000) snprintf(buf, size, "note %s (%.0f Hz)", n, hz);
    else snprintf(buf, size, "note %s (%.1f kHz)", n, hz / 1000);
  }
}

int wave_rowtext(char *buf, int size, int row, int l, int r)
{
  char a[48], b[24];
  (void)row;
  if (l == 0xff)
  {
    if (!r) { snprintf(buf, size, "stop the wavetable"); return PK_STOP; }
    snprintf(buf, size, "loop back to %02X", r);
    return PK_JUMP;
  }
  if (l >= WAVECMD)
  {
    int c = l & 0xf;
    switch (c)
    {
      case CMD_SETAD: case CMD_SETSR: case CMD_SETFILTERCTRL: case CMD_SETFILTERCUTOFF: case CMD_SETMASTERVOL:
      snprintf(buf, size, "%s $%02X", cmdname[c], r);
      break;
      case CMD_SETWAVE:
      wave_name(a, sizeof a, r);
      snprintf(buf, size, "set waveform $%02X (%s)", r, a);
      break;
      case CMD_SETPULSEPTR: case CMD_SETFILTERPTR:
      if (r) snprintf(buf, size, "%s to row %02X", cmdname[c], r);
      else snprintf(buf, size, "%s: stop it", cmdname[c]);
      break;
      case CMD_PORTAUP: case CMD_PORTADOWN: case CMD_TONEPORTA: case CMD_VIBRATO:
      snprintf(buf, size, "%s, param $%02X", cmdname[c], r);
      break;
      default:
      snprintf(buf, size, "command %X%02X %s", c, r, cmdname[c]);
    }
    return PK_MODULATE;
  }
  notetext(b, sizeof b, r);
  if (l == 0)
  {
    snprintf(buf, size, "keep waveform, %s", b);
    return PK_EMPTY;
  }
  if (l <= WAVELASTDELAY)
  {
    snprintf(buf, size, "wait %d frame%s, then %s", l, l == 1 ? "" : "s", b);
    return PK_MODULATE;
  }
  if (l >= WAVESILENT)
  {
    wave_name(a, sizeof a, l & 0xf);
    snprintf(buf, size, "inaudible: %s, %s", a, b);
    return PK_SET;
  }
  wave_name(a, sizeof a, l);
  snprintf(buf, size, "%s, %s", a, b);
  return PK_SET;
}

static void pitchstr(char *buf, int size, int abs, int p)
{
  if (abs) wave_notename(buf, size, p);
  else if (p > 0) snprintf(buf, size, "+%d", p);
  else snprintf(buf, size, "%d", p);
}

void summarize_wave(WAVESUM *s, const unsigned char *left, const unsigned char *right, int startrow, double tickrate)
{
  int ptr = startrow;
  int wavetime = 0;
  int wave = 0, pabs = 0, pitch = 0;
  int t, k;
  static unsigned char shist[WSUM_MAXTICKS][5]; // state at the start of each tick
  static short pshist[WSUM_MAXTICKS];
  int from, to;
  char a[48];

  memset(s, 0, sizeof *s);
  s->startrow = (startrow > 0 && startrow <= 255) ? startrow : 0;
  if (!s->startrow)
  {
    strcpy(s->waves, "no program");
    strcpy(s->timing, "the waveform stays as it is");
    snprintf(s->oneline, sizeof s->oneline, "no wavetable program");
    return;
  }

  // the rows, from the start until the first jump or stop (or the end of the table)
  for (k = s->startrow; k <= 255; k++)
  {
    if (s->nrows >= SUM_MAXROWS) { s->moreRows = 1; break; }
    s->rows[s->nrows].row = k;
    s->rows[s->nrows].kind = wave_rowtext(s->rows[s->nrows].text, sizeof s->rows[0].text, k, left[k - 1], right[k - 1]);
    s->nrows++;
    if (left[k - 1] == 0xff) break;
  }

  for (t = 0; t < WSUM_MAXTICKS; t++)
  {
    int l, r, cur, adv = 1;
    // state at the start of the tick, to find where the program repeats
    shist[t][0] = ptr; shist[t][1] = wavetime; shist[t][2] = wave; shist[t][3] = pabs; shist[t][4] = 0;
    pshist[t] = pitch;
    for (k = 0; k < t; k++)
      if (!memcmp(shist[k], shist[t], 4) && pshist[k] == pshist[t]) break;
    if (k < t)
    {
      s->looped = 1;
      s->loopstart = k;
      s->looplen = t - k;
      break;
    }
    if (!ptr)
    {
      s->stopped = 1;
      s->stoptick = t;
      break;
    }
    cur = ptr;
    l = left[ptr - 1];
    r = right[ptr - 1];
    s->row[t] = 0;
    if (l > WAVELASTDELAY)
    {
      s->row[t] = cur;
      if (l < WAVESILENT) wave = l;
      else if (l <= WAVELASTSILENT) wave = l & 0xf;
      else if (l <= WAVELASTCMD)
      {
        s->hascmds = 1;
        if ((l & 0xf) == CMD_SETWAVE) wave = r;
      }
    }
    else if (wavetime != l)
    {
      wavetime++;
      adv = 0;
    }
    if (adv)
    {
      wavetime = 0;
      ptr = (ptr + 1) & 0xff;
      if (!ptr) { /* ran off the end of the table: stops */ }
      else if (left[ptr - 1] == 0xff) ptr = right[ptr - 1];
      if (!(l >= WAVECMD && l <= WAVELASTCMD) && r != 0x80)
      {
        if (r < 0x80) { pabs = 0; pitch = r < 0x60 ? r : r - 128; }
        else { pabs = 1; pitch = r & 0x7f; }
        s->row[t] = cur;
      }
    }
    s->wave[t] = wave;
    s->pitchabs[t] = pabs;
    s->pitch[t] = pitch;
    if (wave && !s->firstwave) s->firstwave = wave;
    s->shapes |= wave & 0xf0;
  }
  s->ticks = t;
  if (t >= WSUM_MAXTICKS) s->capped = 1;
  if (!s->looped && !s->stopped && !s->capped) s->stopped = 1;
  if (s->stopped && !s->looped && s->stoptick == 0) s->stoptick = s->ticks;

  // --- the waveforms over the period that matters
  from = 0;
  to = s->looped ? s->loopstart + s->looplen : s->ticks;
  {
    int n = 0, last = -1, seq[6];
    for (t = from; t < to; t++)
      if (s->wave[t] && s->wave[t] != last)
      {
        last = s->wave[t];
        if (n < 6) seq[n] = last;
        n++;
      }
    s->waves[0] = 0;
    if (!n) strcpy(s->waves, "no waveform is set");
    else
    {
      for (k = 0; k < n && k < 4; k++)
      {
        wave_name(a, sizeof a, seq[k]);
        if (k) strncat(s->waves, k == 1 && n == 2 ? ", then " : ", ", sizeof s->waves - strlen(s->waves) - 1);
        strncat(s->waves, a, sizeof s->waves - strlen(s->waves) - 1);
      }
      if (n > 4) strncat(s->waves, ", ...", sizeof s->waves - strlen(s->waves) - 1);
    }
  }

  // --- the pitch: the distinct consecutive values
  {
    int n = 0, last = 1 << 20, lastabs = -1, same = 1;
    int firstlen = 0, curlen = 0;
    char p[16];
    s->notes[0] = 0;
    for (t = from; t < to; t++)
    {
      if (s->pitch[t] != last || s->pitchabs[t] != lastabs)
      {
        if (n == 1) firstlen = curlen;
        else if (n > 1 && curlen != firstlen) same = 0;
        last = s->pitch[t];
        lastabs = s->pitchabs[t];
        curlen = 0;
        n++;
        if (n <= 6)
        {
          pitchstr(p, sizeof p, lastabs, last);
          if (n > 1) strncat(s->notes, " ", sizeof s->notes - strlen(s->notes) - 1);
          strncat(s->notes, p, sizeof s->notes - strlen(s->notes) - 1);
        }
        else if (n == 7) strncat(s->notes, " ...", sizeof s->notes - strlen(s->notes) - 1);
      }
      curlen++;
    }
    if (n <= 1)
    {
      if (n == 1 && lastabs) { pitchstr(p, sizeof p, 1, last); snprintf(s->notes, sizeof s->notes, "fixed note %s", p); }
      else strcpy(s->notes, "stays on the played note");
    }
    else
    {
      char head[24];
      snprintf(head, sizeof head, "%s ", s->looped && n >= 3 ? "arpeggio" : "notes");
      memmove(s->notes + strlen(head), s->notes, strlen(s->notes) + 1);
      memcpy(s->notes, head, strlen(head));
      (void)same;
    }
    s->notes[55] = 0;
  }

  // --- timing
  {
    char d[24];
    if (s->looped)
    {
      sum_duration(d, sizeof d, s->looplen, tickrate);
      if (s->loopstart) snprintf(s->timing, sizeof s->timing, "repeats from tick %d, every %dt (%s)", s->loopstart, s->looplen, d);
      else snprintf(s->timing, sizeof s->timing, "repeats every %dt (%s)", s->looplen, d);
    }
    else if (s->capped) snprintf(s->timing, sizeof s->timing, "runs on for more than %d ticks", WSUM_MAXTICKS);
    else
    {
      sum_duration(d, sizeof d, s->stoptick, tickrate);
      snprintf(s->timing, sizeof s->timing, "ends after %dt (%s), then keeps the last waveform", s->stoptick, d);
    }
  }

  // --- remark
  s->note[0] = 0;
  {
    int jj = 0;
    for (k = 0; k < s->nrows; k++)
      if (s->rows[k].kind == PK_JUMP && s->rows[k].row != 0)
      {
        int target = right[s->rows[k].row - 1];
        if (target && target <= 255 && left[target - 1] == 0xff) jj = 1;
      }
    if (jj) strcpy(s->note, "jump to a jump row: played as a note step");
    else if (s->hascmds) strcpy(s->note, "runs pattern commands from the table");
    else if (s->shapes && !(s->wave[s->looped ? s->loopstart : 0] & 1) && s->firstwave && !(s->firstwave & 1))
      strcpy(s->note, "first waveform has the gate off: no sound starts");
  }

  // --- one line: the waveforms in order, how long, and the notes
  {
    char sh[200], w1[48], *c;
    const char *nt = s->notes;
    int n = 0, last = -1;
    sh[0] = 0;
    for (t = 0; t < (s->looped ? s->loopstart + s->looplen : s->ticks) && n < 3; t++)
      if (s->wave[t] && s->wave[t] != last)
      {
        last = s->wave[t];
        wave_name(w1, sizeof w1, last);
        if ((c = strstr(w1, " (gate off)"))) *c = 0;
        if (n) strcat(sh, ">");
        strcat(sh, w1);
        n++;
      }
    if (!n) strcpy(sh, "no waveform");
    if (!strncmp(nt, "notes ", 6)) nt += 6;
    else if (!strncmp(nt, "arpeggio ", 9)) nt += 9;
    if (s->looped) snprintf(s->oneline, sizeof s->oneline, "%s, loop %dt: %s", sh, s->looplen, nt);
    else snprintf(s->oneline, sizeof s->oneline, "%s, %dt: %s", sh, s->stoptick, nt);
    s->oneline[95] = 0;
  }
}
