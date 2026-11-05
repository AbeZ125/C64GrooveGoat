//
// C64 GrooveGoat - the Furnace export in the editor (the conversion itself is in gfurnace.c)
//

#define GFURNACEUI_C

#include "goattrk2.h"
#include "gfurnace.h"
#include "gfurnaceimp.h"
#include "gfurnacesong.h"
#include "gstepedit.h"
#include "gdialog.h"
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

static char furnacepath[MAX_PATHNAME];
static char writeerror[96];          // why the last file could not be written ("" = nothing went wrong)
static char failedfile[MAX_FILENAME + 8];

// A slot that was never used: a new instrument has the default first waveform and gate timer, but no name, no
// envelope and no table programs
static int instrument_is_empty(int n)
{
  const INSTR *in = &instr[n];
  int c;
  for (c = 0; c < MAX_TABLES; c++)
    if (in->ptr[c]) return 0;
  return !(in->name[0] || in->ad || in->sr);
}

// Converts instrument n and writes it to "file". Returns 1 if written; warnings gets the FURNACE_W_ bits.
static int writeinstrument(int n, const char *file, int *warnings)
{
  static unsigned char buf[FURNACE_MAXFILE];
  FURNACEINFO info;
  FILE *fp;
  INSTR copy = instr[n];
  int size;
  if (!copy.name[0]) snprintf(copy.name, sizeof copy.name, "Instr %02X", n);
  size = furnace_instrument(buf, sizeof buf, &copy, ltable[WTBL], rtable[WTBL], ltable[PTBL], rtable[PTBL],
                          ltable[FTBL], rtable[FTBL], &info);
  if (!size) { snprintf(writeerror, sizeof writeerror, "the instrument did not fit in a Furnace file"); snprintf(failedfile, sizeof failedfile, "%s", file); return 0; }
  errno = 0;
  fp = fopen(file, "wb");
  if (!fp)
  {
    snprintf(writeerror, sizeof writeerror, "%s", errno ? strerror(errno) : "could not open the file");
    snprintf(failedfile, sizeof failedfile, "%s", file);
    return 0;
  }
  if (fwrite(buf, size, 1, fp) != 1)
  {
    snprintf(writeerror, sizeof writeerror, "%s", errno ? strerror(errno) : "could not write the file");
    snprintf(failedfile, sizeof failedfile, "%s", file);
    fclose(fp);
    return 0;
  }
  if (fclose(fp))
  {
    snprintf(writeerror, sizeof writeerror, "%s", errno ? strerror(errno) : "could not close the file");
    snprintf(failedfile, sizeof failedfile, "%s", file);
    return 0;
  }
  if (warnings) *warnings |= info.warnings;
  return 1;
}

static int isfolder(const char *path)
{
  struct stat st;
  return (!stat(path, &st)) && (st.st_mode & S_IFDIR);
}

static int exists(const char *file)
{
  struct stat st;
  return !stat(file, &st);
}

// Writes every used instrument into the current folder: "NN name.fui", or "Instr NN.fui" for one without a name.
// Returns how many; first gets the name of the first file.
static int exportfailed;            // how many instruments the last export-all could not write

static int exportall(int *warnings, char *first, int firstsize)
{
  int c, written = 0, failed = 0;
  writeerror[0] = 0;
  for (c = 1; c < MAX_INSTR; c++)
  {
    char base[MAX_FILENAME], file[MAX_FILENAME + 8];
    if (instrument_is_empty(c)) continue;
    furnace_filename(base, sizeof base, &instr[c], c);
    if (instr[c].name[0]) snprintf(file, sizeof file, "%02X %s", c, base);
    else snprintf(file, sizeof file, "%s", base);
    if (writeinstrument(c, file, warnings))
    {
      if (!written) snprintf(first, firstsize, "%s", file);
      written++;
    }
    else failed++;
  }
  exportfailed = failed;
  return written;
}

// For the scripted tests (GG_SCREEN=furnaceall GG_FUIDIR=folder): all instruments into a folder, no dialogs
int furnaceexportall_to(const char *dir)
{
  char first[MAX_FILENAME];
  int warnings = 0;
  if (chdir(dir)) return -1;
  return exportall(&warnings, first, sizeof first);
}


// The loaded song (the subsong being edited) as a Furnace module file. Returns the bytes written, 0 on failure (msg says
// why). warnings gets the FSONG_W_... bits, instrwarnings the FURNACE_W_... bits.
int furnacesong_to(const char *path, char *msg, int msgsize, int *warnings, int *instrwarnings)
{
  static FSONGIN in;
  static FSONGINFO info;
  unsigned char *buf = (unsigned char *)malloc(FSONG_MAXFILE);
  FILE *f;
  int c, sub = editorInfo.esnum;
  int size;

  if (warnings) *warnings = 0;
  if (instrwarnings) *instrwarnings = 0;
  if (!buf) { snprintf(msg, msgsize, "Out of memory"); return 0; }
  memset(&in, 0, sizeof in);
  for (c = 0; c < MAX_CHN; c++)
  {
    in.order[c] = songorder[sub][c];
    in.songlen[c] = songlen[sub][c];
  }
  in.pattern = pattern;
  in.pattlen = pattlen;
  in.numsids = numsids;
  in.ntsc = editorInfo.ntsc;
  in.sidmodel = editorInfo.sidmodel;
  in.multiplier = editorInfo.multiplier;
  if (in.multiplier < 1) in.multiplier = 1;
  in.instr = instr;
  in.ltable = &ltable[0][0];
  in.rtable = &rtable[0][0];
  in.name = songname;
  in.author = authorname;
  size = furnace_song(buf, FSONG_MAXFILE, &in, &info);
  if (!size)
  {
    snprintf(msg, msgsize, "%s", info.error[0] ? info.error : "The song could not be exported");
    free(buf);
    return 0;
  }
  f = fopen(path, "wb");
  if (!f || fwrite(buf, 1, size, f) != (size_t)size)
  {
    snprintf(msg, msgsize, "Could not write %s: %s", path, strerror(errno));
    if (f) fclose(f);
    free(buf);
    return 0;
  }
  fclose(f);
  free(buf);
  if (warnings) *warnings = info.warnings;
  if (instrwarnings) *instrwarnings = info.instrwarnings;
  snprintf(msg, msgsize, "%d instruments, %d patterns, %d orders of %d rows", info.instruments, info.patterns, info.orders, info.patternlen);
  return size;
}

// A message the person cannot miss: which folder, which file, and why
static void showfailure(const char *folder, int count)
{
  char l1[200], l2[200], l3[200];
  const char *lines[3];
  if (count > 1) snprintf(l1, sizeof l1, "%d instruments could not be written. The first one:", count);
  else snprintf(l1, sizeof l1, "The file could not be written:");
  snprintf(l2, sizeof l2, "%s%s%s", folder, (folder[0] && folder[strlen(folder) - 1] != '/' && folder[strlen(folder) - 1] != '\\') ? "/" : "", failedfile);
  snprintf(l3, sizeof l3, "Reason: %s. Try another folder.", writeerror[0] ? writeerror : "unknown");
  lines[0] = l1; lines[1] = l2; lines[2] = l3;
  dlg_message("EXPORT TO FURNACE", lines, 3);
}

// The row where an identical program is in the table already, or 0. Instruments share programs in GoatTracker (a pulse
// sweep for a whole set of sounds), so an import that finds its program there uses it instead of adding a copy.
static int makerows(int tab, const FURNACEIMPORT *imp, unsigned char *gl, unsigned char *gr, int start)
{
  if (tab == FTBL) return furnace_filterrows(imp, gl, gr, start);
  return step_generate(tab == WTBL ? &imp->wave : &imp->pulse, gl, gr, start);
}

static int findsame(int tab, const FURNACEIMPORT *imp, int rows)
{
  unsigned char gl[300], gr[300];
  int s, len = gettablelen(tab);
  for (s = 1; s + rows - 1 <= len; s++)
  {
    int n = makerows(tab, imp, gl, gr, s);
    if (n == rows && !memcmp(gl, &ltable[tab][s - 1], rows) && !memcmp(gr, &rtable[tab][s - 1], rows)) return s;
  }
  return 0;
}

// Reads a Furnace C64 instrument file into slot num: the ADSR and first waveform, and the wave, pulse and filter
// programs as table rows after the rows that are in use. Nothing is changed if the file cannot be read or the rows do
// not fit. Returns 1 if done; *warnings gets the FIMP_W_ bits.
static int importinstrument(const char *file, int num, int *warnings, char *error, int errsize)
{
  static FURNACEIMPORT imp;
  static unsigned char data[FURNACE_MAXFILE * 8];
  unsigned char gl[3][300], gr[3][300];
  int rows[3] = {0, 0, 0}, start[3] = {0, 0, 0}, shared[3] = {0, 0, 0}, tab, size, r;
  FILE *fp = fopen(file, "rb");
  if (!fp) { snprintf(error, errsize, "Could not open %s", file); return 0; }
  size = (int)fread(data, 1, sizeof data, fp);
  fclose(fp);
  r = furnace_import(data, size, &imp);
  if (r == FIMP_NOTFUI) { snprintf(error, errsize, "That is not a Furnace instrument file (.fui)"); return 0; }
  if (r == FIMP_NOTC64) { snprintf(error, errsize, "That Furnace instrument is for another chip: only C64 instruments can be imported"); return 0; }
  if (r != FIMP_OK) { snprintf(error, errsize, "The Furnace file is damaged"); return 0; }

  // the rows go after the rows other instruments use, so the old programs of the slot are freed first; if the
  // new ones do not fit, the snapshot (the one Load Instrument uses when a file does not fit) is put back
  stopsong();
  snapshotsongdata(1);
  deleteinstrtable(num);
  for (tab = WTBL; tab <= FTBL; tab++)
  {
    int has = tab == WTBL ? imp.haswave : (tab == PTBL ? imp.haspulse : imp.hasfilter);
    if (!has) continue;
    start[tab] = gettablelen(tab) + 1;
    rows[tab] = makerows(tab, &imp, gl[tab], gr[tab], start[tab]);
    shared[tab] = findsame(tab, &imp, rows[tab]);
    if (shared[tab]) { start[tab] = shared[tab]; continue; }
    if (start[tab] - 1 + rows[tab] > MAX_TABLELEN)
    {
      static const char *names[3] = {"wave", "pulse", "filter"};
      snapshotsongdata(0);                 // put everything back
      snprintf(error, errsize, "Not enough free rows in the %s table (%d needed): SHIFT+O there frees unused rows", names[tab], rows[tab]);
      return 0;
    }
  }
  clearinstr(num);
  instr[num].ad = (unsigned char)imp.ad;
  instr[num].sr = (unsigned char)imp.sr;
  instr[num].firstwave = (unsigned char)imp.firstwave;
  snprintf(instr[num].name, sizeof instr[num].name, "%s", imp.name);
  for (tab = WTBL; tab <= FTBL; tab++)
  {
    if (!rows[tab]) continue;
    if (!shared[tab])
    {
      memcpy(&ltable[tab][start[tab] - 1], gl[tab], rows[tab]);
      memcpy(&rtable[tab][start[tab] - 1], gr[tab], rows[tab]);
    }
    instr[num].ptr[tab] = (unsigned char)start[tab];
  }
  if (!instr[num].name[0])
  {
    // no name in the file: the file name without the folder and the extension
    const char *base = strrchr(file, '/'), *bs = strrchr(file, '\\');
    int i = 0;
    if (bs > base) base = bs;
    base = base ? base + 1 : file;
    while (base[i] && base[i] != '.' && i < MAX_INSTRNAMELEN - 1) { instr[num].name[i] = base[i]; i++; }
    instr[num].name[i] = 0;
  }
  *warnings = imp.warnings;
  return 1;
}

#include <dirent.h>

static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

// For the scripted tests (GG_SCREEN=furnaceroundtrip GG_FUIDIR=folder): exports every instrument to the folder,
// imports each file into a free slot from 0x20 on, exports those slots again and counts the files that differ.
// Prints "roundtrip: N files, M differ".
int furnaceroundtrip(const char *dir)
{
  static unsigned char a[FURNACE_MAXFILE], b[FURNACE_MAXFILE];
  char *names[64];
  struct dirent *de;
  DIR *d;
  char first[MAX_FILENAME];
  int warnings = 0, n = 0, i, differ = 0, slot = 0x20;
  if (chdir(dir)) return -1;
  if (!getenv("GG_FUIONLY")) exportall(&warnings, first, sizeof first);   // GG_FUIONLY: the files are in the folder already
  d = opendir(".");
  if (!d) return -1;
  while ((de = readdir(d)) && n < 64)
    if (strstr(de->d_name, ".fui")) names[n++] = strdup(de->d_name);
  closedir(d);
  qsort(names, n, sizeof names[0], cmpstr);
  for (i = 0; i < n; i++)
  {
    char error[160];
    FILE *fp;
    int size, w = 0, written;
    FURNACEINFO info;
    INSTR copy;
    while (slot < MAX_INSTR && !instrument_is_empty(slot)) slot++;
    if (slot >= MAX_INSTR) break;
    if (!importinstrument(names[i], slot, &w, error, sizeof error)) { printf("import failed: %s: %s\n", names[i], error); differ++; continue; }
    fp = fopen(names[i], "rb");
    size = fp ? (int)fread(a, 1, sizeof a, fp) : 0;
    if (fp) fclose(fp);
    copy = instr[slot];
    written = furnace_instrument(b, sizeof b, &copy, ltable[WTBL], rtable[WTBL], ltable[PTBL], rtable[PTBL], ltable[FTBL], rtable[FTBL], &info);
    if (written != size || memcmp(a, b, size))
    {
      if (!(w & (FIMP_W_TOOLONG | FIMP_W_TICK0))) differ++;   // a warned loss is expected
      printf("%s: %d vs %d bytes (warned: %02x)\n", names[i], size, written, w);
    }
    slot++;
  }
  printf("roundtrip: %d files, %d differ without a warning\n", n, differ);
  return differ;
}

static void furnaceimport(void)
{
  char name[MAX_FILENAME], path[MAX_PATHNAME], filter[8], title[80], error[160], warn[160], msg[sizeof statusmessage];
  int warnings = 0, num = editorInfo.einum ? editorInfo.einum : 1;
  const char *home = getenv("HOME");
  if (!home) home = getenv("USERPROFILE");
  name[0] = 0;
  strcpy(filter, "*.fui");
  if (!furnacepath[0]) snprintf(furnacepath, MAX_PATHNAME, "%s/Documents", home ? home : ".");
  snprintf(path, MAX_PATHNAME, "%s", furnacepath);
  snprintf(title, sizeof title, "IMPORT A FURNACE INSTRUMENT (.fui) INTO SLOT %02X", num);
  if (!fileselector(name, path, filter, title, 0)) return;
  if (!getcwd(furnacepath, MAX_PATHNAME)) return;
  if (!importinstrument(name, num, &warnings, error, sizeof error))
  {
    const char *lines[3] = {"IMPORT FROM FURNACE", "", error};
    dlg_message("IMPORT FROM FURNACE", lines, 3);
    return;
  }
  editorInfo.einum = num;
  songmodified = 1;
  furnace_importwarningtext(warn, sizeof warn, warnings);
  snprintf(msg, sizeof msg, "Imported %s into slot %02X", name, num);
  if (warn[0] && strlen(msg) + strlen(warn) + 22 < sizeof msg) snprintf(statusmessage, sizeof statusmessage, "%s. Not carried over: %s", msg, warn);
  else setstatusmessage(msg);
  showinstrtable();
}

// Export of the whole song as a Furnace module (.fur)
static void exportsong(void)
{
  char name[MAX_FILENAME], path[MAX_PATHNAME], filter[8], msg[200], warn[160], iwarn[160], line[200], file[MAX_FILENAME + 8];
  int warnings = 0, iwarnings = 0, size, i, n = 0;

  for (i = 0; songname[i] && n < 40; i++)
    if (isalnum((unsigned char)songname[i]) || songname[i] == ' ' || songname[i] == '-' || songname[i] == '_') name[n++] = songname[i];
  name[n] = 0;
  if (!name[0]) snprintf(name, sizeof name, "song");
  strcpy(filter, "*.fur");
  if (!furnacepath[0]) snprintf(furnacepath, MAX_PATHNAME, "%s", songpath);
  snprintf(path, MAX_PATHNAME, "%s", furnacepath);
  if (!fileselector(name, path, filter, "EXPORT SONG TO FURNACE (.fur)", 3)) return;
  if (!getcwd(furnacepath, MAX_PATHNAME)) return;
  snprintf(file, sizeof file, "%s", name);
  if (!strchr(file, '.')) strcat(file, ".fur");
  if (exists(file) && !dlg_confirm("EXPORT TO FURNACE", "The file exists. Replace it?")) return;
  size = furnacesong_to(file, msg, sizeof msg, &warnings, &iwarnings);
  if (!size)
  {
    const char *lines[3];
    snprintf(line, sizeof line, "%s%s%s", furnacepath, "/", file);
    lines[0] = "The song could not be exported:";
    lines[1] = line;
    lines[2] = msg;
    dlg_message("EXPORT TO FURNACE", lines, 3);
    return;
  }
  if (!exists(file))
  {
    const char *lines[2];
    snprintf(line, sizeof line, "%s/%s", furnacepath, file);
    lines[0] = "The file is not in the folder after writing it:";
    lines[1] = line;
    dlg_message("EXPORT TO FURNACE", lines, 2);
    return;
  }
  furnace_songwarningtext(warn, sizeof warn, warnings);
  furnace_warningtext(iwarn, sizeof iwarn, iwarnings);
  {
    char out[sizeof statusmessage];
    snprintf(out, sizeof out, "Exported the song to %s in %s (%s)", file, furnacepath, msg);
    if ((warn[0] || iwarn[0]) && strlen(out) + strlen(warn) + strlen(iwarn) + 40 < sizeof out)
      snprintf(out + strlen(out), sizeof out - strlen(out), ". Not carried over: %s%s%s", warn, (warn[0] && iwarn[0]) ? ", " : "", iwarn);
    setstatusmessage(out);
  }
}

void furnaceexport(void)
{
  static const char *lines[] = {
    "Saves the instrument as a Furnace C64 instrument (.fui):",
    "ADSR, waveform, ring/sync, pitch steps, pulse width and filter.",
    "Vibrato and commands in the wavetable are not carried over. The song goes out as a .fur."
  };
  static const char *options[] = {"Export this instrument", "Export all instruments", "Import a .fui", "Export the song (.fur)", "Cancel"};
  char name[MAX_FILENAME], path[MAX_PATHNAME], filter[8], text[96], warn[160];
  int choice, warnings = 0, written = 0, hidden;
  int num = editorInfo.einum ? editorInfo.einum : 1;

  choice = dlg_choose("FURNACE", lines, 3, options, 5, 0);
  if (choice == 2) { furnaceimport(); return; }
  if (choice == 3) { exportsong(); return; }
  if ((choice != 0) && (choice != 1)) return;

  if (choice == 0)
  {
    if (instrument_is_empty(num))
    {
      setstatusmessage("This instrument is empty, nothing to export");
      return;
    }
    furnace_filename(name, sizeof name, &instr[num], num);
  }
  else name[0] = 0;
  strcpy(filter, "*.fui");
  if (!furnacepath[0])
  {
    // a folder that Furnace's file dialog shows: not the (hidden) settings folder or the instrument library
    const char *home = getenv("HOME");
    char docs[MAX_PATHNAME];
    if (!home) home = getenv("USERPROFILE");
    snprintf(docs, sizeof docs, "%s/Documents", home ? home : ".");
    snprintf(furnacepath, MAX_PATHNAME, "%s", (home && isfolder(docs)) ? docs : (home ? home : songpath));
  }
  snprintf(path, MAX_PATHNAME, "%s", furnacepath);
  if (!fileselector(name, path, filter,
                    choice == 0 ? "EXPORT TO FURNACE (.fui)" : "EXPORT ALL TO FURNACE: open the folder for the files", 3))
    return;
  if (!getcwd(furnacepath, MAX_PATHNAME)) return;

  if (choice == 0)
  {
    char file[MAX_FILENAME + 8];
    snprintf(file, sizeof file, "%s", name);
    if (!strchr(file, '.')) strcat(file, ".fui");
    if (exists(file) && !dlg_confirm("EXPORT TO FURNACE", "The file exists. Replace it?")) return;
    writeerror[0] = 0;
    if (!writeinstrument(num, file, &warnings))
    {
      showfailure(furnacepath, 0);
      return;
    }
    written = 1;
    snprintf(name, sizeof name, "%s", file);
  }
  else
  {
    written = exportall(&warnings, name, sizeof name);
    if (!written)
    {
      if (exportfailed) showfailure(furnacepath, exportfailed);
      else setstatusmessage("No instruments to export");
      return;
    }
    if (exportfailed)
    {
      showfailure(furnacepath, exportfailed);
      return;
    }
  }
  // make sure the file is really there before saying so
  if (!exists(name))
  {
    snprintf(writeerror, sizeof writeerror, "the file is not in the folder after writing it");
    snprintf(failedfile, sizeof failedfile, "%s", name);
    showfailure(furnacepath, 1);
    return;
  }

  hidden = (strstr(furnacepath, "/.") != NULL) || (strstr(furnacepath, "\\.") != NULL);
  furnace_warningtext(warn, sizeof warn, warnings);
  if (choice == 0) snprintf(text, sizeof text, "Exported %s to Furnace", name);
  else snprintf(text, sizeof text, "Exported %d instruments to Furnace (%s ...)", written, name);
  {
    char msg[sizeof statusmessage];
    snprintf(msg, sizeof msg, "%s in %s%s", text, furnacepath, hidden ? " (a hidden folder, Furnace's file dialog may not show it)" : "");
    if (warn[0] && strlen(msg) + strlen(warn) + 22 < sizeof msg)
      snprintf(statusmessage, sizeof statusmessage, "%s. Not carried over: %s", msg, warn);
    else setstatusmessage(msg);
  }
}
