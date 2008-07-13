/*
 *	langdef.c -- language definition handling for NOCC
 *	Copyright (C) 2006-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "treecheck.h"
#include "parser.h"
#include "typecheck.h"
#include "fcnlib.h"
#include "extn.h"
#include "dfa.h"
#include "dfaerror.h"
#include "langdef.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"
#include "langdeflookup.h"


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
	int i;

	if (!lde) {
		nocc_warning ("ldef_freelangdefent(): NULL pointer!");
		return;
	}
	switch (lde->type) {
	case LDE_INVALID:
		break;
	case LDE_GRL:
	case LDE_RFUNC:
		if (lde->u.redex.name) {
			sfree (lde->u.redex.name);
			lde->u.redex.name = NULL;
		}
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
	case LDE_KEYWORD:
		if (lde->u.keyword) {
			sfree (lde->u.keyword);
			lde->u.keyword = NULL;
		}
		break;
	case LDE_SYMBOL:
		if (lde->u.symbol) {
			sfree (lde->u.symbol);
			lde->u.symbol = NULL;
		}
		break;
	case LDE_DFAERR:
		if (lde->u.dfaerror.dfaname) {
			sfree (lde->u.dfaerror.dfaname);
			lde->u.dfaerror.dfaname = NULL;
		}
		if (lde->u.dfaerror.msg) {
			sfree (lde->u.dfaerror.msg);
			lde->u.dfaerror.msg = NULL;
		}
		break;
	case LDE_TNODE:
		if (lde->u.tnode.name) {
			sfree (lde->u.tnode.name);
			lde->u.tnode.name = NULL;
		}
		for (i=0; i<DA_CUR (lde->u.tnode.descs); i++) {
			char *desc = DA_NTHITEM (lde->u.tnode.descs, i);

			if (desc) {
				sfree (desc);
			}
		}
		dynarray_trash (lde->u.tnode.descs);
		if (lde->u.tnode.invafter) {
			sfree (lde->u.tnode.invafter);
			lde->u.tnode.invafter = NULL;
		}
		if (lde->u.tnode.invbefore) {
			sfree (lde->u.tnode.invbefore);
			lde->u.tnode.invbefore = NULL;
		}
		break;
	}
	lde->type = LDE_INVALID;
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


/*{{{  static langdefsec_t *ldef_ensuresection (langdef_t *ldef, const char *ident, const char *rfname, const int lineno)*/
/*
 *	make sure that a section exists, uses ldef->cursec if set
 */
static langdefsec_t *ldef_ensuresection (langdef_t *ldef, const char *ident, const char *rfname, const int lineno)
{
	langdefsec_t *lsec = ldef->cursec;

	if (!lsec) {
		nocc_warning ("%s outside section at %s:%d, creating omnipotent section", ident ?: "(unknown)", rfname, lineno);

		lsec = ldef_newlangdefsec ();
		lsec->ident = string_dup ("");
		lsec->ldef = ldef;

		dynarray_add (ldef->sections, lsec);
		ldef->cursec = lsec;
	}
	return lsec;
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
	if (*(bits[0]) == '.') {
		langdeflookup_t *ldl = langdeflookup_lookup (bits[0] + 1, strlen (bits[0] + 1));

		if (!ldl) {
			/*{{{  unknown directive!*/
			nocc_error ("unknown directive %s at %s:%d", bits[0], rfname, lineno);
			rval = -1;
			goto out_local;
			/*}}}*/
		} else {
			switch (ldl->ldl) {
				/*{{{  .IDENT -- identifying the language definition file*/
			case LDL_IDENT:
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

				break;
				/*}}}*/
				/*{{{  .DESC -- general description for the language definition*/
			case LDL_DESC:
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

				break;
				/*}}}*/
				/*{{{  .MAINTAINER -- person(s) responsible for this language definition*/
			case LDL_MAINTAINER:
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

				break;
				/*}}}*/
				/*{{{  .SECTION -- starting a named section of definitions*/
			case LDL_SECTION:
				{
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
				}
				break;
				/*}}}*/
				/*{{{  .GRULE -- generic reduction name and rule*/
			case LDL_GRULE:
				{
					langdefsec_t *lsec = NULL;
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

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = LDE_GRL;
					lfe->u.redex.name = string_dup (bits[1]);
					lfe->u.redex.desc = dstr;

					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .RFUNC -- reduction function (must be registered!)*/
			case LDL_RFUNC:
				{
					langdefsec_t *lsec = NULL;
					langdefent_t *lfe = NULL;

					if (nbits < 3) {
						goto out_malformed;
					}

					string_dequote (bits[1]);
					string_dequote (bits[2]);

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = LDE_RFUNC;
					lfe->u.redex.name = string_dup (bits[1]);
					lfe->u.redex.desc = string_dup (bits[2]);

					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .BNF, .TABLE -- DFA BNF-rule or transition-table*/
			case LDL_BNF:
			case LDL_TABLE:
				{
					langdefsec_t *lsec = NULL;
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

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = (ldl->ldl == LDL_BNF) ? LDE_DFABNF : LDE_DFATRANS;
					lfe->u.dfarule = dstr;

					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .KEYWORD -- language keyword*/
			case LDL_KEYWORD:
				{
					langdefsec_t *lsec = NULL;
					langdefent_t *lfe = NULL;

					if (nbits < 2) {
						goto out_malformed;
					}
					string_dequote (bits[1]);

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = LDE_KEYWORD;
					lfe->u.keyword = string_dup (bits[1]);

					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .SYMBOL -- language symbol*/
			case LDL_SYMBOL:
				{
					langdefsec_t *lsec = NULL;
					langdefent_t *lfe = NULL;

					if (nbits < 2) {
						goto out_malformed;
					}
					string_dequote (bits[1]);

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = LDE_SYMBOL;
					lfe->u.symbol = string_dup (bits[1]);

					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .DFAERR -- DFA error handler message*/
			case LDL_DFAERR:
				{
					langdefsec_t *lsec = NULL;
					langdefent_t *lfe = NULL;
					dfaerrorsource_e esrc = DFAERRSRC_INVALID;
					dfaerrorreport_e erep = DFAERR_NONE;

					if (nbits < 4) {
						goto out_malformed;
					}

					string_dequote (bits[1]);		/* shouldn't be quoted, but we'll go with it if so */
					string_dequote (bits[2]);
					string_dequote (bits[3]);

					esrc = dfaerror_decodesource (bits[1]);
					if (esrc == DFAERRSRC_INVALID) {
						nocc_error ("unknown DFA error type [%s] at %s:%d", bits[1], rfname, lineno);
						rval = -1;
						goto out_local;
					}

					for (i=4; bits[i]; i++) {
						dfaerrorreport_e lerep = dfaerror_decodereport (bits[i]);

						if (lerep == DFAERR_INVALID) {
							nocc_error ("unknown DFA error report [%s] at %s:%d", bits[i], rfname, lineno);
							rval = -1;
							goto out_local;
						}
						erep |= lerep;
					}

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = LDE_DFAERR;
					lfe->u.dfaerror.source = (int)esrc;
					lfe->u.dfaerror.rcode = (int)erep;
					lfe->u.dfaerror.dfaname = string_dup (bits[2]);
					lfe->u.dfaerror.msg = string_dup (bits[3]);

					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .TNODE -- treenode definition (for checking, not defining!)*/
			case LDL_TNODE:
				{
					langdefsec_t *lsec = NULL;
					langdefent_t *lfe = NULL;
					char *cdefs;
					int nsub, nname, nhook;
					int i, nextidx;

					if (nbits < 3) {
						goto out_malformed;
					}

					string_dequote (bits[1]);		/* node name */

					/* should have (nsub,nnode,nhook) next */
					if (sscanf (bits[2], "(%d,%d,%d)", &nsub, &nname, &nhook) != 3) {
						nocc_error ("badly formatted node counts in .TNODE definition [%s] at %s:%d", bits[2], rfname, lineno);
						rval = -1;
						goto out_local;
					} else if ((nsub < 0) || (nname < 0) || (nhook < 0)) {
						nocc_error ("invalid sub-node, name or hook count [%s] at %s:%d", bits[2], rfname, lineno);
						rval = -1;
						goto out_local;
					}

					lsec = ldef_ensuresection (ldef, bits[0], rfname, lineno);

					lfe = ldef_newlangdefent ();
					lfe->ldef = ldef;
					lfe->lineno = lineno;

					lfe->type = LDE_TNODE;
					lfe->u.tnode.name = string_dup (bits[1]);
					lfe->u.tnode.nsub = nsub;
					lfe->u.tnode.nname = nname;
					lfe->u.tnode.nhook = nhook;
					dynarray_init (lfe->u.tnode.descs);

					/* read in textual descriptions */
					for (i=0, nextidx=3; (i < (nsub + nname + nhook)) && (nextidx < nbits); i++, nextidx++) {
						string_dequote (bits[nextidx]);
						dynarray_add (lfe->u.tnode.descs, string_dup (bits[nextidx]));
					}

					if (i != (nsub + nname + nhook)) {
						ldef_freelangdefent (lfe);
						nocc_error ("was expecting %d definitions in .TNODE definition for [%s] at %s:%d", (nsub + nname + nhook), bits[1], rfname, lineno);
						rval = -1;
						goto out_local;
					}

					/* scoop up extra things */
					for (; nextidx < nbits; nextidx++) {
						langdeflookup_t *nldl = langdeflookup_lookup (bits[nextidx], strlen (bits[nextidx]));

						if (nldl && (nldl->ldl == LDL_KINVALID)) {
							/*{{{  INVALID -- specifies when the node is valid*/
							if ((nextidx + 3) > nbits) {
								ldef_freelangdefent (lfe);
								nocc_error ("malformed INVALID setting in .TNODE definition for [%s] at %s:%d", bits[1], rfname, lineno);
								rval = -1;
								goto out_local;
							}

							string_dequote (bits[nextidx + 2]);

							nldl = langdeflookup_lookup (bits[nextidx + 1], strlen (bits[nextidx + 1]));
							if (nldl && (nldl->ldl == LDL_BEFORE)) {
								/* node invalid before a particular pass */
								if (lfe->u.tnode.invbefore) {
									ldef_freelangdefent (lfe);
									nocc_error ("already have INVALID BEFORE setting in .TNODE definition for [%s] at %s:%d", bits[1], rfname, lineno);
									rval = -1;
									goto out_local;
								}
								lfe->u.tnode.invbefore = string_dup (bits[nextidx + 2]);
							} else if (nldl && (nldl->ldl == LDL_AFTER)) {
								/* node invalid after a particular pass */
								if (lfe->u.tnode.invafter) {
									ldef_freelangdefent (lfe);
									nocc_error ("already have INVALID AFTER setting in .TNODE definition for [%s] at %s:%d", bits[1], rfname, lineno);
									rval = -1;
									goto out_local;
								}
								lfe->u.tnode.invafter = string_dup (bits[nextidx + 2]);
							} else {
								ldef_freelangdefent (lfe);
								nocc_error ("unknown INVALID setting [%s] in .TNODE definition for [%s] at %s:%d", bits[nextidx + 1], bits[1], rfname, lineno);
								rval = -1;
								goto out_local;
							}

							nextidx += 2;
							/*}}}*/
						} else {
							/*{{{  something else -- bad*/
							ldef_freelangdefent (lfe);
							nocc_error ("unknown setting [%s] in .TNODE definition for [%s] at %s:%d", bits[nextidx], bits[1], rfname, lineno);
							rval = -1;
							goto out_local;
							/*}}}*/
						}
					}

					/* finally, add to section entities */
					dynarray_add (lsec->ents, lfe);
				}
				break;
				/*}}}*/
				/*{{{  .IMPORT -- import definitions from another file*/
			case LDL_IMPORT:
				{
					langdef_t *ildef;

					if (nbits < 2) {
						goto out_malformed;
					}
					string_dequote (bits[1]);

					ildef = langdef_readdefs (bits[1]);
					if (!ildef) {
						nocc_error ("failed to import %s at %s:%d", bits[1], rfname, lineno);
						rval = -1;
						goto out_local;
					}

					for (i=0; i<DA_CUR (ildef->sections); i++) {
						langdefsec_t *ilsec = DA_NTHITEM (ildef->sections, i);
						langdefsec_t *lsec = NULL;
						int j;

						if (!strcmp (ilsec->ident, ildef->ident)) {
							/* this is the pre-init section, import into other definition pre-init */
							lsec = ldef_ensuresection (ldef, ldef->ident, rfname, lineno);
						} else {
							/* anything else, import flat */
							lsec = ldef_ensuresection (ldef, ilsec->ident, rfname, lineno);
						}

						for (j=0; j<DA_CUR (ilsec->ents); j++) {
							langdefent_t **ildep = DA_NTHITEMADDR (ilsec->ents, j);

							dynarray_add (lsec->ents, *ildep);
							*ildep = NULL;
						}
						dynarray_trash (ilsec->ents);
						dynarray_init (ilsec->ents);
					}

					langdef_freelangdef (ildef);
				}
				break;
				/*}}}*/
			default:
				nocc_error ("unknown directive %s at %s:%d", bits[0], rfname, lineno);
				rval = -1;
				goto out_local;
			}
		}
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

/*{{{  int langdef_init_tokens (langdefsec_t *lsec, const unsigned int langtag, origin_t *origin)*/
/*
 *	registers tokens defined in a particular section
 *	returns 0 on success, non-zero on failure
 */
int langdef_init_tokens (langdefsec_t *lsec, const unsigned int langtag, origin_t *origin)
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
			/*{{{  LDE_KEYWORD -- new keyword*/
		case LDE_KEYWORD:
			keywords_add (lde->u.keyword, -1, langtag, origin);
			break;
			/*}}}*/
			/*{{{  LDE_SYMBOL -- new symbol*/
		case LDE_SYMBOL:
			/* if the symbol is already in use, merge in language tag */
			symbols_add (lde->u.symbol, strlen (lde->u.symbol), langtag, origin);
			break;
			/*}}}*/
		}
	}
	return rval;
}
/*}}}*/
/*{{{  int langdef_init_nodes (langdefsec_t *lsec, origin_t *origin)*/
/*
 *	creates new node types and tags defined in a particular section
 *	returns 0 on success, non-zero on failure
 */
int langdef_init_nodes (langdefsec_t *lsec, origin_t *origin)
{
	int rval = 0;
	int i;

	if (!lsec) {
		/* probably failed somewhere earlier */
		return 0;
	}

	for (i=0; i<DA_CUR (lsec->ents); i++) {
		langdefent_t *lde = DA_NTHITEM (lsec->ents, i);

		switch (lde->type) {
		default:
			break;
		/* FIXME! */
		}
	}
	return rval;
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
			/*{{{  LDE_GRL -- generic reduction rule*/
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
			/*}}}*/
			/*{{{  LDE_RFUNC -- named reduction function (must be registered with fcnlib or extn)*/
		case LDE_RFUNC:
			{
				void *sym;

				/* find the named symbol */
				sym = fcnlib_findfunction (lde->u.redex.desc);
				if (!sym) {
					sym = extn_findsymbol (lde->u.redex.desc);
					if (!sym) {
						nocc_error ("invalid reduction function [%s] in language definition for [%s (%s)], line %d", lde->u.redex.desc, lsec->ldef->ident, lsec->ident, lde->lineno);
						if (lsec->ldef->maintainer) {
							nocc_message ("maintainer for [%s] is: %s", lsec->ldef->ident, lsec->ldef->maintainer);
						}
						rval = -1;
					}
				}
				if (sym) {
					parser_register_reduce (lde->u.redex.name, sym, NULL);		/* FIXME: extra argument? */
				}
			}
			break;
			/*}}}*/
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
/*{{{  int langdef_post_setup (langdefsec_t *lsec)*/
/*
 *	registers any things needed in post-setup (such as DFA error-handler messages)
 *	returns 0 on success, non-zero on failure
 */
int langdef_post_setup (langdefsec_t *lsec)
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
			/*{{{  LDE_DFAERR -- DFA error handling specification*/
		case LDE_DFAERR:
			if (dfaerror_defaulthandler (lde->u.dfaerror.dfaname, lde->u.dfaerror.msg, (dfaerrorsource_e)lde->u.dfaerror.source, (dfaerrorreport_e)lde->u.dfaerror.rcode)) {
				nocc_error ("unable to set DFA error handler in language definition for [%s (%s)], line %d", lsec->ldef->ident, lsec->ident, lde->lineno);
				if (lsec->ldef->maintainer) {
					nocc_message ("maintainer for [%s] is: %s", lsec->ldef->ident, lsec->ldef->maintainer);
				}
				rval = -1;
			}
			break;
			/*}}}*/
		}
	}

	return rval;
}
/*}}}*/
/*{{{  int langdef_treecheck_setup (langdef_t *ldef)*/
/*
 *	runs through all sections in a language definition (order unimportant) and
 *	deals with tree-checking setup (treecheck.c)
 *	returns 0 on success, non-zero on failure
 */
int langdef_treecheck_setup (langdef_t *ldef)
{
	int rval = 0;
	int i;

	if (!compopts.treecheck) {
		/* not wanted */
		return 0;
	}

	for (i=0; i<DA_CUR (ldef->sections); i++) {
		langdefsec_t *lsec = DA_NTHITEM (ldef->sections, i);
		int j;

		for (j=0; j<DA_CUR (lsec->ents); j++) {
			langdefent_t *lde = DA_NTHITEM (lsec->ents, j);

			switch (lde->type) {
			default:
				break;
				/*{{{  LDE_TNODE -- node details*/
			case LDE_TNODE:
				if (treecheck_createcheck (lde->u.tnode.name, lde->u.tnode.nsub, lde->u.tnode.nname, lde->u.tnode.nhook,
						DA_PTR (lde->u.tnode.descs), lde->u.tnode.invbefore, lde->u.tnode.invafter) == NULL) {
					nocc_error ("failed to create tree-check for node type [%s] in language definition for [%s (%s)], line %d", lde->u.tnode.name, lsec->ldef->ident, lsec->ident, lde->lineno);
					rval = -1;
				}
				break;
				/*}}}*/
			}
		}
	}

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


