//
// C64 GrooveGoat - the Furnace export in the editor (the conversion itself is in gfurnace.c)
//

#define GFURNACEUI_C

#include "goattrk2.h"
#include "gfurnace.h"
#include "gdialog.h"
#include <sys/stat.h>

static char furnacepath[MAX_PATHNAME];

static int instrument_is_empty(int n)
{
  const INSTR *in = &instr[n];
  int c;
  for (c = 0; c < MAX_TABLES; c++)
    if (in->ptr[c]) return 0;
  return !(in->name[0] || in->ad || in->sr || in->firstwave);
}

// Converts instrument n and writes it to "file". Returns 1 if written; warnings gets the FURNACE_W_ bits.
static int writeinstrument(int n, const char *file, int *warnings)
{
  static unsigned char buf[FURNACE_MAXFILE];
  FURNACEINFO info;
  FILE *fp;
  int size = furnace_instrument(buf, sizeof buf, &instr[n], ltable[WTBL], rtable[WTBL], ltable[PTBL], rtable[PTBL],
                                ltable[FTBL], rtable[FTBL], &info);
  if (!size) return 0;
  fp = fopen(file, "wb");
  if (!fp) return 0;
  if (fwrite(buf, size, 1, fp) != 1) { fclose(fp); return 0; }
  fclose(fp);
  if (warnings) *warnings |= info.warnings;
  return 1;
}

static int exists(const char *file)
{
  struct stat st;
  return !stat(file, &st);
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
  int choice, warnings = 0, written = 0, c, first = 0;
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
  if (!furnacepath[0]) snprintf(furnacepath, MAX_PATHNAME, "%s", strlen(instrpath) ? instrpath : songpath);
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
    if (!writeinstrument(num, file, &warnings))
    {
      setstatusmessage("Could not write the Furnace file");
      return;
    }
    written = 1;
    snprintf(name, sizeof name, "%s", file);
  }
  else
  {
    for (c = 1; c < MAX_INSTR; c++)
    {
      char base[MAX_FILENAME], file[MAX_FILENAME + 8];
      if (instrument_is_empty(c)) continue;
      furnace_filename(base, sizeof base, &instr[c], c);
      snprintf(file, sizeof file, "%02X %s", c, base);
      if (writeinstrument(c, file, &warnings))
      {
        if (!written) snprintf(name, sizeof name, "%s", file);
        written++;
      }
    }
    (void)first;
    if (!written)
    {
      setstatusmessage("No instruments to export");
      return;
    }
  }

  furnace_warningtext(warn, sizeof warn, warnings);
  if (choice == 0) snprintf(text, sizeof text, "Exported %s to Furnace", name);
  else snprintf(text, sizeof text, "Exported %d instruments to Furnace (%s ...)", written, name);
  if (warn[0] && strlen(text) + strlen(warn) + 22 < sizeof statusmessage)
  {
    snprintf(statusmessage, sizeof statusmessage, "%s. Not carried over: %s", text, warn);
  }
  else setstatusmessage(text);
}
