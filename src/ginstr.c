//
// GOATTRACKER v2 instrument editor
//

#define GINSTR_C

#include "goattrk2.h"

INSTR instrcopybuffer;
int cutinstr = -1;

//int editorInfo.einum;
//int editorInfo.eipos;
//int editorInfo.eicolumn;

void instrumentcommands(void)
{
  switch(rawkey)
  {
    case 0x8:
    case KEY_DEL:
    if ((editorInfo.einum) && (shiftpressed) && (editorInfo.eipos < 9))
    {
      deleteinstrtable(editorInfo.einum);
      clearinstr(editorInfo.einum);
    }
    break;

    case KEY_X:
    if ((editorInfo.einum) && (shiftpressed) && (editorInfo.eipos < 9))
    {
      cutinstr = editorInfo.einum;
      memcpy(&instrcopybuffer, &instr[editorInfo.einum], sizeof(INSTR));
      clearinstr(editorInfo.einum);
    }
    break;

    case KEY_C:
    if ((editorInfo.einum) && (shiftpressed) && (editorInfo.eipos < 9))
    {
      cutinstr = -1;
      memcpy(&instrcopybuffer, &instr[editorInfo.einum], sizeof(INSTR));
    }
    break;

    case KEY_S:
    if ((editorInfo.einum) && (shiftpressed) && (editorInfo.eipos < 9))
    {
      memcpy(&instr[editorInfo.einum], &instrcopybuffer, sizeof(INSTR));
      if (cutinstr != -1)
      {
        int c, d;
        for (c = 0; c < MAX_PATT; c++)
        {
          for (d = 0; d < pattlen[c]; d++)
            if (pattern[c][d*4+1] == cutinstr) pattern[c][d*4+1] = editorInfo.einum;
        }
      }
    }
    break;

    case KEY_V:
    if ((editorInfo.einum) && (shiftpressed) && (editorInfo.eipos < 9))
    {
      memcpy(&instr[editorInfo.einum], &instrcopybuffer, sizeof(INSTR));
    }
    break;

    case KEY_RIGHT:
    if (editorInfo.eipos < 9)
    {
      editorInfo.eicolumn++;
      if (editorInfo.eicolumn > 1)
      {
        editorInfo.eicolumn = 0;
        editorInfo.eipos += 5;
        if (editorInfo.eipos >= 9) editorInfo.eipos -= 10;
        if (editorInfo.eipos < 0) editorInfo.eipos = 8;
      }
    }
    break;

    case KEY_LEFT:
    if (editorInfo.eipos < 9)
    {
      editorInfo.eicolumn--;
      if (editorInfo.eicolumn < 0)
      {
        editorInfo.eicolumn = 1;
        editorInfo.eipos -= 5;
        if (editorInfo.eipos < 0) editorInfo.eipos += 10;
        if (editorInfo.eipos >= 9) editorInfo.eipos = 8;
      }
    }
    break;

    case KEY_DOWN:
    if (editorInfo.eipos < 9)
    {
      editorInfo.eipos++;
      if (editorInfo.eipos > 8) editorInfo.eipos = 0;
    }
    break;

    case KEY_UP:
    if (editorInfo.eipos < 9)
    {
      editorInfo.eipos--;
      if (editorInfo.eipos < 0) editorInfo.eipos = 8;
    }
    break;

    case KEY_N:
    if ((editorInfo.eipos != 9) && (shiftpressed))
    {
      editorInfo.eipos = 9;
      return;
    }
    break;

    case KEY_U:
    if (shiftpressed)
    {
      editorInfo.etlock ^= 1;
      validatetableview();
    }
    break;

    case KEY_F:
    if ((shiftpressed) && (editorInfo.einum)) furnaceexport();
    break;

    case KEY_E:
    // SHIFT+E on a table pointer opens the step or speed editor on the program that the instrument starts, without
    // looking for it in the table; on the Attack/Decay or Sustain/Release value it opens the ADSR sliders
    if ((shiftpressed) && (editorInfo.einum))
    {
      if (editorInfo.eipos < 2) showadsreditor();
      else if (editorInfo.eipos < 6)
      {
        int table = editorInfo.eipos - 2, p = instr[editorInfo.einum].ptr[table];
        if (!p) setstatusmessage("This instrument has no program in that table yet (SHIFT+ENTER makes one)");
        else
        {
          settableview(table, p - 1);        // the table panel shows the program as well
          if (table == STBL) showspeededitor();
          else showstepeditor();
        }
      }
    }
    break;

    case KEY_SPACE:
    if (editorInfo.eipos != 9)
    {
      if (!shiftpressed)
        playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn);
      else
        releasenote(editorInfo.epchn);
    }
    break;

    case KEY_ENTER:
    if (!editorInfo.einum) break;
    switch(editorInfo.eipos)
    {
      case 2:
      case 3:
      case 4:
      case 5:
      {
        int pos;

        if (instr[editorInfo.einum].ptr[editorInfo.eipos-2])
        {
          if ((editorInfo.eipos == 5) && (shiftpressed))
          {
            instr[editorInfo.einum].ptr[STBL] = makespeedtable(instr[editorInfo.einum].ptr[STBL], editorInfo.finevibrato, 1) + 1;
            break;
          }
          pos = instr[editorInfo.einum].ptr[editorInfo.eipos-2] - 1;
        }
        else
        {
          pos = gettablelen(editorInfo.eipos-2);
          if (pos >= MAX_TABLELEN-1) pos = MAX_TABLELEN - 1;
          if (shiftpressed) instr[editorInfo.einum].ptr[editorInfo.eipos-2] = pos + 1;
        }
        gototable(editorInfo.eipos-2, pos);
      }
      return;

      case 9:
      editorInfo.eipos = 0;
      break;
    }
    break;
  }
  if ((editorInfo.eipos == 9) && (editorInfo.einum)) editstring(instr[editorInfo.einum].name, MAX_INSTRNAMELEN);
  if ((hexnybble >= 0) && (editorInfo.eipos < 9) && (editorInfo.einum))
  {
    unsigned char *ptr = &instr[editorInfo.einum].ad;
    ptr += editorInfo.eipos;

    switch(editorInfo.eicolumn)
    {
      case 0:
      *ptr &= 0x0f;
      *ptr |= hexnybble << 4;
      editorInfo.eicolumn++;
      break;

      case 1:
      *ptr &= 0xf0;
      *ptr |= hexnybble;
      editorInfo.eicolumn++;
      if (editorInfo.eicolumn > 1)
      {
        editorInfo.eicolumn = 0;
        editorInfo.eipos++;
        if (editorInfo.eipos >= 9) editorInfo.eipos = 0;
      }
      break;
    }
  }
  // Validate instrument parameters
  if (editorInfo.einum)
  {
    if (!(instr[editorInfo.einum].gatetimer & 0x3f)) instr[editorInfo.einum].gatetimer |= 1;
  }
}


void clearinstr(int num)
{
  memset(&instr[num], 0, sizeof(INSTR));
  if (num)
  {
    if (editorInfo.multiplier)
      instr[num].gatetimer = 2 * editorInfo.multiplier;
    else
      instr[num].gatetimer = 1;

    instr[num].firstwave = 0x9;
  }
}

void gotoinstr(int i)
{
  if (i < 0) return;
  if (i >= MAX_INSTR) return;

  editorInfo.einum = i;
  showinstrtable();

  editorInfo.editmode = EDIT_INSTRUMENT;
}

void nextinstr(void)
{
  editorInfo.einum++;
  if (editorInfo.einum >= MAX_INSTR) editorInfo.einum = MAX_INSTR - 1;
  showinstrtable();
}

void previnstr(void)
{
  editorInfo.einum--;
  if (editorInfo.einum < 0) editorInfo.einum = 0;
  showinstrtable();
}

void showinstrtable(void)
{
  if (!editorInfo.etlock)
  {
    int c;

    for (c = MAX_TABLES-1; c >= 0; c--)
    {
      if (instr[editorInfo.einum].ptr[c])
        settableviewfirst(c, instr[editorInfo.einum].ptr[c] - 1);
    }
  }
}


// Undo support for operations that replace an instrument including its table
// data (library, random generator): table rows may move, which also changes
// other instruments and pattern commands, so all of them are checked.
void *undoinstrbegin(void)
{
  int c;
  for (c = 0; c < MAX_INSTR; c++)
    undoAreaSetCheckForChange(UNDO_AREA_INSTRUMENTS, c, UNDO_AREA_DIRTY_CHECK);
  for (c = 0; c < MAX_TABLES; c++)
  {
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + c, 0, UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + c, 1, UNDO_AREA_DIRTY_CHECK);
  }
  for (c = 0; c < MAX_PATT; c++)
    undoAreaSetCheckForChange(UNDO_AREA_PATTERN, c, UNDO_AREA_DIRTY_CHECK);
  return undoCreateEditorInfo();
}

void undoinstrend(void *ed)
{
  if (!ed) return;
  if (undoValidateUndoAreas((GTUNDO_OBJECT *)ed) == 0)
    undoFreeUndoObject((GTUNDO_OBJECT *)ed);
}

// Voices (bit 0-2 = voice 1-3 of a SID) on which an instrument is used in
// the song: all subtunes, all channels, all patterns of the orderlists
int instrumentvoicemask(int ins)
{
  int mask = 0, s, c, p, d;
  static unsigned char used[MAX_PATT];
  for (c = 0; c < numchannels; c++)
  {
    memset(used, 0, sizeof used);
    for (s = 0; s < MAX_SONGS; s++)
    {
      for (p = 0; p < MAX_SONGLEN; p++)
      {
        unsigned char v = songorder[s][c][p];
        if (v == LOOPSONG) break;
        if (v < MAX_PATT) used[v] = 1;
      }
    }
    for (p = 0; p < MAX_PATT; p++)
    {
      if (!used[p]) continue;
      for (d = 0; d < pattlen[p]; d++)
      {
        if (pattern[p][d*4+1] == ins)
        {
          mask |= 1 << (c % 3);
          break;
        }
      }
    }
  }
  return mask;
}

// Set the voice bits of the filter routing in the filtertable program of an
// instrument. Instruments from the library route all three voices through
// the filter, which would also filter the other voices of the song.
void adaptfiltervoices(int ins, int mask)
{
  int pos, len, c;
  if ((ins <= 0) || (ins >= MAX_INSTR) || (!instr[ins].ptr[FTBL]) || (!mask)) return;
  pos = instr[ins].ptr[FTBL] - 1;
  len = gettablepartlen(FTBL, pos);
  // never change a filter program that something else uses too
  if (tablepartreferenced(FTBL, pos, len, ins)) return;
  for (c = pos; (c < pos + len) && (c < MAX_TABLELEN); c++)
  {
    if ((ltable[FTBL][c] >= 0x80) && (ltable[FTBL][c] != 0xff))
      rtable[FTBL][c] = (rtable[FTBL][c] & 0xf0) | (mask & 0x0f);
  }
}
