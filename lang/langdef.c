/*
 *	langdef.c -- language definition handling for NOCC
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*{{{  includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "parser.h"
#include "langdef.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"


/*}}}*/


/*{{{  static langdefent_t *ldef_newlangdefent (void)*/
/*
 *	creates a new langdefent_t structure
 */
static langdefent_t *ldef_newlangdefent (void)
{
	langdefent_t *lde = (langdefent_t *)smalloc (sizeof (langdefent_t));

	lde->type = LDE_INVALID;
	return lde;
}
/*}}}*/
/*{{{  static void ldef_freelangdefent (langdefent_t *lde)*/
/*
 *	frees a langdefent_t structure (deep free)
 */
static void ldef_freelangdefent (langdefent_t *lde)
{
	if (!lde) {
		nocc_warning ("ldef_freelangdefent(): NULL pointer!");
		return;
	}
	switch (lde->type) {
	case LDE_INVALID:
		break;
	case LDE_REDUCTION:
		if (lde->u.redex.desc) {
			sfree (lde->u.redex.desc);
			lde->u.redex.desc = NULL;
		}
		break;
	case LDE_DFATRANS:
	case LDE_DFABNF:
		if (lde->u.dfarule) {
			sfree (lde->u.dfarule);
			lde->u.dfarule = NULL;
		}
		break;
	}
	sfree (lde);
	return;
}
/*}}}*/
/*{{{  static langdef_t *ldef_newlangdef (void)*/
/*
 *	creates a new langdef_t structure
 */
static langdef_t *ldef_newlangdef (void)
{
	langdef_t *ldef = (langdef_t *)smalloc (sizeof (langdef_t));

	ldef->ident = NULL;
	dynarray_init (ldef->ents);

	return ldef;
}
/*}}}*/
/*{{{  static void ldef_freelangdef (langdef_t *ldef)*/
/*
 *	frees a langdef_t structure (deep free)
 */
static void ldef_freelangdef (langdef_t *ldef)
{
	int i;

	if (!ldef) {
		nocc_warning ("ldef_freelangdef(): NULL pointer!");
		return;
	}
	if (ldef->ident) {
		sfree (ldef->ident);
		ldef->ident = NULL;
	}
	for (i=0; i<DA_CUR (ldef->ents); i++) {
		langdefent_t *lde = DA_NTHITEM (ldef->ents, i);

		ldef_freelangdefent (lde);
	}
	dynarray_trash (ldef->ents);
	sfree (ldef);
	return;
}
/*}}}*/


/*{{{  static int ldef_decodelangdefline (langdef_t *ldef, const char *rfname, const int lineno, char *buf)*/
/*
 *	decodes a single language definition line and places it in the given structure
 *	returns 0 on success, non-zero on failure
 */
static int ldef_decodelangdefline (langdef_t *ldef, const char *rfname, const int lineno, char *buf)
{
	/* FIXME! */
#if 1
	nocc_message ("ldef_decodelangdefline(): at %s:%d: buf = [%s]", rfname, lineno, buf);
#endif

	return 0;
}
/*}}}*/
/*{{{  static int ldef_readlangdefs (langdef_t *ldef, const char *rfname)*/
/*
 *	reads language definitions from a file and places in the given structure
 *	returns 0 on success, non-zero on failure
 */
static int ldef_readlangdefs (langdef_t *ldef, const char *rfname)
{
	FILE *fp;
	char *buf;
	int blen = 0;
	int curline = 0;
	int rval = 0;
	int bufline = 0;
	
	fp = fopen (rfname, "r");
	if (!fp) {
		nocc_error ("ldef_readlangdefs(): failed to open [%s]: %s", rfname, strerror (errno));
		return -1;
	}

	buf = (char *)smalloc (1024);
	for (;;) {
		char *ch;
		char *thisline = (char *)smalloc (1024);
		int tll = 0;
		int instring = 0;

reread_local:
		if (!fgets (thisline, 1023, fp)) {
			/* EOF */
			sfree (thisline);
			break;
		}
		curline++;
		tll = strlen (thisline);

		/* remove trailing whitespace */
		for (ch = thisline + tll; (ch > thisline) && ((ch[-1] == ' ') || (ch[-1] == '\t') || (ch[-1] == '\n') || (ch[-1] == '\r')); ch--, tll--);
		if (ch == thisline) {
			/* blank line */
			goto reread_local;
		}
		*ch = '\0';

		/* scan forward for something which looks like a comment */
		instring = 0;
		for (ch = thisline; (*ch != '\0'); ch++) {
			if (*ch == '\"') {
				instring = !instring;
			} else if (!instring && (*ch == '#')) {
				/* comment starts here */
				*ch = '\0';
				tll = strlen (thisline);

				/* remove trailing whitespace */
				for (ch = thisline + tll; (ch > thisline) && ((ch[-1] == ' ') || (ch[-1] == '\t') || (ch[-1] == '\n') || (ch[-1] == '\r')); ch--, tll--);

				break;		/* for(ch) */
			} else if (!instring && (*ch == '\\')) {
				/* escaped character */
				switch (ch[1]) {
				case '\0':
					/* end-of-line early */
					*ch = '\0';
					ch--, tll--;

					/* remove trailing whitespace */
					for (; (ch > thisline) && ((ch[-1] == ' ') || (ch[-1] == '\t') || (ch[-1] == '\n') || (ch[-1] == '\r')); ch--, tll--);
					break;
				default:
					/* ignore for now */
					break;
				}
			}
		}
		if (instring) {
			nocc_error ("ldef_readlangdefs(): at %s:%d, unbalanced string constant");
			rval = -1;
			sfree (thisline);
			goto out_local;
		}

		/* if the line starts with whitespace, assume a continuation from the previous line */
		if ((*thisline == ' ') || (*thisline == '\t')) {
			char *tmp = (char *)smalloc (blen + tll + 2);
			int tloff = 0;

			/* remove that leading whitespace */
			for (tloff = 0; (tloff < tll) && ((thisline[tloff] == ' ') || (thisline[tloff] == '\t')); tloff++);

			memcpy (tmp, buf, blen);
			tmp[blen] = ' ';			/* separate continuation with a space */
			memcpy (tmp + blen + 1, thisline + tloff, tll - tloff);
			tmp[blen + (tll - tloff) + 1] = '\0';
			blen += ((tll - tloff) + 1);

			sfree (buf);
			buf = tmp;
		} else {
			/* what's in buf, if anything (blen) is ready to go */
			if (blen) {
				rval = ldef_decodelangdefline (ldef, rfname, bufline, buf);
				blen = 0;
				if (rval) {
					nocc_error ("ldef_readlangdefs(): at %s:%d, failed to decode line");
					sfree (thisline);
					goto out_local;
				}
			}
			/* move thisline into buffer */
			strcpy (buf, thisline);
			blen = tll;
			bufline = curline;
		}

		sfree (thisline);
	}

	/* maybe one bit left over */
	if (blen) {
		rval = ldef_decodelangdefline (ldef, rfname, bufline, buf);
		blen = 0;
		if (rval) {
			nocc_error ("ldef_readlangdefs(): at %s:%d, failed to decode line");
			goto out_local;
		}
	}

out_local:
	sfree (buf);
	fclose (fp);

	return rval;
}
/*}}}*/


/*{{{  langdef_t *langdef_readdefs (const char *fname)*/
/*
 *	reads a language definition file (epaths searched for these if not absolute or in the CWD)
 *	returns langdef_t structure on success, NULL on failure
 */
langdef_t *langdef_readdefs (const char *fname)
{
	langdef_t *ldef = NULL;
	char *rfname;

	if (!access (fname, R_OK)) {
		rfname = string_dup (fname);
	} else {
		int i;

		rfname = (char *)smalloc (FILENAME_MAX + 1);
		/* try in extension paths */
		for (i=0; i<DA_CUR (compopts.epath); i++) {
			snprintf (rfname, FILENAME_MAX, "%s/%s", DA_NTHITEM (compopts.epath, i), fname);

			if (!access (rfname, R_OK)) {
				break;		/* for(i) */
			}
		}
		if (i == DA_CUR (compopts.epath)) {
			sfree (rfname);
			nocc_error ("langdef_readdefs(): failed to find readable definition file [%s]", fname);
			return NULL;
		}
	}

	ldef = ldef_newlangdef ();
	if (ldef_readlangdefs (ldef, rfname)) {
		/* failed, return NULL */
		ldef_freelangdef (ldef);
		ldef = NULL;
	}

	sfree (rfname);

	return ldef;
}
/*}}}*/
/*{{{  void langdef_freelangdef (langdef_t *ldef)*/
/*
 *	called to free a langdef_t structure
 */
void langdef_freelangdef (langdef_t *ldef)
{
	if (ldef) {
		ldef_freelangdef (ldef);
	}
	return;
}
/*}}}*/


/*{{{  int langdef_init (void)*/
/*
 *	called to initialise the language definition bits
 *	returns 0 on success, non-zero on failure
 */
int langdef_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int langdef_shutdown (void)*/
/*
 *	called to shut-down the language definition bits
 *	returns 0 on success, non-zero on failure
 */
int langdef_shutdown (void)
{
	return 0;
}
/*}}}*/


