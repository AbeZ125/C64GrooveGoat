//
// C64 GrooveGoat - import of a Furnace C64 instrument file (.fui), see gfurnaceimp.h
//
// The file format is the one that gfurnace.c writes (see there): "FINS", the engine version, the instrument type
// (3 = C64) and a list of features with a two letter code and a 16 bit length: NA (name), 64 (the C64 settings),
// MA (the macros). Other features are skipped. How Furnace plays the macros (src/engine/platform/c64.cpp in the
// Furnace sources, GPL v2 or later): an absolute duty or cutoff is set, a relative one is added to (the duty:
// taken from) the value of the tick before.
//

#include <stdio.h>
#include <string.h>
#include "gfurnaceimp.h"

#define M 1024                           // ticks replayed
#define MAXMACRO 255
#define ARP_ABSOLUTE 0x40000000

typedef struct
{
  int has;
  int len, loop, release, mode, wordsize, delay, speed;
  int val[MAXMACRO];
} IMACRO;

typedef struct
{
  const unsigned char *d;
  int size, pos, bad;
} READER;

static int r8(READER *r)
{
  if (r->pos >= r->size) { r->bad = 1; return 0; }
  return r->d[r->pos++];
}

static int r16(READER *r) { int a = r8(r); return a | (r8(r) << 8); }

static int sext(int v, int bits) { return v & (1 << (bits - 1)) ? v - (1 << bits) : v; }

static int readmacros(READER *r, int end, IMACRO *mac, int *other)
{
  int hdr = r16(r), start;
  if (hdr < 8) return 0;
  start = r->pos;
  while (r->pos < end && !r->bad)
  {
    IMACRO m;
    int code = r8(r), i, ws;
    if (code == 255) break;
    memset(&m, 0, sizeof m);
    m.len = r8(r);
    m.loop = r8(r);
    m.release = r8(r);
    m.mode = r8(r);
    ws = r8(r);
    m.wordsize = (ws >> 6) & 3;
    m.delay = r8(r);
    m.speed = r8(r);
    r->pos += hdr - 8;                   // a longer header than we know
    if (m.speed < 1) m.speed = 1;
    for (i = 0; i < m.len && !r->bad; i++)
    {
      int v;
      if (m.wordsize == 0) v = r8(r);
      else if (m.wordsize == 1) v = sext(r8(r), 8);
      else if (m.wordsize == 2) v = sext(r16(r), 16);
      else { int lo = r16(r); v = (int)(((unsigned)lo) | ((unsigned)r16(r) << 16)); }
      if (i < MAXMACRO) m.val[i] = v;
    }
    if (m.len > MAXMACRO) m.len = MAXMACRO;
    m.has = 1;
    if ((code >= 1 && code <= 3) || (code >= 5 && code <= 8)) { if (code < 16) mac[code] = m; }
    else *other = 1;
  }
  (void)start;
  return !r->bad;
}

// The value of a macro at tick t, or "none" while its delay runs
static int macro_at(const IMACRO *m, int t, int none, int *warn)
{
  int k, idx;
  if (!m->has || m->len <= 0) return none;
  if (m->mode != 0) { *warn |= FIMP_W_MODE; return none; }
  if (m->release != 255) *warn |= FIMP_W_RELEASE;
  if (t < m->delay) return none;
  k = (t - m->delay) / m->speed;
  if (k < m->len) idx = k;
  else if (m->loop != 255 && m->loop < m->len) idx = m->loop + (k - m->loop) % (m->len - m->loop);
  else idx = m->len - 1;
  return m->val[idx];
}

// The shortest program (length 1..STEP_MAX and a loop point) that plays v[0..count) exactly. Returns 0 if there is none.
static int compress(const int *v, int count, int *len, int *loop)
{
  int l, lp, i;
  for (l = 1; l <= STEP_MAX; l++)
  {
    for (lp = 0; lp < l; lp++)
    {
      int period = l - lp, ok = 1;
      for (i = l; i < count; i++)
        if (v[i] != v[lp + (i - lp) % period]) { ok = 0; break; }
      if (ok)
      {
        *len = l;
        *loop = (period == 1 && lp == l - 1) ? 0 : lp + 1;       // holding the last value is no loop
        return 1;
      }
    }
  }
  return 0;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int furnace_import(const unsigned char *data, int size, FURNACEIMPORT *out)
{
  static IMACRO mac[16];
  static int seq[M], comp[M], vals[M], cut[M], mode[M], res[M], route[M], nib[M], note[M], duty[M];
  READER r;
  int version, type, have64 = 0, flags1 = 0, flags2 = 0, startduty = 0x800, startcut = 0, startres = 0, other = 0;
  int staticnib, ring, sync, staticmode, t, len, loop, n, warn = 0;
  int anywave, anyfilter;

  memset(out, 0, sizeof *out);
  memset(mac, 0, sizeof mac);
  if (size < 8 || memcmp(data, "FINS", 4)) return FIMP_NOTFUI;
  r.d = data; r.size = size; r.pos = 4; r.bad = 0;
  version = r16(&r);
  type = r16(&r);
  (void)version;
  if (type != 3) return FIMP_NOTC64;
  out->ad = 0x00; out->sr = 0xf0;

  while (r.pos + 4 <= size)
  {
    int c0 = r8(&r), c1 = r8(&r), flen = r16(&r), end;
    if (c0 == 'E' && c1 == 'N') break;
    end = r.pos + flen;
    if (flen < 0 || end > size) return FIMP_BAD;
    if (c0 == 'N' && c1 == 'A')
    {
      int i = 0;
      while (r.pos < end && r.d[r.pos] && i < MAX_INSTRNAMELEN) out->name[i++] = (char)r.d[r.pos++];
      out->name[i] = 0;
    }
    else if (c0 == '6' && c1 == '4')
    {
      if (flen < 8) return FIMP_BAD;
      flags1 = r8(&r);
      flags2 = r8(&r);
      out->ad = r8(&r);
      out->sr = r8(&r);
      startduty = r16(&r) & 0xfff;
      { int w = r16(&r); startcut = w & 0x7ff; startres = (w >> 12) & 15; }
      have64 = 1;
    }
    else if (c0 == 'M' && c1 == 'A')
    {
      READER sub = r;
      sub.size = end;
      if (!readmacros(&sub, end, mac, &other)) return FIMP_BAD;
    }
    r.pos = end;
    r.bad = 0;
  }
  if (!have64) { flags1 = 0; flags2 = 0; }

  staticnib = flags1 & 15;               // tri 1, saw 2, pulse 4, noise 8: the high nibble of the waveform register
  ring = (flags2 & 0x40) ? 4 : 0;
  sync = (flags2 & 0x80) ? 2 : 0;
  staticmode = ((flags2 & 1) ? 1 : 0) | ((flags2 & 4) ? 2 : 0) | ((flags2 & 2) ? 4 : 0);   // lp 1, bp 2, hp 4

  // ---- every parameter tick by tick ----------------------------------------------------------------------------
  for (t = 0; t < M; t++)
  {
    nib[t] = macro_at(&mac[3], t, staticnib, &warn) & 15;
    {
      int a = macro_at(&mac[1], t, 0, &warn);
      if (a >= 0 && (a & ARP_ABSOLUTE))                // (a negative step has the bit set too)
      {
        int nn = a & 0xff;
        if (nn < 1 || nn > 95) { warn |= FIMP_W_NOTERANGE; nn = clampi(nn, 1, 95); }
        note[t] = 0x80 | nn;
      }
      else
      {
        if (a < -32 || a > 95) { warn |= FIMP_W_NOTERANGE; a = clampi(a, -32, 95); }
        note[t] = a >= 0 ? a : a + 128;
      }
    }
  }
  {
    int d = startduty, c = startcut;
    for (t = 0; t < M; t++)
    {
      int dv = macro_at(&mac[2], t, -1, &warn), cv = macro_at(&mac[8], t, -1, &warn);
      if (dv >= 0)
      {
        if (flags1 & 0x80) d = dv & 4095;
        else d = (d - dv) & 4095;
      }
      if (cv >= 0)
      {
        if (flags2 & 0x10) c = clampi(cv, 0, 2047);
        else c = clampi(c + cv, 0, 2047);
      }
      duty[t] = d;
      cut[t] = c;
      mode[t] = macro_at(&mac[5], t, staticmode, &warn) & 7;
      res[t] = macro_at(&mac[6], t, startres, &warn) & 15;
      route[t] = macro_at(&mac[7], t, (flags1 & 0x10) ? 1 : 0, &warn) & 1;
    }
  }
  if (other) warn |= FIMP_W_OTHER;

  // ---- the first waveform ---------------------------------------------------------------------------------------
  out->firstwave = nib[0] ? ((nib[0] << 4) | 1 | ring | sync) : 0x09;

  // ---- wave and pitch: tick 0 is the first waveform, tick k+1 is step k --------------------------------------------
  anywave = mac[1].has || mac[3].has;
  if (note[0] != 0) warn |= FIMP_W_TICK0;
  step_default(&out->wave, STEP_WAVE);
  if (anywave)
  {
    for (t = 0; t < M - 1; t++) comp[t] = (nib[t + 1] << 8) | note[t + 1];
    if (!compress(comp, M - 1, &len, &loop))
    {
      len = STEP_MAX; loop = 0; warn |= FIMP_W_TOOLONG;
    }
    if (!(len == 1 && loop == 0 && (comp[0] >> 8) == nib[0] && (comp[0] & 0xff) == 0))
    {
      out->haswave = 1;
      out->wave.n = len;
      out->wave.loop = loop;
      for (n = 0; n < len; n++)
      {
        out->wave.wave[n] = ((comp[n] >> 8) << 4) | 1 | ring | sync;
        out->wave.note[n] = comp[n] & 0xff;
      }
    }
  }

  // ---- pulse width ------------------------------------------------------------------------------------------------
  step_default(&out->pulse, STEP_PULSE);
  {
    int pulseused = (staticnib & 4) != 0;
    for (t = 0; t < M && !pulseused; t++) if (nib[t] & 4) pulseused = 1;
    if (mac[2].has || (pulseused && startduty != 0x800))
    {
      if (duty[0] != duty[1] && mac[2].has) warn |= FIMP_W_TICK0;
      for (t = 0; t < M - 1; t++) vals[t] = duty[t + 1];
      if (!compress(vals, M - 1, &len, &loop)) { len = STEP_MAX; loop = 0; warn |= FIMP_W_TOOLONG; }
      out->haspulse = 1;
      out->pulse.n = len;
      out->pulse.loop = loop;
      for (n = 0; n < len; n++) out->pulse.width[n] = vals[n];
    }
  }

  // ---- filter: one value per tick, from the first -------------------------------------------------------------------
  step_default(&out->filter, STEP_FILTER);
  anyfilter = 0;
  for (t = 0; t < M; t++) if (mode[t]) { anyfilter = 1; break; }
  if (anyfilter && (mac[5].has || mac[6].has || mac[7].has || mac[8].has || (flags1 & 0x40)))
  {
    // one value per tick: the cutoff and the passband, resonance and voices that go with it
    for (t = 0; t < M; t++)
    {
      vals[t] = (cut[t] >> 3) | (((mode[t] & 7) << 4) << 8) | (res[t] << 16) | ((route[t] ? 7 : 0) << 20);
      if (mode[t] != mode[0] || res[t] != res[0] || route[t] != route[0]) out->filtervaries = 1;
    }
    if (!compress(vals, M, &len, &loop)) { len = STEP_MAX; loop = 0; warn |= FIMP_W_TOOLONG; }
    out->hasfilter = 1;
    out->filter.n = len;
    out->filter.loop = loop;
    for (n = 0; n < len; n++)
    {
      out->filter.cutoff[n] = vals[n] & 0xff;
      out->ftype[n] = (vals[n] >> 8) & 0x70;
      out->fctrl[n] = ((vals[n] >> 16) & 15) << 4 | ((vals[n] >> 20) & 7);
    }
    out->filter.ftype = out->ftype[0];
    out->filter.fres = out->fctrl[0] >> 4;
    out->filter.fvoices = out->fctrl[0] & 15;
    for (n = 0; n < len; n++) if (out->fctrl[n] & 7) { warn |= FIMP_W_VOICES; break; }
  }

  out->warnings = warn;
  (void)seq;
  return FIMP_OK;
}

void furnace_importwarningtext(char *buf, int size, int warnings)
{
  static const struct { int bit; const char *text; } list[] = {
    {FIMP_W_RELEASE, "release points of macros"},
    {FIMP_W_MODE, "ADSR or LFO macros"},
    {FIMP_W_TOOLONG, "a program was longer than the step limit and is cut"},
    {FIMP_W_NOTERANGE, "pitch steps outside the range (clamped)"},
    {FIMP_W_VOICES, "the filter applies to all three voices"},
    {FIMP_W_OTHER, "volume or other macros"},
    {FIMP_W_TICK0, "pulse width or pitch on the very first tick (the program starts on the second)"}
  };
  int i, n = 0;
  if (size < 1) return;
  buf[0] = 0;
  for (i = 0; i < (int)(sizeof list / sizeof list[0]); i++)
  {
    if (!(warnings & list[i].bit)) continue;
    if (n && strlen(buf) + 2 < (size_t)size) strncat(buf, ", ", size - strlen(buf) - 1);
    strncat(buf, list[i].text, size - strlen(buf) - 1);
    n++;
  }
}

int furnace_filterrows(const FURNACEIMPORT *imp, unsigned char *l, unsigned char *r, int startrow)
{
  const STEPMODEL *m = &imp->filter;
  int rows = 0, i, loopidx = m->loop - 1, rowofloop = 0;
  if (!imp->filtervaries) return step_generate(m, l, r, startrow);
  for (i = 0; i < m->n;)
  {
    int changed = (i == 0) || (i == loopidx) || imp->ftype[i] != imp->ftype[i - 1] || imp->fctrl[i] != imp->fctrl[i - 1];
    if (i == loopidx) rowofloop = startrow + rows;
    if (changed)
    {
      // the settings (no tick of their own), then the cutoff, which takes this tick
      l[rows] = (unsigned char)(0x80 | (imp->ftype[i] & 0x70));
      r[rows] = (unsigned char)imp->fctrl[i];
      rows++;
      l[rows] = 0x00;
      r[rows] = (unsigned char)m->cutoff[i];
      rows++;
      i++;
    }
    else
    {
      int d = (signed char)((m->cutoff[i] - m->cutoff[i - 1]) & 0xff), k = 1;
      while ((i + k < m->n) && (i + k != loopidx) && (k < 127) &&
             imp->ftype[i + k] == imp->ftype[i + k - 1] && imp->fctrl[i + k] == imp->fctrl[i + k - 1] &&
             ((signed char)((m->cutoff[i + k] - m->cutoff[i + k - 1]) & 0xff) == d)) k++;
      l[rows] = (unsigned char)k;
      r[rows] = (unsigned char)(d & 0xff);
      rows++;
      i += k;
    }
  }
  l[rows] = 0xff;
  r[rows] = (unsigned char)(m->loop ? rowofloop : 0);
  rows++;
  return rows;
}
