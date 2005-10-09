/*
 *	library.c -- libraries/separate-compilation for NOCC
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "opts.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "parsepriv.h"
#include "dfa.h"
#include "names.h"
#include "betrans.h"
#include "library.h"
#include "target.h"


/*}}}*/


/*{{{  private types*/
struct TAG_libnodehook;

typedef struct TAG_libtaghook {
	struct TAG_libnodehook *lnh;
	char *name;
	int ws, vs, ms, adjust;
} libtaghook_t;

typedef struct TAG_libnodehook {
	lexfile_t *lf;		/* lexfile where the library directive was */
	char *libname;		/* e.g. "mylib" (responsible for output-file name)*/
	char *langname;		/* e.g. "occam-pi" */
	char *targetname;	/* e.g. "kroc-etc-unknown" */
	DYNARRAY (char *, autoinclude);
	DYNARRAY (char *, autouse);

	DYNARRAY (libtaghook_t *, entries);
} libnodehook_t;


/*}}}*/
/*{{{  private data*/
static tndef_t *tnd_libnode = NULL;
static ntdef_t *tag_libnode = NULL;
static tndef_t *tnd_libtag = NULL;
static ntdef_t *tag_publictag = NULL;

static char *libpath = NULL;
static int allpublic = 0;

static chook_t *libchook = NULL;


STATICDYNARRAY (libnodehook_t *, entrystack);

/*}}}*/


/*{{{  static int lib_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for library options
 *	returns 0 on success, non-zero on failure
 */
static int lib_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;
	char *ch;

	switch (optv) {
		/*{{{  --liboutpath <path>*/
	case 1:
		if ((ch = strchr (**argwalk, '=')) != NULL) {
			ch++;
		} else {
			(*argwalk)++;
			(*argleft)--;
			if (!**argwalk || !*argleft) {
				nocc_error ("missing argument for option %s", (*argwalk)[-1]);
				return -1;
			}
			ch = **argwalk;
		}
		if (libpath) {
			nocc_warning ("replacing library path (was %s)", libpath);
			sfree (libpath);
		}
		libpath = string_dup (ch);
		break;
		/*}}}*/
		/*{{{  --liballpublic*/
	case 2:
		allpublic = 1;
		break;
		/*}}}*/
	default:
		nocc_error ("lib_opthandler(): unknown option [%s]", **argwalk);
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  static void lib_isetindent (FILE *stream, int indent)*/
/*
 *	produces indentation (debugging+output)
 */
static void lib_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static libtaghook_t *lib_newlibtaghook (libnodehook_t *lnh, char *ename)*/
/*
 *	creates a new libtaghook_t structure
 */
static libtaghook_t *lib_newlibtaghook (libnodehook_t *lnh, char *ename)
{
	libtaghook_t *lth = (libtaghook_t *)smalloc (sizeof (libtaghook_t));

	lth->lnh = lnh;
	lth->name = ename ? string_dup (ename) : NULL;
	lth->ws = 0;
	lth->vs = 0;
	lth->ms = 0;
	lth->adjust = 0;

	return lth;
}
/*}}}*/
/*{{{  static void lib_libtaghook_free (void *hook)*/
/*
 *	frees a libtag hook
 */
static void lib_libtaghook_free (void *hook)
{
	libtaghook_t *lth = (libtaghook_t *)hook;

	if (lth) {
		if (lth->name) {
			sfree (lth->name);
		}

		sfree (lth);
	}
	return;
}
/*}}}*/
/*{{{  static void *lib_libtaghook_copy (void *hook)*/
/*
 *	copies a libtag hook
 */
static void *lib_libtaghook_copy (void *hook)
{
	libtaghook_t *lth = (libtaghook_t *)hook;

	if (lth) {
		libtaghook_t *newlth = lib_newlibtaghook (lth->lnh, lth->name);

		newlth->ws = lth->ws;
		newlth->vs = lth->vs;
		newlth->ms = lth->ms;
		newlth->adjust = lth->adjust;

		return (void *)newlth;
	}

	return NULL;
}
/*}}}*/
/*{{{  static void lib_libtaghook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for libtag hook (debugging)
 */
static void lib_libtaghook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	libtaghook_t *lth = (libtaghook_t *)hook;

	lib_isetindent (stream, indent);
	fprintf (stream, "<libtaghook addr=\"0x%8.8x\" name=\"%s\" ws=\"%d\" vs=\"%d\" ms=\"%d\" adjust=\"%d\" />\n", (unsigned int)lth, lth->name ?: "(null)", lth->ws, lth->vs, lth->ms, lth->adjust);

	return;
}
/*}}}*/


/*{{{  static libnodehook_t *lib_newlibnodehook (lexfile_t *lf, char *libname)*/
/*
 *	creates a new libnodehook_t structure
 */
static libnodehook_t *lib_newlibnodehook (lexfile_t *lf, char *libname)
{
	libnodehook_t *lnh = (libnodehook_t *)smalloc (sizeof (libnodehook_t));

	lnh->lf = lf;
	lnh->libname = string_dup (libname);
	lnh->langname = string_dup (lf->parser->langname);
	lnh->targetname = NULL;				/* set in betrans */
	dynarray_init (lnh->autoinclude);
	dynarray_init (lnh->autouse);

	dynarray_init (lnh->entries);

	return lnh;
}
/*}}}*/
/*{{{  static void lib_libnodehook_free (void *hook)*/
/*
 *	frees a libnode hook
 */
static void lib_libnodehook_free (void *hook)
{
	libnodehook_t *lnh = (libnodehook_t *)hook;

	if (lnh) {
		int i;

		if (lnh->libname) {
			sfree (lnh->libname);
		}
		if (lnh->langname) {
			sfree (lnh->langname);
		}
		if (lnh->targetname) {
			sfree (lnh->targetname);
		}
		for (i=0; i<DA_CUR (lnh->autoinclude); i++) {
			if (DA_NTHITEM (lnh->autoinclude, i)) {
				sfree (DA_NTHITEM (lnh->autoinclude, i));
			}
		}
		for (i=0; i<DA_CUR (lnh->autouse); i++) {
			if (DA_NTHITEM (lnh->autouse, i)) {
				sfree (DA_NTHITEM (lnh->autouse, i));
			}
		}
		dynarray_trash (lnh->autoinclude);
		dynarray_trash (lnh->autouse);

		sfree (lnh);
	}
	return;
}
/*}}}*/
/*{{{  static void *lib_libnodehook_copy (void *hook)*/
/*
 *	copies a libnode hook
 */
static void *lib_libnodehook_copy (void *hook)
{
	libnodehook_t *lnh = (libnodehook_t *)hook;

	if (lnh) {
		libnodehook_t *newlnh = lib_newlibnodehook (NULL, lnh->libname);
		int i;

		newlnh->langname = string_dup (lnh->langname);
		newlnh->targetname = string_dup (lnh->targetname);
		for (i=0; i<DA_CUR (lnh->autoinclude); i++) {
			dynarray_add (newlnh->autoinclude, string_dup (DA_NTHITEM (lnh->autoinclude, i)));
		}
		for (i=0; i<DA_CUR (lnh->autouse); i++) {
			dynarray_add (newlnh->autouse, string_dup (DA_NTHITEM (lnh->autouse, i)));
		}

		return (void *)newlnh;
	}

	return NULL;
}
/*}}}*/
/*{{{  static void lib_libnodehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for libnode hook (debugging)
 */
static void lib_libnodehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	libnodehook_t *lnh = (libnodehook_t *)hook;
	int i;

	lib_isetindent (stream, indent);
	fprintf (stream, "<libnodehook addr=\"0x%8.8x\">\n", (unsigned int)lnh);

	lib_isetindent (stream, indent + 1);
	fprintf (stream, "<library name=\"%s\" namespace=\"%s\">\n", lnh->libname, lnh->libname);

	if (lnh->langname) {
		lib_isetindent (stream, indent + 2);
		fprintf (stream, "<lang name=\"%s\" />\n", lnh->langname);
	}
	if (lnh->targetname) {
		lib_isetindent (stream, indent + 2);
		fprintf (stream, "<target name=\"%s\" />\n", lnh->targetname);
	}

	for (i=0; i<DA_CUR (lnh->autoinclude); i++) {
		char *iname = DA_NTHITEM (lnh->autoinclude, i);

		if (iname) {
			lib_isetindent (stream, indent + 2);
			fprintf (stream, "<srcinclude path=\"%s\" />\n", iname);
		}
	}
	for (i=0; i<DA_CUR (lnh->autouse); i++) {
		char *lname = DA_NTHITEM (lnh->autouse, i);

		if (lname) {
			lib_isetindent (stream, indent + 2);
			fprintf (stream, "<srcuse path=\"%s\" />\n", lname);
		}
	}
	for (i=0; i<DA_CUR (lnh->entries); i++) {
		libtaghook_t *entry = DA_NTHITEM (lnh->entries, i);

		if (entry) {
			lib_libtaghook_dumptree (node, (void *)entry, indent + 2, stream);
		}
	}

	lib_isetindent (stream, indent + 1);
	fprintf (stream, "</library>\n");

	lib_isetindent (stream, indent);
	fprintf (stream, "</libnodehook>\n");
	return;
}
/*}}}*/


/*{{{  static int lib_betrans_libnode (tnode_t **nodep, target_t *target)*/
/*
 *	does back-end transform on a library node
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_betrans_libnode (tnode_t **nodep, target_t *target)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (*nodep, 0);
	int slen = 0;

#if 0
fprintf (stderr, "lib_betrans_libnode(): here!\n");
#endif
	if (!lnh) {
		nocc_internal ("lib_betrans_libnode(): NULL hook!");
		return 0;
	}
	slen += target->tarch ? strlen (target->tarch) : 7;
	slen += target->tvendor ? strlen (target->tvendor) : 7;
	slen += target->tos ? strlen (target->tos) : 7;

	lnh->targetname = (char *)smalloc (slen + 4);
	sprintf (lnh->targetname, "%s-%s-%s", target->tarch ?: "unknown", target->tvendor ?: "unknown", target->tos ?: "unknown");

	dynarray_add (entrystack, lnh);
	betrans_subtree (tnode_nthsubaddr (*nodep, 0), target);
	dynarray_delitem (entrystack, DA_CUR (entrystack) - 1);

	return 0;
}
/*}}}*/
/*{{{  static int lib_betrans_libtag (tnode_t **nodep, target_t *target)*/
/*
 *	does back-end transform on a library tag node
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_betrans_libtag (tnode_t **nodep, target_t *target)
{
	libtaghook_t **lthp = (libtaghook_t **)tnode_nthhookaddr (*nodep, 0);
	libnodehook_t *lnh;

	if (!*lthp) {
		nocc_error ("lib_betrans_libtag(): node has no hook");
		return 1;
	}
	if (!DA_CUR (entrystack)) {
		nocc_error ("lib_betrans_libtag(): no library..");
		return 1;
	}
	lnh = DA_NTHITEM (entrystack, DA_CUR (entrystack) - 1);

	if ((*lthp)->lnh) {
		nocc_warning ("lib_betrans_libtag(): tag already linked to library");
		return 1;
	}

	(*lthp)->lnh = lnh;
	dynarray_add (lnh->entries, *lthp);

	return 1;
}
/*}}}*/


/*{{{  int library_init (void)*/
/*
 *	initialises library handling
 *	returns 0 on success, non-zero on failure
 */
int library_init (void)
{
	int i;
	compops_t *cops;

	/*{{{  nocc:libnode -- LIBNODE*/
	i = -1;
	tnd_libnode = tnode_newnodetype ("nocc:libnode", &i, 1, 0, 1, TNF_TRANSPARENT);
	tnd_libnode->hook_free = lib_libnodehook_free;
	tnd_libnode->hook_copy = lib_libnodehook_copy;
	tnd_libnode->hook_dumptree = lib_libnodehook_dumptree;
	cops = tnode_newcompops ();
	cops->betrans = lib_betrans_libnode;
	tnd_libnode->ops = cops;

	i = -1;
	tag_libnode = tnode_newnodetag ("LIBNODE", &i, tnd_libnode, NTF_NONE);
	/*}}}*/
	/*{{{  nocc:libtag -- PUBLICTAG*/
	i = -1;
	tnd_libtag = tnode_newnodetype ("nocc:libtag", &i, 1, 0, 1, TNF_TRANSPARENT);
	tnd_libtag->hook_free = lib_libtaghook_free;
	tnd_libtag->hook_copy = lib_libtaghook_copy;
	tnd_libtag->hook_dumptree = lib_libtaghook_dumptree;
	cops = tnode_newcompops ();
	cops->betrans = lib_betrans_libtag;
	tnd_libtag->ops = cops;

	i = -1;
	tag_publictag = tnode_newnodetag ("PUBLICTAG", &i, tnd_libtag, NTF_NONE);
	/*}}}*/

	/*{{{  command-line options: "--liboutpath <path>", "--liballpublic"*/
	opts_add ("liboutpath", '\0', lib_opthandler, (void *)1, "0output directory for library info");
	opts_add ("liballpublic", '\0', lib_opthandler, (void *)2, "1all top-level entries public in library");

	/*}}}*/
	/*{{{  others*/
	dynarray_init (entrystack);

	libchook = tnode_newchook ("lib:mark");

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  int library_shutdown (void)*/
/*
 *	shuts-down library handling
 *	returns 0 on success, non-zero on failure
 */
int library_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  tnode_t *library_newlibnode (lexfile_t *lf, char *libname)*/
/*
 *	creates a new library node, used when building libraries
 *	returns library-node on success, NULL on failure
 */
tnode_t *library_newlibnode (lexfile_t *lf, char *libname)
{
	tnode_t *lnode;
	libnodehook_t *lnh = lib_newlibnodehook (lf, libname);

	lnode = tnode_create (tag_libnode, lf, NULL, lnh);

	return lnode;
}
/*}}}*/
/*{{{  int library_addincludes (tnode_t *libnode, char *iname)*/
/*
 *	adds an "auto-include" to a library
 *	returns 0 on success, non-zero on failure
 */
int library_addincludes (tnode_t *libnode, char *iname)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (libnode, 0);

	dynarray_add (lnh->autoinclude, string_dup (iname));
	return 0;
}
/*}}}*/
/*{{{  int library_adduses (tnode_t *libnode, char *lname)*/
/*
 *	adds an "auto-use" to a library
 *	returns 0 on success, non-zero on failure
 */
int library_adduses (tnode_t *libnode, char *lname)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (libnode, 0);

	dynarray_add (lnh->autouse, string_dup (lname));
	return 0;
}
/*}}}*/
/*{{{  tnode_t *library_newlibpublictag (lexfile_t *lf, char *name)*/
/*
 *	creates a new public-tag node, used when building libraries
 *	returns node on success, NULL on failure
 */
tnode_t *library_newlibpublictag (lexfile_t *lf, char *name)
{
	tnode_t *pnode;
	libtaghook_t *lth = lib_newlibtaghook (NULL, name);

	pnode = tnode_create (tag_publictag, lf, NULL, lth);

	return pnode;
}
/*}}}*/
/*{{{  int library_markpublic (tnode_t *node)*/
/*
 *	marks a node as being public
 *	returns 0 on success, non-zero on failure
 */
int library_markpublic (tnode_t *node)
{
	return 0;
}
/*}}}*/



