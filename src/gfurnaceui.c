//
// C64 GrooveGoat - the Furnace export in the editor (the conversion itself is in gfurnace.c)
//

#define GFURNACEUI_C

#include "goattrk2.h"
#include "gfurnace.h"
#include "gdialog.h"
#include <sys/stat.h>
#include <errno.h>

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

void furnaceexport(void)
{
  static const char *lines[] = {
    "Saves the instrument as a Furnace C64 instrument (.fui):",
    "ADSR, waveform, ring/sync, pitch steps, pulse width and filter.",
    "Vibrato and commands in the wavetable are not carried over."
  };
  static const char *options[] = {"This instrument", "All instruments", "Cancel"};
  char name[MAX_FILENAME], path[MAX_PATHNAME], filter[8], text[96], warn[160];
  int choice, warnings = 0, written = 0, hidden;
  int num = editorInfo.einum ? editorInfo.einum : 1;

  choice = dlg_choose("EXPORT TO FURNACE", lines, 3, options, 3, 0);
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
