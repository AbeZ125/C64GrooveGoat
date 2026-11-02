/*
 * GOATTRACKER reSID interface
 *
 * C64 GrooveGoat: up to 8 reSID / reSID-fp instances, mixed to stereo with
 * a configurable panning per SID.
 */

#define GSID_C

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "resid/sid.h"
#include "resid-fp/sidfp.h"

#include "gsid.h"
#include "gscope.h"
#include "gsound.h"

int clockrate;
int samplerate;
int sidcount = 1;
unsigned char sidreg[MAX_SIDS][NUMSIDREGS];
int sidpan[MAX_SIDS];   // -100 (left) .. 100 (right)

unsigned char sidorder[] =
  {0x18,0x17,0x16,0x15,
   0x14,0x13,0x12,0x11,0x10,0x0f,0x0e,
   0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,
   0x06,0x05,0x04,0x03,0x02,0x01,0x00};

unsigned char altsidorder[] =
  {0x00,0x01,0x02,0x03,0x04,0x05,0x06,
   0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,
   0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,
   0x15,0x16,0x17,0x18};

static SID *sid[MAX_SIDS] = {0};
static SIDFP *sidfp[MAX_SIDS] = {0};
static short *sidbuf = NULL;
static float *mixbuf = NULL;
static int sidbufsize = 0;
static float pangainl[MAX_SIDS];
static float pangainr[MAX_SIDS];

FILTERPARAMS filterparams =
  {0.50f, 3.3e6f, 1.0e-4f,
   1147036.4394268463f, 274228796.97550374f, 1.0066634233403395f, 16125.154840564108f,
   5.5f, 20.f,
   0.9613160610660189f};

extern unsigned residdelay;

static float outputvolume = 0.8f;
static float curvolume = 0.8f;

// Oscilloscope capture: extra reSID instances in the fast sampling mode
// that get the same register writes and record the signal of each voice
static SID *scopesid[MAX_SIDS] = {0};
static unsigned chipmodel = 0;
static short *scopeout = NULL;        // [sid][sample][voice]
static int scopestride = 0;           // samples per SID in scopeout
static short *scopetmp = NULL;
static int scopetmpsize = 0;

// Render only one SID, centred and at full level (-1 = normal mix)
static int solosid = -1;

// Output volume of the editor in percent (0-100)
void sid_setvolume(int percent)
{
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  outputvolume = percent / 100.0f;
}

// Soft limiter: linear up to 75% of full scale, then a smooth knee instead
// of hard clipping
static inline short softclip(float v)
{
  const float knee = 24576.0f;
  float a = v < 0 ? -v : v;
  if (a > knee)
  {
    a = knee + (32767.0f - knee) * tanhf((a - knee) / (32767.0f - knee));
    v = v < 0 ? -a : a;
  }
  if (v > 32767.0f) v = 32767.0f;
  if (v < -32768.0f) v = -32768.0f;
  return (short)v;
}

// Set the volume without fading (offline rendering)
void sid_setvolumeimmediate(int percent)
{
  sid_setvolume(percent);
  curvolume = outputvolume;
}

// Mute voices at the SID output (bit 0-2 = voice 1-3); used for rendering
// single voices. The oscillators keep running for sync & ring modulation.
void sid_setvoicemask(int s, int mutemask)
{
  int v;
  if ((s < 0) || (s >= MAX_SIDS)) return;
  for (v = 0; v < 3; v++)
  {
    if (sid[s]) sid[s]->mute(v, (mutemask >> v) & 1);
    if (sidfp[s]) sidfp[s]->mute(v, (mutemask >> v) & 1);
  }
}

// Enable oscilloscope capture: buf receives for every SID "stride" samples
// of 3 voices (short [sidcount][stride][3]). NULL disables.
void sid_setscope(short *buf, int stride)
{
  int s;
  scopeout = buf;
  scopestride = stride;
  for (s = 0; s < MAX_SIDS; s++)
  {
    if ((buf) && (s < sidcount) && (!scopesid[s]))
    {
      scopesid[s] = new SID;
      scopesid[s]->set_sampling_parameters(clockrate, SAMPLE_FAST, samplerate);
      scopesid[s]->reset();
      scopesid[s]->set_chip_model(chipmodel == 1 ? MOS8580 : MOS6581);
    }
    if (((!buf) || (s >= sidcount)) && (scopesid[s]))
    {
      delete scopesid[s];
      scopesid[s] = NULL;
    }
  }
}

// The live oscilloscope: the same capture, into a buffer of its own, fed to the scope history (gscope) as it is made.
// Not used while an export has the capture.
#define LIVESCOPE_STRIDE 16384
static short livescope[MAX_SIDS * LIVESCOPE_STRIDE * 3];

// True if the capture runs for all SIDs.
int sid_scopeactive(void)
{
  int s;
  if (!scopeout) return 0;
  for (s = 0; s < sidcount; s++)
    if (!scopesid[s]) return 0;
  return 1;
}

void sid_livescope(int on)
{
  if (on)
  {
    if ((!scopeout) || (scopeout == livescope)) sid_setscope(livescope, LIVESCOPE_STRIDE);
  }
  else if (scopeout == livescope) sid_setscope(NULL, 0);
}

void sid_setsolosid(int s)
{
  solosid = (s >= 0) && (s < MAX_SIDS) ? s : -1;
}

void sid_setpanning(int nsids, int width)
{
  int c;
  // Keep the sum of all SIDs below full scale: every SID gets 1/sqrt(n)
  float mastergain = 1.0f / sqrtf((float)(nsids > 0 ? nsids : 1));

  for (c = 0; c < MAX_SIDS; c++)
  {
    int pan = 0;
    if ((width > 0) && (nsids > 1))
    {
      // Spread SIDs alternately left/right, inner pairs closer to centre
      switch (nsids)
      {
        case 2: pan = (c & 1) ? 60 : -60; break;
        case 4: { static const int p[4] = {-80, 80, -35, 35}; pan = p[c & 3]; } break;
        default: { static const int p[8] = {-90, 90, -65, 65, -40, 40, -15, 15}; pan = p[c & 7]; } break;
      }
      if (width > 100) width = 100;
      pan = pan * width / 100;
    }
    sidpan[c] = pan;
    {
      float a = (pan + 100) / 200.0f * 1.5707963f;
      // constant power panning, normalized so that centre = 1.0 on both sides
      float l = cosf(a) * 1.41421356f;
      float r = sinf(a) * 1.41421356f;
      // never louder than a centred SID on one side
      if (l > 1.0f) l = 1.0f;
      if (r > 1.0f) r = 1.0f;
      pangainl[c] = l * mastergain;
      pangainr[c] = r * mastergain;
    }
  }
}

void sid_init(int speed, unsigned m, unsigned ntsc, unsigned interpolate, unsigned customclockrate, unsigned usefp, int nsids)
{
  int c, s;

  if (ntsc) clockrate = NTSCCLOCKRATE;
    else clockrate = PALCLOCKRATE;

  if (customclockrate)
    clockrate = customclockrate;

  samplerate = speed;
  chipmodel = m;
  for (s = 0; s < MAX_SIDS; s++)
    if (scopesid[s]) { delete scopesid[s]; scopesid[s] = NULL; }
  if (nsids < 1) nsids = 1;
  if (nsids > MAX_SIDS) nsids = MAX_SIDS;

  for (s = 0; s < MAX_SIDS; s++)
  {
    int want = s < nsids;
    if ((!usefp) || (!want))
    {
      if (sidfp[s])
      {
        delete sidfp[s];
        sidfp[s] = NULL;
      }
    }
    if ((usefp) || (!want))
    {
      if (sid[s])
      {
        delete sid[s];
        sid[s] = NULL;
      }
    }
    if (!want) continue;
    if ((!usefp) && (!sid[s])) sid[s] = new SID;
    if ((usefp) && (!sidfp[s])) sidfp[s] = new SIDFP;

    switch(interpolate)
    {
      case 0:
      if (sid[s]) sid[s]->set_sampling_parameters(clockrate, SAMPLE_FAST, speed);
      if (sidfp[s]) sidfp[s]->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, speed);
      break;

      default:
      if (sid[s]) sid[s]->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, speed);
      if (sidfp[s]) sidfp[s]->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, speed);
      break;
    }

    if (sid[s]) sid[s]->reset();
    if (sidfp[s]) sidfp[s]->reset();
    for (c = 0; c < NUMSIDREGS; c++)
      sidreg[s][c] = 0x00;

    if (m == 1)
    {
      if (sid[s]) sid[s]->set_chip_model(MOS8580);
      if (sidfp[s]) sidfp[s]->set_chip_model(MOS8580);
    }
    else
    {
      if (sid[s]) sid[s]->set_chip_model(MOS6581);
      if (sidfp[s]) sidfp[s]->set_chip_model(MOS6581);
    }

    if (sidfp[s])
    {
      sidfp[s]->get_filter().set_distortion_properties(
        filterparams.distortionrate,
        filterparams.distortionpoint,
        filterparams.distortioncfthreshold);
      sidfp[s]->get_filter().set_type3_properties(
        filterparams.type3baseresistance,
        filterparams.type3offset,
        filterparams.type3steepness,
        filterparams.type3minimumfetresistance);
      sidfp[s]->get_filter().set_type4_properties(
        filterparams.type4k,
        filterparams.type4b);
      sidfp[s]->set_voice_nonlinearity(
        filterparams.voicenonlinearity);
    }
  }
  for (s = nsids; s < MAX_SIDS; s++)
    for (c = 0; c < NUMSIDREGS; c++)
      sidreg[s][c] = 0x00;

  sidcount = nsids;
}

unsigned char sid_getorder(unsigned char index, unsigned int adparam)
{
  if (adparam >= 0xf000)
    return altsidorder[index];
  else
    return sidorder[index];
}

int sid_getenv(int s, int v)
{
  if ((s < 0) || (s >= sidcount) || (v < 0) || (v > 2)) return 0;
  if (sid[s]) return sid[s]->read_env(v);
  if (sidfp[s]) return sidfp[s]->read_env(v);
  return 0;
}

static inline int clockone(int s, int cycles, short *ptr, int samples)
{
  cycle_count delta = cycles;
  if (sid[s]) return sid[s]->clock(delta, ptr, samples);
  if (sidfp[s]) return sidfp[s]->clock(delta, ptr, samples);
  return 0;
}

static inline void writeone(int s, unsigned char reg, unsigned char value)
{
  if (sid[s]) sid[s]->write(reg, value);
  if (sidfp[s]) sidfp[s]->write(reg, value);
}

// Render one SID into a mono buffer, writing its registers first
static int fillone(int s, short *ptr, int samples, unsigned int adparam)
{
  int tdelta;
  int result = 0;
  int total = 0;
  int c;

  int badline = rand() % NUMSIDREGS;

  tdelta = clockrate * samples / samplerate;
  if (tdelta <= 0)
  {
    memset(ptr, 0, samples * sizeof(short));
    return samples;
  }

  for (c = 0; c < NUMSIDREGS; c++)
  {
    unsigned char o = sid_getorder(c, adparam);

    // Possible random badline delay once per writing
    if ((badline == c) && (residdelay))
    {
      result = clockone(s, residdelay, ptr, samples);
      total += result;
      ptr += result;
      samples -= result;
      tdelta -= residdelay;
    }

    writeone(s, o, sidreg[s][o]);

    result = clockone(s, SIDWRITEDELAY, ptr, samples);
    total += result;
    ptr += result;
    samples -= result;
    tdelta -= SIDWRITEDELAY;

    if (tdelta <= 0) break;
  }

  if (tdelta > 0)
  {
    result = clockone(s, tdelta, ptr, samples);
    total += result;
    ptr += result;
    samples -= result;
  }

  // Loop extra cycles until all samples produced
  while (samples > 0)
  {
    tdelta = clockrate * samples / samplerate;
    if (tdelta <= 0) tdelta = 1;
    result = clockone(s, tdelta, ptr, samples);
    total += result;
    ptr += result;
    samples -= result;
  }

  return total;
}

// Clock the oscilloscope SID with the same register writes as fillone()
static void fillscope(int s, int samples, unsigned int adparam)
{
  SID *sc = scopesid[s];
  short *out = scopeout + s * scopestride * 3;
  int tdelta, c, done = 0;
  cycle_count delta;

  if (scopetmpsize < samples)
  {
    free(scopetmp);
    scopetmpsize = samples + 1024;
    scopetmp = (short *)malloc(scopetmpsize * sizeof(short));
    if (!scopetmp)
    {
      scopetmpsize = 0;
      return;
    }
  }
  tdelta = clockrate * samples / samplerate;
  for (c = 0; c < NUMSIDREGS; c++)
  {
    unsigned char o = sid_getorder(c, adparam);
    sc->write(o, sidreg[s][o]);
    sc->scopebuf = out + done * 3;
    delta = SIDWRITEDELAY;
    done += sc->clock(delta, scopetmp + done, samples - done);
    tdelta -= SIDWRITEDELAY;
    if (tdelta <= 0) break;
  }
  while (done < samples)
  {
    sc->scopebuf = out + done * 3;
    delta = tdelta > 0 ? tdelta : clockrate * (samples - done) / samplerate + 1;
    done += sc->clock(delta, scopetmp + done, samples - done);
    tdelta = 0;
  }
  sc->scopebuf = NULL;
}

// Fill an output buffer. If stereo, the buffer is interleaved L/R and must
// have room for 2 * samples values.
int sid_fillbuffer(short *ptr, int samples, unsigned int adparam, int stereo)
{
  int s, c;

  if (samples <= 0) return 0;

  if (sidbufsize < samples)
  {
    free(sidbuf);
    free(mixbuf);
    sidbufsize = samples + 1024;
    sidbuf = (short *)malloc(sidbufsize * sizeof(short));
    mixbuf = (float *)malloc(sidbufsize * 2 * sizeof(float));
    if ((!sidbuf) || (!mixbuf))
    {
      sidbufsize = 0;
      return 0;
    }
  }

  {
    float *mix = mixbuf;
    memset(mix, 0, samples * 2 * sizeof(float));
    for (s = 0; s < sidcount; s++)
    {
      float gl = pangainl[s], gr = pangainr[s];
      if ((!sid[s]) && (!sidfp[s])) continue;
      if (solosid >= 0)
      {
        if (s != solosid) continue;
        gl = gr = 1.0f;
      }
      fillone(s, sidbuf, samples, adparam);
      if ((scopeout) && (scopesid[s]) && (samples <= scopestride))
      {
        int v;
        fillscope(s, samples, adparam);
        for (v = 0; v < 3; v++) scope_add(s * 3 + v, scopeout + s * scopestride * 3 + v, 3, samples);
      }
      for (c = 0; c < samples; c++)
      {
        float v = sidbuf[c];
        mix[c * 2] += v * gl;
        mix[c * 2 + 1] += v * gr;
      }
    }
    // Volume changes are faded in over about 20 ms to avoid "zipper" clicks
    float target = outputvolume;
    float step = 1.0f / (0.02f * (samplerate > 0 ? samplerate : 48000));
    for (c = 0; c < samples; c++)
    {
      if (curvolume < target) { curvolume += step; if (curvolume > target) curvolume = target; }
      else if (curvolume > target) { curvolume -= step; if (curvolume < target) curvolume = target; }
      if (stereo)
      {
        ptr[c * 2] = softclip(mix[c * 2] * curvolume);
        ptr[c * 2 + 1] = softclip(mix[c * 2 + 1] * curvolume);
      }
      else
        ptr[c] = softclip((mix[c * 2] + mix[c * 2 + 1]) * 0.5f * curvolume);
    }
  }
  return samples;
}
