//
// C64 GrooveGoat - export of an instrument to a Furnace instrument file (.fui), see gfurnace.h
//
// The file format is the one of Furnace's instrument files: "FINS", the engine version, the instrument type (3 =
// C64) and a list of features, each with a two letter code and a 16 bit length: NA (name), 64 (the C64 settings),
// MA (the macros). See papers/newIns.md and src/engine/instrument.cpp in the Furnace sources (GPL v2 or later).
//
// Timing: the playroutine does nothing to the waveform, pitch and pulse on the first tick of a note except set the
// first waveform (a table step runs from the second tick on), so those three are shifted one tick; the filter
// runs on every tick. Furnace macros have one value per tick and hold their last value when they end.
//

#include <stdio.h>
#include <string.h>
#include "gfurnace.h"
#include "gsummary.h"
#include "gwsummary.h"
#include "gfsummary.h"

#define M 1024                           // ticks replayed; the macros are made to fit all of them
#define MACRO_ARP 1
#define MACRO_DUTY 2
#define MACRO_WAVE 3
#define MACRO_EX1 5                      // filter passband (lp 1, bp 2, hp 4)
#define MACRO_EX2 6                      // filter resonance
#define MACRO_EX3 7                      // this voice goes through the filter
#define MACRO_ALG 8                      // filter cutoff (11 bit)
#define ARP_ABSOLUTE 0x40000000          // Furnace: a note number instead of a step from the note played

typedef struct
{
  unsigned char *buf;
  int size, pos, overflow;
} WRITER;

static void put8(WRITER *w, int v)
{
  if (w->pos < w->size) w->buf[w->pos] = (unsigned char)v;
  else w->overflow = 1;
  w->pos++;
}

static void put16(WRITER *w, int v) { put8(w, v & 0xff); put8(w, (v >> 8) & 0xff); }
static void put32(WRITER *w, unsigned v) { put16(w, v & 0xffff); put16(w, (v >> 16) & 0xffff); }
static void putcode(WRITER *w, const char *c) { put8(w, c[0]); put8(w, c[1]); }

// Fills out[0..M) from a replay of "ticks" values that ends in a loop (loopstart, looplen) or in a stop (the last
// value goes on). With no replay (ticks 0) it is "none" all through.
static void extend(int *out, const int *src, int ticks, int looped, int loopstart, int looplen, int none)
{
  int t;
  for (t = 0; t < M; t++)
  {
    if (ticks <= 0) out[t] = none;
    else if (t < ticks) out[t] = src[t];
    else if (looped && looplen > 0 && loopstart >= 0 && loopstart + looplen <= ticks)
      out[t] = src[loopstart + (t - loopstart) % looplen];
    else out[t] = src[ticks - 1];
  }
}

// The shortest macro (length 1..255 and a loop point) that plays v[0..M) exactly. Returns 0 if there is none.
static int compress(const int *v, int *len, int *loop)
{
  int l, lp, i;
  for (l = 1; l <= FURNACE_MAXMACRO; l++)
  {
    for (lp = 0; lp < l; lp++)
    {
      int period = l - lp, ok = 1;
      for (i = l; i < M; i++)
        if (v[i] != v[lp + (i - lp) % period]) { ok = 0; break; }
      if (ok)
      {
        *len = l;
        // holding the last value is what a macro without a loop does
        *loop = (period == 1 && lp == l - 1) ? 255 : lp;
        return 1;
      }
    }
  }
  return 0;
}

typedef struct
{
  int code;
  int len, loop;
  int val[FURNACE_MAXMACRO];
} MACRO;

// Makes a macro of v[0..M); returns 0 when it is not needed (one value, the one that is already set in the C64 block)
static int makemacro(MACRO *m, int code, const int *v, int defvalue, int *truncated)
{
  int i;
  m->code = code;
  if (!compress(v, &m->len, &m->loop))
  {
    m->len = FURNACE_MAXMACRO;
    m->loop = 255;
    *truncated = 1;
  }
  for (i = 0; i < m->len; i++) m->val[i] = v[i];
  return !(m->len == 1 && m->val[0] == defvalue);
}

static void putmacro(WRITER *w, const MACRO *m)
{
  int i, lo = m->val[0], hi = m->val[0], wordsize;
  for (i = 1; i < m->len; i++)
  {
    if (m->val[i] < lo) lo = m->val[i];
    if (m->val[i] > hi) hi = m->val[i];
  }
  if (lo >= 0 && hi <= 255) wordsize = 0;
  else if (lo >= -128 && hi <= 127) wordsize = 64;
  else if (lo >= -32768 && hi <= 32767) wordsize = 128;
  else wordsize = 192;

  put8(w, m->code);
  put8(w, m->len);
  put8(w, m->loop);
  put8(w, 255);                          // no release point
  put8(w, 0);                            // a sequence
  put8(w, wordsize);                     // 8 bit unsigned/signed, 16 bit, 32 bit
  put8(w, 0);                            // delay
  put8(w, 1);                            // speed
  for (i = 0; i < m->len; i++)
  {
    if (wordsize == 192) put32(w, (unsigned)m->val[i]);
    else if (wordsize == 128) put16(w, m->val[i]);
    else put8(w, m->val[i]);
  }
}

int furnace_instrument(unsigned char *out, int outsize, const INSTR *ins,
                       const unsigned char *wleft, const unsigned char *wright,
                       const unsigned char *pleft, const unsigned char *pright,
                       const unsigned char *fleft, const unsigned char *fright,
                       FURNACEINFO *info)
{
  static WAVESUM ws;
  static PULSESUM ps;
  static FILTSUM fs;
  static int wave[M], arp[M], duty[M], cut[M], mode[M], res[M], route[M];
  static int tmp[M], tmp2[M];
  static MACRO macros[8];
  FURNACEINFO local;
  WRITER w;
  int t, nm = 0, truncated = 0, haswave, hasfilter, haspulse;
  int staticwave = 0, ring = 0, sync = 0, ringticks = 0, syncticks = 0, wavticks = 0;
  int anyroute = 0, voices = 0;
  int flags1, flags2, cutoffword, startduty, startcut, startmode, startres;
  int lenpos, i;

  if (!info) info = &local;
  memset(info, 0, sizeof *info);
  memset(&w, 0, sizeof w);
  w.buf = out;
  w.size = outsize;

  haswave = ins->ptr[WTBL] != 0;
  haspulse = ins->ptr[PTBL] != 0;
  hasfilter = ins->ptr[FTBL] != 0;
  info->filter = hasfilter;

  if (ins->ptr[STBL] && ins->vibdelay) info->warnings |= FURNACE_W_VIBRATO;

  // ---- waveform and pitch (wavetable) -------------------------------------------------------------------------
  {
    int first = (ins->firstwave >= 1 && ins->firstwave < 0xfe) ? ins->firstwave : 0;
    int cur = first;
    wave[0] = first;
    arp[0] = 0;
    if (haswave)
    {
      int base[M], basep[M];
      memset(&ws, 0, sizeof ws);
      summarize_wave(&ws, wleft, wright, ins->ptr[WTBL], 50);
      for (t = 0; t < ws.ticks && t < WSUM_MAXTICKS; t++)
      {
        tmp[t] = ws.wave[t];
        tmp2[t] = ws.pitchabs[t] ? (ARP_ABSOLUTE | (ws.pitch[t] & 0x7f)) : ws.pitch[t];
      }
      extend(base, tmp, ws.ticks, ws.looped, ws.loopstart, ws.looplen, 0);
      extend(basep, tmp2, ws.ticks, ws.looped, ws.loopstart, ws.looplen, 0);
      for (t = 1; t < M; t++)
      {
        if (base[t - 1]) cur = base[t - 1];     // 0 = no waveform chosen yet: the first waveform stays
        wave[t] = cur;
        arp[t] = basep[t - 1];
      }
      if (ws.hascmds) info->warnings |= FURNACE_W_WAVECMDS;
    }
    else
    {
      for (t = 1; t < M; t++) { wave[t] = cur; arp[t] = 0; }
      info->warnings |= FURNACE_W_NOWAVETABLE;
    }
  }
  for (t = 0; t < M; t++)
  {
    int nib = (wave[t] >> 4) & 15;
    if (nib)
    {
      if (!staticwave) staticwave = nib;
      wavticks++;
      if (wave[t] & 4) ringticks++;
      if (wave[t] & 2) syncticks++;
    }
  }
  ring = wavticks && ringticks == wavticks;
  sync = wavticks && syncticks == wavticks;
  if ((ringticks && !ring) || (syncticks && !sync)) info->warnings |= FURNACE_W_RINGSYNC;

  // ---- pulse width (pulsetable) -------------------------------------------------------------------------------
  startduty = 0x800;
  if (haspulse)
  {
    int base[M];
    memset(&ps, 0, sizeof ps);
    summarize_pulse(&ps, pleft, pright, ins->ptr[PTBL], 50);
    for (t = 0; t < ps.ticks && t < SUM_MAXTICKS && t < M; t++) tmp[t] = ps.width[t] & 0xfff;
    extend(base, tmp, ps.ticks > M ? M : ps.ticks, ps.looped, ps.loopstart, ps.looplen, ps.startwidth & 0xfff);
    startduty = (ps.startknown && ps.ticks) ? base[0] : (ps.startwidth & 0xfff);
    duty[0] = startduty;
    for (t = 1; t < M; t++) duty[t] = base[t - 1];
  }
  else
    for (t = 0; t < M; t++) duty[t] = startduty;

  // ---- filter (filtertable) -----------------------------------------------------------------------------------
  startcut = 0; startmode = 0; startres = 0;
  if (hasfilter)
  {
    int bc[M], bt[M], bk[M], n;
    memset(&fs, 0, sizeof fs);
    summarize_filter(&fs, fleft, fright, ins->ptr[FTBL], 50);
    n = fs.ticks > FSUM_MAXTICKS ? FSUM_MAXTICKS : fs.ticks;
    for (t = 0; t < n && t < M; t++) tmp[t] = fs.cutoff[t] & 0xff;
    extend(bc, tmp, n > M ? M : n, fs.looped, fs.loopstart, fs.looplen, 0);
    for (t = 0; t < n && t < M; t++) tmp[t] = fs.type[t];
    extend(bt, tmp, n > M ? M : n, fs.looped, fs.loopstart, fs.looplen, 0);
    for (t = 0; t < n && t < M; t++) tmp[t] = fs.ctrl[t];
    extend(bk, tmp, n > M ? M : n, fs.looped, fs.loopstart, fs.looplen, 0);
    for (t = 0; t < M; t++)
    {
      cut[t] = bc[t] << 3;                // the table has the top 8 bits of the 11 bit cutoff
      mode[t] = (bt[t] >> 4) & 7;
      res[t] = (bk[t] >> 4) & 15;
      route[t] = (bk[t] & 7) ? 1 : 0;
      if (bk[t] & 7) anyroute = 1;
      voices |= bk[t] & 7;
    }
    startcut = cut[0]; startmode = mode[0]; startres = res[0];
    if (voices && voices != 7) info->warnings |= FURNACE_W_FILTERVOICES;
  }

  // ---- the macros ---------------------------------------------------------------------------------------------
  for (t = 0; t < M; t++) tmp[t] = (wave[t] >> 4) & 15;
  if (makemacro(&macros[nm], MACRO_WAVE, tmp, staticwave, &truncated)) { info->wavelen = macros[nm].len; nm++; }
  if (haswave && makemacro(&macros[nm], MACRO_ARP, arp, 0, &truncated)) { info->pitchlen = macros[nm].len; nm++; }
  if (haspulse && makemacro(&macros[nm], MACRO_DUTY, duty, startduty, &truncated)) { info->dutylen = macros[nm].len; nm++; }
  if (hasfilter)
  {
    if (makemacro(&macros[nm], MACRO_EX1, mode, startmode, &truncated)) nm++;
    if (makemacro(&macros[nm], MACRO_EX2, res, startres, &truncated)) nm++;
    if (makemacro(&macros[nm], MACRO_EX3, route, anyroute, &truncated)) nm++;
    if (makemacro(&macros[nm], MACRO_ALG, cut, startcut, &truncated)) { info->cutlen = macros[nm].len; nm++; }
  }
  if (truncated) info->warnings |= FURNACE_W_TRUNCATED;
  info->macros = nm;

  // ---- the file ------------------------------------------------------------------------------------------------
  put8(&w, 'F'); put8(&w, 'I'); put8(&w, 'N'); put8(&w, 'S');
  put16(&w, FURNACE_FILEVERSION);
  put8(&w, 3);                           // instrument type: C64
  put8(&w, 0);

  if (ins->name[0])
  {
    int n = (int)strnlen(ins->name, MAX_INSTRNAMELEN);
    putcode(&w, "NA");
    put16(&w, n + 1);
    for (i = 0; i < n; i++) put8(&w, (unsigned char)ins->name[i]);
    put8(&w, 0);
  }

  flags1 = 0x80;                         // the pulse width is an absolute value
  if (hasfilter) flags1 |= 0x40;         // start the filter from the values below at every note
  if (anyroute) flags1 |= 0x10;          // the voice goes through the filter
  if (staticwave & 8) flags1 |= 8;
  if (staticwave & 4) flags1 |= 4;
  if (staticwave & 2) flags1 |= 2;
  if (staticwave & 1) flags1 |= 1;
  flags2 = 0;
  if (sync) flags2 |= 0x80;
  if (ring) flags2 |= 0x40;
  if (hasfilter) flags2 |= 0x10;         // the cutoff macro is an absolute value
  if (startmode & 4) flags2 |= 2;
  if (startmode & 2) flags2 |= 4;
  if (startmode & 1) flags2 |= 1;
  cutoffword = (startcut & 0x7ff) | ((startres & 15) << 12);

  putcode(&w, "64");
  put16(&w, 9);
  put8(&w, flags1);
  put8(&w, flags2);
  put8(&w, ins->ad);                     // attack high nibble, decay low: the same as the SID registers
  put8(&w, ins->sr);                     // sustain high nibble, release low
  put16(&w, startduty & 0xfff);
  put16(&w, cutoffword);
  put8(&w, 0);                           // resonance high nibble (SID2 only), reset duty off

  if (nm)
  {
    putcode(&w, "MA");
    lenpos = w.pos;
    put16(&w, 0);
    put16(&w, 8);                        // size of a macro header
    // Furnace writes them in the order of their codes
    for (t = 0; t < 9; t++)
      for (i = 0; i < nm; i++)
        if (macros[i].code == t) putmacro(&w, &macros[i]);
    put8(&w, 255);                       // no more macros
    if (!w.overflow)
    {
      int payload = w.pos - lenpos - 2;
      out[lenpos] = payload & 0xff;
      out[lenpos + 1] = (payload >> 8) & 0xff;
    }
  }

  if (w.overflow) { info->size = 0; return 0; }
  info->size = w.pos;
  return w.pos;
}

void furnace_warningtext(char *buf, int size, int warnings)
{
  char t[200];
  t[0] = 0;
  if (warnings & FURNACE_W_VIBRATO) strcat(t, "vibrato, ");
  if (warnings & FURNACE_W_WAVECMDS) strcat(t, "commands in the wavetable, ");
  if (warnings & FURNACE_W_RINGSYNC) strcat(t, "ring/sync that changes, ");
  if (warnings & FURNACE_W_TRUNCATED) strcat(t, "a program longer than 255 ticks (cut), ");
  if (warnings & FURNACE_W_FILTERVOICES) strcat(t, "filter voices, ");
  if (strlen(t) >= 2) t[strlen(t) - 2] = 0;
  snprintf(buf, size, "%s", t);
}

void furnace_filename(char *buf, int size, const INSTR *ins, int number)
{
  char name[MAX_INSTRNAMELEN + 1];
  int i, n = 0;
  for (i = 0; i < MAX_INSTRNAMELEN && ins->name[i]; i++)
  {
    unsigned char c = (unsigned char)ins->name[i];
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' ||
        (c == ' ' && n > 0))
      name[n++] = (char)c;
  }
  while (n > 0 && name[n - 1] == ' ') n--;
  name[n] = 0;
  if (n) snprintf(buf, size, "%s.fui", name);
  else snprintf(buf, size, "instrument %02X.fui", number & 0xff);
}
