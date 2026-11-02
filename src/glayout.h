//
// C64 GrooveGoat screen layout (character cell coordinates)
//

#ifndef GLAYOUT_H
#define GLAYOUT_H

// Toolbar & settings bar
#define TOOLBAR_Y 0
#define SETTINGS_Y 1

// Pattern editor
#define PATT_X 0
#define PATT_Y 2                      // panel header row
#define PATT_TOOLS_Y 3                // pattern tool buttons (transpose, mark, cut ...)
// The oscilloscopes (on / off) take two rows above the channel headers
extern unsigned showscope;
#define SCOPE_H (showscope ? 2 : 0)
#define PATT_SCOPE_Y 4                // oscilloscope row (a picture 2 cells high for every channel)
#define PATT_CHNHDR_Y (4 + SCOPE_H)   // channel header row
#define PATT_ROWS_Y (5 + SCOPE_H)     // first pattern row
#define PATT_W 94
#define VISIBLECHANNELS 6
#define CHN_W 15                      // width of one channel column
// With one SID the three channels are centred in the pattern area
#define CHN_OFS ((numchannels <= 3) ? (PATT_W - 3 * CHN_W) / 2 - 1 : 0)
#define CHN_X(i) (PATT_X + 2 + CHN_OFS + (i) * CHN_W)

// Right panel
#define RP_X 95
#define RP_W 45

#define ORD_Y 2                       // orderlist header
#define ORD_ROWS_Y 3
#define VISIBLEORDERCHNS 6
#define ORD_LABEL_W 5
#define ORD_X (RP_X + ORD_LABEL_W)

#define INS_Y 10                      // instrument header
#define INS_ROWS_Y 11
#define INS_COL1_X (RP_X + 1)
#define INS_COL2_X (RP_X + 23)
#define INS_VAL_OFS 16                // offset of the hex value inside a column

#define ADSR_Y 16                     // ADSR graph (3 rows) below the instrument
#define ADSR_W 20                     // graph width in cells

#define TBL_Y 20                      // table header
#define TBL_ROWS_Y 21
#define TBL_X(t) (RP_X + 1 + (t) * 11)

// The piano keyboard (on/off) takes 3 rows plus a separator above the status
// area; the pattern editor, the tables and the song info move up for it
extern unsigned showpiano;
#define PIANO_H 3
#define PIANO_Y (STATUS_Y - 1 - PIANO_H)           // 43
#define PIANO_SEP_Y (PIANO_Y - 1)                  // 42

#define NAMES_Y (showpiano ? 38 : 42)          // song info header
#define NAMES_ROWS_Y (NAMES_Y + 1)
#define NAMES_X (RP_X + 1)
#define NAMES_VAL_X (RP_X + 11)

// Bottom status area
#define STATUS_Y 47
#define POSITIONS_Y 48
#define MESSAGE_Y 49

// Output volume slider at the bottom right (20 steps of 5%)
#define VOL_W 20
#define VOL_X (MAX_COLUMNS - 1 - 5 - VOL_W)
#define VOL_LABEL_X (VOL_X - 6)

#endif
