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
#include "dfa.h"
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

	lde->ldef = NULL;
	lde->lineno = 0;

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
	case LDE_GRL:
		if (lde->u.redex.name) {
			sfree (lde->u.redex.name);
			lde->u.redex.name = NULL;
		}
		if (lde->u.redex.desc) {
			sfree (lde->u.redex.desc);
			lde->u.redex.desc = NULL;
		}
		lde->type = LDE_INVALID;
		break;
	case LDE_DFATRANS:
	case LDE_DFABNF:
		if (lde->u.dfarule) {
			sfree (lde->u.dfarule);
			lde->u.dfarule = NULL;
		}
		lde->type = LDE_INVALID;
		break;
	}
	sfree (lde);
	return;
}
/*}}}*/
/*{{{  static langdefsec_t *ldef_newlangdefsec (void)*/
/*
 *	creates a new langdefsec_t structure
 */
static langdefsec_t *ldef_newlangdefsec (void)
{
	langdefsec_t *lsec = (langdefsec_t *)smalloc (sizeof (langdefsec_t));

	lsec->ldef = NULL;
	lsec->ident = NULL;
	dynarray_init (lsec->ents);

	return lsec;
}
/*}}}*/
/*{{{  static void ldef_freelangdefsec (langdefsec_t *lsec)*/
/*
 *	frees a langdefsec_t structure (deep free)
 */
static void ldef_freelangdefsec (langdefsec_t *lsec)
{
	int i;

	if (!lsec) {
		nocc_warning ("ldef_freelangdefsec(): NULL pointer!");
		return;
	}
	if (lsec->ident) {
		sfree (lsec->ident);
		lsec->ident = NULL;
	}
	for (i=0; i<DA_CUR (lsec->ents); i++) {
		langdefent_t *lde = DA_NTHITEM (lsec->ents, i);

		ldef_freelangdefent (lde);
	}
	dynarray_trash (lsec->ents);
	sfree (lsec);

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
	ldef->desc = NULL;
	ldef->maintainer = NULL;
	dynarray_init (ldef->sections);

	ldef->cursec = NULL;

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
	if (ldef->desc) {
		sfree (ldef->desc);
		ldef->desc = NULL;
	}
	if (ldef->maintainer) {
		sfree (ldef->maintainer);
		ldef->maintainer = NULL;
	}

	for (i=0; i<DA_CUR (ldef->sections); i++) {
		langdefsec_t *lsec = DA_NTHITEM (ldef->sections, i);

		ldef_freelangdefsec (lsec);
	}
	dynarray_trash (ldef->sections);
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
	char *local = string_dup (buf);
	char **bits = split_string2 (local, ' ', '\t');
	int nbits, i;
	int rval = 0;

	for (nbits=0; bits[nbits]; nbits++);

#if 0
	nocc_message ("ldef_decodelangdefline(): at %s:%d: buf = [%s], %d bits", rfname, lineno, buf, nbits);
#endif

	if (!nbits) {
		nocc_error ("bad definition at %s:%d", rfname, lineno);
		rval = -1;
		goto out_local;
	}
	if (!strcmp (bits[0], ".IDENT")) {
		/*{{{  .IDENT -- identifying the language definition file*/
		if (nbits != 2) {
			goto out_malformed;
		}
		string_dequote (bits[1]);
#if 0
		nocc_message ("ldef_decodelangdefline(): dequoted IDENT is [%s]", bits[1]);
#endif
		if (ldef->ident && strlen (ldef->ident)) {
			nocc_warning ("already got ident at %s:%d, currently [%s]", rfname, lineno, ldef->ident);
		} else {
			if (ldef->ident) {
				sfree (ldef->ident);
			}
			ldef->ident = string_dup (bits[1]);
		}

		/*}}}*/
	} else if (!strcmp (bits[0], ".DESC")) {
		/*{{{  .DESC -- general description for the language definition*/
		if (nbits != 2) {
			goto out_malformed;
		}
		string_dequote (bits[1]);
		if (ldef->desc && strlen (ldef->desc)) {
			nocc_warning ("already got description at %s:%d, currently [%s]", rfname, lineno, ldef->desc);
		} else {
			if (ldef->desc) {
				sfree (ldef->desc);
			}
			ldef->desc = string_dup (bits[1]);
		}

		/*}}}*/
	} else if (!strcmp (bits[0], ".MAINTAINER")) {
		/*{{{  .MAINTAINER -- person(s) responsible for this language definition*/
		if (nbits != 2) {
			goto out_malformed;
		}
		string_dequote (bits[1]);
		if (ldef->maintainer && strlen (ldef->maintainer)) {
			nocc_warning ("already got maintainer at %s:%d, currently [%s]", rfname, lineno, ldef->maintainer);
		} else {
			if (ldef->maintainer) {
				sfree (ldef->maintainer);
			}
			ldef->maintainer = string_dup (bits[1]);
		}

		/*}}}*/
	} else if (!strcmp (bits[0], ".SECTION")) {
		/*{{{  .SECTION -- starting a named section of definitions*/
		langdefsec_t *lsec = NULL;

		if (nbits != 2) {
			goto out_malformed;
		}
		string_dequote (bits[1]);
		for (i=0; i<DA_CUR (ldef->sections); i++) {
			lsec = DA_NTHITEM (ldef->sections, i);

			if (!strcmp (lsec->ident, bits[1])) {
				/* already got this section */
				nocc_warning ("already got a section called [%s] at %s:%d, adding to it", bits[1], rfname, lineno);
				break;		/* for(i) */
			}
			lsec = NULL;
		}
		if (!lsec) {
			/* create a new section */
			lsec = ldef_newlangdefsec ();
			lsec->ident = string_dup (bits[1]);
			lsec->ldef = ldef;

			dynarray_add (ldef->sections, lsec);
		}
		ldef->cursec = lsec;

		/*}}}*/
	} else if (!strcmp (bits[0], ".GRULE")) {
		/*{{{  .GRULE -- generic reduction name and rule*/
		langdefsec_t *lsec = ldef->cursec;
		langdefent_t *lfe = NULL;
		char *dstr = NULL;
		int dlen = 0;

		if (nbits < 3) {
			goto out_malformed;
		}

		string_dequote (bits[1]);
		for (i=2, dlen=0; i<nbits; dlen += strlen (bits[i]), i++) {
			string_dequote (bits[i]);
		}
		dstr = smalloc (dlen + 1);		/* descriptions not separated */
		for (i=2, dlen=0; i<nbits; dlen += strlen (bits[i]), i++) {
			strcpy (dstr + dlen, bits[i]);
		}

		if (!lsec) {
			nocc_warning ("%s outside section at %s:%d, creating omnipotent section", bits[0], rfname, lineno);

			lsec = ldef_newlangdefsec ();
			lsec->ident = string_dup ("");
			lsec->ldef = ldef;

			dynarray_add (ldef->sections, lsec);
			ldef->cursec = lsec;
		}

		lfe = ldef_newlangdefent ();
		lfe->ldef = ldef;
		lfe->lineno = lineno;

		lfe->type = LDE_GRL;
		lfe->u.redex.name = string_dup (bits[1]);
		lfe->u.redex.desc = dstr;

		dynarray_add (lsec->ents, lfe);

		/*}}}*/
	} else if (!strcmp (bits[0], ".BNF") || !strcmp (bits[0], ".TABLE")) {
		/*{{{  .BNF, .TABLE -- DFA BNF-rule or transition-table*/
		langdefsec_t *lsec = ldef->cursec;
		langdefent_t *lfe = NULL;
		char *dstr = NULL;
		int dlen = 0;

		if (nbits < 2) {
			goto out_malformed;
		}

		for (i=1, dlen=0; i<nbits; dlen += strlen (bits[i]), i++) {
			string_dequote (bits[i]);
		}
		dstr = smalloc (dlen + nbits);		/* descriptions separated by whitespace */
		for (i=1, dlen=0; i<nbits;) {
			strcpy (dstr + dlen, bits[i]);
			dlen += strlen (bits[i]);
			i++;
			if (i < nbits) {
				dstr[dlen] = ' ';
				dlen++;
			}
		}

		if (!lsec) {
			nocc_warning ("%s outside section at %s:%d, creating omnipotent section", bits[0], rfname, lineno);

			lsec = ldef_newlangdefsec ();
			lsec->ident = string_dup ("");
			lsec->ldef = ldef;

			dynarray_add (ldef->sections, lsec);
			ldef->cursec = lsec;
		}

		lfe = ldef_newlangdefent ();
		lfe->ldef = ldef;
		lfe->lineno = lineno;

		lfe->type = (!strcmp (bits[0], ".BNF")) ? LDE_DFABNF : LDE_DFATRANS;
		lfe->u.dfarule = dstr;

		dynarray_add (lsec->ents, lfe);

		/*}}}*/
	} else if ((bits[0][0] == '.') && (bits[0][1] >= 'A') && (bits[0][1] <= 'Z')) {
		/*{{{  unknown directive!*/
		nocc_error ("unknown directive %s at %s:%d", bits[0], rfname, lineno);
		rval = -1;
		goto out_local;
		/*}}}*/
	} else {
		/*{{{  unexpected stuff*/
		nocc_error ("unexpected data at %s:%d, starting [%s]", rfname, lineno, bits[0]);
		rval = -1;
		goto out_local;
		/*}}}*/
	}

	/* bit goto happy in here.. */
	goto out_local;

out_malformed:
	nocc_error ("malformed %s directive at %s:%d", bits[0] + 1, rfname, lineno);
	rval = -1;

out_local:
	sfree (bits);
	sfree (local);

	return rval;
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
			nocc_error ("ldef_readlangdefs(): at %s:%d, unbalanced string constant", rfname, curline);
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
					nocc_error ("ldef_readlangdefs(): at %s:%d, failed to decode line", rfname, bufline);
					sfree (thisline);
					goto out_local;
				}
			}
			/* move thisline into buffer */
			sfree (buf);
			buf = string_dup (thisline);
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
			nocc_error ("ldef_readlangdefs(): at %s:%d, failed to decode line", rfname, bufline);
			if (ldef->maintainer) {
				nocc_message ("maintainer for [%s] is: %s", ldef->ident, ldef->maintainer);
			}
			goto out_local;
		}
	}

out_local:
	sfree (buf);
	fclose (fp);

	return rval;
}
/*}}}*/


/*{{{  langdefsec_t *langdef_findsection (langdef_t *ldef, const char *ident)*/
/*
 *	searches through a language definition for a particular section
 *	returns NULL if not found
 */
langdefsec_t *langdef_findsection (langdef_t *ldef, const char *ident)
{
	int i;

	for (i=0; i<DA_CUR (ldef->sections); i++) {
		langdefsec_t *lsec = DA_NTHITEM (ldef->sections, i);

		if (!lsec->ident || !strcmp (lsec->ident, ident)) {
			return lsec;
		}
	}
	nocc_warning ("langdef_findsection(): no such section [%s] in language definition [%s]", ident, ldef->ident);
	return NULL;
}
/*}}}*/
/*{{{  int langdef_hassection (langdef_t *ldef, const char *ident)*/
/*
 *	tests to see whether the given language definition has a particular section
 *	returns non-zero if found, 0 otherwise
 */
int langdef_hassection (langdef_t *ldef, const char *ident)
{
	int i;

	for (i=0; i<DA_CUR (ldef->sections); i++) {
		langdefsec_t *lsec = DA_NTHITEM (ldef->sections, i);

		if (!lsec->ident || !strcmp (lsec->ident, ident)) {
			return 1;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  int langdef_reg_reducers (langdefsec_t *lsec)*/
/*
 *	registers generic reductions in a particular section
 *	returns 0 on success, non-zero on failure
 */
int langdef_reg_reducers (langdefsec_t *lsec)
{
	int rval = 0;
	int i;

	if (!lsec) {
		/* means we probably failed elsewhere first */
		return 0;
	}

	for (i=0; i<DA_CUR (lsec->ents); i++) {
		langdefent_t *lde = DA_NTHITEM (lsec->ents, i);

		switch (lde->type) {
		default:
			break;
		case LDE_GRL:
			{
				void *rule = parser_decode_grule (lde->u.redex.desc);

#if 0
				nocc_message ("langdef_reg_reducers(): decoded rule: [%s]", lde->u.redex.desc);
#endif
				if (!rule) {
					nocc_error ("invalid reduction rule in language definition for [%s (%s)], line %d", lsec->ldef->ident, lsec->ident, lde->lineno);
					if (lsec->ldef->maintainer) {
						nocc_message ("maintainer for [%s] is: %s", lsec->ldef->ident, lsec->ldef->maintainer);
					}
					rval = -1;
				} else {
					parser_register_grule (lde->u.redex.name, rule);
				}
			}
			break;
		}
	}

	return rval;
}
/*}}}*/
/*{{{  dfattbl_t **langdef_init_dfatrans (langdefsec_t *lsec, int *ntrans)*/
/*
 *	registers DFA rules in a particular section
 *	returns rule table on success, NULL on failure (always succeeds)
 */
dfattbl_t **langdef_init_dfatrans (langdefsec_t *lsec, int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);
	int i;

	dynarray_init (transtbl);

	if (lsec) {
		for (i=0; i<DA_CUR (lsec->ents); i++) {
			langdefent_t *lde = DA_NTHITEM (lsec->ents, i);

			switch (lde->type) {
			default:
				break;
			case LDE_DFATRANS:
				{
					dfattbl_t *dfat = dfa_transtotbl (lde->u.dfarule);

					if (!dfat) {
						nocc_error ("invalid DFA transition table in language definition for [%s (%s)], line %d", lsec->ldef->ident, lsec->ident, lde->lineno);
						if (lsec->ldef->maintainer) {
							nocc_message ("maintainer for [%s] is: %s", lsec->ldef->ident, lsec->ldef->maintainer);
						}
					} else {
						dynarray_add (transtbl, dfat);
					}
				}
				break;
			case LDE_DFABNF:
				{
					dfattbl_t *dfat = dfa_bnftotbl (lde->u.dfarule);

					if (!dfat) {
						nocc_error ("invalid DFA BNF in language definition for [%s (%s)], line %d", lsec->ldef->ident, lsec->ident, lde->lineno);
						if (lsec->ldef->maintainer) {
							nocc_message ("maintainer for [%s] is: %s", lsec->ldef->ident, lsec->ldef->maintainer);
						}
					} else {
						dynarray_add (transtbl, dfat);
					}
				}
				break;
			}
		}
	}

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
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
	} else {
		ldef->cursec = NULL;
		if (compopts.verbose) {
			nocc_message ("language definitions loaded, [%s]", ldef->desc ?: rfname);
		}
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


