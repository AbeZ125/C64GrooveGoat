//
// C64 GrooveGoat - a one line description of the orderlist entry under the cursor
//

#ifndef GORDHINT_H
#define GORDHINT_H

// list is one voice's orderlist, len its length (the end mark RST is list[len], the restart position list[len+1]),
// pos the cursor position. Writes at most outsize-1 characters (the display has room for 43). Returns the text
// length, 0 (and an empty string) if there is nothing to say.
int order_hint(const unsigned char *list, int len, int pos, char *out, int outsize);

#endif
