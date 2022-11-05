/*
 *	Z80 - Macro - Assembler
 *	Copyright (C) 1987-2022 by Udo Munk
 *	Copyright (C) 2022 by Thomas Eberhardt
 *
 *	History:
 *	17-SEP-1987 Development under Digital Research CP/M 2.2
 *	28-JUN-1988 Switched to Unix System V.3
 *	22-OCT-2006 changed to ANSI C for modern POSIX OS's
 *	03-FEB-2007 more ANSI C conformance and reduced compiler warnings
 *	18-MAR-2007 use default output file extension dependent on format
 *	04-OCT-2008 fixed comment bug, ';' string argument now working
 *	22-FEB-2014 fixed is...() compiler warnings
 *	13-JAN-2016 fixed buffer overflow, new expression parser from Didier
 *	02-OCT-2017 bug fixes in expression parser from Didier
 *	28-OCT-2017 added variable symbol length and other improvements
 *	15-MAY-2018 mark unreferenced symbols in listing
 *	30-JUL-2021 fix verbose option
 *	28-JAN-2022 added syntax check for OUT (n),A
 *	24-SEP-2022 added undocumented Z80 instructions and 8080 mode (TE)
 *	04-OCT-2022 new expression parser (TE)
 *	25-OCT-2022 Intel-like macros (TE)
 */

/*
 *	symbol table functions
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "z80a.h"
#include "z80aglb.h"

struct sym *look_sym(char *);
struct sym *get_sym(char *);
struct sym *new_sym(char *);
unsigned hash(char *);
int namecmp(const void *, const void *);
int valcmp(const void *, const void *);

/* z80amain.c */
extern void fatal(int, const char *);

/* z80aout.c */
extern void asmerr(int);

static struct sym *symtab[HASHSIZE];	/* symbol table */
static unsigned symcnt;			/* number of symbols defined */
static struct sym **symarray;		/* sorted symbol table */
static int symsort;			/* sort mode for iterator */
static unsigned symidx;			/* hash table index for iterator */
static struct sym *symptr;		/* symbol pointer for iterator */

/*
 *	hash search on symbol table symtab
 *
 *	Input: pointer to string with symbol
 *
 *	Output: pointer to table element, or NULL if not found
 */
struct sym *look_sym(char *sym_name)
{
	register struct sym *sp;

	for (sp = symtab[hash(sym_name)]; sp != NULL; sp = sp->sym_next)
		if (strcmp(sym_name, sp->sym_name) == 0)
			return(sp);
	return(NULL);
}

/*
 *	hash search on symbol table symtab, increase refcnt if found
 *
 *	Input: pointer to string with symbol
 *
 *	Output: pointer to table element, or NULL if not found
 */
struct sym *get_sym(char *sym_name)
{
	register struct sym *sp;

	for (sp = symtab[hash(sym_name)]; sp != NULL; sp = sp->sym_next)
		if (strcmp(sym_name, sp->sym_name) == 0) {
			sp->sym_refcnt++;
			return(sp);
		}
	return(NULL);
}

/*
 *	add symbol to symbol table symtab
 *
 *	Input: sym_name pointer to string with symbol name
 *
 *	Output: pointer to table element
 */
struct sym *new_sym(char *sym_name)
{
	register unsigned hashval, n;
	register struct sym *sp;

	n = strlen(sym_name);
	sp = (struct sym *) malloc(sizeof (struct sym));
	if (sp == NULL || (sp->sym_name = (char *) malloc(n + 1)) == NULL)
		fatal(F_OUTMEM, "symbols");
	strcpy(sp->sym_name, sym_name);
	hashval = hash(sym_name);
	sp->sym_next = symtab[hashval];
	symtab[hashval] = sp;
	sp->sym_refcnt = 0;
	if (n > symmax)
		symmax = n;
	symcnt++;
	return(sp);
}

/*
 *	add symbol to symbol table symtab, or modify existing symbol
 *	and increase refcnt
 *
 *	Input: sym_name pointer to string with symbol name
 *	       sym_val  value of symbol
 */
void put_sym(char *sym_name, WORD sym_val)
{
	register struct sym *sp;

	if ((sp = get_sym(sym_name)) == NULL)
		sp = new_sym(sym_name);
	sp->sym_val = sym_val;
}

/*
 *	add label to symbol table, error if symbol already exists
 *	and differs in value
 */
void put_label(void)
{
	register struct sym *sp;

	if ((sp = look_sym(label)) == NULL)
		new_sym(label)->sym_val = pc;
	else if (sp->sym_val != pc)
		asmerr(pass == 1 ? E_MULSYM : E_LBLDIF);
}

/*
 *	hash algorithm
 *
 *	Input: pointer to string with name
 *
 *	Output: hash value
 */
unsigned hash(char *name)
{
	register unsigned hashval;

	for (hashval = 0; *name != '\0';)
		hashval += *name++;
	return(hashval % HASHSIZE);
}

/*
 *	return first symbol for listing, sorted as specified in sort_mode
 */
struct sym *first_sym(int sort_mode)
{
	register unsigned i, j;
	register struct sym *sp;

	if (symcnt == 0)
		return(NULL);
	symsort = sort_mode;
	switch(sort_mode) {
	case SYM_UNSORT:
		symidx = 0;
		while ((symptr = symtab[symidx]) == NULL)
			if (++symidx == HASHSIZE)
				break;
		return(symptr);
	case SYM_SORTN:
	case SYM_SORTA:
		symarray = (struct sym **) malloc(sizeof(struct sym *)
						  * symcnt);
		if (symarray == NULL)
			fatal(F_OUTMEM, "sorting symbol table");
		for (i = 0, j = 0; i < HASHSIZE; i++)
			for (sp = symtab[i]; sp != NULL;
			     sp = sp->sym_next)
				symarray[j++] = sp;
		qsort(symarray, symcnt, sizeof(struct sym *),
		      sort_mode == SYM_SORTN ? namecmp : valcmp);
		symidx = 0;
		return(symarray[symidx]);
	default:
		fatal(F_INTERN, "unknown sort mode in first_sym");
	}
	return(NULL);		/* silence compiler */
}

/*
 *	return next symbol for listing
 */
struct sym *next_sym(void)
{
	if (symsort == SYM_UNSORT) {
		if ((symptr = symptr->sym_next) == NULL) {
			do {
				if (++symidx == HASHSIZE)
					break;
				else
					symptr = symtab[symidx];
			} while (symptr == NULL);
		}
		return(symptr);
	} else if (++symidx < symcnt)
		return(symarray[symidx]);
	return(NULL);
}

/*
 *	compares two symbol names for qsort()
 */
int namecmp(const void *p1, const void *p2)
{
	return(strcmp((*(const struct sym **) p1)->sym_name,
		      (*(const struct sym **) p2)->sym_name));
}

/*
 *	compares two symbol values for qsort(), result like strcmp()
 *	if equal compares symbol names
 */
int valcmp(const void *p1, const void *p2)
{
	register WORD n1, n2;

	n1 = (*(const struct sym **) p1)->sym_val;
	n2 = (*(const struct sym **) p2)->sym_val;
	if (n1 < n2)
		return(-1);
	else if (n1 > n2)
		return(1);
	else
		return(namecmp(p1, p2));
}
