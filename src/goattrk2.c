//
// C64 GrooveGoat 0.8b
// A multi-SID (1/2/4/8 SIDs) music editor for the Commodore 64,
// based on GoatTracker v2.76 (Silver Fork) by Lasse Öörni and contributors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#define GOATTRK2_C

#ifdef __WIN32__
#include <windows.h>
#include <shlobj.h>
#endif

#include "goattrk2.h"
#include "gmidi.h"
#include "gexport.h"
#include "ginslib.h"
#include "grandins.h"
#include "gres.h"
#include "gpiano.h"

int menu = 0;
int recordmode = 1;
int followplay = 0;
int hexnybble = -1;
int stepsize = 4;
int autoadvance = 0;
int defaultpatternlength = 64;
int cursorflash = 0;
int cursorcolortable[] = { 1,2,7,2 };
int exitprogram = 0;
int midinewnote = -1;               // a note from the MIDI keyboard for the pattern editor to enter (see midipump)
static int midiplaying = -1;        // the MIDI note that is sounding
int eamode = 0;

unsigned keypreset = KEY_TRACKER;
unsigned playerversion = 0;
int fileformat = FORMAT_SID;
int zeropageadr = 0xfc;
int playeradr = 0x1000;
unsigned patterndispmode = 0;
unsigned sidaddress = 0xd400;
unsigned customclockrate = 0;
unsigned b = DEFAULTBUF;
unsigned mr = DEFAULTMIXRATE;
unsigned writer = 0;
unsigned hardsid = 0;
unsigned catweasel = 0;
unsigned interpolate = 3;
unsigned residdelay = 0;
unsigned hardsidbufinteractive = 20;
unsigned hardsidbufplayback = 400;
unsigned stereomode = 1;   // 0 = mono, 1-4 = stereo with 100%, 90%, 80%, 70% width

// Stereo width of the editor playback in percent (0 = mono)
int stereowidth(void)
{
  static const int w[5] = {0, 100, 90, 80, 70};
  return w[stereomode <= 4 ? stereomode : 1];
}
unsigned themesetting = THEME_NIGHT;
unsigned prgfrontend = 1;
unsigned prgpiano = 1;          // piano keyboard on the screen of a runnable PRG
int prgpianocmd = -1;           // --piano=0/1 on the command line (not saved)
unsigned mastervolume = 80;
unsigned showwelcome = 1;
unsigned kbdlayoutsetting = 2;
unsigned showpiano = 1;        // piano keyboard above the status bar
unsigned fontsetting = 0;            // the screen font, an index into res_fontnames
float basepitch = 0.0f;
float equaldivisionsperoctave = 12.0f;
int tuningcount = 0;
double tuning[96];

char configbuf[MAX_PATHNAME];
char configfilename[MAX_PATHNAME];
char loadedsongfilename[MAX_FILENAME];
char songfilename[MAX_FILENAME];
char songfilter[MAX_FILENAME];
char songpath[MAX_PATHNAME];
char instrfilename[MAX_FILENAME];
char instrfilter[MAX_FILENAME];
char instrpath[MAX_PATHNAME];
char packedpath[MAX_PATHNAME];
char appdir[MAX_PATHNAME];
char instrlibpath[MAX_PATHNAME];
char examplespath[MAX_PATHNAME];

char *programname = PROGRAMNAME;
char *patchname = PATCHNAME;
char specialnotenames[186];
char scalatuningfilepath[MAX_PATHNAME];
char tuningname[64];

char textbuffer[MAX_PATHNAME];

unsigned char hexkeytbl[] = { '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static char *usage[] = {
  "Usage: groovegoat [songname] [options]",
  "Options:",
  "-Axx Set ADSR parameter for hardrestart in hex. DEFAULT=0F00",
  "-Bxx Set sound buffer length in milliseconds DEFAULT=100",
  "-Cxx Use CatWeasel MK3 PCI SID (0 = off, 1 = on)",
  "-Dxx Pattern row display (0 = decimal, 1 = hex, 2 = decimal w/dots, 3 = hex w/dots)",
  "-Exx Set emulated SID model (0 = 6581 1 = 8580) DEFAULT=8580",
  "-Fxx Set custom SID clock cycles per second (0 = use PAL/NTSC default)",
  "-Gxx Set pitch of A-4 in Hz (0 = use default frequencytable, close to 440Hz)",
  "-Hxx Use HardSID (0 = off, 1 = HardSID ID0 2 = HardSID ID1 etc.)",
  "-Ixx Set reSID interpolation (0 = off, 1 = on, 2 = distortion, 3 = distortion & on) DEFAULT=3",
  "-Jxx Set special note names (2 chars for every note in an octave/cycle, e.g. C-DbD-EbE-F-GbG-AbA-BbB-)",
  "-Kxx Note-entry mode (0 = Protracker, 1 = DMC, 2 = Janko) DEFAULT=Protracker",
  "-Lxx SID memory location in hex. DEFAULT=D400",
  "-Mxx Set sound mixing rate DEFAULT=44100",
  "-Oxx Set pulseoptimization/skipping (0 = off, 1 = on) DEFAULT=on",
  "-Qxx Set equal divisions per octave (12 = default, 8.2019143 = Bohlen-Pierce)",
  "-Rxx Set realtime-effect optimization/skipping (0 = off, 1 = on) DEFAULT=on",
  "-Sxx Set speed multiplier (0 for 25Hz, 1 for 1x, 2 for 2x etc.)",
  "-Vxx Set finevibrato conversion (0 = off, 1 = on) DEFAULT=on",
  "-Xxx Set window type (0 = window, 1 = fullscreen) DEFAULT=window",
  "-Yxx Path to a Scala tuning file .scl",
  "-Zxx Set random reSID write delay in cycles (0 = off) DEFAULT=off",
  "-Txx Set theme (0 = night, 1 = day)",
  "-N   Use NTSC timing",
  "-P   Use PAL timing (DEFAULT)",
  "-W   Write sound output to a file SIDAUDIO.RAW",
  "-?   Show this info again",
  "--export=FILE      Export the song given on the command line and exit",
  "--format=FMT       Export format: sid (default), prg (runnable), prgmusic, bin",
  "--address=XXXX     Player address for the export (hex)",
  "--piano=0|1        Runnable PRG: without / with the piano keyboard",
  "--render=FILE      Render the song to FILE.wav, FILE.mp3 or FILE.mp4 (video) and exit",
  "--seconds=N        Render length (default: song length + fade-out)",
  "--fade=N           Fade-out in seconds",
  "--subtune=N        Subtune to render (0 = first)",
  "--mono             Render in mono",
  "--bitrate=N        MP3 bitrate in kbit/s (default 320)",
  "--stems            WAV: also render every voice into its own file",
  NULL
};

static void initdirectories(void);
static void loadconfig(void);
static void saveconfig(void);
static void handledrop(void);
static void updatetitle(void);
static void runautotest(void);

static char exportname[MAX_PATHNAME];
static char startcwd[MAX_PATHNAME];

// Command line output names are relative to the directory the program was
// started from (loading a song changes the current directory)
static void absolutepath(char *name)
{
  char tmp[MAX_PATHNAME];
  if ((!name[0]) || (!strcmp(name, "-"))) return;
  if ((name[0] == '/') || (name[0] == '\\') || ((name[0]) && (name[1] == ':'))) return;
  snprintf(tmp, MAX_PATHNAME, "%s/%s", startcwd, name);
  strcpy(name, tmp);
}
static int exportformat = EXP_SID;
static unsigned exportbase = 0;
static int exportsids = 0;
static char rendername[MAX_PATHNAME];
static char savename[MAX_PATHNAME];
static int renderseconds = 0;
static int renderstems = 0;
static int renderfade = -1;
static int rendermono = 0;
static int renderbitrate = 0;
static int rendersubtune = 0;

// Offline rendering of a subtune to a 44.1kHz 16-bit stereo WAV file
int main(int argc, char **argv)
{
  int c, d;

  // DEFAULT SETTINGS
  editorInfo.adparam = 0x0f00;
  editorInfo.editmode = EDIT_PATTERN;
  editorInfo.esmarkchn = -1;
  editorInfo.epoctave = 2;
  editorInfo.epmarkchn = -1;
  editorInfo.etmarknum = -1;
  editorInfo.etlock = 1;
  editorInfo.ntsc = 0;
  editorInfo.multiplier = 1;
  editorInfo.sidmodel = 1;
  editorInfo.finevibrato = 1;
  editorInfo.optimizepulse = 1;
  editorInfo.optimizerealtime = 1;

  specialnotenames[0] = 0;
  scalatuningfilepath[0] = 0;
  if (!getcwd(startcwd, MAX_PATHNAME)) startcwd[0] = 0;

  initdirectories();
  loadconfig();

  // Init pathnames
  initpaths();

  // Scan command line
  for (c = 1; c < argc; c++)
  {
#ifdef __WIN32__
    if ((argv[c][0] == '-') || (argv[c][0] == '/'))
#else
    if (argv[c][0] == '-')
#endif
    {
      int y;
      switch (argv[c][1])
      {
      case '-':
        if (!strncmp(argv[c], "--export=", 9))
        {
          strncpy(exportname, argv[c] + 9, MAX_PATHNAME - 1);
          break;
        }
        if (!strncmp(argv[c], "--format=", 9))
        {
          const char *f = argv[c] + 9;
          if (!strcmp(f, "sid")) exportformat = EXP_SID;
          else if (!strcmp(f, "prg")) exportformat = EXP_PRGPLAYER;
          else if (!strcmp(f, "prgmusic")) exportformat = EXP_PRG;
          else if (!strcmp(f, "bin")) exportformat = EXP_BIN;
          break;
        }
        if (!strncmp(argv[c], "--address=", 10))
        {
          sscanf(argv[c] + 10, "%x", &exportbase);
          break;
        }
        if (!strncmp(argv[c], "--render=", 9))
        {
          strncpy(rendername, argv[c] + 9, MAX_PATHNAME - 1);
          strcpy(exportname, "-");
          break;
        }
        if (!strncmp(argv[c], "--save=", 7))
        {
          strncpy(savename, argv[c] + 7, MAX_PATHNAME - 1);
          strcpy(exportname, "-");
          break;
        }
        if (!strncmp(argv[c], "--seconds=", 10))
        {
          renderseconds = atoi(argv[c] + 10);
          break;
        }
        if (!strncmp(argv[c], "--piano=", 8))
        {
          prgpianocmd = atoi(argv[c] + 8) ? 1 : 0;
          break;
        }
        if (!strcmp(argv[c], "--stems"))
        {
          renderstems = 1;
          break;
        }
        if (!strcmp(argv[c], "--mono"))
        {
          rendermono = 1;
          break;
        }
        if (!strncmp(argv[c], "--fade=", 7))
        {
          renderfade = atoi(argv[c] + 7);
          break;
        }
        if (!strncmp(argv[c], "--bitrate=", 10))
        {
          renderbitrate = atoi(argv[c] + 10);
          break;
        }
        if (!strncmp(argv[c], "--subtune=", 10))
        {
          rendersubtune = atoi(argv[c] + 10);
          break;
        }
        if (!strncmp(argv[c], "--sids=", 7))
        {
          exportsids = atoi(argv[c] + 7);
          break;
        }
        if (!strncmp(argv[c], "--options=", 10))
        {
          sscanf(argv[c] + 10, "%u", &playerversion);
          break;
        }
        if (strcmp(argv[c], "--help"))
          break;
        // fallthrough
      case '?':
        for (y = 0; usage[y]; ++y)
          printf("%s\n", usage[y]);
        return EXIT_SUCCESS;

      case 'Z': sscanf(&argv[c][2], "%u", &residdelay); break;
      case 'A': sscanf(&argv[c][2], "%x", &editorInfo.adparam); break;
      case 'S': sscanf(&argv[c][2], "%u", &editorInfo.multiplier); break;
      case 'B': sscanf(&argv[c][2], "%u", &b); break;
      case 'D': sscanf(&argv[c][2], "%u", &patterndispmode); break;
      case 'E': sscanf(&argv[c][2], "%u", &editorInfo.sidmodel); break;
      case 'I': sscanf(&argv[c][2], "%u", &interpolate); break;
      case 'K': sscanf(&argv[c][2], "%u", &keypreset); break;
      case 'L': sscanf(&argv[c][2], "%x", &sidaddress); break;
      case 'N': editorInfo.ntsc = 1; customclockrate = 0; break;
      case 'P': editorInfo.ntsc = 0; customclockrate = 0; break;
      case 'F': sscanf(&argv[c][2], "%u", &customclockrate); break;
      case 'M': sscanf(&argv[c][2], "%u", &mr); break;
      case 'O': sscanf(&argv[c][2], "%u", &editorInfo.optimizepulse); break;
      case 'R': sscanf(&argv[c][2], "%u", &editorInfo.optimizerealtime); break;
      case 'H': sscanf(&argv[c][2], "%u", &hardsid); break;
      case 'V': sscanf(&argv[c][2], "%u", &editorInfo.finevibrato); break;
      case 'W': writer = 1; break;
      case 'X': sscanf(&argv[c][2], "%d", &win_fullscreen); break;
      case 'C': sscanf(&argv[c][2], "%u", &catweasel); break;
      case 'G': sscanf(&argv[c][2], "%f", &basepitch); break;
      case 'Q': sscanf(&argv[c][2], "%f", &equaldivisionsperoctave); break;
      case 'J': sscanf(&argv[c][2], "%185s", specialnotenames); break;
      case 'Y': sscanf(&argv[c][2], "%255s", scalatuningfilepath); break;
      case 'T': sscanf(&argv[c][2], "%u", &themesetting); break;
      }
    }
    else
    {
      // macOS passes -psn_ arguments when started from Finder; ignore others too
      char startpath[MAX_PATHNAME];

      if (strlen(argv[c]) >= MAX_PATHNAME) continue;
      strncpy(songfilename, argv[c], MAX_FILENAME - 1);
      songfilename[MAX_FILENAME - 1] = 0;
      for (d = strlen(argv[c]) - 1; d >= 0; d--)
      {
        if ((argv[c][d] == '/') || (argv[c][d] == '\\'))
        {
          strcpy(startpath, argv[c]);
          startpath[d + 1] = 0;
          if (!chdir(startpath))
          {
            getcwd(songpath, MAX_PATHNAME);
            strcpy(packedpath, songpath);
          }
          strncpy(songfilename, &argv[c][d + 1], MAX_FILENAME - 1);
          break;
        }
      }
    }
  }

  // Validate parameters
  editorInfo.sidmodel &= 1;
  editorInfo.adparam &= 0xffff;
  zeropageadr &= 0xff;
  playeradr &= 0xff00;
  sidaddress &= 0xffff;
  if (!stepsize) stepsize = 4;
  if (editorInfo.multiplier > 16) editorInfo.multiplier = 16;
  if (keypreset > 2) keypreset = 0;
  if (editorInfo.finevibrato > 1) editorInfo.finevibrato = 1;
  if (editorInfo.optimizepulse > 1) editorInfo.optimizepulse = 1;
  if (editorInfo.optimizerealtime > 1) editorInfo.optimizerealtime = 1;
  if (residdelay > 63) residdelay = 63;
  if (customclockrate < 100) customclockrate = 0;
  if (fileformat > FORMAT_BIN) fileformat = FORMAT_SID;
  themesetting &= 1;
  if (stereomode > 4) stereomode = 1;
  if (mastervolume > 100) mastervolume = 100;
  // Older versions used 44100 Hz; most sound systems run at 48000 Hz, which
  // avoids resampling by the sound server
  if (mr == 44100) mr = 48000;
  showwelcome &= 1;
  sid_setvolume(mastervolume);

  // Read Scala tuning file
  if ((scalatuningfilepath[0]) && (scalatuningfilepath[0] != '0') && (scalatuningfilepath[1] != '\0'))
    readscalatuningfile();

  // Calculate frequencytable if necessary
  if (basepitch < 0.0f)
    basepitch = 0.0f;
  if (basepitch > 0.0f)
    calculatefreqtable();

  // Set special note names
  if (specialnotenames[1] != '\0')
    setspecialnotenames();

  // Test helper: preview library instruments offline
  if (getenv("GG_PREVIEWTEST"))
  {
    initchannels();
    clearsong(1, 1, 1, 1, 1);
    return library_previewtest(atoi(getenv("GG_PREVIEWTEST"))) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // Test helper: render all random instrument recipes
  if (getenv("GG_RANDOMTEST"))
  {
    initchannels();
    clearsong(1, 1, 1, 1, 1);
    return randominstrument_selftest(getenv("GG_RANDOMTEST")) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // Command line export without a window
  if (exportname[0])
  {
    int ok;
    absolutepath(exportname);
    absolutepath(savename);
    absolutepath(rendername);
    initchannels();
    clearsong(1, 1, 1, 1, 1);
    if (strlen(songfilename))
    {
      loadsong();
      if (!strncmp(statusmessage, "Could not", 9))
      {
        fprintf(stderr, "error: %s\n", statusmessage);
        return EXIT_FAILURE;
      }
    }
    if (exportsids) setnumsids(exportsids);
    // Test helpers: duplicate SID 1's orderlists to all SIDs, set addresses
    if (getenv("GG_DUPSIDS"))
    {
      int sn, ch;
      for (sn = 0; sn < MAX_SONGS; sn++)
      {
        if (!songlen[sn][0]) continue;
        for (ch = 3; ch < numchannels; ch++)
        {
          memcpy(songorder[sn][ch], songorder[sn][ch % 3], MAX_SONGLEN + 2);
          songlen[sn][ch] = songlen[sn][ch % 3];
        }
      }
      countpatternlengths();
    }
    if (getenv("GG_SIDADDR"))
    {
      const char *sa = getenv("GG_SIDADDR");
      int k = 0;
      while ((*sa) && (k < MAX_SIDS))
      {
        sidaddr[k++] = strtoul(sa, (char **)&sa, 16);
        if (*sa == ',') sa++;
      }
    }
    if (getenv("GG_TRACE"))
    {
      // Test helper: print the editor playroutine's SID registers per frame
      int frames = atoi(getenv("GG_TRACE"));
      int f, sn, r;
      initsong(getenv("GG_SUBTUNE") ? atoi(getenv("GG_SUBTUNE")) : 0, PLAY_BEGINNING);
      for (f = 0; f < frames; f++)
      {
        playroutine();
        printf("%04d:", f);
        for (sn = 0; sn < numsids; sn++)
        {
          printf(" ");
          for (r = 0; r < 25; r++) printf("%02x", sidreg[sn][r]);
        }
        printf("\n");
      }
      return EXIT_SUCCESS;
    }
    if (getenv("GG_REPLACETEST"))
      return library_replacetest() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (getenv("GG_RANDOMREPLACETEST"))
      return randominstrument_replacetest() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (savename[0])
    {
      strncpy(songfilename, savename, MAX_FILENAME - 1);
      ok = savesong();
      printf("%s\n", statusmessage);
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (rendername[0])
      return commandlinerender(rendername, rendersubtune, renderseconds, renderstems, renderfade, rendermono, renderbitrate) ? EXIT_SUCCESS : EXIT_FAILURE;
    ok = commandlineexport(exportname, exportformat, exportbase ? (int)(exportbase & 0xff00) : playeradr);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // Set screenmode
  theme = themesetting;
  if (fontsetting >= RES_NUMFONTS) fontsetting = 0;
  if (!initscreen())
  {
    fprintf(stderr, "Could not open the program window.\n");
    return EXIT_FAILURE;
  }
  setfont(fontsetting);

  // Keyboard layout: detect from the system keymap on the first start
  if (kbdlayoutsetting > 1)
    kbdlayout = (SDL_GetKeyFromScancode(SDL_SCANCODE_Z) == SDLK_y) ? 1 : 0;
  else
    kbdlayout = kbdlayoutsetting;

  // Reset channels/song
  initchannels();
  clearsong(1, 1, 1, 1, 1);

  undoInitAllAreas();

  // Init sound
  if (!sound_init(b, mr, writer, hardsid, editorInfo.sidmodel, editorInfo.ntsc, editorInfo.multiplier, catweasel, interpolate, customclockrate))
  {
    const char *lines[2] = {"Sound initialization failed.", "The editor works, but you will not hear anything."};
    dlg_message("Sound", lines, 2);
  }

  // Load song if applicable
  if (strlen(songfilename))
  {
    loadsong();
    do_sound_init();
    undoInitAllAreas();
  }
  else
    setstatusmessage("Welcome to C64 GrooveGoat " PROGRAMVERSION " - press F12 or click Help for the keyboard reference");

  if (getenv("GG_AUTOTEST"))
  {
    runautotest();
    return EXIT_SUCCESS;
  }

  // Start editor mainloop
  printmainscreen();
  if (!getenv("GG_NOMIDI")) midi_init();
  if (midi_names()[0]) { snprintf(statusmessage, sizeof statusmessage, "MIDI input: %.200s", midi_names()); printmainscreen(); }
  backup_init();
  if ((showwelcome) && (!getenv("GG_SCRIPT"))) showwelcomewindow();
  while (!exitprogram)
  {
    waitkeymouse();
    handledrop();
    docommand();
    updatetitle();
    backup_tick();
  }
  backup_shutdown();
  midi_shutdown();

  // Shutdown sound output now
  if (getenv("GG_AUDIODEBUG")) fprintf(stderr, "audio underruns: %u\n", snd_getunderruns());
  sound_uninit();
  themesetting = theme;
  saveconfig();

  return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// Directories & configuration
// ---------------------------------------------------------------------------

static void initdirectories(void)
{
  char *base = SDL_GetBasePath();
  char *home;

  appdir[0] = 0;
  if (base)
  {
    strncpy(appdir, base, MAX_PATHNAME - 1);
    appdir[MAX_PATHNAME - 1] = 0;
    SDL_free(base);
  }
  // Instrument library: next to the program, or in the app bundle resources
  snprintf(instrlibpath, MAX_PATHNAME, "%sinstruments", appdir);
  // Example songs: next to the program, or in the app bundle / AppImage
  snprintf(examplespath, MAX_PATHNAME, "%sexamples", appdir);

#ifdef __WIN32__
  {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
      snprintf(configfilename, MAX_PATHNAME, "%s\\C64 GrooveGoat", path);
      mkdir(configfilename);
      strncat(configfilename, "\\groovegoat.cfg", MAX_PATHNAME - strlen(configfilename) - 1);
    }
    else
      snprintf(configfilename, MAX_PATHNAME, "%sgroovegoat.cfg", appdir);
  }
  home = getenv("USERPROFILE");
#else
  home = getenv("HOME");
  if (home)
  {
    snprintf(configfilename, MAX_PATHNAME, "%s/.groovegoat", home);
    mkdir(configfilename, S_IRUSR | S_IWUSR | S_IXUSR);
    strncat(configfilename, "/groovegoat.cfg", MAX_PATHNAME - strlen(configfilename) - 1);
  }
  else
    strcpy(configfilename, "groovegoat.cfg");
#endif

  // When started from a desktop launcher the working directory is often
  // "/" or the program folder; start in the user's home folder instead.
  {
    char cwd[MAX_PATHNAME];
    if ((!getcwd(cwd, MAX_PATHNAME)) || (!strcmp(cwd, "/")) || (strstr(cwd, ".app/Contents")))
    {
      if (home)
      {
        char docs[MAX_PATHNAME];
#ifdef __WIN32__
        snprintf(docs, MAX_PATHNAME, "%s\\Documents", home);
#else
        snprintf(docs, MAX_PATHNAME, "%s/Documents", home);
#endif
        if (chdir(docs)) chdir(home);
      }
    }
  }
}

// Path of a file in the configuration folder (next to groovegoat.cfg)
void configpath(char *dest, const char *file)
{
  char dir[MAX_PATHNAME];
  char *sep;
  strncpy(dir, configfilename, MAX_PATHNAME - 1);
  dir[MAX_PATHNAME - 1] = 0;
  sep = strrchr(dir, '/');
  if ((!sep) || ((strrchr(dir, '\\')) && (strrchr(dir, '\\') > sep))) sep = strrchr(dir, '\\');
  if (sep) sep[1] = 0;
  else dir[0] = 0;
  snprintf(dest, MAX_PATHNAME, "%s%s", dir, file);
}

static void loadconfig(void)
{
  FILE *configfile = fopen(configfilename, "rt");
  if (!configfile) return;

  getparam(configfile, &b);
  getparam(configfile, &mr);
  getparam(configfile, &hardsid);
  getparam(configfile, &editorInfo.sidmodel);
  getparam(configfile, &editorInfo.ntsc);
  getparam(configfile, (unsigned *)&fileformat);
  getparam(configfile, (unsigned *)&playeradr);
  getparam(configfile, (unsigned *)&zeropageadr);
  getparam(configfile, &playerversion);
  getparam(configfile, &keypreset);
  getparam(configfile, (unsigned *)&stepsize);
  getparam(configfile, &editorInfo.multiplier);
  getparam(configfile, &catweasel);
  getparam(configfile, &editorInfo.adparam);
  getparam(configfile, &interpolate);
  getparam(configfile, &patterndispmode);
  getparam(configfile, &sidaddress);
  getparam(configfile, &editorInfo.finevibrato);
  getparam(configfile, &editorInfo.optimizepulse);
  getparam(configfile, &editorInfo.optimizerealtime);
  getparam(configfile, &residdelay);
  getparam(configfile, &customclockrate);
  getparam(configfile, &hardsidbufinteractive);
  getparam(configfile, &hardsidbufplayback);
  getfloatparam(configfile, &filterparams.distortionrate);
  getfloatparam(configfile, &filterparams.distortionpoint);
  getfloatparam(configfile, &filterparams.distortioncfthreshold);
  getfloatparam(configfile, &filterparams.type3baseresistance);
  getfloatparam(configfile, &filterparams.type3offset);
  getfloatparam(configfile, &filterparams.type3steepness);
  getfloatparam(configfile, &filterparams.type3minimumfetresistance);
  getfloatparam(configfile, &filterparams.type4k);
  getfloatparam(configfile, &filterparams.type4b);
  getfloatparam(configfile, &filterparams.voicenonlinearity);
  getparam(configfile, (unsigned*)&win_fullscreen);
  getparam(configfile, &bigwindow);
  getfloatparam(configfile, &basepitch);
  getfloatparam(configfile, &equaldivisionsperoctave);
  getstringparam(configfile, specialnotenames);
  getstringparam(configfile, scalatuningfilepath);
  getparam(configfile, &themesetting);
  getparam(configfile, &stereomode);
  getparam(configfile, &prgfrontend);
  getparam(configfile, &mastervolume);
  getparam(configfile, &showwelcome);
  getparam(configfile, &kbdlayoutsetting);
  getparam(configfile, &showpiano);
  getparam(configfile, &prgpiano);
  getparam(configfile, &fontsetting);
  fclose(configfile);
  showpiano &= 1;
  prgpiano &= 1;
  if (!strcmp(specialnotenames, "-")) specialnotenames[0] = 0;
  if (!strcmp(scalatuningfilepath, "-")) scalatuningfilepath[0] = 0;
}

static void saveconfig(void)
{
  FILE *configfile = fopen(configfilename, "wt");
  if (!configfile) return;
  fprintf(configfile, ";------------------------------------------------------------------------------\n"
    ";C64 GrooveGoat config file. Rows starting with ; are comments. Hexadecimal     \n"
    ";parameters are to be preceded with $ and decimal parameters with nothing.     \n"
    ";------------------------------------------------------------------------------\n"
    "\n"
    ";reSID buffer length (in milliseconds)\n%d\n\n"
    ";reSID mixing rate (in Hz)\n%d\n\n"
    ";Hardsid device number (0 = off)\n%d\n\n"
    ";reSID model (0 = 6581, 1 = 8580)\n%d\n\n"
    ";Timing mode (0 = PAL, 1 = NTSC)\n%d\n\n"
    ";Packer/relocator fileformat (0 = SID, 1 = PRG, 2 = BIN)\n%d\n\n"
    ";Packer/relocator player address\n$%04x\n\n"
    ";Packer/relocator zeropage baseaddress\n$%02x\n\n"
    ";Packer/relocator player options\n%d\n\n"
    ";Key entry mode (0 = Protracker, 1 = DMC, 2 = Janko)\n%d\n\n"
    ";Pattern highlight step size\n%d\n\n"
    ";Speed multiplier (0 = 25Hz, 1 = 1X, 2 = 2X etc.)\n%d\n\n"
    ";Use CatWeasel SID (0 = off, 1 = on)\n%d\n\n"
    ";Hardrestart ADSR parameter\n$%04x\n\n"
    ";reSID interpolation (0 = off, 1 = on, 2 = distortion, 3 = distortion & on)\n%d\n\n"
    ";Pattern display mode (0 = decimal, 1 = hex, 2 = decimal w/dots, 3 = hex w/dots)\n%d\n\n"
    ";SID baseaddress\n$%04x\n\n"
    ";Finevibrato mode (0 = off, 1 = on)\n%d\n\n"
    ";Pulseskipping (0 = off, 1 = on)\n%d\n\n"
    ";Realtime effect skipping (0 = off, 1 = on)\n%d\n\n"
    ";Random reSID write delay in cycles (0 = off)\n%d\n\n"
    ";Custom SID clock cycles per second (0 = use PAL/NTSC default)\n%d\n\n"
    ";HardSID interactive mode buffer size (in milliseconds, 0 = maximum/no flush)\n%d\n\n"
    ";HardSID playback mode buffer size (in milliseconds, 0 = maximum/no flush)\n%d\n\n"
    ";reSID-fp distortion rate\n%f\n\n"
    ";reSID-fp distortion point\n%f\n\n"
    ";reSID-fp distortion CF threshold\n%f\n\n"
    ";reSID-fp type 3 base resistance\n%f\n\n"
    ";reSID-fp type 3 base offset\n%f\n\n"
    ";reSID-fp type 3 base steepness\n%f\n\n"
    ";reSID-fp type 3 minimum FET resistance\n%f\n\n"
    ";reSID-fp type 4 k\n%f\n\n"
    ";reSID-fp type 4 b\n%f\n\n"
    ";reSID-fp voice nonlinearity\n%f\n\n"
    ";Window type (0 = window, 1 = fullscreen)\n%d\n\n"
    ";window scale factor (unused, the window can be resized freely)\n%d\n\n"
    ";Base pitch of A-4 in Hz (0 = use default frequencytable)\n%f\n\n"
    ";Equal divisions per octave (12 = default, 8.2019143 = Bohlen-Pierce)\n%f\n\n"
    ";Special note names (2 chars for every note in an octave/cycle, - = none)\n%s\n\n"
    ";Path to a Scala tuning file .scl (- = none)\n%s\n\n"
    ";Theme (0 = night, 1 = day)\n%d\n\n"
    ";Stereo output for multi-SID songs (0 = mono, 1 = stereo)\n%d\n\n"
    ";PRG export includes the player frontend (0 = no, 1 = yes)\n%d\n\n"
    ";Output volume in percent (0-100)\n%d\n\n"
    ";Show the welcome window at startup (0 = no, 1 = yes)\n%d\n\n"
    ";Keyboard layout for shortcuts (0 = EN QWERTY, 1 = DE QWERTZ, 2 = detect)\n%d\n\n"
    ";Show the piano keyboard (0 = no, 1 = yes)\n%d\n\n"
    ";Piano keyboard on the screen of runnable PRGs (0 = no, 1 = yes)\n%d\n\n"
    ";Screen font (0 = Fira Code, 1 = MesloLGS NF)\n%d\n\n",
    b,
    mr,
    hardsid,
    editorInfo.sidmodel,
    editorInfo.ntsc,
    fileformat,
    playeradr,
    zeropageadr,
    playerversion,
    keypreset,
    stepsize,
    editorInfo.multiplier,
    catweasel,
    editorInfo.adparam,
    interpolate,
    patterndispmode,
    sidaddress,
    editorInfo.finevibrato,
    editorInfo.optimizepulse,
    editorInfo.optimizerealtime,
    residdelay,
    customclockrate,
    hardsidbufinteractive,
    hardsidbufplayback,
    filterparams.distortionrate,
    filterparams.distortionpoint,
    filterparams.distortioncfthreshold,
    filterparams.type3baseresistance,
    filterparams.type3offset,
    filterparams.type3steepness,
    filterparams.type3minimumfetresistance,
    filterparams.type4k,
    filterparams.type4b,
    filterparams.voicenonlinearity,
    win_fullscreen,
    bigwindow,
    basepitch,
    equaldivisionsperoctave,
    specialnotenames[0] ? specialnotenames : "-",
    scalatuningfilepath[0] ? scalatuningfilepath : "-",
    themesetting,
    stereomode,
    prgfrontend,
    mastervolume,
    showwelcome,
    kbdlayout,
    showpiano,
    prgpiano,
    fontsetting);
  fclose(configfile);
}

static void updatetitle(void)
{
  static char lasttitle[MAX_PATHNAME] = {0};
  char title[MAX_PATHNAME];
  if (strlen(loadedsongfilename))
    snprintf(title, MAX_PATHNAME, "C64 GrooveGoat %s - %s%s", PROGRAMVERSION, loadedsongfilename, songmodified ? " *" : "");
  else
    snprintf(title, MAX_PATHNAME, "C64 GrooveGoat %s%s", PROGRAMVERSION, songmodified ? " *" : "");
  if (strcmp(title, lasttitle))
  {
    strcpy(lasttitle, title);
    plat_settitle(title);
  }
}

// Files dropped onto the window: songs are loaded, instruments go into the
// current instrument slot
static void handledrop(void)
{
  char *name;
  char *slash;
  char dir[MAX_PATHNAME];
  int len;

  if (!win_dropfile[0]) return;
  name = win_dropfile;
  len = strlen(name);
  strcpy(dir, name);
  slash = strrchr(dir, '/');
#ifdef __WIN32__
  {
    char *bs = strrchr(dir, '\\');
    if ((!slash) || (bs > slash)) slash = bs;
  }
#endif
  if (slash)
  {
    slash[1] = 0;
    name = win_dropfile + (slash - dir) + 1;
  }
  else dir[0] = 0;

  if ((len > 4) && ((!cmpname(win_dropfile + len - 4, ".ins"))))
  {
    if (editorInfo.einum)
    {
      if (dir[0]) chdir(dir);
      strncpy(instrfilename, name, MAX_FILENAME - 1);
      loadinstrument();
      getcwd(instrpath, MAX_PATHNAME);
      snprintf(statusmessage, sizeof statusmessage, "Loaded instrument %s into slot %02X", instrfilename, editorInfo.einum);
    }
  }
  else
  {
    if ((!songmodified) || (dlg_confirm("Load song", "Discard the changes of the current song?")))
    {
      if (dir[0]) chdir(dir);
      strncpy(songfilename, name, MAX_FILENAME - 1);
      loadsong();
      getcwd(songpath, MAX_PATHNAME);
      do_sound_init();
      undoInitAllAreas();
    }
  }
  win_dropfile[0] = 0;
  printmainscreen();
}

// Headless self test used by the build: renders screens and exits
static void runautotest(void)
{
  const char *shot = getenv("GG_AUTOTEST");
  const char *sids = getenv("GG_SIDS");
  const char *th = getenv("GG_THEME");
  int frames = 10;
  char name[MAX_PATHNAME];

  if (sids) changenumsids(atoi(sids));
  if (th) settheme(atoi(th));
  if (getenv("GG_PLAY")) initsong(0, PLAY_BEGINNING);
  if (getenv("GG_SCREEN"))
  {
    const char *scr = getenv("GG_SCREEN");
    strncpy(autoshotname, shot, sizeof autoshotname - 1);
    autoshotframes = getenv("GG_SHOTFRAMES") ? atoi(getenv("GG_SHOTFRAMES")) : 3;
    printmainscreen();
    if (!strcmp(scr, "help")) onlinehelp(0, 0);
    if (!strcmp(scr, "manual")) showmanual();
    if (!strcmp(scr, "history")) showhistory();
    if (!strcmp(scr, "info")) showinfo();
    if (!strcmp(scr, "welcome")) showwelcomewindow();
    if (!strcmp(scr, "sidinfo")) showsidinfo();
    if (!strcmp(scr, "exportsid")) exportsid();
    if (!strcmp(scr, "exportprg")) exportprg();
    if (!strcmp(scr, "exportaudio")) exportaudio(0);
    if (!strcmp(scr, "exportvideo")) exportaudio(1);
    if (!strcmp(scr, "library")) instrumentlibrary();
    if (!strcmp(scr, "random")) randominstrument();
    if (!strcmp(scr, "load")) load();
    if (!strcmp(scr, "loadins")) { editorInfo.editmode = EDIT_INSTRUMENT; load(); }
    if (!strcmp(scr, "clear")) clear();
    if (!strcmp(scr, "quit")) quit();
  }
  while (frames--)
  {
    printstatus();
    fliptoscreen();
    getkey();
  }
  snprintf(name, MAX_PATHNAME, "%s", shot);
  savescreenshot(name);
  if (getenv("GG_HELPSHOT"))
  {
    snprintf(name, MAX_PATHNAME, "%s", getenv("GG_HELPSHOT"));
    helpscreenshot(name);
  }
  sound_uninit();
}

// ---------------------------------------------------------------------------
// Main loop helpers
// ---------------------------------------------------------------------------

void waitkey(void)
{
  for (;;)
  {
    displayupdate();
    getkey();
    if ((rawkey) || (key)) break;
    if (win_quitted) break;
  }

  converthex();
}

// Handles the events from the MIDI keyboard. Returns 1 if the editor has something to do (a note to enter).
// Notes are entered like the computer keyboard enters them (the note column of the pattern editor, or the
// chosen instrument played as a test note anywhere else); note off releases the test note.
void editor_noteon(int n, int record)
{
  if ((record) && (editorInfo.editmode == EDIT_PATTERN) && (!editorInfo.epcolumn)) midinewnote = n;
  else playtestnote(n, editorInfo.einum, editorInfo.epchn);
}

void editor_noteoff(void)
{
  if (songinit == PLAY_STOPPED) releasenote(editorInfo.epchn);
}

int editor_heldnote(void)
{
  return midiplaying >= 0 ? midi_tracker_note(midiplaying, FIRSTNOTE, LASTNOTE) : -1;
}

static int midipump(void)
{
  GGMIDIEVENT e;
  int work = 0;
  if (midi_changed())
  {
    if (midi_names()[0]) snprintf(statusmessage, sizeof statusmessage, "MIDI input: %.200s", midi_names());
    else snprintf(statusmessage, sizeof statusmessage, "MIDI input: no device");
    work = 1;
  }
  while (midi_poll(&e))
  {
    if (e.type == MIDI_NOTEON)
    {
      int n = midi_tracker_note(e.a, FIRSTNOTE, LASTNOTE);
      if (n < 0) continue;
      midiplaying = e.a;
      editor_noteon(n, 1);
      if (midinewnote >= 0) work = 1;
    }
    else if (e.type == MIDI_NOTEOFF)
    {
      if (e.a == midiplaying) { editor_noteoff(); midiplaying = -1; }
    }
    else if (e.type == MIDI_PROGRAM)
    {
      int ins = e.a + 1;
      if (ins >= MAX_INSTR) ins = MAX_INSTR - 1;
      editorInfo.einum = ins;
      snprintf(statusmessage, sizeof statusmessage, "MIDI program change: instrument %02X", ins);
      work = 1;
    }
  }
  return work;
}

void waitkeymouse(void)
{
  for (;;)
  {
    displayupdate();
    getkey();
    if ((rawkey) || (key)) break;
    if (win_quitted) break;
    if (mouseb) break;
    if ((prevmouseb) && (!mouseb)) break;   // button released (piano keys)
    if (mousewheel) break;
    if (win_dropfile[0]) break;
    if (midipump()) break;
  }

  converthex();
}

void waitkeymousenoupdate(void)
{
  for (;;)
  {
    fliptoscreen();
    getkey();
    if ((rawkey) || (key)) break;
    if (win_quitted) break;
    if (mouseb) break;
  }

  converthex();
}

void waitkeynoupdate(void)
{
  for (;;)
  {
    fliptoscreen();
    getkey();
    if ((rawkey) || (key)) break;
    if ((mouseb) && (!prevmouseb)) break;
    if (win_quitted) break;
  }
}

void converthex()
{
  int c;

  hexnybble = -1;
  for (c = 0; c < 16; c++)
  {
    if (tolower(key) == hexkeytbl[c])
    {
      if (c >= 10)
      {
        if (!shiftpressed) hexnybble = c;
      }
      else
      {
        hexnybble = c;
      }
    }
  }
}

void do_sound_init(void)
{
  sound_init(b, mr, writer, hardsid, editorInfo.sidmodel, editorInfo.ntsc, editorInfo.multiplier, catweasel, interpolate, customclockrate);
}

void toggle_interpolation_mode(void)
{
  interpolate++;
  if (interpolate == 2)  // skip FP only mode
    interpolate++;
  interpolate &= 3;
  stopsong();
  do_sound_init();
}

void changenumsids(int n)
{
  int c;
  if (n == numsids) return;
  if (n < numsids)
  {
    int lost = 0;
    for (c = n * 3; c < numchannels; c++)
      if (channelhasdata(c)) lost = 1;
    if (lost)
    {
      char buf[128];
      sprintf(buf, "Channels of SID %d-%d contain music. Remove them?", n + 1, numsids);
      if (!dlg_confirm("Change SID count", buf)) return;
    }
  }
  undoInitAllAreas();
  setnumsids(n);
  do_sound_init();
  undoInitAllAreas();
  validatechnview();
  snprintf(statusmessage, sizeof statusmessage, "Now using %d SID%s (%d channels)%s", numsids, numsids > 1 ? "s" : "", numchannels,
    numsids > 2 ? " - songs are saved in the C64 GrooveGoat .ggt format" : "");
}

void cyclefont(void)
{
  fontsetting = (getfont() + 1) % RES_NUMFONTS;
  setfont(fontsetting);
  snprintf(statusmessage, sizeof statusmessage, "Screen font: %s", res_fontnames[fontsetting]);
  printmainscreen();
}

void togglethemekey(void)
{
  settheme(theme ^ 1);
  themesetting = theme;
  printmainscreen();
}

void newsong(void)
{
  static const int sidchoices[4] = {1, 2, 4, 8};
  const char *lines[3] = {"How many SID chips should the new song use?", "", ""};
  const char *opts[5] = {"1 SID", "2 SIDs", "4 SIDs", "8 SIDs", "Cancel"};
  int def = 0, sel, c;

  if (songmodified) lines[2] = "The changes of the current song will be lost.";
  for (c = 0; c < 4; c++)
    if (sidchoices[c] == numsids) def = c;
  sel = dlg_choose("New song", lines, 3, opts, 5, def);
  if ((sel < 0) || (sel > 3)) return;
  memset(songfilename, 0, sizeof songfilename);
  clearsong(1, 1, 1, 1, 1);
  clearinactivechannels();
  if (sidchoices[sel] != numsids)
  {
    setnumsids(sidchoices[sel]);
    do_sound_init();
    validatechnview();
  }
  undoInitAllAreas();
  songmodified = 0;
  snprintf(statusmessage, sizeof statusmessage, "New song with %d SID%s (%d channels)", numsids, numsids > 1 ? "s" : "", numchannels);
}

// Piano keyboard on / off: the pattern editor, the tables and the song info
// give the piano their lowest rows
void togglepiano(void)
{
  int t;
  showpiano ^= 1;
  for (t = 0; t < MAX_TABLES; t++)
  {
    if ((t == editorInfo.etnum) && (editorInfo.etpos - editorInfo.etview[t] >= VISIBLETABLEROWS))
      editorInfo.etview[t] = editorInfo.etpos - VISIBLETABLEROWS + 1;
  }
  editorInfo.epview = editorInfo.eppos - VISIBLEPATTROWS / 2;
  setstatusmessage(showpiano ? "Piano keyboard on (CTRL+B)" : "Piano keyboard off (CTRL+B)");
  printmainscreen();
}

// Pattern tool buttons: run the editor command of the shortcut
static void patternkey(int k)
{
  editorInfo.editmode = EDIT_PATTERN;
  shiftpressed = 1;
  key = 0;
  rawkey = k;
}

void docommand(void)
{
  // "GUI" operation :)
  mousecommands();

  // CTRL/CMD+R random instrument, CTRL/CMD+L instrument library. Handled
  // before the editors, where CTRL otherwise works like SHIFT.
  if (((ctrlpressed) || (metapressed)) && (!altpressed) && (rawkey == KEY_B))
  {
    togglepiano();
    key = 0;
    rawkey = 0;
  }
  if (((ctrlpressed) || (metapressed)) && (!altpressed) && ((rawkey == KEY_R) || (rawkey == KEY_L)))
  {
    if (rawkey == KEY_R) randominstrument();
    else instrumentlibrary();
    editorInfo.editmode = EDIT_INSTRUMENT;
    key = 0;
    rawkey = 0;
  }

  GTUNDO_OBJECT *ed = undoCreateEditorInfo();
  int oldundo = undoCounter;

  // Mode-specific commands
  switch (editorInfo.editmode)
  {
  case EDIT_ORDERLIST:
    for (int i = 0;i < numchannels;i++)
      undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST, i + (editorInfo.esnum*MAX_CHN), UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_LEN, 0, UNDO_AREA_DIRTY_CHECK);
    orderlistcommands();
    break;

  case EDIT_INSTRUMENT:
    undoAreaSetCheckForChange(UNDO_AREA_INSTRUMENTS, editorInfo.einum, UNDO_AREA_DIRTY_CHECK);
    instrumentcommands();
    break;

  case EDIT_TABLES:
    undoAreaSetCheckForChange(UNDO_AREA_INSTRUMENTS, editorInfo.einum, UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + editorInfo.etnum, 0, UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + editorInfo.etnum, 1, UNDO_AREA_DIRTY_CHECK);
    tablecommands();
    break;

  case EDIT_PATTERN:
    undoAreaSetCheckForChange(UNDO_AREA_PATTERN, editorUndoInfo.editorInfo[editorInfo.epchn].epnum, UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_PATTERN_LEN, 0, UNDO_AREA_DIRTY_CHECK);
    for (int i = 0;i < numchannels;i++)
      undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST, i + (editorInfo.esnum*MAX_CHN), UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_LEN, 0, UNDO_AREA_DIRTY_CHECK);
    patterncommands();
    break;

  case EDIT_NAMES:
    {
      char oldnames[3][MAX_STR];
      memcpy(oldnames[0], songname, MAX_STR);
      memcpy(oldnames[1], authorname, MAX_STR);
      memcpy(oldnames[2], copyrightname, MAX_STR);
      namecommands();
      if (memcmp(oldnames[0], songname, MAX_STR) || memcmp(oldnames[1], authorname, MAX_STR) || memcmp(oldnames[2], copyrightname, MAX_STR))
        songmodified = 1;
    }
    break;
  }

  if (undoValidateUndoAreas(ed) == 0)
    undoFreeUndoObject((GTUNDO_OBJECT*)ed);
  if (undoCounter != oldundo) songmodified = 1;

  // General commands
  generalcommands();
  validatechnview();
}

static int lastclickx = -1, lastclicky = -1, lastclicktime = 0;

static void buttoncommand(int id, int param)
{
  switch (id)
  {
    case BTN_NEW: newsong(); break;
    case BTN_OPEN:
    {
      int oldmode = editorInfo.editmode;
      if (editorInfo.editmode == EDIT_INSTRUMENT || editorInfo.editmode == EDIT_TABLES) editorInfo.editmode = EDIT_PATTERN;
      shiftpressed = 0;
      load();
      if (editorInfo.editmode == EDIT_PATTERN) editorInfo.editmode = oldmode;
    }
    break;
    case BTN_SAVE:
    case BTN_SAVEAS:
    {
      int oldmode = editorInfo.editmode;
      if (editorInfo.editmode == EDIT_INSTRUMENT || editorInfo.editmode == EDIT_TABLES) editorInfo.editmode = EDIT_PATTERN;
      save();
      editorInfo.editmode = oldmode;
    }
    break;
    case BTN_LIBRARY:
    instrumentlibrary();
    editorInfo.editmode = EDIT_INSTRUMENT;
    break;
    case BTN_RANDOMINS:
    randominstrument();
    editorInfo.editmode = EDIT_INSTRUMENT;
    break;
    case BTN_ADSRGRAPH:
    editorInfo.editmode = EDIT_INSTRUMENT;
    editorInfo.eipos = 0;
    editorInfo.eicolumn = 0;
    break;
    case BTN_INSLOAD:
    {
      int oldmode = editorInfo.editmode;
      editorInfo.editmode = EDIT_INSTRUMENT;
      if (!editorInfo.einum) editorInfo.einum = 1;
      load();
      if ((oldmode != EDIT_INSTRUMENT) && (oldmode != EDIT_TABLES)) editorInfo.editmode = EDIT_INSTRUMENT;
    }
    break;
    case BTN_INSSAVE:
    {
      editorInfo.editmode = EDIT_INSTRUMENT;
      if (!editorInfo.einum) editorInfo.einum = 1;
      save();
    }
    break;
    case BTN_EXPORTSID: exportsid(); break;
    case BTN_EXPORTPRG: exportprg(); break;
    case BTN_EXPORTAUDIO: exportaudio(0); break;
    case BTN_EXPORTVIDEO: exportaudio(1); break;
    case BTN_THEME: togglethemekey(); break;
    case BTN_FONT: cyclefont(); break;
    case BTN_HELP: onlinehelp(0, 0); break;
    case BTN_MANUAL: showmanual(); break;
    case BTN_HISTORY: showhistory(); break;
    case BTN_INFO: showinfo(); break;
    case BTN_PLAY: initsong(editorInfo.esnum, PLAY_BEGINNING); followplay = shiftpressed; break;
    case BTN_PLAYPOS: initsong(editorInfo.esnum, PLAY_POS); followplay = shiftpressed; break;
    case BTN_PLAYPATT: initsong(editorInfo.esnum, PLAY_PATTERN); followplay = shiftpressed; break;
    case BTN_STOP: stopsong(); break;
    case BTN_FOLLOW: followplay ^= 1; break;
    case BTN_SIDS1: case BTN_SIDS2: case BTN_SIDS4: case BTN_SIDS8:
      changenumsids(param);
      break;
    case BTN_MODEL:
      undoCreateEditorInfoBackup();
      editorInfo.sidmodel ^= 1;
      undoAddEditorSettingsToList();
      do_sound_init();
      break;
    case BTN_NTSC:
      undoCreateEditorInfoBackup();
      editorInfo.ntsc ^= 1;
      undoAddEditorSettingsToList();
      do_sound_init();
      break;
    case BTN_SPEED:
      undoCreateEditorInfoBackup();
      if ((mouseb & MOUSEB_RIGHT) || (mousex < uibuttons[0].x)) prevmultiplier();
      else nextmultiplier();
      undoAddEditorSettingsToList();
      break;
    case BTN_HR: editadsr(); break;
    case BTN_FV:
      undoCreateEditorInfoBackup();
      editorInfo.finevibrato ^= 1;
      undoAddEditorSettingsToList();
      break;
    case BTN_PO:
      undoCreateEditorInfoBackup();
      editorInfo.optimizepulse ^= 1;
      undoAddEditorSettingsToList();
      break;
    case BTN_RO:
      undoCreateEditorInfoBackup();
      editorInfo.optimizerealtime ^= 1;
      undoAddEditorSettingsToList();
      break;
    case BTN_INT:
      interpolate ^= 1;
      interpolate &= 1 + (interpolate << 1);  // turn off FP if INT is 0
      stopsong();
      do_sound_init();
      break;
    case BTN_FP:
      interpolate ^= 2;
      interpolate |= interpolate >> 1;  // turn on INT if FP is 1
      stopsong();
      do_sound_init();
      break;
    case BTN_STEREO:
      // Mono -> Stereo 100% -> 90% -> 80% -> 70% -> Mono (right click: backwards)
      if (mouseb & MOUSEB_RIGHT) stereomode = (stereomode + 4) % 5;
      else stereomode = (stereomode + 1) % 5;
      sid_setpanning(numsids, stereowidth());
      if (stereomode)
      {
        char msg[100];
        sprintf(msg, "Stereo output (%d%% width): SIDs are spread between left and right", stereowidth());
        setstatusmessage(msg);
      }
      else setstatusmessage("Mono output: all SIDs centered");
      break;
    case BTN_OCTAVE:
      if (ui_arrowat(mousex, mousey) < 0)
      {
        if (editorInfo.epoctave > 0) editorInfo.epoctave--;
      }
      else if (ui_arrowat(mousex, mousey) > 0)
      {
        if (editorInfo.epoctave < 7) editorInfo.epoctave++;
      }
      else if (mouseb & MOUSEB_RIGHT)
      {
        if (editorInfo.epoctave > 0) editorInfo.epoctave--;
      }
      else
      {
        if (editorInfo.epoctave < 7) editorInfo.epoctave++;
      }
      break;
    case BTN_RECORD: recordmode ^= 1; break;
    case BTN_AUTOADV:
      autoadvance++;
      if (autoadvance > 2) autoadvance = 0;
      break;
    case BTN_KEYMODE:
      keypreset++;
      if (keypreset > 2) keypreset = 0;
      break;
    case BTN_UNDO: undoPerform(); break;
    case BTN_REDO: redoPerform(); break;
    case BTN_KBDLAYOUT:
      kbdlayout ^= 1;
      snprintf(statusmessage, sizeof statusmessage, kbdlayout ? "Keyboard layout DE (QWERTZ): CTRL+Z undo, CTRL+Y redo, notes Y-M and Q-P" :
        "Keyboard layout EN (QWERTY): CTRL+Z undo, CTRL+Y redo, notes Z-M and Q-P");
      break;
    case BTN_CHNVIEW:
      chnview = param;
      editorInfo.epchn = param;
      if (editorInfo.eppos > pattlen[editorUndoInfo.editorInfo[editorInfo.epchn].epnum])
        editorInfo.eppos = pattlen[editorUndoInfo.editorInfo[editorInfo.epchn].epnum];
      break;
    case BTN_ORDVIEW:
      editorInfo.eschn += param;
      if (editorInfo.eschn < 0) editorInfo.eschn = 0;
      if (editorInfo.eschn >= numchannels) editorInfo.eschn = numchannels - 1;
      break;
    case BTN_MUTE: mutechannel(param); break;
    case BTN_SOLO: solochannel(param); break;
    case BTN_SIDADDR: showsidinfo(); break;
    case BTN_QUIT: quit(); break;
    case BTN_PIANO: togglepiano(); break;
    case BTN_TRANSUP: patternkey(KEY_Q); break;
    case BTN_TRANSDN: patternkey(KEY_A); break;
    case BTN_OCTUP: patternkey(KEY_W); break;
    case BTN_OCTDN: patternkey(KEY_S); break;
    case BTN_MARKUP: patternkey(KEY_UP); break;
    case BTN_MARKDN: patternkey(KEY_DOWN); break;
    case BTN_MARKALL: patternkey(KEY_L); break;
    case BTN_PATTPREV: patternkey(KEY_LEFT); break;
    case BTN_PATTNEXT: patternkey(KEY_RIGHT); break;
    case BTN_CUT: patternkey(KEY_X); break;
    case BTN_COPY: patternkey(KEY_C); break;
    case BTN_PASTE: patternkey(KEY_V); break;
  }
}

void mousecommands(void)
{
  int c;
  int param = 0;
  int id;
  int click = (mouseb) && (!prevmouseb);

  // Mouse wheel scrolls the panel below the pointer
  if (mousewheel)
  {
    int steps = mousewheel > 0 ? mousewheel : -mousewheel;
    int dir = mousewheel > 0 ? -1 : 1;
    while (steps--)
    {
      if ((mousex < PATT_W) && (mousey >= PATT_ROWS_Y) && (mousey < PATT_ROWS_Y + VISIBLEPATTROWS))
      {
        int len = pattlen[editorUndoInfo.editorInfo[editorInfo.epchn].epnum];
        editorInfo.eppos += dir;
        if (editorInfo.eppos < 0) editorInfo.eppos = 0;
        if (editorInfo.eppos > len) editorInfo.eppos = len;
        editorInfo.epview = editorInfo.eppos - VISIBLEPATTROWS/2;
      }
      else if ((mousex >= RP_X) && (mousey >= ORD_ROWS_Y) && (mousey < ORD_ROWS_Y + VISIBLEORDERCHNS))
      {
        editorInfo.esview += dir;
        if (editorInfo.esview < 0) editorInfo.esview = 0;
        if (editorInfo.esview > MAX_SONGLEN) editorInfo.esview = MAX_SONGLEN;
      }
      else if ((mousex >= RP_X) && (mousey >= TBL_ROWS_Y) && (mousey < TBL_ROWS_Y + VISIBLETABLEROWS))
      {
        int t = (mousex - TBL_X(0)) / 11;
        if (t < 0) t = 0;
        if (t >= MAX_TABLES) t = MAX_TABLES - 1;
        editorInfo.etview[t] += dir;
        if (editorInfo.etview[t] < 0) editorInfo.etview[t] = 0;
        if (editorInfo.etview[t] > MAX_TABLELEN - VISIBLETABLEROWS) editorInfo.etview[t] = MAX_TABLELEN - VISIBLETABLEROWS;
        if (t == editorInfo.etnum)
        {
          if (editorInfo.etpos < editorInfo.etview[t]) editorInfo.etpos = editorInfo.etview[t];
          if (editorInfo.etpos >= editorInfo.etview[t] + VISIBLETABLEROWS) editorInfo.etpos = editorInfo.etview[t] + VISIBLETABLEROWS - 1;
        }
      }
      else if ((mousex >= RP_X) && (mousey == INS_Y))
      {
        if (dir < 0) nextinstr(); else previnstr();
      }
    }
  }

  // Mouse wheel over the volume slider
  if ((mousewheel) && (mousey == MESSAGE_Y) && (mousex >= VOL_LABEL_X))
  {
    int v = (int)mastervolume + (mousewheel > 0 ? 5 : -5);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    mastervolume = v;
    sid_setvolume(mastervolume);
  }

  // Piano keyboard: play the clicked key while the button is held (drag for
  // a glissando), release the note when the button is let go
  {
    static int pianonote = -1, pianochn = 0;
    int note = -1;
    if ((showpiano) && (mouseb & MOUSEB_LEFT) && (mousey >= PIANO_Y) && (mousey < PIANO_Y + PIANO_H))
      note = piano_keyat(mousepx, mousepy - PIANO_Y * 32, MAX_COLUMNS * 16, PIANO_H * 32);
    if ((pianonote >= 0) && ((!(mouseb & MOUSEB_LEFT)) || (note < 0)))
    {
      releasenote(pianochn);
      pianonote = -1;
    }
    if ((note >= 0) && (note <= LASTNOTE - FIRSTNOTE) && (note != pianonote) && ((click) || (pianonote >= 0)))
    {
      pianochn = editorInfo.epchn;
      playtestnote(FIRSTNOTE + note, editorInfo.einum, pianochn);
      pianonote = note;
    }
    if ((mousey >= PIANO_Y) && (mousey < PIANO_Y + PIANO_H) && (showpiano)) return;
  }

  if (!mouseb) return;

  // Volume slider: click or drag
  if ((mousey == MESSAGE_Y) && (mousex >= VOL_X - 1) && (mousex < VOL_X + VOL_W + 5))
  {
    int v = (mousex - VOL_X + 1) * 5;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    mastervolume = v;
    sid_setvolume(mastervolume);
    return;
  }
  if ((mousey == MESSAGE_Y) && (mousex >= VOL_LABEL_X) && (mousex < VOL_X - 1) && (click))
  {
    // Speaker symbol: mute / restore
    static unsigned savedvolume = 80;
    if (mastervolume)
    {
      savedvolume = mastervolume;
      mastervolume = 0;
    }
    else mastervolume = savedvolume ? savedvolume : 80;
    sid_setvolume(mastervolume);
    return;
  }

  // Buttons & clickable fields registered by the display code
  id = ui_hitbutton(mousex, mousey, &param);
  if (id != BTN_NONE)
  {
    int repeat = (id == BTN_PATTNUM) || (id == BTN_SUBTUNE) || (id == BTN_INSTRNUM) || (id == BTN_OCTAVE) ||
      (id == BTN_TRANSUP) || (id == BTN_TRANSDN) || (id == BTN_OCTUP) || (id == BTN_OCTDN) ||
      (id == BTN_MARKUP) || (id == BTN_MARKDN) || (id == BTN_PATTPREV) || (id == BTN_PATTNEXT);
    if ((click) || ((repeat) && (mouseheld > HOLDDELAY)))
    {
      switch (id)
      {
        // The < and > arrows always step back / forward; on the value
        // itself left click = next, right click = previous
        case BTN_PATTNUM:
        editorInfo.epchn = param;
        if (mouseb & MOUSEB_LEFT) nextpattern();
        if (mouseb & MOUSEB_RIGHT) prevpattern();
        break;

        case BTN_SUBTUNE:
        if (ui_arrowat(mousex, mousey) < 0) prevsong();
        else if (ui_arrowat(mousex, mousey) > 0) nextsong();
        else if (mouseb & MOUSEB_LEFT) nextsong();
        else if (mouseb & MOUSEB_RIGHT) prevsong();
        break;

        case BTN_INSTRNUM:
        if (ui_arrowat(mousex, mousey) < 0) previnstr();
        else if (ui_arrowat(mousex, mousey) > 0) nextinstr();
        else if (mouseb & MOUSEB_LEFT) nextinstr();
        else if (mouseb & MOUSEB_RIGHT) previnstr();
        break;

        case BTN_SPEED:
        {
          // left half = slower, right half = faster
          int c2;
          for (c2 = 0; c2 < numuibuttons; c2++)
          {
            if (uibuttons[c2].id == BTN_SPEED)
            {
              undoCreateEditorInfoBackup();
              int dir = ui_arrowat(mousex, mousey);
              if (dir < 0) prevmultiplier();
              else if (dir > 0) nextmultiplier();
              else if ((mousex < uibuttons[c2].x + uibuttons[c2].w / 2) || (mouseb & MOUSEB_RIGHT)) prevmultiplier();
              else nextmultiplier();
              undoAddEditorSettingsToList();
              break;
            }
          }
        }
        break;

        default:
        buttoncommand(id, param);
        break;
      }
    }
    return;
  }

  // Pattern editpos
  if ((mousex < PATT_W) && (mousey >= PATT_ROWS_Y) && (mousey < PATT_ROWS_Y + VISIBLEPATTROWS))
  {
    for (c = 0; c < VISIBLECHANNELS; c++)
    {
      int ch = chnview + c;
      int x0 = CHN_X(c) - 1;
      if (ch >= numchannels) break;
      if ((mousex >= x0) && (mousex < x0 + CHN_W - 1))
      {
        int x = mousex - x0;
        int newpos = mousey - PATT_ROWS_Y + editorInfo.epview;
        int len = pattlen[editorUndoInfo.editorInfo[ch].epnum];
        if (newpos < 0) newpos = 0;
        if (newpos > len) newpos = len;

        editorInfo.editmode = EDIT_PATTERN;

        if ((mouseb & (MOUSEB_RIGHT | MOUSEB_MIDDLE)) && (!prevmouseb))
        {
          if ((editorInfo.epmarkchn != ch) || (newpos != editorInfo.epmarkend))
          {
            editorInfo.epmarkchn = ch;
            editorInfo.epmarkstart = editorInfo.epmarkend = newpos;
          }
        }

        if (mouseb & MOUSEB_LEFT)
        {
          editorInfo.epchn = ch;
          if (x < 7) editorInfo.epcolumn = 0;
          else
          {
            editorInfo.epcolumn = x - 7;
            if (editorInfo.epcolumn > 5) editorInfo.epcolumn = 5;
            if (editorInfo.epcolumn < 1) editorInfo.epcolumn = 1;
          }
        }

        if (!prevmouseb)
        {
          if (mouseb & MOUSEB_LEFT)
          {
            // Double click on a row starts playback from there
            if ((lastclickx == mousex) && (lastclicky == mousey) && (SDL_GetTicks() - lastclicktime < 400))
              initsongpos(editorInfo.esnum, PLAY_PATTERN, newpos);
            editorInfo.eppos = newpos;
            lastclickx = mousex;
            lastclicky = mousey;
            lastclicktime = SDL_GetTicks();
          }
        }
        else
        {
          if (mouseb & MOUSEB_LEFT)
          {
            if (mousey == PATT_ROWS_Y) editorInfo.eppos--;
            if (mousey == PATT_ROWS_Y + VISIBLEPATTROWS - 1) editorInfo.eppos++;
          }
        }
        if (editorInfo.eppos < 0) editorInfo.eppos = 0;
        if (editorInfo.eppos > pattlen[editorUndoInfo.editorInfo[editorInfo.epchn].epnum]) editorInfo.eppos = pattlen[editorUndoInfo.editorInfo[editorInfo.epchn].epnum];

        if (mouseb & (MOUSEB_RIGHT | MOUSEB_MIDDLE)) editorInfo.epmarkend = newpos;
        return;
      }
    }
    return;
  }

  // Orderlist editpos
  if ((mousex >= ORD_X) && (mousey >= ORD_ROWS_Y) && (mousey < ORD_ROWS_Y + VISIBLEORDERCHNS))
  {
    int newchn = mousey - ORD_ROWS_Y + ordchnview;
    int newpos = editorInfo.esview + (mousex - ORD_X) / 3;
    int newcolumn = (mousex - ORD_X) % 3;
    if (newchn >= numchannels) return;
    if (newcolumn > 1) newcolumn = 1;
    if (newpos < 0)
    {
      newpos = 0;
      newcolumn = 0;
    }
    if (newpos == songlen[editorInfo.esnum][newchn])
    {
      newpos++;
      newcolumn = 0;
    }
    if (newpos > songlen[editorInfo.esnum][newchn] + 1)
    {
      newpos = songlen[editorInfo.esnum][newchn] + 1;
      newcolumn = 1;
    }

    editorInfo.editmode = EDIT_ORDERLIST;

    if ((mouseb & (MOUSEB_RIGHT | MOUSEB_MIDDLE)) && (!prevmouseb) && (newpos < songlen[editorInfo.esnum][newchn]))
    {
      if ((editorInfo.esmarkchn != newchn) || (newpos != editorInfo.esmarkend))
      {
        editorInfo.esmarkchn = newchn;
        editorInfo.esmarkstart = editorInfo.esmarkend = newpos;
      }
    }

    if (mouseb & MOUSEB_LEFT)
    {
      editorInfo.eschn = newchn;
      editorInfo.eseditpos = newpos;
      editorInfo.escolumn = newcolumn;
    }

    if ((mouseb & (MOUSEB_RIGHT | MOUSEB_MIDDLE)) && (newpos < songlen[editorInfo.esnum][newchn])) editorInfo.esmarkend = newpos;
    return;
  }

  // Instrument editpos
  if ((mousey >= INS_ROWS_Y) && (mousey < INS_ROWS_Y + 5))
  {
    if ((mousex >= INS_COL1_X) && (mousex < INS_COL1_X + INS_VAL_OFS + 2))
    {
      editorInfo.editmode = EDIT_INSTRUMENT;
      editorInfo.eipos = mousey - INS_ROWS_Y;
      editorInfo.eicolumn = mousex - INS_COL1_X - INS_VAL_OFS;
      if (editorInfo.eicolumn < 0) editorInfo.eicolumn = 0;
      return;
    }
    if ((mousex >= INS_COL2_X) && (mousex < INS_COL2_X + INS_VAL_OFS + 2) && (mousey < INS_ROWS_Y + 4))
    {
      editorInfo.editmode = EDIT_INSTRUMENT;
      editorInfo.eipos = mousey - INS_ROWS_Y + 5;
      editorInfo.eicolumn = mousex - INS_COL2_X - INS_VAL_OFS;
      if (editorInfo.eicolumn < 0) editorInfo.eicolumn = 0;
      return;
    }
  }
  if ((mousey == INS_Y) && (mousex >= RP_X + 17) && (mousex < RP_X + 34))
  {
    editorInfo.editmode = EDIT_INSTRUMENT;
    editorInfo.eipos = 9;
    return;
  }

  // Table editpos
  for (c = 0; c < MAX_TABLES; c++)
  {
    if ((mousey >= TBL_ROWS_Y) && (mousey < TBL_ROWS_Y + VISIBLETABLEROWS) && (mousex >= TBL_X(c) + 3) && (mousex <= TBL_X(c) + 7))
    {
      int newpos = mousey - TBL_ROWS_Y + editorInfo.etview[c];
      if (newpos < 0) newpos = 0;
      if (newpos >= MAX_TABLELEN) newpos = MAX_TABLELEN - 1;

      editorInfo.editmode = EDIT_TABLES;

      if ((mouseb & (MOUSEB_RIGHT | MOUSEB_MIDDLE)) && (!prevmouseb))
      {
        if ((editorInfo.etmarknum != c) || (newpos != editorInfo.etmarkend))
        {
          editorInfo.etmarknum = c;
          editorInfo.etmarkstart = editorInfo.etmarkend = newpos;
        }
      }
      if (mouseb & MOUSEB_LEFT)
      {
        editorInfo.etnum = c;
        editorInfo.etpos = newpos;
        editorInfo.etcolumn = mousex - TBL_X(c) - 3;
      }
      if (editorInfo.etcolumn >= 2) editorInfo.etcolumn--;
      if (editorInfo.etcolumn > 3) editorInfo.etcolumn = 3;
      if (editorInfo.etpos < 0) editorInfo.etpos = 0;
      if (editorInfo.etpos > MAX_TABLELEN - 1) editorInfo.etpos = MAX_TABLELEN - 1;

      if (mouseb & (MOUSEB_RIGHT | MOUSEB_MIDDLE)) editorInfo.etmarkend = newpos;
      return;
    }
  }

  // Name editpos
  if ((mousey >= NAMES_ROWS_Y) && (mousey < NAMES_ROWS_Y + 3) && (mousex >= RP_X))
  {
    editorInfo.editmode = EDIT_NAMES;
    editorInfo.enpos = mousey - NAMES_ROWS_Y;
    return;
  }

  // Panel headers switch editing mode
  if (click)
  {
    if ((mousey == PATT_Y) && (mousex < PATT_W)) editorInfo.editmode = EDIT_PATTERN;
    if ((mousey == ORD_Y) && (mousex >= RP_X)) editorInfo.editmode = EDIT_ORDERLIST;
    if ((mousey == INS_Y) && (mousex >= RP_X)) editorInfo.editmode = EDIT_INSTRUMENT;
    if ((mousey == TBL_Y) && (mousex >= RP_X)) editorInfo.editmode = EDIT_TABLES;
    if ((mousey == NAMES_Y) && (mousex >= RP_X)) editorInfo.editmode = EDIT_NAMES;
  }
}

void generalcommands(void)
{
  int c;

  switch (key)
  {
  case '?':
  case '-':
    if ((editorInfo.editmode != EDIT_NAMES) && (editorInfo.editmode != EDIT_ORDERLIST))
    {
      if (!((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos == 9))) previnstr();
    }
    break;

  case '+':
  case '_':
    if ((editorInfo.editmode != EDIT_NAMES) && (editorInfo.editmode != EDIT_ORDERLIST))
    {
      if (!((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos >= 9))) nextinstr();
    }
    break;

  case '*':
    if (editorInfo.editmode != EDIT_NAMES)
    {
      if (!((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos >= 9)))
      {
        if (editorInfo.epoctave < 7) editorInfo.epoctave++;
      }
    }
    break;

  case '/':
  case '\'':
    if (editorInfo.editmode != EDIT_NAMES)
    {
      if (!((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos >= 9)))
      {
        if (editorInfo.epoctave > 0) editorInfo.epoctave--;
      }
    }
    break;

  case '<':
    if (((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos != 9)) || (editorInfo.editmode == EDIT_TABLES))
      previnstr();
    break;

  case '>':
    if (((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos != 9)) || (editorInfo.editmode == EDIT_TABLES))
      nextinstr();
    break;

  case ';':
    for (c = 0; c < numchannels; c++)
    {
      if (editorUndoInfo.editorInfo[c].espos) editorUndoInfo.editorInfo[c].espos--;
      if (editorUndoInfo.editorInfo[c].espos < editorInfo.esview)
      {
        editorInfo.esview = editorUndoInfo.editorInfo[c].espos;
        editorInfo.eseditpos = editorUndoInfo.editorInfo[c].espos;
      }
    }
    updateviewtopos();
    rewindsong();
    break;

  case ':':
    for (c = 0; c < numchannels; c++)
    {
      if (editorUndoInfo.editorInfo[c].espos < songlen[editorInfo.esnum][c] - 1)
        editorUndoInfo.editorInfo[c].espos++;
      if (editorUndoInfo.editorInfo[c].espos - editorInfo.esview >= VISIBLEORDERLIST)
      {
        editorInfo.esview = editorUndoInfo.editorInfo[c].espos - VISIBLEORDERLIST + 1;
        editorInfo.eseditpos = editorUndoInfo.editorInfo[c].espos;
      }
    }
    updateviewtopos();
    rewindsong();
    break;
  }
  if (win_quitted)
  {
    win_quitted = 0;
    shiftpressed = 0;
    mouseb = 1;
    quit();
    mouseb = 0;
  }
  switch (rawkey)
  {
  case KEY_ESC:
    if (!shiftpressed)
      quit();
    else
      clear();
    break;

  case KEY_KPMULTIPLY:
    if ((editorInfo.editmode != EDIT_NAMES) && (!key))
    {
      if (!((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos >= 9)))
      {
        if (editorInfo.epoctave < 7) editorInfo.epoctave++;
      }
    }
    break;

  case KEY_KPDIVIDE:
    if ((editorInfo.editmode != EDIT_NAMES) && (!key))
    {
      if (!((editorInfo.editmode == EDIT_INSTRUMENT) && (editorInfo.eipos >= 9)))
      {
        if (editorInfo.epoctave > 0) editorInfo.epoctave--;
      }
    }
    break;

  case KEY_Z:
    // Undo (CTRL/CMD+Z), redo (CTRL/CMD+SHIFT+Z)
    if (ctrlpressed || metapressed)
    {
      if ((win_keystate[KEY_LEFTSHIFT]) || (win_keystate[KEY_RIGHTSHIFT]))
        redoPerform();
      else
        undoPerform();
    }
    break;

  case KEY_Y:
    // Redo (CTRL/CMD+Y)
    if (ctrlpressed || metapressed)
      redoPerform();
    break;



  case KEY_F12:
    onlinehelp(0, shiftpressed);
    break;

  case KEY_TAB:
    if ((ctrlpressed) || (metapressed)) break;
    if (!shiftpressed) editorInfo.editmode++;
    else editorInfo.editmode--;
    if (editorInfo.editmode > EDIT_NAMES) editorInfo.editmode = EDIT_PATTERN;
    if (editorInfo.editmode < EDIT_PATTERN) editorInfo.editmode = EDIT_NAMES;
    break;

  case KEY_F1:
    initsong(editorInfo.esnum, PLAY_BEGINNING);
    followplay = shiftpressed;
    break;

  case KEY_F2:
    initsong(editorInfo.esnum, PLAY_POS);
    followplay = shiftpressed;
    break;

  case KEY_F3:
    initsong(editorInfo.esnum, PLAY_PATTERN);
    followplay = shiftpressed;
    break;

  case KEY_F4:
    if (shiftpressed)
      mutechannel(editorInfo.epchn);
    else
      stopsong();
    break;

  case KEY_F5:
    if (!shiftpressed)
      editorInfo.editmode = EDIT_PATTERN;
    else prevmultiplier();
    break;

  case KEY_F6:
    if (!shiftpressed)
      editorInfo.editmode = EDIT_ORDERLIST;
    else nextmultiplier();
    break;

  case KEY_F7:
    if (!shiftpressed)
    {
      if (editorInfo.editmode == EDIT_INSTRUMENT)
        editorInfo.editmode = EDIT_TABLES;
      else
        editorInfo.editmode = EDIT_INSTRUMENT;
    }
    else editadsr();
    break;

  case KEY_F8:
    if (!shiftpressed)
      editorInfo.editmode = EDIT_NAMES;
    else
    {
      editorInfo.sidmodel ^= 1;
      do_sound_init();
    }
    break;

  case KEY_F9:
    if ((ctrlpressed) || (metapressed))
      exportaudio(shiftpressed);
    else if (!shiftpressed)
      relocator();
    else
      toggle_interpolation_mode();
    break;

  case KEY_F10:
    load();
    break;

  case KEY_F11:
    save();
    break;
  }
}

void load(void)
{
  if ((editorInfo.editmode != EDIT_INSTRUMENT) && (editorInfo.editmode != EDIT_TABLES))
  {
    if (!shiftpressed)
    {
      if ((songmodified) && (!dlg_confirm("Load song", "Discard the changes of the current song?")))
      {
        key = 0;
        rawkey = 0;
        return;
      }
      if (fileselector(songfilename, songpath, songfilter, "LOAD SONG  (.sng = GoatTracker 1/2 SIDs, .ggt = GrooveGoat 4/8 SIDs)", 0))
      {
        stopsong();
        loadsong();
        do_sound_init();
        undoInitAllAreas();
      }
    }
    else
    {
      if (fileselector(songfilename, songpath, songfilter, "MERGE SONG", 0))
      {
        mergesong();
        undoInitAllAreas();
        songmodified = 1;
      }
    }
  }
  else
  {
    if (editorInfo.einum)
    {
      char title[80];
      sprintf(title, "LOAD INSTRUMENT INTO SLOT %02X  (SPACE = preview)", editorInfo.einum);
      if (fileselector(instrfilename, instrpath, instrfilter, title, FILESEL_INSTRUMENT))
      {
        loadinstrument();
        songmodified = 1;
        snprintf(statusmessage, sizeof statusmessage, "Loaded instrument %s into slot %02X", instrfilename, editorInfo.einum);
      }
    }
  }
  key = 0;
  rawkey = 0;
}

// Is the path inside the bundled instrument library?
static int insidelibrary(const char *path)
{
  char a[MAX_PATHNAME], b[MAX_PATHNAME];
  int l;
  if (!instrlibpath[0]) return 0;
#ifdef __WIN32__
  if (!_fullpath(a, path, MAX_PATHNAME)) strcpy(a, path);
  if (!_fullpath(b, instrlibpath, MAX_PATHNAME)) strcpy(b, instrlibpath);
#else
  if (!realpath(path, a)) strcpy(a, path);
  if (!realpath(instrlibpath, b)) strcpy(b, instrlibpath);
#endif
  l = strlen(b);
  while ((l > 0) && ((b[l - 1] == '/') || (b[l - 1] == '\\'))) b[--l] = 0;
  return (l > 0) && (!strncmp(a, b, l)) && ((a[l] == 0) || (a[l] == '/') || (a[l] == '\\'));
}

void save(void)
{
  if ((editorInfo.editmode != EDIT_INSTRUMENT) && (editorInfo.editmode != EDIT_TABLES))
  {
    int done = 0;

    // Repeat until quit or save successful
    while (!done)
    {
      char title[100];
      if (strlen(loadedsongfilename)) strcpy(songfilename, loadedsongfilename);
      if (numsids > 2)
        sprintf(title, "SAVE SONG (%d SIDs: C64 GrooveGoat .ggt format)", numsids);
      else if (numsids == 2)
        strcpy(title, "SAVE SONG (2 SIDs: GoatTracker Stereo compatible .sng)");
      else
        strcpy(title, "SAVE SONG (GoatTracker 2 compatible .sng)");
      if (fileselector(songfilename, songpath, songfilter, title, 3))
        done = savesong();
      else done = 1;
    }
  }
  else
  {
    if (editorInfo.einum)
    {
      int done = 0;
      int useinstrname = 0;
      char tempfilename[MAX_FILENAME];

      // Repeat until quit or save successful
      while (!done)
      {
        if ((!strlen(instrfilename)) && (strlen(instr[editorInfo.einum].name)))
        {
          useinstrname = 1;
          strcpy(instrfilename, instr[editorInfo.einum].name);
          strcat(instrfilename, ".ins");
          strcpy(tempfilename, instrfilename);
        }

        // Never save into the bundled instrument library (it may be read-only,
        // and inside the macOS app bundle it would break the signature)
        static char userinstrpath[MAX_PATHNAME];
        if (!userinstrpath[0]) strcpy(userinstrpath, songpath);
        if (!insidelibrary(instrpath)) strcpy(userinstrpath, instrpath);
        if (fileselector(instrfilename, userinstrpath, instrfilter, "SAVE INSTRUMENT", 3))
        {
          done = saveinstrument();
          if (done) snprintf(statusmessage, sizeof statusmessage, "Saved instrument %s", instrfilename);
        }
        else done = 1;

        if (useinstrname)
        {
          if (!strcmp(tempfilename, instrfilename))
            memset(instrfilename, 0, sizeof instrfilename);
        }
      }
    }
  }
  key = 0;
  rawkey = 0;
}

void quit(void)
{
  if ((!shiftpressed) || (mouseb))
  {
    const char *lines[2];
    const char *opts[3] = {"Quit", "Cancel", "Save"};
    int n = 1;
    int r;
    lines[0] = "Really quit C64 GrooveGoat?";
    if (songmodified)
    {
      lines[1] = "The song has unsaved changes.";
      n = 2;
    }
    r = dlg_choose("Quit", lines, n, opts, songmodified ? 3 : 2, 1);
    if (r == 0) exitprogram = 1;
    if (r == 2)
    {
      int oldmode = editorInfo.editmode;
      editorInfo.editmode = EDIT_PATTERN;
      save();
      editorInfo.editmode = oldmode;
      if (!songmodified) exitprogram = 1;
    }
  }
  key = 0;
  rawkey = 0;
}

void clear(void)
{
  int cs = 0;
  int cp = 0;
  int ci = 0;
  int ct = 0;
  int cn = 0;

  if (dlg_confirm("Optimize", "Optimize everything (remove unused patterns, instruments, table data)?"))
  {
    optimizeeverything(1, 1);
    songmodified = 1;
    key = 0;
    rawkey = 0;
    return;
  }

  cs = dlg_confirm("Clear", "Clear orderlists?");
  cp = dlg_confirm("Clear", "Clear patterns?");
  ci = dlg_confirm("Clear", "Clear instruments?");
  ct = dlg_confirm("Clear", "Clear tables?");
  cn = dlg_confirm("Clear", "Clear song name, author and release info?");

  if (cp == 1)
  {
    unsigned len = defaultpatternlength;
    // Ask for the pattern length (decimal shown as hex input for simplicity)
    if (dlg_edithex("Clear patterns", "Default pattern length (hex):", &len, 2))
    {
      if (len < 1) len = 1;
      if (len > MAX_PATTROWS) len = MAX_PATTROWS;
      defaultpatternlength = len;
    }
  }

  if (cs | cp | ci | ct | cn)
  {
    memset(songfilename, 0, sizeof songfilename);
    songmodified = 1;
  }
  clearsong(cs, cp, ci, ct, cn);
  if (cs) clearinactivechannels();

  key = 0;
  rawkey = 0;
}

void editadsr(void)
{
  eamode = 1;
  editorInfo.eacolumn = 0;

  for (;;)
  {
    waitkeymouse();

    if (win_quitted)
    {
      exitprogram = 1;
      key = 0;
      rawkey = 0;
      eamode = 0;
      return;
    }

    if (hexnybble >= 0)
    {
      undoCreateEditorInfoBackup();

      switch (editorInfo.eacolumn)
      {
      case 0:
        editorInfo.adparam &= 0x0fff;
        editorInfo.adparam |= hexnybble << 12;
        break;

      case 1:
        editorInfo.adparam &= 0xf0ff;
        editorInfo.adparam |= hexnybble << 8;
        break;

      case 2:
        editorInfo.adparam &= 0xff0f;
        editorInfo.adparam |= hexnybble << 4;
        break;

      case 3:
        editorInfo.adparam &= 0xfff0;
        editorInfo.adparam |= hexnybble;
        break;
      }
      editorInfo.eacolumn++;
      undoAddEditorSettingsToList();
    }

    switch (rawkey)
    {
    case KEY_F7:
      if (!shiftpressed) break;
      // fallthrough
    case KEY_ESC:
    case KEY_ENTER:
    case KEY_TAB:
      eamode = 0;
      key = 0;
      rawkey = 0;
      return;

    case KEY_BACKSPACE:
      if (!editorInfo.eacolumn) break;
      // fallthrough
    case KEY_LEFT:
      editorInfo.eacolumn--;
      break;

    case KEY_RIGHT:
      editorInfo.eacolumn++;
    }
    editorInfo.eacolumn &= 3;

    if ((mouseb) && (!prevmouseb))
    {
      eamode = 0;
      return;
    }
  }
}

void getparam(FILE *handle, unsigned *value)
{
  char *configptr;

  for (;;)
  {
    if (feof(handle)) return;
    if (!fgets(configbuf, MAX_PATHNAME, handle)) return;
    if ((configbuf[0]) && (configbuf[0] != ';') && (configbuf[0] != ' ') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
  }

  configptr = configbuf;
  if (*configptr == '$')
  {
    *value = 0;
    configptr++;
    for (;;)
    {
      char c = tolower(*configptr++);
      int h = -1;

      if ((c >= 'a') && (c <= 'f')) h = c - 'a' + 10;
      if ((c >= '0') && (c <= '9')) h = c - '0';

      if (h >= 0)
      {
        *value *= 16;
        *value += h;
      }
      else break;
    }
  }
  else
  {
    *value = 0;
    for (;;)
    {
      char c = tolower(*configptr++);
      int d = -1;

      if ((c >= '0') && (c <= '9')) d = c - '0';

      if (d >= 0)
      {
        *value *= 10;
        *value += d;
      }
      else break;
    }
  }
}

void getfloatparam(FILE *handle, float *value)
{
  char *configptr;

  for (;;)
  {
    if (feof(handle)) return;
    if (!fgets(configbuf, MAX_PATHNAME, handle)) return;
    if ((configbuf[0]) && (configbuf[0] != ';') && (configbuf[0] != ' ') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
  }

  configptr = configbuf;
  *value = 0.0f;
  sscanf(configptr, "%f", value);
}

void getstringparam(FILE *handle, char *value)
{
  char *configptr;

  for (;;)
  {
    if (feof(handle)) return;
    if (!fgets(configbuf, MAX_PATHNAME, handle)) return;
    if ((configbuf[0]) && (configbuf[0] != ';') && (configbuf[0] != ' ') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
  }

  configptr = configbuf;

  sscanf(configptr, "%s", value);
}

void prevmultiplier(void)
{
  if (editorInfo.multiplier > 0)
  {
    editorInfo.multiplier--;
    do_sound_init();
  }
}

void nextmultiplier(void)
{
  if (editorInfo.multiplier < 16)
  {
    editorInfo.multiplier++;
    do_sound_init();
  }
}

void calculatefreqtable()
{
  double basefreq = (double)basepitch * (16777216.0 / 985248.0) * pow(2.0, 0.25) / 32.0;
  double cyclebasefreq = basefreq;
  double freq = basefreq;
  int c;
  int i;

  if (tuningcount)
  {
    c = 0;
    while (c < 96)
    {
      for (i = 0; i < tuningcount; i++)
      {
        if (c < 96)
        {
          int intfreq = freq + 0.5;
          if (intfreq > 0xffff)
            intfreq = 0xffff;
          freqtbllo[c] = intfreq & 0xff;
          freqtblhi[c] = intfreq >> 8;
          freq = cyclebasefreq * tuning[i];
          c++;
        }
      }
      cyclebasefreq = freq;
    }
  }
  else
  {
    for (c = 0; c < 8 * 12; c++)
    {
      double note = c;
      double freq = basefreq * pow(2.0, note / (double)equaldivisionsperoctave);
      int intfreq = freq + 0.5;
      if (intfreq > 0xffff)
        intfreq = 0xffff;
      freqtbllo[c] = intfreq & 0xff;
      freqtblhi[c] = intfreq >> 8;
    }
  }
}

void setspecialnotenames()
{
  int i;
  int j;
  int oct;
  char *name;
  char octave[11];

  i = 0;
  oct = 0;
  while (i < 93)
  {
    for (j = 0; j < 186; j += 2)
    {
      if (specialnotenames[j] == '\0')
        break;
      if (i < 93)
      {
        name = malloc(4);
        strncpy(name, specialnotenames + j, 2);
        sprintf(octave, "%d", oct);
        strcpy(name + 2, octave);
        notename[i] = name;
        i++;
      }
    }
    oct++;
  }
}

void readscalatuningfile()
{
  FILE *scalatuningfile;
  char *configptr;
  char strbuf[64];
  char name[3];
  int i;
  double numerator;
  double denominator;
  double centvalue;

  scalatuningfile = fopen(scalatuningfilepath, "rt");
  if (scalatuningfile)
  {
    // Tuning name
    for (;;)
    {
      if (feof(scalatuningfile)) return;
      if (!fgets(configbuf, MAX_PATHNAME, scalatuningfile)) return;
      if ((configbuf[0]) && (configbuf[0] != '!') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
    }
    configptr = configbuf;
    sscanf(configptr, "%63[^\t\n]", tuningname);

    // Tuning count
    for (;;)
    {
      if (feof(scalatuningfile)) return;
      if (!fgets(configbuf, MAX_PATHNAME, scalatuningfile)) return;
      if ((configbuf[0]) && (configbuf[0] != '!') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
    }
    configptr = configbuf;
    sscanf(configptr, "%d", &tuningcount);
    if (tuningcount > 96) tuningcount = 96;

    // Tunings
    for (i = 0; i < tuningcount; i++)
    {
      for (;;)
      {
        if (feof(scalatuningfile)) return;
        if (!fgets(configbuf, MAX_PATHNAME, scalatuningfile)) return;
        if ((configbuf[0]) && (configbuf[0] != '!') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
      }
      configptr = configbuf;
      name[0] = '\0';
      sscanf(configptr, "%63s %2s", strbuf, name);
      if (!i)
      {
        strcpy(specialnotenames, name);
      }
      else
      {
        if (i == tuningcount - 1)
        {
          char *tmp = strdup(specialnotenames);
          strcpy(specialnotenames, name);
          strcat(specialnotenames, tmp);
          free(tmp);
        }
        else
        {
          strcat(specialnotenames, name);
        }
      }
      if (!strchr(strbuf, '.'))
      {
        sscanf(strbuf, "%lf", &numerator);
        if (strchr(strbuf, '/'))
        {
          sscanf(strchr(strbuf, '/') + 1, "%lf", &denominator);
          tuning[i] = numerator / denominator;
        }
      }
      else
      {
        sscanf(configptr, "%lf", &centvalue);
        tuning[i] = pow(2.0, centvalue / 1200.0);
      }
    }
    fclose(scalatuningfile);
  }
}
