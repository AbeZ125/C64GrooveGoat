#ifndef GSID_H
#define GSID_H

#include "gcommon.h"

#define NUMSIDREGS 0x19
#define SIDWRITEDELAY 14 // lda $xxxx,x 4 cycles, sta $d400,x 5 cycles, dex 2 cycles, bpl 3 cycles

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  float distortionrate;
  float distortionpoint;
  float distortioncfthreshold;
  float type3baseresistance;
  float type3offset;
  float type3steepness;
  float type3minimumfetresistance;
  float type4k;
  float type4b;
  float voicenonlinearity;
} FILTERPARAMS;

void sid_init(int speed, unsigned m, unsigned ntsc, unsigned interpolate, unsigned customclockrate, unsigned usefp, int nsids);
int sid_fillbuffer(short *ptr, int samples, unsigned int adparam, int stereo);
unsigned char sid_getorder(unsigned char index, unsigned int adparam);
int sid_getenv(int s, int v);
void sid_setvolume(int percent);
void sid_setvolumeimmediate(int percent);
void sid_setvoicemask(int s, int mutemask);
void sid_setscope(short *buf, int stride);
int sid_scopeactive(void);
void sid_livescope(int on);
// width: stereo width in percent (0 = mono, 100 = full spread)
void sid_setpanning(int nsids, int width);
void sid_setsolosid(int s);

#ifndef GSID_C
extern unsigned char sidreg[MAX_SIDS][NUMSIDREGS];
extern FILTERPARAMS filterparams;
extern int sidpan[MAX_SIDS];
extern int sidcount;
#endif

#ifdef __cplusplus
}
#endif

#endif
