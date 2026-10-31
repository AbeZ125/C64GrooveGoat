#ifndef GOATTRK2_H
#define GOATTRK2_H

#define PROGRAMNAME "C64 GrooveGoat"
#define PROGRAMVERSION "0.92b"
#define PROGRAMAUTHOR "IT-Dummkopfen"
#define PATCHNAME "based on GoatTracker v2.76 (Silver Fork)"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include "gplatform.h"
#include "gaudio.h"
#include "gfileio.h"

#include "gcommon.h"
#include "gconsole.h"
#include "gsound.h"
#include "gsid.h"
#include "gundo.h"
#include "gsong.h"
#include "gplay.h"
#include "gdisplay.h"
#include "greloc.h"
#include "gfile.h"
#include "gpattern.h"
#include "gorder.h"
#include "ginstr.h"
#include "gtable.h"
#include "glayout.h"
#include "gdialog.h"
#include "gadsr.h"
#include "gsumui.h"
#include "gbackup.h"

#define EDIT_PATTERN 0
#define EDIT_ORDERLIST 1
#define EDIT_INSTRUMENT 2
#define EDIT_TABLES 3
#define EDIT_NAMES 4

#define KEY_TRACKER 0
#define KEY_DMC 1
#define KEY_JANKO 2

#define VISIBLEPATTROWS (showpiano ? 37 : 41)
#define VISIBLEORDERLIST 13
#define VISIBLETABLEROWS (showpiano ? 16 : 20)
#define VISIBLEFILES 30

#define PGUPDNREPEAT 8

#ifndef GOATTRK2_C
extern int menu;
//extern int editorInfo.editmode;
extern int recordmode;
extern int followplay;
extern int hexnybble;
extern int stepsize;
extern int autoadvance;
extern int defaultpatternlength;
extern int cursorflash;
extern int cursorcolortable[];
extern int exitprogram;
//extern int eacolumn;
extern int eamode;
extern unsigned keypreset;
extern unsigned playerversion;
extern int fileformat;
extern int zeropageadr;
extern int playeradr;
//extern unsigned sidmodel;
//extern unsigned multiplier;
//extern unsigned adparam;
//extern unsigned ntsc;
extern unsigned patterndispmode;
extern unsigned sidaddress;
//extern unsigned finevibrato;
//extern unsigned optimizepulse;
//extern unsigned optimizerealtime;
//extern unsigned usefinevib;
extern unsigned b;
extern unsigned mr;
extern unsigned writer;
extern unsigned hardsid;
extern unsigned catweasel;
extern unsigned interpolate;
extern unsigned hardsidbufinteractive;
extern unsigned hardsidbufplayback;
extern float basepitch;
extern char configbuf[MAX_PATHNAME];
extern char loadedsongfilename[MAX_FILENAME];
extern char songfilename[MAX_FILENAME];
extern char songfilter[MAX_FILENAME];
extern char songpath[MAX_PATHNAME];
extern char instrfilename[MAX_FILENAME];
extern char instrfilter[MAX_FILENAME];
extern char instrpath[MAX_PATHNAME];
extern char packedpath[MAX_PATHNAME];
extern char *programname;
extern char *patchname;
extern char *notename[];
extern char textbuffer[MAX_PATHNAME];
extern unsigned stereomode;
int stereowidth(void);
void togglepiano(void);
extern char appdir[MAX_PATHNAME];
extern char instrlibpath[MAX_PATHNAME];
extern char examplespath[MAX_PATHNAME];
extern unsigned char hexkeytbl[16];
extern unsigned prgfrontend;
extern unsigned prgpiano;
extern int prgpianocmd;
extern unsigned themesetting;
#endif

extern char statusmessage[256];
extern unsigned mastervolume;
extern unsigned showwelcome;
extern int chnview;
extern int ordchnview;

void getparam(FILE *handle, unsigned *value);
void getfloatparam(FILE *handle, float *value);
void getstringparam(FILE *handle, char *value);
void waitkey(void);
void waitkeymouse(void);
void waitkeynoupdate(void);
void waitkeymousenoupdate(void);
void converthex(void);
void docommand(void);
void onlinehelp(int standalone, int context);
void mousecommands(void);
void generalcommands(void);
void load(void);
void save(void);
void quit(void);
void clear(void);
void prevmultiplier(void);
void nextmultiplier(void);
void editadsr(void);
void calculatefreqtable(void);
void setspecialnotenames(void);
void readscalatuningfile(void);
void do_sound_init(void);
void changenumsids(int n);
void togglethemekey(void);
void cyclefont(void);
void newsong(void);
void exportsid(void);
void exportprg(void);
void showinfo(void);
void showmanual(void);
void showhistory(void);
void showwelcomewindow(void);
void configpath(char *dest, const char *file);
void showsidinfo(void);
void helpscreenshot(const char *filename);
int calcsonglength(int subtune);
int calcsonglengthex(int subtune, int *stopped);
void toggle_interpolation_mode(void);

#endif
