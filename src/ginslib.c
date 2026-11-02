//
// C64 GrooveGoat - instrument library browser
//
// Lists the bundled instrument library (instruments/6581|8580/<category>/
// *.ins) with search, categories, favourites, preview and an ADSR graph.
//

#include "goattrk2.h"
#include "gres.h"
#include "ginslib.h"
#include "gfile.h"

#define MAX_LIBCATS 64
#define LIB_W 124
#define LIB_H 44
#define LIST_ROWS 31

typedef struct
{
  char *path;       // relative to the library: 8580/Bass/acid_303.ins
  char *name;       // file name without extension
  int cat;          // category index
  int model;        // 0 = 6581, 1 = 8580, 2 = other
  int fav;
} LIBENTRY;

static LIBENTRY *lib = NULL;
static int libcount = 0, libsize = 0;
static char *catnames[MAX_LIBCATS];
static int numcats = 0;
static int scanned = 0;

static char favfile[MAX_PATHNAME];
static char search[64];
static int curcat = 0;       // 0 = all, 1 = favourites, 2.. = categories
static int curmodel = -1;    // 0 = 6581, 1 = 8580
static int libpos = 0, libview = 0;

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------

static int findcat(const char *name)
{
  int c;
  for (c = 0; c < numcats; c++)
    if (!strcmp(catnames[c], name)) return c;
  if (numcats >= MAX_LIBCATS) return MAX_LIBCATS - 1;
  catnames[numcats] = strdup(name);
  return numcats++;
}

static void addentry(const char *rel, const char *file, int cat, int model)
{
  LIBENTRY *e;
  char *dot;
  if (libcount >= libsize)
  {
    LIBENTRY *n;
    libsize = libsize ? libsize * 2 : 1024;
    n = (LIBENTRY *)realloc(lib, libsize * sizeof(LIBENTRY));
    if (!n) return;
    lib = n;
  }
  e = &lib[libcount++];
  e->path = strdup(rel);
  e->name = strdup(file);
  dot = strrchr(e->name, '.');
  if (dot) *dot = 0;
  e->cat = cat;
  e->model = model;
  e->fav = 0;
}

static int isins(const char *name)
{
  int l = strlen(name);
  return (l > 4) && (!strcasecmp(name + l - 4, ".ins"));
}

static int isdir(const char *path)
{
  struct stat st;
  return (!stat(path, &st)) && (S_ISDIR(st.st_mode));
}

static int islibrary(const char *path)
{
  char b[MAX_PATHNAME];
  snprintf(b, sizeof b, "%s/6581", path);
  if (isdir(b)) return 1;
  snprintf(b, sizeof b, "%s/8580", path);
  return isdir(b);
}

// The library is the folder with the instruments (6581 and 8580 folders with a folder for every category). It is
// looked for where the user pointed to (FOLDER... in the library, saved in the config), next to the program, in the
// source tree the program was built in (build/<target>/ is two folders down), in the folder of the app bundle and in
// the user's settings folder. The first folder that has it is used.
void library_locate(void)
{
  char c[8][MAX_PATHNAME];
  const char *home = getenv("HOME");
  int n = 0, i;
  if (!home) home = getenv("USERPROFILE");
  if (instrlibcfg[0]) snprintf(c[n++], MAX_PATHNAME, "%s", instrlibcfg);
  snprintf(c[n++], MAX_PATHNAME, "%sinstruments", appdir);
  snprintf(c[n++], MAX_PATHNAME, "%s../../instruments", appdir);
  snprintf(c[n++], MAX_PATHNAME, "%s../instruments", appdir);
  snprintf(c[n++], MAX_PATHNAME, "%s../Resources/instruments", appdir);
  if (home) snprintf(c[n++], MAX_PATHNAME, "%s/.groovegoat/instruments", home);
  for (i = 0; i < n; i++)
  {
    if (islibrary(c[i]))
    {
      if (strcmp(instrlibpath, c[i])) { snprintf(instrlibpath, MAX_PATHNAME, "%s", c[i]); scanned = 0; }
      return;
    }
  }
}

// Lets the user find the library: open any instrument in it (the folder above the 6581 / 8580 folder is the library,
// or else the folder of the instrument). Returns 1 if a folder was chosen.
static int library_choose(void)
{
  char name[MAX_FILENAME], path[MAX_PATHNAME], root[MAX_PATHNAME], filter[8];
  const char *home = getenv("HOME");
  int k;
  name[0] = 0;
  strcpy(filter, "*.ins");
  if (!home) home = getenv("USERPROFILE");
  snprintf(path, MAX_PATHNAME, "%s", isdir(instrlibpath) ? instrlibpath : (home ? home : "."));
  if (!fileselector(name, path, filter, "FIND THE INSTRUMENT LIBRARY: open any instrument inside it", FILESEL_INSTRUMENT)) return 0;
  if (!getcwd(root, MAX_PATHNAME)) return 0;
  // up to the folder that holds the 6581 / 8580 folder
  {
    char up[MAX_PATHNAME];
    snprintf(up, sizeof up, "%s", root);
    for (k = 0; k < 4; k++)
    {
      char *sep = strrchr(up, '/');
      const char *base;
      if (!sep) sep = strrchr(up, '\\');
      if (!sep) break;
      base = sep + 1;
      if ((!strcmp(base, "6581")) || (!strcmp(base, "8580")))
      {
        if (sep == up) sep++;
        *sep = 0;
        snprintf(root, MAX_PATHNAME, "%s", up);
        break;
      }
      *sep = 0;
    }
  }
  snprintf(instrlibcfg, MAX_PATHNAME, "%s", root);
  snprintf(instrlibpath, MAX_PATHNAME, "%s", root);
  return 1;
}

// Forget the list, so that it is read again from instrlibpath
static void rescanlibrary(void)
{
  int c;
  for (c = 0; c < libcount; c++) { free(lib[c].path); free(lib[c].name); }
  libcount = 0;
  for (c = 0; c < numcats; c++) free(catnames[c]);
  numcats = 0;
  scanned = 0;
}

static void scandir_rec(const char *rel, int depth, int model, int cat)
{
  char full[MAX_PATHNAME];
  DIR *d;
  struct dirent *de;
  snprintf(full, sizeof full, "%s/%s", instrlibpath, rel);
  d = opendir(full);
  if (!d) return;
  while ((de = readdir(d)))
  {
    char r[MAX_PATHNAME], f[MAX_PATHNAME];
    if (de->d_name[0] == '.') continue;
    if (rel[0]) snprintf(r, sizeof r, "%s/%s", rel, de->d_name);
    else snprintf(r, sizeof r, "%s", de->d_name);
    snprintf(f, sizeof f, "%s/%s", instrlibpath, r);
    if (isdir(f))
    {
      if (depth > 3) continue;
      if (depth == 0)
      {
        int m = 2;
        if (strstr(de->d_name, "6581")) m = 0;
        else if (strstr(de->d_name, "8580")) m = 1;
        scandir_rec(r, depth + 1, m, cat);
      }
      else if (depth == 1) scandir_rec(r, depth + 1, model, findcat(de->d_name));
      else scandir_rec(r, depth + 1, model, cat);
    }
    else if (isins(de->d_name))
    {
      if (cat < 0) cat = findcat("Other");
      addentry(r, de->d_name, cat, model);
    }
  }
  closedir(d);
}

static int cmpentry(const void *a, const void *b)
{
  const LIBENTRY *x = (const LIBENTRY *)a, *y = (const LIBENTRY *)b;
  int c = strcasecmp(catnames[x->cat], catnames[y->cat]);
  if (c) return c;
  return strcasecmp(x->name, y->name);
}

static void loadfavourites(void)
{
  FILE *h;
  char line[MAX_PATHNAME];
  int c;
  configpath(favfile, "favourites.txt");
  h = fopen(favfile, "rt");
  if (!h) return;
  while (fgets(line, sizeof line, h))
  {
    int l = strlen(line);
    while ((l > 0) && ((line[l - 1] == '\n') || (line[l - 1] == '\r'))) line[--l] = 0;
    for (c = 0; c < libcount; c++)
      if (!strcmp(lib[c].path, line)) lib[c].fav = 1;
  }
  fclose(h);
}

static void savefavourites(void)
{
  FILE *h = fopen(favfile, "wt");
  int c;
  if (!h) return;
  for (c = 0; c < libcount; c++)
    if (lib[c].fav) fprintf(h, "%s\n", lib[c].path);
  fclose(h);
}

static void scanlibrary(void)
{
  if (scanned) return;
  scanned = 1;
  scandir_rec("", 0, 2, -1);
  if (libcount) qsort(lib, libcount, sizeof(LIBENTRY), cmpentry);
  loadfavourites();
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

static int *view = NULL;
static int viewcount = 0;

static int matches(const LIBENTRY *e)
{
  if ((curmodel >= 0) && (e->model != 2) && (e->model != curmodel)) return 0;
  if ((curcat == 1) && (!e->fav)) return 0;
  if ((curcat >= 2) && (e->cat != curcat - 2)) return 0;
  if (search[0])
  {
    // every word of the search text must appear in the name or category
    char words[64], *w, *next;
    strcpy(words, search);
    for (w = words; w; w = next)
    {
      next = strchr(w, ' ');
      if (next) *next++ = 0;
      if (!*w) continue;
      char hay[256];
      int c;
      snprintf(hay, sizeof hay, "%s %s", e->name, catnames[e->cat]);
      for (c = 0; hay[c]; c++) hay[c] = tolower((unsigned char)hay[c]);
      for (c = 0; w[c]; c++) w[c] = tolower((unsigned char)w[c]);
      if (!strstr(hay, w))
      {
        // "_" in names counts as a space
        for (c = 0; hay[c]; c++) if (hay[c] == '_') hay[c] = ' ';
        if (!strstr(hay, w)) return 0;
      }
    }
  }
  return 1;
}

static void buildview(void)
{
  int c;
  free(view);
  view = (int *)malloc((libcount + 1) * sizeof(int));
  viewcount = 0;
  if (!view) return;
  for (c = 0; c < libcount; c++)
    if (matches(&lib[c])) view[viewcount++] = c;
  if (libpos >= viewcount) libpos = viewcount - 1;
  if (libpos < 0) libpos = 0;
}

// ---------------------------------------------------------------------------
// Preview (temporarily loads the instrument into the current slot)
// ---------------------------------------------------------------------------

// complete copy of instruments, tables and patterns before the preview
static void *previewdata = NULL;
static int previewing = 0;

static void restoreslot(void)
{
  if (!previewing) return;
  snd_lock();
  releasenote(editorInfo.epchn);
  songdata_restore(previewdata);
  snd_unlock();
  previewing = 0;
}

static int loadintoslot(const LIBENTRY *e)
{
  char save[MAX_FILENAME];
  char full[MAX_PATHNAME];
  FILE *test;
  snprintf(full, sizeof full, "%s/%s", instrlibpath, e->path);
  test = fopen(full, "rb");
  if (!test) return 0;
  fclose(test);
  strcpy(save, instrfilename);
  strncpy(instrfilename, full, MAX_FILENAME - 1);
  instrfilename[MAX_FILENAME - 1] = 0;
  loadinstrument();
  strcpy(instrfilename, save);
  return !instrloadfailed;
}

static void preview(const LIBENTRY *e)
{
  if (isplaying())
  {
    setstatusmessage("Stop the music to preview instruments");
    return;
  }
  snd_lock();
  releasenote(editorInfo.epchn);
  if (previewing)
  {
    songdata_restore(previewdata);
  }
  else
  {
    songdata_free(previewdata);
    previewdata = songdata_save();
  }
  previewing = 1;
  if (loadintoslot(e))
  {
    adaptfiltervoices(editorInfo.einum, 1 << (editorInfo.epchn % 3));
    preview_note(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn);
  }
  snd_unlock();
}

// Read AD/SR and first wave of an instrument file for the info panel
static int readinsinfo(const LIBENTRY *e, int *ad, int *sr, int *wave, char *name)
{
  char full[MAX_PATHNAME];
  unsigned char hdr[4 + 9 + MAX_INSTRNAMELEN];
  FILE *h;
  snprintf(full, sizeof full, "%s/%s", instrlibpath, e->path);
  h = fopen(full, "rb");
  if (!h) return 0;
  if (fread(hdr, sizeof hdr, 1, h) != 1)
  {
    fclose(h);
    return 0;
  }
  fclose(h);
  if (memcmp(hdr, "GTI", 3)) return 0;
  *ad = hdr[4];
  *sr = hdr[5];
  *wave = hdr[12];
  memcpy(name, hdr + 13, MAX_INSTRNAMELEN);
  name[MAX_INSTRNAMELEN - 1] = 0;
  return 1;
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

static const char *catlabel(int c)
{
  if (c == 0) return "All instruments";
  if (c == 1) return GS_STAR " Favourites";
  return catnames[c - 2];
}

void instrumentlibrary(void)
{
  int x0 = (MAX_COLUMNS - LIB_W) / 2, y0 = (MAX_ROWS - LIB_H) / 2;
  int listx = x0 + 28, listw = 62;
  int infox = x0 + 92;
  int done = 0, c;
  Uint32 lastclicktime = 0;
  int lastclickpos = -1;
  int lastinfo = -1;
  int iad = 0, isr = 0, iwave = 0;
  char iname[MAX_INSTRNAMELEN];

  if (!editorInfo.einum) editorInfo.einum = 1;
  library_locate();
  scanlibrary();
  if (curmodel < 0) curmodel = editorInfo.sidmodel ? 1 : 0;
  previewing = 0;
  preview_begin();
  buildview();
  key = rawkey = 0;

  while (!done)
  {
    char buf[200];
    int numrows = numcats + 2;
    int hitcat = -1, hitrow = -1, hitstar = -1, hitbtn = 0;
    const LIBENTRY *sel = viewcount ? &lib[view[libpos]] : NULL;

    if (libpos < libview) libview = libpos;
    if (libpos >= libview + LIST_ROWS) libview = libpos - LIST_ROWS + 1;
    if (libview < 0) libview = 0;

    printstatus();
    sprintf(buf, "Instrument library  " GS_BULLET "  load into slot %02X", editorInfo.einum);
    dlg_drawframe(x0, y0, LIB_W, LIB_H, buf);

    // search field and chip model
    printglyph(x0 + 3, y0 + 2, MKCOL(14, C_PANEL), GL_SEARCH);
    printtext(x0 + 5, y0 + 2, MKCOL(7, C_PANEL), "Search");
    printblankc(x0 + 12, y0 + 2, MKCOL(15, C_BEATROW), 30);
    printtext(x0 + 12, y0 + 2, MKCOL(15, C_BEATROW), search);
    printcursor(x0 + 12 + strlen(search), y0 + 2, 1);
    printtext(x0 + 46, y0 + 2, MKCOL(7, C_PANEL), "SID");
    ui_drawbutton(x0 + 50, y0 + 2, " 6581 ", 1001, 0, curmodel == 0);
    ui_drawbutton(x0 + 57, y0 + 2, " 8580 ", 1002, 0, curmodel == 1);
    if ((mousey == y0 + 2) && (mousex >= x0 + 50) && (mousex < x0 + 56)) hitbtn = 1;
    if ((mousey == y0 + 2) && (mousex >= x0 + 57) && (mousex < x0 + 63)) hitbtn = 2;
    ui_drawbutton(infox, y0 + 2, " FOLDER... ", 1003, 0, 0);
    if ((mousey == y0 + 2) && (mousex >= infox) && (mousex < infox + 11)) hitbtn = 6;
    sprintf(buf, "%d of %d instruments", viewcount, libcount);
    printtext(listx + listw - (int)strlen(buf), y0 + 2, MKCOL(3, C_PANEL), buf);

    // categories
    printtext(x0 + 3, y0 + 4, MKCOL(14, C_PANEL), "CATEGORY");
    for (c = 0; (c < numrows) && (c < LIB_H - 9); c++)
    {
      int y = y0 + 5 + c;
      int isel = (c == curcat);
      printblankc(x0 + 2, y, MKCOL(8, isel ? C_SELECT : C_PANEL), 24);
      printtext(x0 + 3, y, MKCOL(isel ? 15 : (c < 2 ? 7 : 8), isel ? C_SELECT : C_PANEL), catlabel(c));
      if ((mousey == y) && (mousex >= x0 + 2) && (mousex < x0 + 26)) hitcat = c;
    }

    // instrument list
    printtext(listx, y0 + 4, MKCOL(14, C_PANEL), "   NAME                                    CATEGORY");
    for (c = 0; c < LIST_ROWS; c++)
    {
      int y = y0 + 5 + c;
      int idx = libview + c;
      printblankc(listx - 1, y, MKCOL(8, C_PANEL), listw + 1);
      if (idx < viewcount)
      {
        const LIBENTRY *e = &lib[view[idx]];
        int isel = (idx == libpos);
        int bg = isel ? C_SELECT : (c & 1 ? C_PANEL : C_BEATROW);
        printblankc(listx - 1, y, MKCOL(8, bg), listw + 1);
        printglyph(listx, y, MKCOL(e->fav ? 14 : 3, bg), e->fav ? GL_STAR : GL_STAROFF);
        snprintf(buf, sizeof buf, "%-38.38s", e->name);
        printtext(listx + 3, y, MKCOL(isel ? 15 : 8, bg), buf);
        snprintf(buf, sizeof buf, "%.14s%s", catnames[e->cat], e->model == 2 ? "" : (e->model ? " 8580" : " 6581"));
        printtext(listx + 43, y, MKCOL(7, bg), buf);
        if ((mousey == y) && (mousex >= listx - 1) && (mousex < listx + listw))
        {
          if (mousex <= listx + 1) hitstar = idx;
          else hitrow = idx;
        }
      }
    }
    if (viewcount > LIST_ROWS)
    {
      // scroll position
      int barh = LIST_ROWS * LIST_ROWS / viewcount;
      int bary = (LIST_ROWS - barh) * libview / (viewcount - LIST_ROWS);
      if (barh < 1) barh = 1;
      for (c = 0; c < LIST_ROWS; c++)
        printglyph(listx + listw, y0 + 5 + c, MKCOL((c >= bary) && (c < bary + barh) ? 8 : C_LINE, C_PANEL), GL_VLINE);
    }

    // info about the selected instrument
    fillarea(infox, y0 + 4, LIB_W - (infox - x0) - 2, 20, MKCOL(8, C_PANEL), ' ');
    if (sel)
    {
      int ci = view[libpos];
      if (ci != lastinfo)
      {
        if (!readinsinfo(sel, &iad, &isr, &iwave, iname)) iad = isr = iwave = 0, iname[0] = 0;
        lastinfo = ci;
      }
      printtext(infox, y0 + 4, MKCOL(14, C_PANEL), "SELECTED");
      snprintf(buf, sizeof buf, "%.28s", sel->name);
      printtext(infox, y0 + 5, MKCOL(15, C_PANEL), buf);
      snprintf(buf, sizeof buf, "%.28s", catnames[sel->cat]);
      printtext(infox, y0 + 6, MKCOL(7, C_PANEL), buf);
      sprintf(buf, "Chip: %s", sel->model == 2 ? "any" : (sel->model ? "MOS 8580" : "MOS 6581"));
      printtext(infox, y0 + 7, MKCOL(7, C_PANEL), buf);
      sprintf(buf, "AD %02X  SR %02X", iad, isr);
      printtext(infox, y0 + 9, MKCOL(8, C_PANEL), buf);
      fillarea(infox, y0 + 10, 28, 3, MKCOL(8, C_PANEL), ' ');
      drawadsrgraph(1, infox, y0 + 10, 28, 3, iad, isr, MKCOL(11, C_PANEL));
      {
        char t1[16], t2[16], t3[16];
        adsr_timestr(t1, adsr_attackms(iad >> 4));
        adsr_timestr(t2, adsr_decayms(iad & 15));
        adsr_timestr(t3, adsr_decayms(isr & 15));
        sprintf(buf, "A %s  D %s", t1, t2);
        printtext(infox, y0 + 13, MKCOL(7, C_PANEL), buf);
        sprintf(buf, "S %X/F  R %s", isr >> 4, t3);
        printtext(infox, y0 + 14, MKCOL(7, C_PANEL), buf);
      }
      ui_drawbutton(infox, y0 + 16, sel->fav ? " " GS_STAR " Favourite " : " " GS_STAROFF " Favourite ", 1003, 0, sel->fav);
      ui_drawbutton(infox, y0 + 18, " " GS_PLAY " Preview ", 1004, 0, 0);
      ui_drawbutton(infox, y0 + 20, " " GS_LOADINS " Load into slot ", 1005, 0, 0);
      if (mousey == y0 + 16 && mousex >= infox && mousex < infox + 13) hitbtn = 3;
      if (mousey == y0 + 18 && mousex >= infox && mousex < infox + 11) hitbtn = 4;
      if (mousey == y0 + 20 && mousex >= infox && mousex < infox + 18) hitbtn = 5;
      if (isplaying()) printtext(infox, y0 + 22, MKCOL(12, C_PANEL), "Stop the music to preview");
    }

    printtext(x0 + 3, y0 + LIB_H - 3, MKCOL(3, C_PANEL),
      "Type to search  UP/DOWN/PGUP/PGDN select  LEFT/RIGHT category  TAB 6581/8580  SPACE or click: preview  INS/* favourite");
    printtext(x0 + 3, y0 + LIB_H - 2, MKCOL(3, C_PANEL), "ENTER / double click = load into the instrument slot   ESC = close");

    if (!libcount)                      // in the empty list, over everything else
    {
      printtext(listx + 2, y0 + 8, MKCOL(15, C_PANEL), "The instrument library was not found in:");
      printtext(listx + 2, y0 + 9, MKCOL(7, C_PANEL), instrlibpath);
      printtext(listx + 2, y0 + 11, MKCOL(15, C_PANEL), "Click FOLDER... (top right) and open any instrument");
      printtext(listx + 2, y0 + 12, MKCOL(15, C_PANEL), "of the library: its folder is remembered.");
    }
    fliptoscreen();
    getkey();
    if (win_quitted)
    {
      done = 1;
      break;
    }

    if (mousewheel)
    {
      libpos -= mousewheel * 3;
      if (libpos < 0) libpos = 0;
      if (libpos >= viewcount) libpos = viewcount - 1;
      if (libpos < 0) libpos = 0;
    }

    if ((mouseb & MOUSEB_LEFT) && (!prevmouseb))
    {
      if (hitcat >= 0)
      {
        curcat = hitcat;
        libpos = libview = 0;
        buildview();
      }
      else if (hitstar >= 0)
      {
        lib[view[hitstar]].fav ^= 1;
        savefavourites();
        if (curcat == 1) buildview();
      }
      else if (hitrow >= 0)
      {
        Uint32 now = SDL_GetTicks();
        if ((hitrow == lastclickpos) && (now - lastclicktime < 450))
          rawkey = KEY_ENTER;
        else
        {
          // a click selects the instrument and plays it at once
          rawkey = isplaying() ? 0 : KEY_SPACE;
          if (isplaying()) setstatusmessage("Stop the music to preview instruments");
        }
        libpos = hitrow;
        lastclickpos = hitrow;
        lastclicktime = now;
      }
      else if (hitbtn == 1 || hitbtn == 2)
      {
        curmodel = hitbtn - 1;
        buildview();
      }
      else if (hitbtn == 6)
      {
        restoreslot();
        if (library_choose())
        {
          rescanlibrary();
          scanlibrary();
          libpos = 0;
          libview = 0;
          curcat = 0;
          buildview();
          setstatusmessage(libcount ? "Instrument library found" : "No instruments in that folder");
        }
        key = rawkey = 0;
      }
      else if (hitbtn == 3) rawkey = KEY_INS;
      else if (hitbtn == 4) rawkey = KEY_SPACE;
      else if (hitbtn == 5) rawkey = KEY_ENTER;
      else if ((mousex < x0) || (mousex >= x0 + LIB_W) || (mousey < y0) || (mousey >= y0 + LIB_H))
        done = 1;
    }
    if ((mouseb & MOUSEB_RIGHT) && (!prevmouseb) && (hitrow >= 0))
    {
      libpos = hitrow;
      rawkey = KEY_SPACE;
    }

    // typing: search text
    if ((key >= 32) && (key < 127) && (key != ' ') && (key != '*') && (strlen(search) < 30) &&
        (!ctrlpressed) && (!altpressed) && (!metapressed))
    {
      int l = strlen(search);
      search[l] = key;
      search[l + 1] = 0;
      libpos = libview = 0;
      buildview();
    }
    if (key == '*') rawkey = KEY_INS;

    // the selection may have been changed by the mouse
    sel = viewcount ? &lib[view[libpos]] : NULL;

    switch (rawkey)
    {
      case KEY_ESC:
      if (search[0])
      {
        search[0] = 0;
        buildview();
      }
      else done = 1;
      break;

      case KEY_BACKSPACE:
      if (search[0])
      {
        search[strlen(search) - 1] = 0;
        buildview();
      }
      break;

      case KEY_UP: if (libpos > 0) libpos--; break;
      case KEY_DOWN: if (libpos < viewcount - 1) libpos++; break;
      case KEY_PGUP: libpos -= LIST_ROWS - 1; if (libpos < 0) libpos = 0; break;
      case KEY_PGDN: libpos += LIST_ROWS - 1; if (libpos >= viewcount) libpos = viewcount - 1; if (libpos < 0) libpos = 0; break;
      case KEY_HOME: libpos = 0; break;
      case KEY_END: libpos = viewcount ? viewcount - 1 : 0; break;

      case KEY_LEFT:
      case KEY_RIGHT:
      curcat += rawkey == KEY_LEFT ? -1 : 1;
      if (curcat < 0) curcat = numrows - 1;
      if (curcat >= numrows) curcat = 0;
      libpos = libview = 0;
      buildview();
      break;

      case KEY_TAB:
      curmodel ^= 1;
      buildview();
      break;

      case KEY_INS:
      if (sel)
      {
        lib[view[libpos]].fav ^= 1;
        savefavourites();
        if (curcat == 1) buildview();
      }
      break;

      case KEY_SPACE:
      if (sel) preview(sel);
      break;

      case KEY_ENTER:
      if (sel)
      {
        void *ud;
        restoreslot();
        ud = undoinstrbegin();
        if (loadintoslot(sel))
        {
          // filter only the voices on which the instrument is used in the song
          int mask = instrumentvoicemask(editorInfo.einum);
          if (!mask) mask = 1 << (editorInfo.epchn % 3);
          adaptfiltervoices(editorInfo.einum, mask);
          songmodified = 1;
          snprintf(statusmessage, sizeof statusmessage, "Loaded %s (%s) into instrument slot %02X", sel->name, catnames[sel->cat], editorInfo.einum);
        }
        undoinstrend(ud);
        done = 2;
      }
      break;
    }
  }
  if (done != 2) restoreslot();
  previewing = 0;
  preview_end();
  songdata_free(previewdata);
  previewdata = NULL;
  key = rawkey = 0;
  printmainscreen();
}

// Test helper (GG_PREVIEWTEST=n): previews every n-th library instrument of
// the current SID model offline and reports the ones that stay silent
int library_previewtest(int step)
{
  static short buf[2048];
  int c, silent = 0, tested = 0;
  scanlibrary();
  curmodel = editorInfo.sidmodel ? 1 : 0;
  curcat = 0;
  search[0] = 0;
  buildview();
  sid_init(48000, editorInfo.sidmodel, 0, 0, 0, 0, 1);
  sid_setpanning(1, 0);
  editorInfo.einum = 1;
  editorInfo.epchn = 0;
  preview_begin();
  for (c = 0; c < viewcount; c += step)
  {
    const LIBENTRY *e = &lib[view[c]];
    double sum = 0, framemax = 0;
    int f, i, n = 0, loudframes = 0;
    preview(e);
    for (f = 0; f < 40; f++)
    {
      playroutine();
      sid_fillbuffer(buf, 960, editorInfo.adparam, 0);
      if (f >= 1)
      {
        double fs = 0;
        for (i = 0; i < 960; i++, n++) fs += (double)buf[i] * buf[i];
        sum += fs;
        if (sqrt(fs / 960) > framemax) framemax = sqrt(fs / 960);
        if (sqrt(fs / 960) > 100) loudframes++;
        if ((getenv("GG_PREVIEWNAME")) && (strstr(e->path, getenv("GG_PREVIEWNAME"))))
          printf("%s frame %d rms %.0f env %d\n", e->path, f, sqrt(fs / 960), sid_getenv(0, 0));
      }
    }
    releasenote(0);
    for (f = 0; f < 10; f++)
    {
      playroutine();
      sid_fillbuffer(buf, 960, editorInfo.adparam, 0);
    }
    tested++;
    // silent: audible for less than 3 frames (60 ms), i.e. only a click
    if (loudframes < 3)
    {
      silent++;
      printf("silent: %s (rms %.1f, loudest frame %.1f, %d audible frames)\n", e->path, sqrt(sum / n), framemax, loudframes);
    }
  }
  restoreslot();
  preview_end();
  printf("tested %d, silent %d\n", tested, silent);
  return silent == 0;
}

// ---------------------------------------------------------------------------
// Test helper (GG_REPLACETEST): replaces every instrument of the loaded song
// with library instruments and checks that everything else in the song still
// plays the same table programs
// ---------------------------------------------------------------------------

static int progsig(char *out, int t, int ptr, int depth)
{
  int pos, n = 0, steps;
  if ((!ptr) || (t < 0) || (t >= MAX_TABLES)) return sprintf(out, "-");
  pos = ptr - 1;
  if (t == STBL) return sprintf(out, "S%02x%02x", ltable[STBL][pos], rtable[STBL][pos]);
  for (steps = 0; (steps < 64) && (pos < MAX_TABLELEN); steps++, pos++)
  {
    unsigned char l = ltable[t][pos], r = rtable[t][pos];
    if (l == 0xff)
    {
      // loop inside the program: relative offset; jump elsewhere: what is there
      if (!r) n += sprintf(out + n, "END");
      else if ((r - 1 >= ptr - 1) && (r - 1 <= pos)) n += sprintf(out + n, "L%d", r - 1 - (ptr - 1));
      else if (depth < 2)
      {
        n += sprintf(out + n, "T(");
        n += progsig(out + n, t, r, depth + 1);
        n += sprintf(out + n, ")");
      }
      break;
    }
    n += sprintf(out + n, "%02x%02x.", l, r);
    // wavetable commands that point into other tables
    if ((t == WTBL) && (l >= WAVECMD) && (l <= WAVELASTCMD) && (depth < 1))
    {
      int cmd = l & 0xf;
      if ((cmd >= CMD_SETWAVEPTR) && (cmd <= CMD_SETWAVEPTR + 2)) n += progsig(out + n, cmd - CMD_SETWAVEPTR, r, depth + 1);
      if (((cmd >= CMD_PORTAUP) && (cmd <= CMD_VIBRATO)) || (cmd == CMD_FUNKTEMPO)) n += progsig(out + n, STBL, r, depth + 1);
    }
  }
  return n;
}

char *songsignature(int skipins)
{
  static char *buf = NULL;
  int n = 0, i, t, p, d;
  if (!buf) buf = (char *)malloc(8 * 1024 * 1024);
  for (i = 1; i < MAX_INSTR; i++)
  {
    if (i == skipins) continue;
    n += sprintf(buf + n, "I%d:%02x%02x%02x%02x%02x:", i, instr[i].ad, instr[i].sr, instr[i].vibdelay, instr[i].gatetimer, instr[i].firstwave);
    for (t = 0; t < MAX_TABLES; t++)
    {
      n += progsig(buf + n, t, instr[i].ptr[t], 0);
      buf[n++] = '|';
    }
  }
  for (p = 0; p < MAX_PATT; p++)
  {
    for (d = 0; d < pattlen[p]; d++)
    {
      int cmd = pattern[p][d*4+2], data = pattern[p][d*4+3];
      n += sprintf(buf + n, "%02x%02x%02x", pattern[p][d*4], pattern[p][d*4+1], cmd);
      if ((cmd >= CMD_SETWAVEPTR) && (cmd <= CMD_SETWAVEPTR + 2)) n += progsig(buf + n, cmd - CMD_SETWAVEPTR, data, 0);
      else if ((((cmd >= CMD_PORTAUP) && (cmd <= CMD_VIBRATO)) || (cmd == CMD_FUNKTEMPO)) && (data)) n += progsig(buf + n, STBL, data, 0);
      else n += sprintf(buf + n, "%02x", data);
    }
  }
  buf[n] = 0;
  return buf;
}

int library_replacetest(void)
{
  int ins, errors = 0, k;
  void *orig;
  scanlibrary();
  curmodel = editorInfo.sidmodel ? 1 : 0;
  curcat = 0;
  search[0] = 0;
  buildview();
  orig = songdata_save();
  for (ins = 1; ins < MAX_INSTR; ins++)
  {
    char *before;
    void *state;
    int mask = instrumentvoicemask(ins);
    if ((!instr[ins].ptr[WTBL]) && (!mask)) continue;
    songdata_restore(orig);
    editorInfo.einum = ins;
    before = strdup(songsignature(ins));
    state = songdata_save();

    // 1. many previews, then cancel: nothing may change
    previewing = 0;
    for (k = 0; k < 40; k++) preview(&lib[view[(k * 131 + ins * 7) % viewcount]]);
    restoreslot();
    {
      void *now = songdata_save();
      if (memcmp(now, state, sizeof(instr) + sizeof(ltable) + sizeof(rtable) + sizeof(pattern)))
      {
        printf("instrument %02X: previews changed the song data\n", ins);
        errors++;
      }
      songdata_free(now);
    }

    // 2. load library instruments into the slot: the rest of the song must stay the same
    for (k = 0; k < 5; k++)
    {
      const LIBENTRY *e = &lib[view[(k * 977 + ins * 31) % viewcount]];
      if (loadintoslot(e))
      {
        int m = mask ? mask : 1;
        adaptfiltervoices(ins, m);
        if (strcmp(before, songsignature(ins)))
        {
          const char *a = before, *b = songsignature(ins);
          int i = 0;
          while ((a[i]) && (a[i] == b[i])) i++;
          if (getenv("GG_REPLACEDEBUG")) printf("diff at %d: ...%.60s\n                ...%.60s\n", i, a + (i > 20 ? i - 20 : 0), b + (i > 20 ? i - 20 : 0));
          printf("instrument %02X replaced by %s: the rest of the song changed\n", ins, e->path);
          errors++;
        }
        if (instr[ins].ptr[FTBL])
        {
          int p = instr[ins].ptr[FTBL] - 1;
          if ((ltable[FTBL][p] >= 0x80) && ((rtable[FTBL][p] & 7) != (m & 7)))
          {
            printf("instrument %02X: filter routing %x, used on voices %x\n", ins, rtable[FTBL][p] & 7, m);
            errors++;
          }
        }
      }
      else printf("instrument %02X: %s did not fit (%s)\n", ins, e->path, statusmessage);
    }
    printf("instrument %02X (voices %x): ok\n", ins, mask);
    free(before);
    songdata_free(state);
  }
  songdata_restore(orig);
  songdata_free(orig);
  printf("errors: %d\n", errors);
  return errors == 0;
}
