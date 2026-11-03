#ifndef GINSTR_H
#define GINSTR_H

#ifndef GINSTR_C
//extern int editorInfo.einum;
//extern int editorInfo.eipos;
//extern int editorInfo.eicolumn;
extern INSTR instrcopybuffer;
#endif

void instrumentcommands(void);
void furnaceexport(void);
int furnaceexportall_to(const char *dir);
void nextinstr(void);
void previnstr(void);
void clearinstr(int num);
void gotoinstr(int i);
void showinstrtable(void);
void *undoinstrbegin(void);
void undoinstrend(void *ed);
int instrumentvoicemask(int ins);
void adaptfiltervoices(int ins, int mask);

#endif
