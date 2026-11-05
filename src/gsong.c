//
// GOATTRACKER v2 song data model, loading/saving/conversion
//

#define GSONG_C

#include "goattrk2.h"

INSTR instr[MAX_INSTR];
unsigned char ltable[MAX_TABLES][MAX_TABLELEN];
unsigned char rtable[MAX_TABLES][MAX_TABLELEN];
unsigned char songorder[MAX_SONGS][MAX_CHN][MAX_SONGLEN+2];
unsigned char pattern[MAX_PATT][MAX_PATTROWS*4+4];
char songname[MAX_STR];
char authorname[MAX_STR];
char copyrightname[MAX_STR];
int pattlen[MAX_PATT];
int songlen[MAX_SONGS][MAX_CHN];
int highestusedpattern;
int highestusedinstr;
int numsids = 1;
int numchannels = 3;
unsigned sidaddr[MAX_SIDS] = {0xd400, 0xd420, 0xd440, 0xd460, 0xd480, 0xd4a0, 0xd4c0, 0xd4e0};
int songmodified = 0;

int songvalid(int s)
{
  int c;
  if ((s < 0) || (s >= MAX_SONGS)) return 0;
  for (c = 0; c < numchannels; c++)
  {
    if (!songlen[s][c]) return 0;
  }
  return 1;
}

int countsongs(void)
{
  int c = MAX_SONGS - 1;
  for (;;)
  {
    if (songvalid(c)) break;
    if (c == 0) break;
    c--;
  }
  return c + 1;
}

// Find a pattern that is not used by any orderlist and is empty
static int findfreepattern(int start)
{
  int c, d;
  findusedpatterns();
  for (c = start; c < MAX_PATT; c++)
  {
    int empty = 1;
    if (pattused[c]) continue;
    for (d = 0; d < pattlen[c]; d++)
    {
      if ((pattern[c][d*4] != REST) || (pattern[c][d*4+1]) || (pattern[c][d*4+2]) || (pattern[c][d*4+3]))
      {
        empty = 0;
        break;
      }
    }
    if (empty) return c;
  }
  return -1;
}

// Does a channel contain anything else than an empty default orderlist?
int channelhasdata(int c)
{
  int s, d;
  for (s = 0; s < MAX_SONGS; s++)
  {
    for (d = 0; d < songlen[s][c]; d++)
    {
      int p = songorder[s][c][d];
      if (p < MAX_PATT)
      {
        int e;
        for (e = 0; e < pattlen[p]; e++)
        {
          if ((pattern[p][e*4] != REST) || (pattern[p][e*4+1]) || (pattern[p][e*4+2]) || (pattern[p][e*4+3]))
            return 1;
        }
      }
    }
  }
  return 0;
}

// Erase orderlists of channels that are not in use
void clearinactivechannels(void)
{
  int s, c;
  for (c = numchannels; c < MAX_CHN; c++)
  {
    for (s = 0; s < MAX_SONGS; s++)
    {
      memset(&songorder[s][c][0], 0, MAX_SONGLEN+2);
      songorder[s][c][0] = LOOPSONG;
      songlen[s][c] = 0;
    }
    editorUndoInfo.editorInfo[c].epnum = 0;
    editorUndoInfo.editorInfo[c].espos = 0;
    editorUndoInfo.editorInfo[c].esend = 0;
  }
}

// Change the amount of SIDs (1, 2, 4 or 8). New channels get their own
// empty patterns in all existing subtunes, removed channels are cleared.
void setnumsids(int n)
{
  int oldchannels = numchannels;
  int newchannels;
  int s, c;
  int validsongs[MAX_SONGS];

  if ((n != 1) && (n != 2) && (n != 4) && (n != 8)) return;
  if (n == numsids) return;

  stopsong();
  countpatternlengths();
  for (s = 0; s < MAX_SONGS; s++) validsongs[s] = songvalid(s);

  newchannels = n * 3;
  if (newchannels > oldchannels)
  {
    int nextpatt = 0;
    for (c = oldchannels; c < newchannels; c++)
    {
      int p = -1;
      for (s = 0; s < MAX_SONGS; s++)
      {
        memset(&songorder[s][c][0], 0, MAX_SONGLEN+2);
        if (validsongs[s])
        {
          if (p < 0)
          {
            p = findfreepattern(nextpatt);
            if (p < 0) p = 0;
            nextpatt = p + 1;
          }
          songorder[s][c][0] = p;
          songorder[s][c][1] = LOOPSONG;
          songorder[s][c][2] = 0;
          songlen[s][c] = 1;
        }
        else
        {
          songorder[s][c][0] = LOOPSONG;
          songlen[s][c] = 0;
        }
      }
      // Reserve the pattern so the next channel gets another one
      if (p >= 0) pattused[p] = 1;
      editorUndoInfo.editorInfo[c].epnum = p >= 0 ? p : 0;
      editorUndoInfo.editorInfo[c].espos = 0;
      editorUndoInfo.editorInfo[c].esend = 0;
      chn[c].mute = 0;
    }
  }
  else
  {
    for (c = newchannels; c < MAX_CHN; c++)
    {
      for (s = 0; s < MAX_SONGS; s++)
      {
        memset(&songorder[s][c][0], 0, MAX_SONGLEN+2);
        songorder[s][c][0] = LOOPSONG;
        songlen[s][c] = 0;
      }
      chn[c].mute = 0;
    }
  }

  numsids = n;
  numchannels = newchannels;
  if (editorInfo.epchn >= numchannels) editorInfo.epchn = numchannels - 1;
  if (editorInfo.eschn >= numchannels) editorInfo.eschn = numchannels - 1;
  if (editorInfo.epmarkchn >= numchannels) editorInfo.epmarkchn = -1;
  if (editorInfo.esmarkchn >= numchannels) editorInfo.esmarkchn = -1;
  countpatternlengths();
  songmodified = 1;
}

// Write the GoatTracker v2 song body (shared by .sng and .ggt)
static void writesongbody(FILE *handle, int nch)
{
  int c, d;
  int length;
  int amount;
  int writebytes;

  // Determine amount of patterns & instruments
  countpatternlengths();
  for (c = 1; c < MAX_INSTR; c++)
  {
    if ((instr[c].ad) || (instr[c].sr) || (instr[c].ptr[0]) || (instr[c].ptr[1]) ||
        (instr[c].ptr[2]) || (instr[c].vibdelay) || (instr[c].ptr[3]))
    {
      if (c > highestusedinstr) highestusedinstr = c;
    }
  }

  // Write infotexts
  fwrite(songname, sizeof songname, 1, handle);
  fwrite(authorname, sizeof authorname, 1, handle);
  fwrite(copyrightname, sizeof copyrightname, 1, handle);

  // Determine amount of songs to be saved
  amount = countsongs();

  fwrite8(handle, amount);
  // Write songorderlists
  for (d = 0; d < amount; d++)
  {
    for (c = 0; c < nch; c++)
    {
      length = songlen[d][c]+1;
      fwrite8(handle, length);
      writebytes = length;
      writebytes++;
      fwrite(songorder[d][c], writebytes, 1, handle);
    }
  }
  // Write amount of instruments
  fwrite8(handle, highestusedinstr);
  // Write instruments
  for (c = 1; c <= highestusedinstr; c++)
  {
    fwrite8(handle, instr[c].ad);
    fwrite8(handle, instr[c].sr);
    fwrite8(handle, instr[c].ptr[WTBL]);
    fwrite8(handle, instr[c].ptr[PTBL]);
    fwrite8(handle, instr[c].ptr[FTBL]);
    fwrite8(handle, instr[c].ptr[STBL]);
    fwrite8(handle, instr[c].vibdelay);
    fwrite8(handle, instr[c].gatetimer);
    fwrite8(handle, instr[c].firstwave);
    fwrite(&instr[c].name, MAX_INSTRNAMELEN, 1, handle);
  }
  // Write tables
  for (c = 0; c < MAX_TABLES; c++)
  {
    writebytes = gettablelen(c);
    fwrite8(handle, writebytes);
    fwrite(ltable[c], writebytes, 1, handle);
    fwrite(rtable[c], writebytes, 1, handle);
  }
  // Write patterns
  amount = highestusedpattern + 1;
  fwrite8(handle, amount);
  for (c = 0; c < amount; c++)
  {
    length = pattlen[c]+1;
    fwrite8(handle, length);
    fwrite(pattern[c], length * 4, 1, handle);
  }
}

static int hasextension(const char *name, const char *ext)
{
  int nl = strlen(name);
  int el = strlen(ext);
  if (nl < el) return 0;
  return !cmpname((char *)name + nl - el, (char *)ext);
}

static void setextension(char *name, const char *ext)
{
  int c;
  for (c = strlen(name) - 1; c >= 0; c--)
  {
    if ((name[c] == '/') || (name[c] == '\\')) break;
    if (name[c] == '.')
    {
      name[c] = 0;
      break;
    }
  }
  if (strlen(name) + strlen(ext) < MAX_FILENAME) strcat(name, ext);
}

// Save the song. 1 or 2 SIDs: GoatTracker (Stereo) compatible .sng,
// 4 or 8 SIDs (or a .ggt name): C64 GrooveGoat format.
// Write the song into a file (GTS5 = GoatTracker 2 / Stereo format, or GGT1
// for more than 2 SIDs). Does not change any editor state.
int writesongfile(const char *name, int ggt)
{
  int c;
  FILE *handle = fopen(name, "wb");
  if (!handle) return 0;
  if (ggt)
  {
    fwrite("GGT1", 4, 1, handle);
    fwrite8(handle, numsids);
    fwrite8(handle, (editorInfo.ntsc ? 1 : 0) | (editorInfo.sidmodel ? 2 : 0));
    fwrite8(handle, editorInfo.multiplier);
    fwritele16(handle, editorInfo.adparam);
    for (c = 0; c < numsids; c++) fwritele16(handle, sidaddr[c]);
    fwrite8(handle, 0); // amount of extra bytes (reserved for future use)
  }
  else
  {
    fwrite("GTS5", 4, 1, handle);
  }
  writesongbody(handle, numchannels);
  return fclose(handle) == 0;
}

int savesong(void)
{
  int c;
  int ggt = 0;
  FILE *handle;

  if (strlen(songfilename) < MAX_FILENAME-4)
  {
    int extfound = 0;
    for (c = strlen(songfilename)-1; c >= 0; c--)
    {
      if (songfilename[c] == '.') extfound = 1;
    }
    if (!extfound) strcat(songfilename, numsids > 2 ? ".ggt" : ".sng");
  }
  if (hasextension(songfilename, ".ggt")) ggt = 1;
  if ((numsids > 2) && (!ggt))
  {
    setextension(songfilename, ".ggt");
    ggt = 1;
  }

  // keep the previous version of the file as NAME.bak
  backup_copybeforesave(songfilename);
  if (writesongfile(songfilename, ggt))
  {
    strcpy(loadedsongfilename, songfilename);
    backup_saved();
    songmodified = 0;
    if (ggt)
      snprintf(statusmessage, sizeof statusmessage, "Saved %s (%d SIDs, C64 GrooveGoat format)", songfilename, numsids);
    else if (numsids == 2)
      snprintf(statusmessage, sizeof statusmessage, "Saved %s (2 SIDs, GoatTracker Stereo format)", songfilename);
    else
      snprintf(statusmessage, sizeof statusmessage, "Saved %s (GoatTracker 2 format)", songfilename);
    return 1;
  }
  snprintf(statusmessage, sizeof statusmessage, "Could not save %s!", songfilename);
  return 0;
}

int saveinstrument(void)
{
  int c;
  char ident[] = {'G', 'T', 'I', '5'};
  FILE *handle;

  if (strlen(instrfilename) < MAX_FILENAME-4)
  {
    int extfound = 0;
    for (c = strlen(instrfilename)-1; c >= 0; c--)
    {
      if (instrfilename[c] == '.') extfound = 1;
    }
    if (!extfound) strcat(instrfilename, ".ins");
  }

  handle = fopen(instrfilename, "wb");
  if (handle)
  {
    fwrite(ident, 4, 1, handle);

    // Write instrument
    fwrite8(handle, instr[editorInfo.einum].ad);
    fwrite8(handle, instr[editorInfo.einum].sr);
    fwrite8(handle, instr[editorInfo.einum].ptr[WTBL]);
    fwrite8(handle, instr[editorInfo.einum].ptr[PTBL]);
    fwrite8(handle, instr[editorInfo.einum].ptr[FTBL]);
    fwrite8(handle, instr[editorInfo.einum].ptr[STBL]);
    fwrite8(handle, instr[editorInfo.einum].vibdelay);
    fwrite8(handle, instr[editorInfo.einum].gatetimer);
    fwrite8(handle, instr[editorInfo.einum].firstwave);
    fwrite(&instr[editorInfo.einum].name, MAX_INSTRNAMELEN, 1, handle);
    for (c = 0; c < MAX_TABLES; c++)
    {
      if (instr[editorInfo.einum].ptr[c])
      {
        int pos = instr[editorInfo.einum].ptr[c] - 1;
        int len = gettablepartlen(c, pos);
        fwrite8(handle, len);
        fwrite(&ltable[c][pos], len, 1, handle);
        fwrite(&rtable[c][pos], len, 1, handle);
      }
      else fwrite8(handle, 0);
    }
    fclose(handle);
    return 1;
  }
  return 0;
}

// Parse (or just validate) a GoatTracker v2 song body for nch channels.
// Returns number of bytes consumed, or -1 on error.
static int parsesongbody(const unsigned char *buf, int len, int nch, int apply)
{
  int pos = 0;
  int c, d;
  int amount;

#define NEED(n) if (pos + (n) > len) return -1;
  NEED(MAX_STR * 3);
  if (apply)
  {
    memcpy(songname, buf + pos, MAX_STR);
    memcpy(authorname, buf + pos + MAX_STR, MAX_STR);
    memcpy(copyrightname, buf + pos + MAX_STR * 2, MAX_STR);
    songname[MAX_STR-1] = 0;
    authorname[MAX_STR-1] = 0;
    copyrightname[MAX_STR-1] = 0;
  }
  pos += MAX_STR * 3;

  // Songorderlists
  NEED(1);
  amount = buf[pos++];
  if (amount > MAX_SONGS) return -1;
  for (d = 0; d < amount; d++)
  {
    for (c = 0; c < nch; c++)
    {
      int length, loadsize;
      NEED(1);
      length = buf[pos++];
      loadsize = length + 1;
      if (loadsize > MAX_SONGLEN + 2) return -1;
      NEED(loadsize);
      // The orderlist must end with the loop mark
      if ((length < 1) || (buf[pos + length - 1] != LOOPSONG)) return -1;
      if (apply) memcpy(songorder[d][c], buf + pos, loadsize);
      pos += loadsize;
    }
  }
  // Instruments
  NEED(1);
  amount = buf[pos++];
  if (amount >= MAX_INSTR) return -1;
  for (c = 1; c <= amount; c++)
  {
    NEED(9 + MAX_INSTRNAMELEN);
    if (apply)
    {
      instr[c].ad = buf[pos];
      instr[c].sr = buf[pos+1];
      instr[c].ptr[WTBL] = buf[pos+2];
      instr[c].ptr[PTBL] = buf[pos+3];
      instr[c].ptr[FTBL] = buf[pos+4];
      instr[c].ptr[STBL] = buf[pos+5];
      instr[c].vibdelay = buf[pos+6];
      instr[c].gatetimer = buf[pos+7];
      instr[c].firstwave = buf[pos+8];
      memcpy(instr[c].name, buf + pos + 9, MAX_INSTRNAMELEN);
      instr[c].name[MAX_INSTRNAMELEN-1] = 0;
    }
    pos += 9 + MAX_INSTRNAMELEN;
  }
  // Tables
  for (c = 0; c < MAX_TABLES; c++)
  {
    int loadsize;
    NEED(1);
    loadsize = buf[pos++];
    NEED(loadsize * 2);
    if (apply)
    {
      memcpy(ltable[c], buf + pos, loadsize);
      memcpy(rtable[c], buf + pos + loadsize, loadsize);
    }
    pos += loadsize * 2;
  }
  // Patterns
  NEED(1);
  amount = buf[pos++];
  if (amount > MAX_PATT) return -1;
  for (c = 0; c < amount; c++)
  {
    int length;
    NEED(1);
    length = buf[pos++] * 4;
    if (length > MAX_PATTROWS * 4 + 4) return -1;
    NEED(length);
    if (apply) memcpy(pattern[c], buf + pos, length);
    pos += length;
  }
#undef NEED
  return pos;
}

static unsigned char *readwholefile(const char *name, int *len)
{
  FILE *handle = fopen(name, "rb");
  unsigned char *buf;
  long size;
  if (!handle) return NULL;
  fseek(handle, 0, SEEK_END);
  size = ftell(handle);
  fseek(handle, 0, SEEK_SET);
  if ((size <= 0) || (size > 16*1024*1024))
  {
    fclose(handle);
    return NULL;
  }
  buf = (unsigned char *)malloc(size);
  if (!buf)
  {
    fclose(handle);
    return NULL;
  }
  if (fread(buf, size, 1, handle) != 1)
  {
    free(buf);
    fclose(handle);
    return NULL;
  }
  fclose(handle);
  *len = (int)size;
  return buf;
}

// Determine the channel count of a GTS3-5 body (3 = GoatTracker,
// 6 = GoatTracker Stereo) by checking which one parses consistently.
static int detectchannels(const unsigned char *body, int len)
{
  if (parsesongbody(body, len, 3, 0) == len) return 3;
  if (parsesongbody(body, len, 6, 0) == len) return 6;
  if (parsesongbody(body, len, 3, 0) > 0) return 3;
  if (parsesongbody(body, len, 6, 0) > 0) return 6;
  return 0;
}

// Makes a song of the body of a song file (the bytes after the GTS5 identifier) in memory, as loading a file does:
// the song of the Furnace import. Returns 1 if the body was a song of nch channels, 0 if not (nothing is changed).
int loadsongbody(const unsigned char *body, int len, int nch, int ntsc, int sidmodel, int mult)
{
  int c;
  if (nch < 3 || nch > MAX_CHN || nch % 3) return 0;
  if (parsesongbody(body, len, nch, 0) <= 0) return 0;
  clearsong(1,1,1,1,1);
  numsids = nch / 3;
  numchannels = nch;
  for (c = 0; c < MAX_SIDS; c++) sidaddr[c] = 0xd400 + c * 0x20;
  editorInfo.ntsc = ntsc ? 1 : 0;
  editorInfo.sidmodel = sidmodel ? 1 : 0;
  if (mult >= 1 && mult <= 16) editorInfo.multiplier = mult;
  clearinactivechannels();
  parsesongbody(body, len, nch, 1);
  countpatternlengths();
  songchange();
  return 1;
}

void loadsong(void)
{
  int c;
  int ok = 0;
  char ident[4];
  char bodyversion = '5';
  FILE *handle;
  int len = 0;
  unsigned char *buf = readwholefile(songfilename, &len);

  statusmessage[0] = 0;

  if ((buf) && (len >= 4))
  {
    memcpy(ident, buf, 4);
    bodyversion = ident[3];

    // C64 GrooveGoat multi-SID format
    if (!memcmp(ident, "GGT1", 4))
    {
      int pos = 4;
      int n, nch, extra;
      if (len > 10)
      {
        n = buf[pos++];
        if ((n == 1) || (n == 2) || (n == 4) || (n == 8))
        {
          int flags = buf[pos++];
          int mult = buf[pos++];
          int adp = buf[pos] | (buf[pos+1] << 8);
          pos += 2;
          nch = n * 3;
          if (pos + n * 2 + 1 <= len)
          {
            unsigned addrs[MAX_SIDS];
            for (c = 0; c < n; c++)
            {
              addrs[c] = buf[pos] | (buf[pos+1] << 8);
              pos += 2;
            }
            extra = buf[pos++];
            pos += extra;
            if ((pos < len) && (parsesongbody(buf + pos, len - pos, nch, 0) > 0))
            {
              clearsong(1,1,1,1,1);
              numsids = n;
              numchannels = nch;
              for (c = 0; c < n; c++) sidaddr[c] = addrs[c];
              for (c = n; c < MAX_SIDS; c++) sidaddr[c] = sidaddr[c-1] + 0x20;
              editorInfo.ntsc = flags & 1;
              editorInfo.sidmodel = (flags >> 1) & 1;
              if (mult <= 16) editorInfo.multiplier = mult;
              editorInfo.adparam = adp;
              clearinactivechannels();
              parsesongbody(buf + pos, len - pos, nch, 1);
              ok = 1;
            }
          }
        }
      }
      if (ok)
      {
        countpatternlengths();
        songchange();
        snprintf(statusmessage, sizeof statusmessage, "Loaded %s (%d SIDs, C64 GrooveGoat format)", songfilename, numsids);
        bodyversion = '5';  // GGT1 contains a GTS5 body: no old format conversions
      }
    }

    // GoatTracker v2 / GoatTracker Stereo
    if ((!memcmp(ident, "GTS3", 4)) || (!memcmp(ident, "GTS4", 4)) || (!memcmp(ident, "GTS5", 4)))
    {
      int nch = detectchannels(buf + 4, len - 4);
      if (nch)
      {
        clearsong(1,1,1,1,1);
        numsids = nch / 3;
        numchannels = nch;
        for (c = 0; c < MAX_SIDS; c++) sidaddr[c] = 0xd400 + c * 0x20;
        clearinactivechannels();
        parsesongbody(buf + 4, len - 4, nch, 1);
        ok = 1;
        countpatternlengths();
        songchange();
        if (nch == 6)
          snprintf(statusmessage, sizeof statusmessage, "Loaded %s (2 SIDs, GoatTracker Stereo format)", songfilename);
        else
          snprintf(statusmessage, sizeof statusmessage, "Loaded %s (GoatTracker 2 format)", songfilename);
      }
    }
  }
  if (buf) free(buf);

  if (ok) goto LOADDONE;

  handle = fopen(songfilename, "rb");

  if (handle)
  {
    fread(ident, 4, 1, handle);
    if ((!memcmp(ident, "GTS2", 4)) || (!memcmp(ident, "GTS!", 4)))
    {
      // Old formats are always single SID
      if (numsids != 1)
      {
        clearsong(1,1,1,1,1);
        numsids = 1;
        numchannels = 3;
        clearinactivechannels();
      }
    }

    // Goattracker v2.xx (3-table) import
    if (!memcmp(ident, "GTS2", 4))
    {
      int d;
      int length;
      int amount;
      int loadsize;
      clearsong(1,1,1,1,1);
      ok = 1;

      // Read infotexts
      fread(songname, sizeof songname, 1, handle);
      fread(authorname, sizeof authorname, 1, handle);
      fread(copyrightname, sizeof copyrightname, 1, handle);

      // Read songorderlists
      amount = fread8(handle);
      for (d = 0; d < amount; d++)
      {
        for (c = 0; c < MAX_CHN; c++)
        {
          length = fread8(handle);
          loadsize = length;
          loadsize++;
          fread(songorder[d][c], loadsize, 1, handle);
        }
      }
      // Read instruments
      amount = fread8(handle);
      for (c = 1; c <= amount; c++)
      {
        instr[c].ad = fread8(handle);
        instr[c].sr = fread8(handle);
        instr[c].ptr[WTBL] = fread8(handle);
        instr[c].ptr[PTBL] = fread8(handle);
        instr[c].ptr[FTBL] = fread8(handle);
        instr[c].vibdelay = fread8(handle);
        instr[c].ptr[STBL] = makespeedtable(fread8(handle), editorInfo.finevibrato, 0) + 1;
        instr[c].gatetimer = fread8(handle);
        instr[c].firstwave = fread8(handle);
        fread(&instr[c].name, MAX_INSTRNAMELEN, 1, handle);
      }
      // Read tables
      for (c = 0; c < MAX_TABLES-1; c++)
      {
        loadsize = fread8(handle);
        fread(ltable[c], loadsize, 1, handle);
        fread(rtable[c], loadsize, 1, handle);
      }
      // Read patterns
      amount = fread8(handle);
      for (c = 0; c < amount; c++)
      {
        int d;
        length = fread8(handle) * 4;
        fread(pattern[c], length, 1, handle);

        // Convert speedtable-requiring commands
        for (d = 0; d < length; d++)
        {
          switch (pattern[c][d*4+2])
          {
            case CMD_FUNKTEMPO:
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], MST_FUNKTEMPO, 0) + 1;
            break;

            case CMD_PORTAUP:
            case CMD_PORTADOWN:
            case CMD_TONEPORTA:
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], MST_PORTAMENTO, 0) + 1;
            break;

            case CMD_VIBRATO:
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], editorInfo.finevibrato, 0) + 1;
            break;
          }
        }
      }
      countpatternlengths();
      songchange();
    }
    // Goattracker 1.xx import
    if (!memcmp(ident, "GTS!", 4))
    {
      int d;
      int length;
      int amount;
      int loadsize;
      int fw = 0;
      int fp = 0;
      int ff = 0;
      int fi = 0;
      int numfilter = 0;
      unsigned char filtertable[256];
      unsigned char filtermap[64];
      int arpmap[32][256];
      unsigned char pulse[32], pulseadd[32], pulselimitlow[32], pulselimithigh[32];
      int filterjumppos[64];

      clearsong(1,1,1,1,1);
      ok = 1;

      // Read infotexts
      fread(songname, sizeof songname, 1, handle);
      fread(authorname, sizeof authorname, 1, handle);
      fread(copyrightname, sizeof copyrightname, 1, handle);

      // Read songorderlists
      amount = fread8(handle);
      for (d = 0; d < amount; d++)
      {
        for (c = 0; c < MAX_CHN; c++)
        {
          length = fread8(handle);
          loadsize = length;
          loadsize++;
          fread(songorder[d][c], loadsize, 1, handle);
        }
      }

      // Convert instruments
      for (c = 1; c < 32; c++)
      {
        unsigned char wavelen;

        instr[c].ad = fread8(handle);
        instr[c].sr = fread8(handle);
        pulse[c] = fread8(handle);
        pulseadd[c] = fread8(handle);
        pulselimitlow[c] = fread8(handle);
        pulselimithigh[c] = fread8(handle);
        instr[c].ptr[FTBL] = fread8(handle); // Will be converted later
        if (instr[c].ptr[FTBL] > numfilter) numfilter = instr[c].ptr[FTBL];
        if (pulse[c] & 1) instr[c].gatetimer |= 0x80; // "No hardrestart" flag
        pulse[c] &= 0xfe;
        wavelen = fread8(handle)/2;
        fread(&instr[c].name, MAX_INSTRNAMELEN, 1, handle);
        instr[c].ptr[WTBL] = fw+1;

        // Convert wavetable
        for (d = 0; d < wavelen; d++)
        {
          if (fw < MAX_TABLELEN)
          {
            ltable[WTBL][fw] = fread8(handle);
            rtable[WTBL][fw] = fread8(handle);
            if (ltable[WTBL][fw] == 0xff)
              if (rtable[WTBL][fw]) rtable[WTBL][fw] += instr[c].ptr[WTBL]-1;
            if ((ltable[WTBL][fw] >= 0x8) && (ltable[WTBL][fw] <= 0xf))
              ltable[WTBL][fw] |= 0xe0;
            fw++;
          }
          else
          {
            fread8(handle);
            fread8(handle);
          }
        }

        // Remove empty wavetable afterwards
        if ((wavelen == 2) && (!ltable[WTBL][fw-2]) && (!rtable[WTBL][fw-2]))
        {
          instr[c].ptr[WTBL] = 0;
          fw -= 2;
          ltable[WTBL][fw] = 0;
          rtable[WTBL][fw] = 0;
          ltable[WTBL][fw+1] = 0;
          rtable[WTBL][fw+1] = 0;
        }

        // Convert pulsetable
        if (pulse[c])
        {
          int pulsetime, pulsedist, hlpos;

          // Check for duplicate pulse settings
          for (d = 1; d < c; d++)
          {
            if ((pulse[d] == pulse[c]) && (pulseadd[d] == pulseadd[c]) && (pulselimitlow[d] == pulselimitlow[c]) &&
                (pulselimithigh[d] == pulselimithigh[c]))
            {
              instr[c].ptr[PTBL] = instr[d].ptr[PTBL];
              goto PULSEDONE;
            }
          }

          // Initial pulse setting
          if (fp >= MAX_TABLELEN) goto PULSEDONE;
          instr[c].ptr[PTBL] = fp+1;
          ltable[PTBL][fp] = 0x80 | (pulse[c] >> 4);
          rtable[PTBL][fp] = pulse[c] << 4;
          fp++;

          // Pulse modulation
          if (pulseadd[c])
          {
            int startpulse = pulse[c]*16;
            int currentpulse = pulse[c]*16;
            // Phase 1: From startpos to high limit
            pulsedist = pulselimithigh[c]*16 - currentpulse;
            if (pulsedist > 0)
            {
              pulsetime = pulsedist/pulseadd[c];
              currentpulse += pulsetime*pulseadd[c];
              while (pulsetime)
              {
                int acttime = pulsetime;
                if (acttime > 127) acttime = 127;
                if (fp >= MAX_TABLELEN) goto PULSEDONE;
                ltable[PTBL][fp] = acttime;
                rtable[PTBL][fp] = pulseadd[c] / 2;
                fp++;
                pulsetime -= acttime;
              }
            }

            hlpos = fp;
            // Phase 2: from high limit to low limit
            pulsedist = currentpulse - pulselimitlow[c]*16;
            if (pulsedist > 0)
            {
              pulsetime = pulsedist/pulseadd[c];
              currentpulse -= pulsetime*pulseadd[c];
              while (pulsetime)
              {
                int acttime = pulsetime;
                if (acttime > 127) acttime = 127;
                if (fp >= MAX_TABLELEN) goto PULSEDONE;
                ltable[PTBL][fp] = acttime;
                rtable[PTBL][fp] = -(pulseadd[c] / 2);
                fp++;
                pulsetime -= acttime;
              }
            }

            // Phase 3: from low limit back to startpos/high limit
            if ((startpulse < pulselimithigh[c]*16) && (startpulse > currentpulse))
            {
              pulsedist = startpulse - currentpulse;
              if (pulsedist > 0)
              {
                pulsetime = pulsedist/pulseadd[c];
                while (pulsetime)
                {
                  int acttime = pulsetime;
                  if (acttime > 127) acttime = 127;
                  if (fp >= MAX_TABLELEN) goto PULSEDONE;
                  ltable[PTBL][fp] = acttime;
                  rtable[PTBL][fp] = pulseadd[c] / 2;
                  fp++;
                  pulsetime -= acttime;
                }
              }
              // Pulse jump back to beginning
              if (fp >= MAX_TABLELEN) goto PULSEDONE;
              ltable[PTBL][fp] = 0xff;
              rtable[PTBL][fp] = instr[c].ptr[PTBL] + 1;
              fp++;
            }
            else
            {
              pulsedist = pulselimithigh[c]*16 - currentpulse;
              if (pulsedist > 0)
              {
                pulsetime = pulsedist/pulseadd[c];
                while (pulsetime)
                {
                  int acttime = pulsetime;
                  if (acttime > 127) acttime = 127;
                  if (fp >= MAX_TABLELEN) goto PULSEDONE;
                  ltable[PTBL][fp] = acttime;
                  rtable[PTBL][fp] = pulseadd[c] / 2;
                  fp++;
                  pulsetime -= acttime;
                }
              }
              // Pulse jump back to beginning
              if (fp >= MAX_TABLELEN) goto PULSEDONE;
              ltable[PTBL][fp] = 0xff;
              rtable[PTBL][fp] = hlpos + 1;
              fp++;
            }
          }
          else
          {
            // Pulse stopped
            if (fp >= MAX_TABLELEN) goto PULSEDONE;
            ltable[PTBL][fp] = 0xff;
            rtable[PTBL][fp] = 0;
            fp++;
          }
          PULSEDONE: {}
        }
      }
      // Convert patterns
      amount = fread8(handle);
      for (c = 0; c < amount; c++)
      {
        length = fread8(handle);
        for (d = 0; d < length/3; d++)
        {
          unsigned char note, cmd, data, instr;
          note = fread8(handle);
          cmd = fread8(handle);
          data = fread8(handle);
          instr = cmd >> 3;
          cmd &= 7;

          switch(note)
          {
            default:
            note += FIRSTNOTE;
            if (note > LASTNOTE) note = REST;
            break;

            case OLDKEYOFF:
            note = KEYOFF;
            break;

            case OLDREST:
            note = REST;
            break;

            case ENDPATT:
            break;
          }
          switch(cmd)
          {
            case 5:
            cmd = CMD_SETFILTERPTR;
            if (data > numfilter) numfilter = data;
            break;

            case 7:
            if (data < 0xf0)
              cmd = CMD_SETTEMPO;
            else
            {
              cmd = CMD_SETMASTERVOL;
              data &= 0x0f;
            }
            break;
          }
          pattern[c][d*4] = note;
          pattern[c][d*4+1] = instr;
          pattern[c][d*4+2] = cmd;
          pattern[c][d*4+3] = data;
        }
      }
      countpatternlengths();
      fi = highestusedinstr + 1;
      songchange();

      // Read filtertable
      fread(filtertable, 256, 1, handle);

      // Convert filtertable
      for (c = 0; c < 64; c++)
      {
        filterjumppos[c] = -1;
        filtermap[c] = 0;
        if (filtertable[c*4+3] > numfilter) numfilter = filtertable[c*4+3];
      }

      if (numfilter > 63) numfilter = 63;

      for (c = 1; c <= numfilter; c++)
      {
        filtermap[c] = ff+1;

        if (filtertable[c*4]|filtertable[c*4+1]|filtertable[c*4+2]|filtertable[c*4+3])
        {
          // Filter set
          if (filtertable[c*4])
          {
            ltable[FTBL][ff] = 0x80 + (filtertable[c*4+1] & 0x70);
            rtable[FTBL][ff] = filtertable[c*4];
            ff++;
            if (filtertable[c*4+2])
            {
              ltable[FTBL][ff] = 0x00;
              rtable[FTBL][ff] = filtertable[c*4+2];
              ff++;
            }
          }
          else
          {
            // Filter modulation
            int time = filtertable[c*4+1];

            while (time)
            {
              int acttime = time;
              if (acttime > 127) acttime = 127;
              ltable[FTBL][ff] = acttime;
              rtable[FTBL][ff] = filtertable[c*4+2];
              ff++;
              time -= acttime;
            }
          }

          // Jump to next step: unnecessary if follows directly
          if (filtertable[c*4+3] != c+1)
          {
            filterjumppos[c] = ff;
            ltable[FTBL][ff] = 0xff;
            rtable[FTBL][ff] = filtertable[c*4+3]; // Fix the jump later
            ff++;
          }
        }
      }

      // Now fix jumps as the filterstep mapping is known
      for (c = 1; c <= numfilter; c++)
      {
        if (filterjumppos[c] != -1)
          rtable[FTBL][filterjumppos[c]] = filtermap[rtable[FTBL][filterjumppos[c]]];
      }

      // Fix filterpointers in instruments
      for (c = 1; c < 32; c++)
        instr[c].ptr[FTBL] = filtermap[instr[c].ptr[FTBL]];

      // Now fix pattern commands
      memset(arpmap, 0, sizeof arpmap);
      for (c = 0; c < MAX_PATT; c++)
      {
        unsigned char i = 0;
        for (d = 0; d <= MAX_PATTROWS; d++)
        {
          if (pattern[c][d*4+1]) i = pattern[c][d*4+1];

          // Convert portamento & vibrato
          if (pattern[c][d*4+2] == CMD_PORTAUP)
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], MST_PORTAMENTO, 0) + 1;
          if (pattern[c][d*4+2] == CMD_PORTADOWN)
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], MST_PORTAMENTO, 0) + 1;
          if (pattern[c][d*4+2] == CMD_TONEPORTA)
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], MST_PORTAMENTO, 0) + 1;
          if (pattern[c][d*4+2] == CMD_VIBRATO)
            pattern[c][d*4+3] = makespeedtable(pattern[c][d*4+3], MST_NOFINEVIB, 0) + 1;

          // Convert filterjump
          if (pattern[c][d*4+2] == CMD_SETFILTERPTR)
            pattern[c][d*4+3] = filtermap[pattern[c][d*4+3]];

          // Convert funktempo
          if ((pattern[c][d*4+2] == CMD_SETTEMPO) && (!pattern[c][d*4+3]))
          {
            pattern[c][d*4+2] = CMD_FUNKTEMPO;
            pattern[c][d*4+3] = makespeedtable((filtertable[2] << 4) | (filtertable[3] & 0x0f), MST_FUNKTEMPO, 0) + 1;
          }
          // Convert arpeggio
          if ((pattern[c][d*4+2] == CMD_DONOTHING) && (pattern[c][d*4+3]))
          {
            // Must be in conjunction with a note
            if ((pattern[c][d*4] >= FIRSTNOTE) && (pattern[c][d*4] <= LASTNOTE))
            {
              unsigned char param = pattern[c][d*4+3];
              if (i)
              {
                // Old arpeggio
                if (arpmap[i][param])
                {
                  // As command, or as instrument?
                  if (arpmap[i][param] < 256)
                  {
                    pattern[c][d*4+2] = CMD_SETWAVEPTR;
                    pattern[c][d*4+3] = arpmap[i][param];
                  }
                  else
                  {
                    pattern[c][d*4+1] = arpmap[i][param] - 256;
                    pattern[c][d*4+3] = 0;
                  }
                }
                else
                {
                  int e;
                  unsigned char arpstart;
                  unsigned char arploop;

                  // New arpeggio
                  // Copy first the instrument's wavetable up to loop/end point
                  arpstart = fw + 1;
                  if (instr[i].ptr[WTBL])
                  {
                    for (e = instr[i].ptr[WTBL]-1;; e++)
                    {
                      if (ltable[WTBL][e] == 0xff) break;
                      if (fw < MAX_TABLELEN)
                      {
                        ltable[WTBL][fw] = ltable[WTBL][e];
                        fw++;
                      }
                    }
                  }
                  // Then make the arpeggio
                  arploop = fw + 1;
                  if (fw < MAX_TABLELEN-3)
                  {
                    ltable[WTBL][fw] = (param & 0x80) >> 7;
                    rtable[WTBL][fw] = (param  & 0x70) >> 4;
                    fw++;
                    ltable[WTBL][fw] = (param & 0x80) >> 7;
                    rtable[WTBL][fw] = (param & 0xf);
                    fw++;
                    ltable[WTBL][fw] = (param & 0x80) >> 7;
                    rtable[WTBL][fw] = 0;
                    fw++;
                    ltable[WTBL][fw] = 0xff;
                    rtable[WTBL][fw] = arploop;
                    fw++;

                    // Create new instrument if possible
                    if (fi < MAX_INSTR)
                    {
                      arpmap[i][param] = fi + 256;
                      instr[fi] = instr[i];
                      instr[fi].ptr[WTBL] = arpstart;
                      // Add arpeggio parameter to new instrument name
                      if (strlen(instr[fi].name) < MAX_INSTRNAMELEN-3)
                      {
                        char arpname[8];
                        sprintf(arpname, "0%02X", param&0x7f);
                        strcat(instr[fi].name, arpname);
                      }
                      fi++;
                    }
                    else
                    {
                      arpmap[i][param] = arpstart;
                    }
                  }

                  if (arpmap[i][param])
                  {
                    // As command, or as instrument?
                    if (arpmap[i][param] < 256)
                    {
                      pattern[c][d*4+2] = CMD_SETWAVEPTR;
                      pattern[c][d*4+3] = arpmap[i][param];
                    }
                    else
                    {
                      pattern[c][d*4+1] = arpmap[i][param] - 256;
                      pattern[c][d*4+3] = 0;
                    }
                  }
                }
              }
            }
            // If arpeggio could not be converted, databyte zero
            if (!pattern[c][d*4+2])
              pattern[c][d*4+3] = 0;
          }
        }
      }
    }
    fclose(handle);
    if ((ok) && (!statusmessage[0]))
      snprintf(statusmessage, sizeof statusmessage, "Loaded %s (imported old GoatTracker format)", songfilename);
  }
  LOADDONE:
  if (!ok)
    snprintf(statusmessage, sizeof statusmessage, "Could not load %s - unknown or damaged file", songfilename);
  if (ok)
  {
    songmodified = 0;
    strcpy(loadedsongfilename, songfilename);
    backup_saved();

    // Reset table views
    for (c = 0; c < MAX_TABLES; c++) settableview(c, 0);

    // Convert pulsemodulation speed of < v2.4 songs
    if (bodyversion < '4')
    {
      for (c = 0; c < MAX_TABLELEN; c++)
      {
        if ((ltable[PTBL][c] < 0x80) && (rtable[PTBL][c]))
        {
          int speed = ((signed char)rtable[PTBL][c]);
          speed <<= 1;
          if (speed > 127) speed = 127;
          if (speed < -128) speed = -128;
          rtable[PTBL][c] = speed;
        }
      }
    }

    // Convert old legato/nohr parameters
    if (bodyversion < '5')
    {
        for (c = 1; c < MAX_INSTR; c++)
        {
            if (instr[c].firstwave >= 0x80)
            {
                instr[c].gatetimer |= 0x80;
                instr[c].firstwave &= 0x7f;
            }
            if (!instr[c].firstwave) instr[c].gatetimer |= 0x40;
        }
    }
  }
}

// Snapshots of all data that loading or deleting an instrument can change:
// the instruments (table pointers of all instruments move when table rows are
// deleted), the tables and the patterns (table commands like 8XY or 4XY are
// renumbered). Used by the instrument previews, which load instruments only
// temporarily.
typedef struct
{
  INSTR instr[MAX_INSTR];
  unsigned char l[MAX_TABLES][MAX_TABLELEN];
  unsigned char r[MAX_TABLES][MAX_TABLELEN];
  unsigned char patt[MAX_PATT][MAX_PATTROWS*4+4];
} SONGDATA;

void *songdata_save(void)
{
  SONGDATA *d = (SONGDATA *)malloc(sizeof(SONGDATA));
  if (!d) return NULL;
  memcpy(d->instr, instr, sizeof instr);
  memcpy(d->l, ltable, sizeof ltable);
  memcpy(d->r, rtable, sizeof rtable);
  memcpy(d->patt, pattern, sizeof pattern);
  return d;
}

void songdata_restore(const void *data)
{
  const SONGDATA *d = (const SONGDATA *)data;
  if (!d) return;
  memcpy(instr, d->instr, sizeof instr);
  memcpy(ltable, d->l, sizeof ltable);
  memcpy(rtable, d->r, sizeof rtable);
  memcpy(pattern, d->patt, sizeof pattern);
}

void songdata_free(void *data)
{
  free(data);
}

// Copy of instruments, tables and patterns, so that a failed instrument load
// can be undone completely (save = 1 stores, save = 0 restores)
int instrloadfailed = 0;

void snapshotsongdata(int save)
{
  static INSTR *si = NULL;
  static unsigned char *sl = NULL, *sr = NULL, *sp = NULL;
  if (!si)
  {
    si = (INSTR *)malloc(sizeof instr);
    sl = (unsigned char *)malloc(sizeof ltable);
    sr = (unsigned char *)malloc(sizeof rtable);
    sp = (unsigned char *)malloc(sizeof pattern);
    if ((!si) || (!sl) || (!sr) || (!sp)) return;
  }
  if (save)
  {
    memcpy(si, instr, sizeof instr);
    memcpy(sl, ltable, sizeof ltable);
    memcpy(sr, rtable, sizeof rtable);
    memcpy(sp, pattern, sizeof pattern);
  }
  else
  {
    memcpy(instr, si, sizeof instr);
    memcpy(ltable, sl, sizeof ltable);
    memcpy(rtable, sr, sizeof rtable);
    memcpy(pattern, sp, sizeof pattern);
  }
}

void loadinstrument(void)
{
  char ident[4];
  FILE *handle;
  int c,d;
  int pulsestart = -1;
  int pulseend = -1;

  handle = fopen(instrfilename, "rb");
  if (handle)
  {
    int overflow = 0;
    stopsong();
    snapshotsongdata(1);
    fread(ident, 4, 1, handle);

    if ((!memcmp(ident, "GTI3", 4)) || (!memcmp(ident, "GTI4", 4)) || (!memcmp(ident, "GTI5", 4)))
    {
      unsigned char optr[4];

      instr[editorInfo.einum].ad = fread8(handle);
      instr[editorInfo.einum].sr = fread8(handle);
      optr[0] = fread8(handle);
      optr[1] = fread8(handle);
      optr[2] = fread8(handle);
      optr[3] = fread8(handle);
      instr[editorInfo.einum].vibdelay = fread8(handle);
      instr[editorInfo.einum].gatetimer = fread8(handle);
      instr[editorInfo.einum].firstwave = fread8(handle);
      fread(&instr[editorInfo.einum].name, MAX_INSTRNAMELEN, 1, handle);

      // Erase old tabledata
      deleteinstrtable(editorInfo.einum);

      // Load new tabledata
      for (c = 0; c < MAX_TABLES; c++)
      {
        int start = gettablelen(c);
        int len = fread8(handle);
        if (start + len > MAX_TABLELEN) overflow = 1;

        if (len)
        {
          for (d = start; (d < start+len) && (d < MAX_TABLELEN); d++)
            ltable[c][d] = fread8(handle);
          while (d < start+len)
          {
            fread8(handle);
            d++;
          }
          for (d = start; (d < start+len) && (d < MAX_TABLELEN); d++)
            rtable[c][d] = fread8(handle);
          while (d < start+len)
          {
            fread8(handle);
            d++;
          }
          if (c != STBL)
          {
            for (d = start; (d < start+len) && (d < MAX_TABLELEN); d++)
            {
              if (ltable[c][d] == 0xff)
              {
                if (rtable[c][d])
                  rtable[c][d] = rtable[c][d] - optr[c] + start + 1;
              }
            }
          }
          if (c == PTBL)
          {
            pulsestart = start;
            pulseend = start + len;
          }
          instr[editorInfo.einum].ptr[c] = start + 1;
        }
        else instr[editorInfo.einum].ptr[c] = 0;
      }
    }

    // Goattracker v2.xx (3-table) import
    if (!memcmp(ident, "GTI2", 4))
    {
      unsigned char optr[3];

      instr[editorInfo.einum].ad = fread8(handle);
      instr[editorInfo.einum].sr = fread8(handle);
      optr[0] = fread8(handle);
      optr[1] = fread8(handle);
      optr[2] = fread8(handle);
      instr[editorInfo.einum].vibdelay = fread8(handle);
      instr[editorInfo.einum].ptr[STBL] = makespeedtable(fread8(handle), editorInfo.finevibrato, 0) + 1;
      instr[editorInfo.einum].gatetimer = fread8(handle);
      instr[editorInfo.einum].firstwave = fread8(handle);
      fread(&instr[editorInfo.einum].name, MAX_INSTRNAMELEN, 1, handle);

      // Erase old tabledata
      deleteinstrtable(editorInfo.einum);

      // Load new tabledata
      for (c = 0; c < MAX_TABLES-1; c++)
      {
        int start = gettablelen(c);
        int len = fread8(handle);
        if (start + len > MAX_TABLELEN) overflow = 1;

        if (len)
        {
          for (d = start; (d < start+len) && (d < MAX_TABLELEN); d++)
            ltable[c][d] = fread8(handle);
          while (d < start+len)
          {
            fread8(handle);
            d++;
          }
          for (d = start; (d < start+len) && (d < MAX_TABLELEN); d++)
            rtable[c][d] = fread8(handle);
          while (d < start+len)
          {
            fread8(handle);
            d++;
          }
          for (d = start; (d < start+len) && (d < MAX_TABLELEN); d++)
          {
            if (ltable[c][d] == 0xff)
            {
              if (rtable[c][d])
                rtable[c][d] = rtable[c][d] - optr[c] + start + 1;
            }
          }
          if (c == PTBL)
          {
            pulsestart = start;
            pulseend = start + len;
          }
          instr[editorInfo.einum].ptr[c] = start + 1;
        }
        else instr[editorInfo.einum].ptr[c] = 0;
      }
    }
    // Goattracker 1.xx import
    if (!memcmp(ident, "GTI!", 4))
    {

      unsigned char pulse, pulseadd, pulselimitlow, pulselimithigh, wavelen;
      unsigned char filtertemp[4];
      int fw, fp, ff;

      // Erase old tabledata
      deleteinstrtable(editorInfo.einum);

      fw = gettablelen(WTBL);
      fp = gettablelen(PTBL);
      ff = gettablelen(FTBL);

      instr[editorInfo.einum].ad = fread8(handle);
      instr[editorInfo.einum].sr = fread8(handle);
      if (editorInfo.multiplier)
        instr[editorInfo.einum].gatetimer = 2 * editorInfo.multiplier;
      else
        instr[editorInfo.einum].gatetimer = 1;
      instr[editorInfo.einum].firstwave = 0x9;
      pulse = fread8(handle);
      pulseadd = fread8(handle);
      pulselimitlow = fread8(handle);
      pulselimithigh = fread8(handle);
      instr[editorInfo.einum].ptr[FTBL] = fread8(handle) ? ff+1 : 0;
      if (pulse & 1) instr[editorInfo.einum].gatetimer |= 0x80; // "No hardrestart" flag
        wavelen = fread8(handle)/2;
      fread(&instr[editorInfo.einum].name, MAX_INSTRNAMELEN, 1, handle);
      instr[editorInfo.einum].ptr[WTBL] = fw+1;

      // Convert wavetable
      for (d = 0; d < wavelen; d++)
      {
        if (fw < MAX_TABLELEN)
        {
          ltable[WTBL][fw] = fread8(handle);
          rtable[WTBL][fw] = fread8(handle);
          if (ltable[WTBL][fw] == 0xff)
            if (rtable[WTBL][fw]) rtable[WTBL][fw] += instr[editorInfo.einum].ptr[WTBL]-1;
          fw++;
        }
        else
        {
          fread8(handle);
          fread8(handle);
        }
      }

      // Remove empty wavetable afterwards
      if ((wavelen == 2) && (!ltable[WTBL][fw-2]) && (!rtable[WTBL][fw-2]))
      {
        instr[editorInfo.einum].ptr[WTBL] = 0;
        fw -= 2;
        ltable[WTBL][fw] = 0;
        rtable[WTBL][fw] = 0;
        ltable[WTBL][fw+1] = 0;
        rtable[WTBL][fw+1] = 0;
      }

      // Convert pulsetable
      pulse &= 0xfe;
      if (pulse)
      {
        int pulsetime, pulsedist, hlpos;

        // Initial pulse setting
        if (fp >= MAX_TABLELEN) goto PULSEDONE;
        pulsestart = fp;
        instr[editorInfo.einum].ptr[PTBL] = fp+1;
        ltable[PTBL][fp] = 0x80 | (pulse >> 4);
        rtable[PTBL][fp] = pulse << 4;
        fp++;

        // Pulse modulation
        if (pulseadd)
        {
          int startpulse = pulse*16;
          int currentpulse = pulse*16;
          // Phase 1: From startpos to high limit
          pulsedist = pulselimithigh*16 - currentpulse;
          if (pulsedist > 0)
          {
            pulsetime = pulsedist/pulseadd;
            currentpulse += pulsetime*pulseadd;
            while (pulsetime)
            {
              int acttime = pulsetime;
              if (acttime > 127) acttime = 127;
              if (fp >= MAX_TABLELEN) goto PULSEDONE;
              ltable[PTBL][fp] = acttime;
              rtable[PTBL][fp] = pulseadd / 2;
              fp++;
              pulsetime -= acttime;
            }
          }

          hlpos = fp;
          // Phase 2: from high limit to low limit
          pulsedist = currentpulse - pulselimitlow*16;
          if (pulsedist > 0)
          {
            pulsetime = pulsedist/pulseadd;
            currentpulse -= pulsetime*pulseadd;
            while (pulsetime)
            {
              int acttime = pulsetime;
              if (acttime > 127) acttime = 127;
              if (fp >= MAX_TABLELEN) goto PULSEDONE;
              ltable[PTBL][fp] = acttime;
              rtable[PTBL][fp] = -(pulseadd / 2);
              fp++;
              pulsetime -= acttime;
            }
          }

          // Phase 3: from low limit back to startpos/high limit
          if ((startpulse < pulselimithigh*16) && (startpulse > currentpulse))
          {
            pulsedist = startpulse - currentpulse;
            if (pulsedist > 0)
            {
              pulsetime = pulsedist/pulseadd;
              while (pulsetime)
              {
                int acttime = pulsetime;
                if (acttime > 127) acttime = 127;
                if (fp >= MAX_TABLELEN) goto PULSEDONE;
                ltable[PTBL][fp] = acttime;
                rtable[PTBL][fp] = pulseadd / 2;
                fp++;
                pulsetime -= acttime;
              }
            }
            // Pulse jump back to beginning
            if (fp >= MAX_TABLELEN) goto PULSEDONE;
            ltable[PTBL][fp] = 0xff;
            rtable[PTBL][fp] = instr[editorInfo.einum].ptr[PTBL] + 1;
            fp++;
          }
          else
          {
            pulsedist = pulselimithigh*16 - currentpulse;
            if (pulsedist > 0)
            {
              pulsetime = pulsedist/pulseadd;
              while (pulsetime)
              {
                int acttime = pulsetime;
                if (acttime > 127) acttime = 127;
                if (fp >= MAX_TABLELEN) goto PULSEDONE;
                ltable[PTBL][fp] = acttime;
                rtable[PTBL][fp] = pulseadd / 2;
                fp++;
                pulsetime -= acttime;
              }
            }
            // Pulse jump back to beginning
            if (fp >= MAX_TABLELEN) goto PULSEDONE;
            ltable[PTBL][fp] = 0xff;
            rtable[PTBL][fp] = hlpos + 1;
            fp++;
          }
        }
        else
        {
          // Pulse stopped
          if (fp >= MAX_TABLELEN) goto PULSEDONE;
          ltable[PTBL][fp] = 0xff;
          rtable[PTBL][fp] = 0;
          fp++;
        }
        PULSEDONE:
        pulseend = fp;
      }

      // Convert filter (if any)
      if ((instr[editorInfo.einum].ptr[FTBL]) && (ff < MAX_TABLELEN-2))
      {
        fread(filtertemp, sizeof filtertemp, 1, handle);
        // Filter set
        if (filtertemp[0])
        {
          ltable[FTBL][ff] = 0x80 + (filtertemp[1] & 0x70);
          rtable[FTBL][ff] = filtertemp[0];
          ff++;
          if (filtertemp[2])
          {
            ltable[FTBL][ff] = 0x00;
            rtable[FTBL][ff] = filtertemp[2];
            ff++;
          }
        }
        else
        {
          // Filter modulation
          int time = filtertemp[1];

          while (time)
          {
            int acttime = time;
            if (acttime > 127) acttime = 127;
            ltable[FTBL][ff] = acttime;
            rtable[FTBL][ff] = filtertemp[2];
            ff++;
            time -= acttime;
          }
        }

        // Jump to next step: always end the filter
        ltable[FTBL][ff] = 0xff;
        rtable[FTBL][ff] = 0;
        ff++;
      }
    }
    
    fclose(handle);

    // Convert pulsemodulation speed of < v2.4 instruments
    if ((ident[3] < '4') && (pulsestart != -1))
    {
      for (c = pulsestart; (c < pulseend) && (c < MAX_TABLELEN); c++)
      {
        if ((ltable[PTBL][c] < 0x80) && (rtable[PTBL][c]))
        {
          int speed = ((signed char)rtable[PTBL][c]);
          speed <<= 1;
          if (speed > 127) speed = 127;
          if (speed < -128) speed = -128;
          rtable[PTBL][c] = speed;
        }
      }
    }
    // Convert old legato/nohr parameters
    if (ident[3] < '5')
    {
      if (instr[editorInfo.einum].firstwave >= 0x80)
      {
        instr[editorInfo.einum].firstwave &= 0x7f;
        instr[editorInfo.einum].gatetimer |= 0x80;
      }
      if (!instr[editorInfo.einum].firstwave) instr[editorInfo.einum].gatetimer |= 0x40;
    }
    // Not enough room in the tables: keep the song as it was
    if (overflow)
    {
      snapshotsongdata(0);
      snprintf(statusmessage, sizeof statusmessage, "Not enough free table rows for %s - delete unused table data first (SHIFT+O in the tables)", instrfilename);
      instrloadfailed = 1;
    }
    else instrloadfailed = 0;
  }
}

void clearsong(int cs, int cp, int ci, int ct, int cn)
{
  int c;

  if (!(cs | cp | ci | ct | cn)) return;

  stopsong();

  for (c = 0; c < MAX_SIDS; c++) sidfilt[c].masterfader = 0x0f;
  editorInfo.epmarkchn = -1;
  editorInfo.etmarknum = -1;
  editorInfo.esmarkchn = -1;
  followplay = 0;

  for (c = 0; c < MAX_CHN; c++)
  {
    int d;
    chn[c].mute = 0;
    if (editorInfo.multiplier)
      chn[c].tempo = editorInfo.multiplier*6-1;
    else
      chn[c].tempo = 6-1;
    chn[c].pattptr = 0;
    if (cs)
    {
      memset(loadedsongfilename, 0, sizeof loadedsongfilename);
      for (d = 0; d < MAX_SONGS; d++)
      {
        memset(&songorder[d][c][0], 0, MAX_SONGLEN+2);
        if ((!d) && (c < numchannels))
        {
          songorder[d][c][0] = c;
          songorder[d][c][1] = LOOPSONG;
        }
        else
        {
          songorder[d][c][0] = LOOPSONG;
        }
      }
	  editorUndoInfo.editorInfo[c].epnum = songorder[0][c][0];
	  editorUndoInfo.editorInfo[c].espos = 0;
	  editorUndoInfo.editorInfo[c].esend = 0;
    }
  }
  if (cs)
  {
    songmodified = 0;
    backup_saved();
    editorInfo.esview = 0;
    editorInfo.eseditpos = 0;
    editorInfo.escolumn = 0;
    editorInfo.eschn = 0;
    editorInfo.esnum = 0;
    editorInfo.eppos = 0;
    editorInfo.epview =-VISIBLEPATTROWS/2;
    editorInfo.epcolumn = 0;
    editorInfo.epchn = 0;
  }
  if (cn)
  {
    memset(songname, 0, sizeof songname);
    memset(authorname, 0, sizeof authorname);
    memset(copyrightname, 0, sizeof copyrightname);
    editorInfo.enpos = 0;
  }
  if (cp)
  {
    memset(loadedsongfilename, 0, sizeof loadedsongfilename);
    for (c = 0; c < MAX_PATT; c++)
      clearpattern(c);
  }
  if (ci)
  {
    for (c = 0; c < MAX_INSTR; c++)
      clearinstr(c);
    memset(&instrcopybuffer, 0, sizeof(INSTR));
    editorInfo.eipos = 0;
    editorInfo.eicolumn = 0;
    editorInfo.einum = 1;
  }
  if (ct == 1)
  {
    for (c = MAX_TABLES-1; c >= 0; c--)
    {
      memset(ltable[c], 0, MAX_TABLELEN);
      memset(rtable[c], 0, MAX_TABLELEN);
      settableview(c, 0);
    }
  }
  countpatternlengths();

  if (cs)
	  undoInitAllAreas();
}

void countpatternlengths(void)
{
  int c, d, e;

  highestusedpattern = 0;
  highestusedinstr = 0;
  for (c = 0; c < MAX_PATT; c++)
  {
    for (d = 0; d <= MAX_PATTROWS; d++)
    {
      if (pattern[c][d*4] == ENDPATT) break;
      if ((pattern[c][d*4] != REST) || (pattern[c][d*4+1]) || (pattern[c][d*4+2]) || (pattern[c][d*4+3]))
        highestusedpattern = c;
      if (pattern[c][d*4+1] > highestusedinstr) highestusedinstr = pattern[c][d*4+1];
    }
    pattlen[c] = d;
  }

  for (e = 0; e < MAX_SONGS; e++)
  {
    for (c = 0; c < MAX_CHN; c++)
    {
      for (d = 0; d < MAX_SONGLEN; d++)
      {
        if (songorder[e][c][d] >= LOOPSONG) break;
        if ((songorder[e][c][d] < REPEAT) && (songorder[e][c][d] > highestusedpattern))
          highestusedpattern = songorder[e][c][d];
      }
      songlen[e][c] = d;
    }
  }
}

void countthispattern(void)
{
  int c, d, e;

  c = editorUndoInfo.editorInfo[editorInfo.epchn].epnum;
  for (d = 0; d <= MAX_PATTROWS; d++)
  {
    if (pattern[c][d*4] == ENDPATT) break;
  }
  pattlen[c] = d;

  e = editorInfo.esnum;
  c = editorInfo.eschn;
  for (d = 0; d < MAX_SONGLEN; d++)
  {
    if (songorder[e][c][d] >= LOOPSONG) break;
    if (songorder[e][c][d] > highestusedpattern)
      highestusedpattern = songorder[e][c][d];
  }
  songlen[e][c] = d;
}

int insertpattern(int p)
{
  int c, d, e;

  findusedpatterns();
  if (p >= MAX_PATT-2) return 0;
  if (pattused[MAX_PATT-1]) return 0;

  // Mark all patterns from this point onwards for undo.
  for (int i = p + 1;i < MAX_PATT - p - 2;i++)
  {
	  undoAreaSetCheckForChange(UNDO_AREA_PATTERN, i, UNDO_AREA_DIRTY_CHECK);
  }

  memmove(pattern[p+2], pattern[p+1], (MAX_PATT-p-2)*(MAX_PATTROWS*4+4));  
  countpatternlengths();

  for (c = 0; c < MAX_SONGS; c++)
  {
    if (songvalid(c))
    {
      for (d = 0; d < MAX_CHN; d++)
      {
        for (e = 0; e < songlen[c][d]; e++)
        {
          if ((songorder[c][d][e] < REPEAT) && (songorder[c][d][e] > p) && (songorder[c][d][e] != MAX_PATT-1))
            songorder[c][d][e]++;
        }
      }
    }
  }
  
  for (c = 0; c < MAX_CHN; c++)
  {
    if ((editorUndoInfo.editorInfo[c].epnum > p) && (editorUndoInfo.editorInfo[c].epnum != MAX_PATT-1)) editorUndoInfo.editorInfo[c].epnum++;
  }

  return 1;
}

void deletepattern(int p)
{
  int c, d, e;

  if (p == MAX_PATT-1) return;

  memmove(pattern[p], pattern[p+1], (MAX_PATT-p-1)*(MAX_PATTROWS*4+4));
  clearpattern(MAX_PATT-1);
  countpatternlengths();

  for (c = 0; c < MAX_SONGS; c++)
  {
    if (songvalid(c))
    {
      for (d = 0; d < MAX_CHN; d++)
      {
        for (e = 0; e < songlen[c][d]; e++)
        {
          if ((songorder[c][d][e] < REPEAT) && (songorder[c][d][e] > p))
            songorder[c][d][e]--;
        }
      }
    }
  }
  
  for (c = 0; c < MAX_CHN; c++)
  {
    if (editorUndoInfo.editorInfo[c].epnum > p) editorUndoInfo.editorInfo[c].epnum--;
  }
}

void clearpattern(int p)
{
  int c;

  memset(pattern[p], 0, MAX_PATTROWS*4);
  for (c = 0; c < defaultpatternlength; c++) pattern[p][c*4] = REST;
  for (c = defaultpatternlength; c <= MAX_PATTROWS; c++) pattern[p][c*4] = ENDPATT;
}

void findusedpatterns(void)
{
  int c, d, e;

  countpatternlengths();
  memset(pattused, 0, sizeof pattused);
  for (c = 0; c < MAX_SONGS; c++)
  {
    if (songvalid(c))
    {
      for (d = 0; d < MAX_CHN; d++)
      {
        for (e = 0; e < songlen[c][d]; e++)
        {
          if (songorder[c][d][e] < REPEAT)
            pattused[songorder[c][d][e]] = 1;
        }
      }
    }
  }
}

void findduplicatepatterns(void)
{
  int c, d;

  findusedpatterns();

  for (c = 0; c < MAX_PATT; c++)
  {
    if (pattused[c])
    {
      for (d = c+1; d < MAX_PATT; d++)
      {
        if (pattlen[d] == pattlen[c])
        {
          if (!memcmp(pattern[c], pattern[d], pattlen[c]*4))
          {
            int f, g, h;

            for (f = 0; f < MAX_SONGS; f++)
            {
              if (songvalid(f))
              {
                for (g = 0; g < MAX_CHN; g++)
                {
                  for (h = 0; h < songlen[f][g]; h++)
                  {
                    if (songorder[f][g][h] == d)
                      songorder[f][g][h] = c;
                  }
                }
              }
            }
            for (f = 0; f < MAX_CHN; f++)
              if (editorUndoInfo.editorInfo[f].epnum == d) editorUndoInfo.editorInfo[f].epnum = c;
          }
        }
      }
    }
  }

  findusedpatterns();
}

void optimizeeverything(int oi, int ot)
{
  int c, d, e;

  stopsong();

  findduplicatepatterns();

  memset(instrused, 0, sizeof instrused);

  for (c = MAX_PATT-1; c >= 0; c--)
  {
    if (pattused[c])
    {
      for (d = 0; d < MAX_PATTROWS; d++)
      {
        if (pattern[c][d*4] == ENDPATT) break;
        if (pattern[c][d*4+1])
          instrused[pattern[c][d*4+1]] = 1;
      }
    }
    else deletepattern(c);
  }

  countpatternlengths();

  if (oi)
  {
    for (c = MAX_INSTR-2; c >= 1; c--)
    {
      if (!instrused[c])
      {
        clearinstr(c);

        if (c < MAX_INSTR-2)
        {
          memmove(&instr[c], &instr[c+1], (MAX_INSTR-2-c) * sizeof(INSTR));
          clearinstr(MAX_INSTR-2);
          for (d = 0; d < MAX_PATT; d++)
          {
            for (e = 0; e < pattlen[d]; e++)
            {
              if ((pattern[d][e*4+1] > c) && (pattern[d][e*4+1] != MAX_INSTR-1))
                pattern[d][e*4+1]--;
            }
          }
        }
      }
    }
  }

  if (ot)
  {
    for (c = 0; c < MAX_TABLES; c++) optimizetable(c);
  }
}

void mergesong(void)
{
  int c, d, e;
  int songbase;
  int pattbase;
  int instrbase;
  int tablebase[MAX_TABLES];
  int len = 0;
  int pos = 0;
  int fnch = 0;
  int emptypatt = -1;
  unsigned char *buf;

  statusmessage[0] = 0;

  // Determine amount of patterns & instruments
  countpatternlengths();
  highestusedinstr = 0;
  for (c = 1; c < MAX_INSTR; c++)
  {
    if ((instr[c].ad) || (instr[c].sr) || (instr[c].ptr[0]) || (instr[c].ptr[1]) ||
        (instr[c].ptr[2]) || (instr[c].vibdelay) || (instr[c].ptr[3]))
    {
      if (c > highestusedinstr) highestusedinstr = c;
    }
  }

  pattbase = highestusedpattern + 1;
  instrbase = highestusedinstr;
  songbase = countsongs();
  if (!songvalid(songbase - 1)) songbase--;

  for (c = 0; c < MAX_TABLES; c++)
    tablebase[c] = gettablelen(c);

  buf = readwholefile(songfilename, &len);
  if ((!buf) || (len < 4))
  {
    snprintf(statusmessage, sizeof statusmessage, "Could not read %s", songfilename);
    free(buf);
    return;
  }

  if (!memcmp(buf, "GGT1", 4))
  {
    int n = buf[4];
    fnch = n * 3;
    pos = 4 + 1 + 1 + 1 + 2 + n * 2;
    if (pos < len) pos += 1 + buf[pos];
  }
  else if ((!memcmp(buf, "GTS3", 4)) || (!memcmp(buf, "GTS4", 4)) || (!memcmp(buf, "GTS5", 4)))
  {
    fnch = detectchannels(buf + 4, len - 4);
    pos = 4;
  }
  if ((!fnch) || (pos >= len) || (parsesongbody(buf + pos, len - pos, fnch, 0) <= 0))
  {
    snprintf(statusmessage, sizeof statusmessage, "Merge: unsupported or damaged file %s", songfilename);
    free(buf);
    return;
  }
  if (fnch > numchannels)
  {
    snprintf(statusmessage, sizeof statusmessage, "Merge: the song uses %d SIDs - raise the SID count first", fnch / 3);
    free(buf);
    return;
  }

  // Skip infotexts
  pos += MAX_STR * 3;

  // Read songorderlists
  {
    int amount = buf[pos++];
    if (amount + songbase > MAX_SONGS)
    {
      snprintf(statusmessage, sizeof statusmessage, "Merge: too many subtunes");
      goto ABORT;
    }
    for (d = 0; d < amount; d++)
    {
      for (c = 0; c < fnch; c++)
      {
        int length = buf[pos++];
        int loadsize = length + 1;
        memcpy(songorder[songbase + d][c], buf + pos, loadsize);
        pos += loadsize;
        // Remap patterns
        for (e = 0; e < loadsize - 1; e++)
        {
          if (songorder[songbase + d][c][e] < REPEAT)
            songorder[songbase + d][c][e] += pattbase;
        }
      }
      // Channels the merged song does not have play an empty pattern
      for (c = fnch; c < numchannels; c++)
      {
        if (emptypatt < 0)
        {
          emptypatt = findfreepattern(pattbase + 100 < MAX_PATT ? pattbase + 100 : pattbase);
          if (emptypatt < 0) emptypatt = MAX_PATT - 1;
        }
        memset(songorder[songbase + d][c], 0, MAX_SONGLEN + 2);
        songorder[songbase + d][c][0] = emptypatt;
        songorder[songbase + d][c][1] = LOOPSONG;
        songorder[songbase + d][c][2] = 0;
      }
    }
  }
  // Read instruments
  {
    int amount = buf[pos++];
    if (amount + instrbase >= MAX_INSTR)
    {
      snprintf(statusmessage, sizeof statusmessage, "Merge: too many instruments");
      goto ABORT;
    }
    for (c = 1; c <= amount; c++)
    {
      INSTR *ip = &instr[c + instrbase];
      ip->ad = buf[pos];
      ip->sr = buf[pos+1];
      ip->ptr[WTBL] = buf[pos+2];
      ip->ptr[PTBL] = buf[pos+3];
      ip->ptr[FTBL] = buf[pos+4];
      ip->ptr[STBL] = buf[pos+5];
      if (ip->ptr[WTBL] > 0) ip->ptr[WTBL] += tablebase[WTBL];
      if (ip->ptr[PTBL] > 0) ip->ptr[PTBL] += tablebase[PTBL];
      if (ip->ptr[FTBL] > 0) ip->ptr[FTBL] += tablebase[FTBL];
      if (ip->ptr[STBL] > 0) ip->ptr[STBL] += tablebase[STBL];
      ip->vibdelay = buf[pos+6];
      ip->gatetimer = buf[pos+7];
      ip->firstwave = buf[pos+8];
      memcpy(ip->name, buf + pos + 9, MAX_INSTRNAMELEN);
      ip->name[MAX_INSTRNAMELEN-1] = 0;
      pos += 9 + MAX_INSTRNAMELEN;
    }
  }
  // Read tables
  for (c = 0; c < MAX_TABLES; c++)
  {
    int loadsize = buf[pos++];
    if (loadsize + tablebase[c] > MAX_TABLELEN)
    {
      snprintf(statusmessage, sizeof statusmessage, "Merge: tables are full");
      goto ABORT;
    }
    memcpy(&ltable[c][tablebase[c]], buf + pos, loadsize);
    memcpy(&rtable[c][tablebase[c]], buf + pos + loadsize, loadsize);
    pos += loadsize * 2;
    // Remap jumps and tablecommands
    for (d = tablebase[c]; d < tablebase[c] + loadsize; d++)
    {
      if (ltable[c][d] == 0xff && rtable[c][d] > 0)
        rtable[c][d] += tablebase[c];
      if (c == 0 && (ltable[c][d] >= WAVECMD && ltable[c][d] <= WAVELASTCMD))
      {
        int cmd = ltable[c][d] & 0xf;
        if (cmd == CMD_SETWAVEPTR && rtable[c][d] > 0)
          rtable[c][d] += tablebase[WTBL];
        if (cmd == CMD_SETPULSEPTR && rtable[c][d] > 0)
          rtable[c][d] += tablebase[PTBL];
        if (cmd == CMD_SETFILTERPTR && rtable[c][d] > 0)
          rtable[c][d] += tablebase[FTBL];
        if (((cmd >= CMD_PORTAUP && cmd <= CMD_VIBRATO) || cmd == CMD_FUNKTEMPO) && rtable[c][d] > 0)
          rtable[c][d] += tablebase[STBL];
      }
    }
  }
  // Read patterns
  {
    int amount = buf[pos++];
    if (amount + pattbase > MAX_PATT)
    {
      snprintf(statusmessage, sizeof statusmessage, "Merge: too many patterns");
      goto ABORT;
    }
    for (c = 0; c < amount; c++)
    {
      int length = buf[pos++] * 4;
      unsigned char *pp = pattern[c + pattbase];
      memcpy(pp, buf + pos, length);
      pos += length;
      // Remap pattern instruments and commands
      for (d = 0; d < length; d += 4)
      {
        if (pp[d + 1] > 0)
          pp[d + 1] += instrbase;
        if (pp[d + 2] == CMD_SETWAVEPTR && pp[d + 3] > 0)
          pp[d + 3] += tablebase[WTBL];
        if (pp[d + 2] == CMD_SETPULSEPTR && pp[d + 3] > 0)
          pp[d + 3] += tablebase[PTBL];
        if (pp[d + 2] == CMD_SETFILTERPTR && pp[d + 3] > 0)
          pp[d + 3] += tablebase[FTBL];
        if (((pp[d + 2] >= CMD_PORTAUP && pp[d + 2] <= CMD_VIBRATO) ||
          pp[d + 2] == CMD_FUNKTEMPO) && pp[d + 3] > 0)
          pp[d + 3] += tablebase[STBL];
      }
    }
  }
  snprintf(statusmessage, sizeof statusmessage, "Merged %s as new subtune(s) starting at %02X", songfilename, songbase);

  ABORT:
  free(buf);
  countpatternlengths();
  songchange();
}
