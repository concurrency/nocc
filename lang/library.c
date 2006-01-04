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
#include <errno.h>

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
#include "prescope.h"
#include "scope.h"
#include "typecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "library.h"
#include "map.h"
#include "codegen.h"
#include "crypto.h"
#include "target.h"
#include "treeops.h"
#include "xml.h"


/*}}}*/


/*{{{  library-file private types*/
typedef struct TAG_libfile_entry {
	char *name;		/* entry-name */
	char *langname;
	char *targetname;
	char *descriptor;	/* what is actually re-parsed by the compiler */
	int ws, vs, ms, adjust;	/* space required */
} libfile_entry_t;

typedef struct TAG_libfile_srcunit {
	char *fname;		/* short name of source file */
	DYNARRAY (libfile_entry_t *, entries);
	char *hashalgo;
	char *hashvalue;
	int issigned;

	/* below used when parsing, not general info! */
	libfile_entry_t *curentry;
} libfile_srcunit_t;

typedef struct TAG_libfile {
	char *fname;		/* full path (or as much as we have) to library XML */
	char *libname;
	char *namespace;
	char *nativelib;	/* native library name (if we have one) */

	DYNARRAY (libfile_srcunit_t *, srcs);
	DYNARRAY (char *, autoinclude);
	DYNARRAY (char *, autouse);

	/* below used when parsing, not general info! */
	libfile_srcunit_t *curunit;
} libfile_t;
/*}}}*/
/*{{{  private types*/
/*{{{  library definition*/
struct TAG_libnodehook;

typedef struct TAG_libtaghook {
	struct TAG_libnodehook *lnh;
	char *name;
	int ws, vs, ms, adjust;
	char *descriptor;
	tnode_t *bnode;		/* back-end BLOCK associated with this entry */
} libtaghook_t;

typedef struct TAG_libnodehook {
	lexfile_t *lf;		/* lexfile where the library directive was */
	char *libname;		/* e.g. "mylib" (responsible for output-file name) */
	char *namespace;	/* e.g. "mylib" (default namespace) */
	char *langname;		/* e.g. "occam-pi" */
	char *targetname;	/* e.g. "kroc-etc-unknown" */
	char *nativelib;	/* e.g. "libmylib.so" */
	int issepcomp;		/* non-zero if generating a .xlo */
	DYNARRAY (char *, autoinclude);
	DYNARRAY (char *, autouse);
	char *hashalgo;		/* of generated-code */
	char *hashvalue;
	int issigned;

	DYNARRAY (libtaghook_t *, entries);
} libnodehook_t;


/*}}}*/
/*{{{  library usage*/
typedef struct TAG_libusenodehook {
	lexfile_t *lf;
	char *libname;
	char *namespace;
	char *asnamespace;
	libfile_t *libdata;
	tnode_t *decltree;
	DYNARRAY (tnode_t *, decls);
} libusenodehook_t;


/*}}}*/
/*}}}*/
/*{{{  private data*/
static tndef_t *tnd_libnode = NULL;
static ntdef_t *tag_libnode = NULL;
static tndef_t *tnd_libtag = NULL;
static ntdef_t *tag_publictag = NULL;
static tndef_t *tnd_libusenode = NULL;
static ntdef_t *tag_libusenode = NULL;
static tndef_t *tnd_templibusenode = NULL;
static ntdef_t *tag_templibusenode = NULL;

static char *libpath = NULL;
static char *scpath = NULL;
static int allpublic = 0;

static chook_t *libchook = NULL;
static chook_t *uselinkchook = NULL;
static chook_t *descriptorchook = NULL;


STATICDYNARRAY (libnodehook_t *, entrystack);

/*}}}*/
/*{{{  forward decls*/
static void lib_freelibfile (libfile_t *lf);

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
		/*{{{  --scoutpath <path>*/
	case 3:
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
		if (scpath) {
			nocc_warning ("replacing library path (was %s)", scpath);
			sfree (scpath);
		}
		scpath = string_dup (ch);
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
	lth->descriptor = NULL;
	lth->ws = 0;
	lth->vs = 0;
	lth->ms = 0;
	lth->adjust = 0;
	lth->bnode = NULL;

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
		if (lth->descriptor) {
			sfree (lth->descriptor);
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
		newlth->descriptor = lth->descriptor ? string_dup (lth->descriptor) : NULL;

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
	fprintf (stream, "<libtaghook addr=\"0x%8.8x\" name=\"%s\" ws=\"%d\" vs=\"%d\" ms=\"%d\" adjust=\"%d\" bnode=\"0x%8.8x\" descriptor=\"%s\" />\n",
			(unsigned int)lth, lth->name ?: "(null)", lth->ws, lth->vs, lth->ms, lth->adjust, (unsigned int)lth->bnode, lth->descriptor ?: "(null)");

	return;
}
/*}}}*/


/*{{{  static libnodehook_t *lib_newlibnodehook (lexfile_t *lf, char *libname, char *namespace)*/
/*
 *	creates a new libnodehook_t structure
 */
static libnodehook_t *lib_newlibnodehook (lexfile_t *lf, char *libname, char *namespace)
{
	libnodehook_t *lnh = (libnodehook_t *)smalloc (sizeof (libnodehook_t));

	lnh->lf = lf;
	lnh->libname = string_dup (libname);
	lnh->namespace = namespace ? string_dup (namespace) : string_dup ("");
	lnh->langname = lf ? string_dup (lf->parser->langname) : NULL;
	lnh->targetname = NULL;				/* set in betrans */
	lnh->nativelib = NULL;
	lnh->issepcomp = 0;
	dynarray_init (lnh->autoinclude);
	dynarray_init (lnh->autouse);
	lnh->hashalgo = NULL;
	lnh->hashvalue = NULL;
	lnh->issigned = 0;

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
		if (lnh->namespace) {
			sfree (lnh->namespace);
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
		libnodehook_t *newlnh = lib_newlibnodehook (NULL, lnh->libname, lnh->namespace);
		int i;

		newlnh->langname = lnh->langname ? string_dup (lnh->langname) : NULL;
		newlnh->targetname = lnh->targetname ? string_dup (lnh->targetname) : NULL;
		for (i=0; i<DA_CUR (lnh->autoinclude); i++) {
			dynarray_add (newlnh->autoinclude, string_dup (DA_NTHITEM (lnh->autoinclude, i)));
		}
		for (i=0; i<DA_CUR (lnh->autouse); i++) {
			dynarray_add (newlnh->autouse, string_dup (DA_NTHITEM (lnh->autouse, i)));
		}
		newlnh->issepcomp = lnh->issepcomp;

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
	fprintf (stream, "<library name=\"%s\" namespace=\"%s\">\n", lnh->libname, lnh->namespace);

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


/*{{{  static libusenodehook_t *lib_newlibusenodehook (lexfile_t *lf, char *libname)*/
/*
 *	creates a new libusenodehook_t structure
 */
static libusenodehook_t *lib_newlibusenodehook (lexfile_t *lf, char *libname)
{
	libusenodehook_t *lunh = (libusenodehook_t *)smalloc (sizeof (libusenodehook_t));

	lunh->lf = lf;
	lunh->libname = string_dup (libname);
	lunh->namespace = string_dup (libname);
	lunh->asnamespace = NULL;
	lunh->libdata = NULL;
	lunh->decltree = NULL;
	dynarray_init (lunh->decls);

	return lunh;
}
/*}}}*/
/*{{{  static void lib_libusenodehook_free (void *hook)*/
/*
 *	frees a libusenode hook
 */
static void lib_libusenodehook_free (void *hook)
{
	libusenodehook_t *lunh = (libusenodehook_t *)hook;
	int i;

	if (lunh) {
		for (i=0; i<DA_CUR (lunh->decls); i++) {
			tnode_t *dent = DA_NTHITEM (lunh->decls, i);

			if (dent) {
				tnode_free (dent);
			}
		}
		dynarray_trash (lunh->decls);

		if (lunh->libname) {
			sfree (lunh->libname);
		}
		if (lunh->namespace) {
			sfree (lunh->namespace);
		}
		if (lunh->asnamespace) {
			sfree (lunh->asnamespace);
		}
		if (lunh->libdata) {
			lib_freelibfile (lunh->libdata);
		}

		sfree (lunh);
	}
	return;
}
/*}}}*/
/*{{{  static void *lib_libusenodehook_copy (void *hook)*/
/*
 *	copies a libusenode hook
 */
static void *lib_libusenodehook_copy (void *hook)
{
	libusenodehook_t *lunh = (libusenodehook_t *)hook;

	if (lunh) {
		libusenodehook_t *newlunh = lib_newlibusenodehook (NULL, lunh->libname);

		if (lunh->namespace) {
			if (newlunh->namespace) {
				sfree (newlunh->namespace);
			}
			newlunh->namespace = string_dup (lunh->namespace);
		}

		if (lunh->asnamespace) {
			newlunh->asnamespace = string_dup (lunh->asnamespace);
		}

		return (void *)newlunh;
	}

	return NULL;
}
/*}}}*/
/*{{{  static void lib_libusenodehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for libusenode hook (debugging)
 */
static void lib_libusenodehook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	libusenodehook_t *lunh = (libusenodehook_t *)hook;
	int i;

	lib_isetindent (stream, indent);
	fprintf (stream, "<libusenodehook addr=\"0x%8.8x\" libname=\"%s\" namespace=\"%s\" as=\"%s\" >\n", (unsigned int)lunh, lunh->libname, lunh->namespace, lunh->asnamespace ? lunh->asnamespace : lunh->namespace);
	if (lunh->libdata) {
		libfile_t *lf = lunh->libdata;

		if (lf->fname) {
			lib_isetindent (stream, indent + 1);
			fprintf (stream, "<libdesc name=\"%s\" libname=\"%s\" namespace=\"%s\" nativelib=\"%s\" />\n",
					lf->fname ?: "", lf->libname ?: "", lf->namespace ?: "", lf->nativelib ?: "");
		}
		for (i=0; i<DA_CUR (lf->autoinclude); i++) {
			char *ipath = DA_NTHITEM (lf->autoinclude, i);

			lib_isetindent (stream, indent + 1);
			fprintf (stream, "<autoinclude path=\"%s\" />\n", ipath);
		}
		for (i=0; i<DA_CUR (lf->autouse); i++) {
			char *ipath = DA_NTHITEM (lf->autouse, i);

			lib_isetindent (stream, indent + 1);
			fprintf (stream, "<autouse path=\"%s\" />\n", ipath);
		}
		for (i=0; i<DA_CUR (lf->srcs); i++) {
			libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, i);
			int j;

			lib_isetindent (stream, indent + 1);
			fprintf (stream, "<srcunit name=\"%s\">\n", lfsu->fname);
			for (j=0; j<DA_CUR (lfsu->entries); j++) {
				libfile_entry_t *lfent = DA_NTHITEM (lfsu->entries, j);

				lib_isetindent (stream, indent + 2);
				fprintf (stream, "<entry name=\"%s\" language=\"%s\" target=\"%s\" descriptor=\"%s\" ws=\"%d\" vs=\"%d\" ms=\"%d\" adjust=\"%d\" />\n",
						lfent->name, lfent->langname ?: "", lfent->targetname ?: "", lfent->descriptor ?: "", lfent->ws, lfent->vs, lfent->ms, lfent->adjust);
			}
			lib_isetindent (stream, indent + 1);
			fprintf (stream, "</srcunit>\n");
		}
	}
	if (lunh->decltree) {
		lib_isetindent (stream, indent + 1);
		fprintf (stream, "<decltree>\n");
		tnode_dumptree (lunh->decltree, indent + 2, stream);
		lib_isetindent (stream, indent + 1);
		fprintf (stream, "</decltree>\n");
	}

	lib_isetindent (stream, indent + 1);
	fprintf (stream, "<parsedlibdecls>\n");
	for (i=0; i<DA_CUR (lunh->decls); i++) {
		tnode_t *dent = DA_NTHITEM (lunh->decls, i);

		tnode_dumptree (dent, indent + 2, stream);
	}
	lib_isetindent (stream, indent + 1);
	fprintf (stream, "</parsedlibdecls>\n");

	lib_isetindent (stream, indent);
	fprintf (stream, "</libusenodehook>\n");

	return;
}
/*}}}*/


/*{{{  static libfile_entry_t *lib_newlibfile_entry (void)*/
/*
 *	creates a new, blank, libfile_entry_t structure
 */
static libfile_entry_t *lib_newlibfile_entry (void)
{
	libfile_entry_t *lfe = (libfile_entry_t *)smalloc (sizeof (libfile_entry_t));

	lfe->name = NULL;
	lfe->langname = NULL;
	lfe->targetname = NULL;
	lfe->descriptor = NULL;
	lfe->ws = 0;
	lfe->vs = 0;
	lfe->ms = 0;
	lfe->adjust = 0;

	return lfe;
}
/*}}}*/
/*{{{  static void lib_freelibfile_entry (libfile_entry_t *lfe)*/
/*
 *	destroys a libfile_entry_t
 */
static void lib_freelibfile_entry (libfile_entry_t *lfe)
{
	if (lfe->name) {
		sfree (lfe->name);
	}
	if (lfe->langname) {
		sfree (lfe->langname);
	}
	if (lfe->targetname) {
		sfree (lfe->targetname);
	}
	if (lfe->descriptor) {
		sfree (lfe->descriptor);
	}

	sfree (lfe);
	return;
}
/*}}}*/
/*{{{  static libfile_srcunit_t *lib_newlibfile_srcunit (void)*/
/*
 *	creates a new, blank, libfile_srcunit_t structure
 */
static libfile_srcunit_t *lib_newlibfile_srcunit (void)
{
	libfile_srcunit_t *lfsu = (libfile_srcunit_t *)smalloc (sizeof (libfile_srcunit_t));

	lfsu->fname = NULL;
	dynarray_init (lfsu->entries);
	lfsu->hashalgo = NULL;
	lfsu->hashvalue = NULL;
	lfsu->issigned = 0;

	lfsu->curentry = NULL;

	return lfsu;
}
/*}}}*/
/*{{{  static void lib_freelibfile_srcunit (libfile_srcunit_t *lfsu)*/
/*
 *	destroys a libfile_srcunit_t structure (deep)
 */
static void lib_freelibfile_srcunit (libfile_srcunit_t *lfsu)
{
	int i;

	if (lfsu->fname) {
		sfree (lfsu->fname);
	}
	if (lfsu->hashalgo) {
		sfree (lfsu->hashalgo);
	}
	if (lfsu->hashvalue) {
		sfree (lfsu->hashvalue);
	}
	for (i=0; i<DA_CUR (lfsu->entries); i++) {
		libfile_entry_t *lfe = DA_NTHITEM (lfsu->entries, i);

		if (lfe) {
			lib_freelibfile_entry (lfe);
		}
	}
	dynarray_trash (lfsu->entries);

	sfree (lfsu);
	return;
}
/*}}}*/
/*{{{  static libfile_t *lib_newlibfile (void)*/
/*
 *	creates a new, blank, libfile_t structure
 */
static libfile_t *lib_newlibfile (void)
{
	libfile_t *lf = (libfile_t *)smalloc (sizeof (libfile_t));

	lf->fname = NULL;
	lf->libname = NULL;
	lf->namespace = NULL;
	dynarray_init (lf->srcs);
	dynarray_init (lf->autoinclude);
	dynarray_init (lf->autouse);

	lf->curunit = NULL;

	return lf;
}
/*}}}*/
/*{{{  static void lib_freelibfile (libfile_t *lf)*/
/*
 *	destroys a libfile_t structure (deep)
 */
static void lib_freelibfile (libfile_t *lf)
{
	int i;

	if (lf->fname) {
		sfree (lf->fname);
	}
	if (lf->libname) {
		sfree (lf->libname);
	}
	if (lf->namespace) {
		sfree (lf->namespace);
	}
	for (i=0; i<DA_CUR (lf->srcs); i++) {
		libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, i);

		if (lfsu) {
			lib_freelibfile_srcunit (lfsu);
		}
	}
	for (i=0; i<DA_CUR (lf->autoinclude); i++) {
		char *ifile = DA_NTHITEM (lf->autoinclude, i);

		if (ifile) {
			sfree (ifile);
		}
	}
	for (i=0; i<DA_CUR (lf->autouse); i++) {
		char *lfile = DA_NTHITEM (lf->autouse, i);

		if (lfile) {
			sfree (lfile);
		}
	}
	dynarray_trash (lf->srcs);
	dynarray_trash (lf->autoinclude);
	dynarray_trash (lf->autouse);

	sfree (lf);
	return;
}
/*}}}*/


/*{{{  static void lib_xmlhandler_init (xmlhandler_t *xh)*/
/*
 *	called when starting parsing an XML file
 */
static void lib_xmlhandler_init (xmlhandler_t *xh)
{
	libfile_t *lf = (libfile_t *)xh->uhook;

	if (!lf) {
		nocc_internal ("lib_xmlhandler_init(): no libfile_t attached to parser");
		return;
	}
	return;
}
/*}}}*/
/*{{{  static void lib_xmlhandler_final (xmlhandler_t *xh)*/
/*
 *	called when done parsing an XML file
 */
static void lib_xmlhandler_final (xmlhandler_t *xh)
{
	libfile_t *lf = (libfile_t *)xh->uhook;

	if (!lf) {
		nocc_internal ("lib_xmlhandler_final(): no libfile_t attached to parser");
		return;
	}
	return;
}
/*}}}*/
/*{{{  static void lib_xmlhandler_elem_start (xmlhandler_t *xh, void *data, xmlkey_t *key, xmlkey_t **attrkeys, const char **attrvals)*/
/*
 *	called when starting an element in the XML file
 */
static void lib_xmlhandler_elem_start (xmlhandler_t *xh, void *data, xmlkey_t *key, xmlkey_t **attrkeys, const char **attrvals)
{
	libfile_t *lf = (libfile_t *)xh->uhook;
	int i;

	if (!lf) {
		nocc_internal ("lib_xmlhandler_elem_start(): no libfile_t structure attached!");
		return;
	}
	switch (key->type) {
		/*{{{  XMLKEY_LIBRARY -- library info*/
	case XMLKEY_LIBRARY:
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_NAME:
				if (lf->libname && strcmp (lf->libname, attrvals[i])) {
					nocc_warning ("lib_xmlhandler_elem_start(): library names differ, changing [%s] to [%s]", lf->libname, attrvals[i]);
					sfree (lf->libname);
				}
				lf->libname = string_dup (attrvals[i]);
				break;
			case XMLKEY_NAMESPACE:
				if (lf->namespace && strcmp (lf->namespace, attrvals[i])) {
					nocc_warning ("lib_xmlhandler_elem_start(): library namespaces differ, changing [%s] to [%s]", lf->namespace, attrvals[i]);
					sfree (lf->namespace);
				}
				lf->namespace = string_dup (attrvals[i]);
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in library node", attrkeys[i]->name);
				return;
			}
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_NATIVELIB -- native library info*/
	case XMLKEY_NATIVELIB:
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_PATH:
				if (lf->nativelib) {
					nocc_warning ("lib_xmlhandler_elem_start(): already have native-library [%s] for library, not changing", lf->nativelib);
				} else {
					lf->nativelib = string_dup (attrvals[i]);
				}
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in nativelib node", attrkeys[i]->name);
				return;
			}
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_SRCINCLUDE -- source auto-include*/
	case XMLKEY_SRCINCLUDE:
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_PATH:
				{
					int j;

					for (j=0; j<DA_CUR (lf->autoinclude); j++) {
						if (!strcmp (DA_NTHITEM (lf->autoinclude, j), attrvals[i])) {
							break;		/* for() */
						}
					}
					if (j == DA_CUR (lf->autoinclude)) {
						/* add this one */
						dynarray_add (lf->autoinclude, string_dup (attrvals[i]));
					}
				}
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in srcinclude node", attrkeys[i]->name);
				return;
			}
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_SRCUSE -- source auto-use*/
	case XMLKEY_SRCUSE:
		break;
		/*}}}*/
		/*{{{  XMLKEY_LIBUNIT -- library unit (source file)*/
	case XMLKEY_LIBUNIT:
		if (lf->curunit) {
			nocc_error ("lib_xmlhandler_elem_start(): unexpected libunit node");
			return;
		}
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_NAME:
				{
					int j;

					for (j=0; j<DA_CUR (lf->srcs); j++) {
						libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, j);

						if (!strcmp (lfsu->fname, attrvals[i])) {
							break;		/* for() */
						}
					}
					if (j == DA_CUR (lf->srcs)) {
						/* create a fresh one */
						lf->curunit = lib_newlibfile_srcunit ();
						lf->curunit->fname = string_dup (attrvals[i]);
						dynarray_add (lf->srcs, lf->curunit);
					} else {
						lf->curunit = DA_NTHITEM (lf->srcs, j);
					}
				}
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in libunit node", attrkeys[i]->name);
				return;
			}
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_HASH, XMLKEY_SIGNEDHASH -- hashing/digest info for compiler output*/
	case XMLKEY_HASH:
	case XMLKEY_SIGNEDHASH:
		if (!lf->curunit) {
			nocc_error ("lib_xmlhandler_elem_start(): hash node outside of libunit");
			return;
		}
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_HASHALGO:
				if (lf->curunit->hashalgo) {
					nocc_error ("lib_xmlhandler_elem_start(): hashing algorithm already set");
					return;
				}
				lf->curunit->hashalgo = string_dup (attrvals[i]);
				break;
			case XMLKEY_VALUE:
				if (lf->curunit->hashvalue) {
					nocc_error ("lib_xmlhandler_elem_start(): hash value already set");
					return;
				}
				lf->curunit->hashvalue = string_dup (attrvals[i]);
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in hash node", attrkeys[i]->name);
				return;
			}
		}
		lf->curunit->issigned = (key->type == XMLKEY_SIGNEDHASH) ? 1 : 0;

		break;
		/*}}}*/
		/*{{{  XMLKEY_PROC -- process descriptor*/
	case XMLKEY_PROC:
		if (!lf->curunit) {
			nocc_error ("lib_xmlhandler_elem_start(): proc node outside of libunit");
			return;
		} else if (lf->curunit->curentry) {
			nocc_error ("lib_xmlhandler_elem_start(): unexpected proc node");
			return;
		} else {
			libfile_entry_t *lfe = lib_newlibfile_entry ();

			for (i=0; attrkeys[i]; i++) {
				switch (attrkeys[i]->type) {
				case XMLKEY_NAME:
					if (lfe->name) {
						nocc_error ("lib_xmlhandler_elem_start(): unexpected name in proc node");
						return;
					}
					lfe->name = string_dup (attrvals[i]);
					break;
				case XMLKEY_LANGUAGE:
					if (lfe->langname) {
						nocc_error ("lib_xmlhandler_elem_start(): unexpected langname in proc node");
						return;
					}
					lfe->langname = string_dup (attrvals[i]);
					break;
				case XMLKEY_TARGET:
					if (lfe->targetname) {
						nocc_error ("lib_xmlhandler_elem_start(): unexpected targetname in proc node");
						return;
					}
					lfe->targetname = string_dup (attrvals[i]);
					break;
				default:
					nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in proc node", attrkeys[i]->name);
					return;
				}
			}
			/* should have these 3 things at least.. */
			if (!lfe->name || !lfe->langname || !lfe->targetname) {
				nocc_error ("lib_xmlhandler_elem_start(): proc node missing some attributes");
				lib_freelibfile_entry (lfe);
				return;
			}
			for (i=0; i<DA_CUR (lf->curunit->entries); i++) {
				libfile_entry_t *lfent = DA_NTHITEM (lf->curunit->entries, i);

				if (!strcmp (lfent->name, lfe->name)) {
					nocc_error ("lib_xmlhandler_elem_start(): proc node for [%s] already seen", lfe->name);
					lib_freelibfile_entry (lfe);
					return;
				}
			}
			dynarray_add (lf->curunit->entries, lfe);
			lf->curunit->curentry = lfe;
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_DESCRIPTOR -- parseable process descriptor*/
	case XMLKEY_DESCRIPTOR:
		if (!lf->curunit || !lf->curunit->curentry) {
			nocc_error ("lib_xmlhandler_elem_start(): descriptor node outside of proc");
			return;
		}
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_VALUE:
				lf->curunit->curentry->descriptor = string_dup (attrvals[i]);
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in descriptor node", attrkeys[i]->name);
				return;
			}
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_BLOCKINFO -- space requirements*/
	case XMLKEY_BLOCKINFO:
		if (!lf->curunit || !lf->curunit->curentry) {
			nocc_error ("lib_xmlhandler_elem_start(): blockinfo node outside of proc");
			return;
		}
		for (i=0; attrkeys[i]; i++) {
			switch (attrkeys[i]->type) {
			case XMLKEY_ALLOCWS:
				if (sscanf (attrvals[i], "%d", &lf->curunit->curentry->ws) != 1) {
					nocc_internal ("lib_xmlhandler_elem_start(): bad ws attribute [%s] in descriptor node", attrvals[i]);
				}
				break;
			case XMLKEY_ALLOCVS:
				if (sscanf (attrvals[i], "%d", &lf->curunit->curentry->vs) != 1) {
					nocc_internal ("lib_xmlhandler_elem_start(): bad vs attribute [%s] in descriptor node", attrvals[i]);
				}
				break;
			case XMLKEY_ALLOCMS:
				if (sscanf (attrvals[i], "%d", &lf->curunit->curentry->ms) != 1) {
					nocc_internal ("lib_xmlhandler_elem_start(): bad ms attribute [%s] in descriptor node", attrvals[i]);
				}
				break;
			case XMLKEY_ADJUST:
				if (sscanf (attrvals[i], "%d", &lf->curunit->curentry->adjust) != 1) {
					nocc_internal ("lib_xmlhandler_elem_start(): bad adjust attribute [%s] in descriptor node", attrvals[i]);
				}
				break;
			default:
				nocc_internal ("lib_xmlhandler_elem_start(): unknown attribute [%s] in descriptor node", attrkeys[i]->name);
				return;
			}
		}
		break;
		/*}}}*/
		/*{{{  default -- ignore*/
	default:
		break;
		/*}}}*/
	}
	return;
}
/*}}}*/
/*{{{  static void lib_xmlhandler_elem_end (xmlhandler_t *xh, void *data, xmlkey_t *key)*/
/*
 *	called when finishing an element in the XML file
 */
static void lib_xmlhandler_elem_end (xmlhandler_t *xh, void *data, xmlkey_t *key)
{
	libfile_t *lf = (libfile_t *)xh->uhook;

	if (!lf) {
		nocc_internal ("lib_xmlhandler_elem_end(): no libfile_t structure attached!");
		return;
	}
	switch (key->type) {
		/*{{{  XMLKEY_LIBUNIT -- library unit (source file)*/
	case XMLKEY_LIBUNIT:
		if (!lf->curunit) {
			nocc_error ("lib_xmlhandler_elem_end(): closing libunit, but none current");
			return;
		} else {
			lf->curunit = NULL;
		}
		break;
		/*}}}*/
		/*{{{  XMLKEY_PROC -- process descriptor*/
	case XMLKEY_PROC:
		if (!lf->curunit || !lf->curunit->curentry) {
			nocc_error ("lib_xmlhandler_elem_end(): closing proc, but none current");
			return;
		} else {
			lf->curunit->curentry = NULL;
		}
		break;
		/*}}}*/
		/*{{{  default -- ignore*/
	default:
		break;
		/*}}}*/
	}
	return;
}
/*}}}*/
/*{{{  static void lib_xmlhandler_data (xmlhandler_t *xh, void *data, const char *text, int len)*/
/*
 *	called when a chunk of data is found in an XML file
 */
static void lib_xmlhandler_data (xmlhandler_t *xh, void *data, const char *text, int len)
{
	/* ignore.. */
	return;
}
/*}}}*/


/*{{{  static libfile_t *lib_newlibrary (char *libname)*/
/*
 *	creates a new library-file and returns it, returns NULL on failure.
 *	Blanks the library even if one exists
 */
static libfile_t *lib_newlibrary (char *libname)
{
	libfile_t *lf = NULL;
	char fbuf[FILENAME_MAX];
	int flen = 0;

	if (scpath) {
		/*{{{  using separate-compilation directory prefix*/
		flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s", scpath);
		if ((flen > 0) && (fbuf[flen - 1] != '/')) {
			fbuf[flen] = '/';
			flen++;
			fbuf[flen] = '\0';
		}
		/*}}}*/
	}

	flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s.xlo", libname);

	lf = lib_newlibfile ();
	lf->fname = string_dup (fbuf);

	if (!access (fbuf, R_OK)) {
		/* try and delete it */
		if (unlink (fbuf)) {
			nocc_error ("lib_newlibrary(): failed to remove existing .xlo file %s: %s", fbuf, strerror (errno));
		}
	}
	return lf;
}
/*}}}*/
/*{{{  static libfile_t *lib_readlibrary (char *libname, int using)*/
/*
 *	reads a library-file and returns it, returns NULL on failure.
 *	Will return a blank library if none exists, unless "using" is true (in which case compiler library paths are searched)
 */
static libfile_t *lib_readlibrary (char *libname, int using)
{
	libfile_t *lf = NULL;
	char fbuf[FILENAME_MAX];
	int flen = 0;
	int i;

	if (using) {
		/*{{{  must exist, or fail*/
		/* try current directory first */
		flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s.xlb", libname);
		if (access (fbuf, R_OK)) {
			flen = 0;

			/* try .xlo version also */
			flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s.xlo", libname);
			if (access (fbuf, R_OK)) {
				flen = 0;

				/* search */
				for (i=0; i<DA_CUR (compopts.lpath); i++) {
					char *lpath = DA_NTHITEM (compopts.lpath, i);
					int sflen;

					flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s", lpath);
					if (fbuf[flen-1] != '/') {
						fbuf[flen] = '/';
						flen++;
						fbuf[flen] = '\0';
					}
					sflen = flen;
					flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s.xlb", libname);
#if 0
fprintf (stderr, "lib_readlibrary(): trying [%s]\n", fbuf);
#endif
					if (!access (fbuf, R_OK)) {
						break;		/* for() */
					}
					fbuf[flen - 1] = 'o';		/* .xlb -> .xlo */
#if 0
fprintf (stderr, "lib_readlibrary(): trying [%s]\n", fbuf);
#endif
					if (!access (fbuf, R_OK)) {
						break;		/* for() */
					}
					flen = 0;
				}
				if (!flen) {
					/* none found */
					return NULL;
				}
			}
		}
		/*}}}*/
	} else {
		if (libpath) {
			/*{{{  using library-directory prefix*/
			flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s", libpath);
			if ((flen > 0) && (fbuf[flen - 1] != '/')) {
				fbuf[flen] = '/';
				flen++;
				fbuf[flen] = '\0';
			}
			/*}}}*/
		}
		flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s.xlb", libname);
	}

	lf = lib_newlibfile ();
	lf->fname = string_dup (fbuf);
	if (!access (fbuf, R_OK)) {
		/*{{{  read file*/
		xmlhandler_t *lfxh;

		lfxh = xml_new_handler ();
		lfxh->uhook = (void *)lf;
		lfxh->init = lib_xmlhandler_init;
		lfxh->final = lib_xmlhandler_final;
		lfxh->elem_start = lib_xmlhandler_elem_start;
		lfxh->elem_end = lib_xmlhandler_elem_end;
		lfxh->data = lib_xmlhandler_data;

		i = xml_parse_file (lfxh, fbuf);
		xml_del_handler (lfxh);

		if (i) {
			nocc_error ("lib_readlibrary(): failed to parse library in: %s", fbuf);
			lib_freelibfile (lf);
			lf = NULL;
		}
		/*}}}*/
	}	/* else no file, will create a fresh one */
	return lf;
}
/*}}}*/
/*{{{  static int lib_writelibrary (libfile_t *lf)*/
/*
 *	writes out a library-file, will trash any existing file
 *	returns 0 on success, non-zero on failure
 */
static int lib_writelibrary (libfile_t *lf)
{
	char fbuf[FILENAME_MAX];
	int flen = 0;
	FILE *libstream;
	int i;

	if (!strchr (lf->fname, '/') && libpath) {
		/*{{{  using library-directory prefix*/
		flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s", libpath);
		if ((flen > 0) && (fbuf[flen - 1] != '/')) {
			fbuf[flen] = '/';
			flen++;
			fbuf[flen] = '\0';
		}
		/*}}}*/
	}
	flen += snprintf (fbuf + flen, FILENAME_MAX - (flen + 2), "%s", lf->fname);

	libstream = fopen (fbuf, "w");
	if (!libstream) {
		nocc_error ("lib_writelibrary(): failed to open %s for writing: %s", fbuf, strerror (errno));
		return -1;
	}

	fprintf (libstream, "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n");
	fprintf (libstream, "<nocc:libinfo version=\"%s\">\n", VERSION);
	lib_isetindent (libstream, 1);
	fprintf (libstream, "<library name=\"%s\" namespace=\"%s\">\n", lf->libname, lf->namespace);

	if (lf->nativelib) {
		lib_isetindent (libstream, 2);
		fprintf (libstream, "<nativelib path=\"%s\" />\n", lf->nativelib);
	}
	for (i=0; i<DA_CUR (lf->autoinclude); i++) {
		char *ifile = DA_NTHITEM (lf->autoinclude, i);

		lib_isetindent (libstream, 2);
		fprintf (libstream, "<srcinclude path=\"%s\" />\n", ifile);
	}
	for (i=0; i<DA_CUR (lf->autouse); i++) {
		char *lfile = DA_NTHITEM (lf->autouse, i);

		lib_isetindent (libstream, 2);
		fprintf (libstream, "<srcuse path=\"%s\" />\n", lfile);
	}
	for (i=0; i<DA_CUR (lf->srcs); i++) {
		libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, i);
		int j;

		lib_isetindent (libstream, 2);
		fprintf (libstream, "<libunit name=\"%s\">\n", lfsu->fname);

		/* drop hashing information if set */
		if (lfsu->hashalgo && lfsu->hashvalue) {
			lib_isetindent (libstream, 3);
			fprintf (libstream, "<%s hashalgo=\"%s\" value=\"%s\" />\n", lfsu->issigned ? "signedhash" : "hash", lfsu->hashalgo, lfsu->hashvalue);
		}

		for (j=0; j<DA_CUR (lfsu->entries); j++) {
			libfile_entry_t *lfe = DA_NTHITEM (lfsu->entries, j);

			lib_isetindent (libstream, 3);
			fprintf (libstream, "<proc name=\"%s\" language=\"%s\" target=\"%s\">\n", lfe->name, lfe->langname, lfe->targetname);

			lib_isetindent (libstream, 4);
			fprintf (libstream, "<descriptor value=\"%s\" />\n", lfe->descriptor);
			lib_isetindent (libstream, 4);
			fprintf (libstream, "<blockinfo allocws=\"%d\" allocvs=\"%d\" allocms=\"%d\" adjust=\"%d\" />\n", lfe->ws, lfe->vs, lfe->ms, lfe->adjust);

			lib_isetindent (libstream, 3);
			fprintf (libstream, "</proc>\n");
		}
		lib_isetindent (libstream, 2);
		fprintf (libstream, "</libunit>\n");
	}

	lib_isetindent (libstream, 1);
	fprintf (libstream, "</library>\n");
	fprintf (libstream, "</nocc:libinfo>\n");

	fclose (libstream);
	return 0;
}
/*}}}*/
/*{{{  static int lib_mergeintolibrary (libfile_t *lf, libnodehook_t *lnh)*/
/*
 *	merges information from tree-nodes (libnodehook_t) into library-file structures (libfile_t)
 *	returns 0 on success, non-zero on failure
 */
static int lib_mergeintolibrary (libfile_t *lf, libnodehook_t *lnh)
{
	int i;
	libfile_srcunit_t *lfsu = NULL;;

	/*{{{  sort out library-file, initial checks*/
	if (!lf->fname) {
		int flen = 0;

		if (libpath) {
			flen = strlen (libpath);
		}
		if (lnh->libname) {
			flen += strlen (lnh->libname);
		}
		flen += 3;

		lf->fname = (char *)smalloc (flen);
		flen = 0;
		if (libpath) {
			flen = sprintf (lf->fname, "%s", libpath);
		}
		if ((flen > 0) && (lf->fname[flen - 1] != '/')) {
			lf->fname[flen] = '/';
			flen++;
			lf->fname[flen] = '\0';
		}
		if (lnh->libname) {
			flen += sprintf (lf->fname + flen, "%s", lnh->libname);
		}

		nocc_warning ("lib_mergeintolibrary(): library has no file-name, using [%s]", lf->fname);
	}
	if (!lnh->libname) {
		nocc_error ("lib_mergeintolibrary(): library hook node has no name");
		return -1;
	}
	if (!lnh->namespace) {
		nocc_error ("lib_mergeintolibrary(): library hook node has no namespace");
		return -1;
	}
	if (!lnh->langname) {
		nocc_error ("lib_mergeintolibrary(): library hook node has no language-name");
		return -1;
	}
	if (!lnh->targetname) {
		nocc_error ("lib_mergeintolibrary(): library hook node has no target-name");
		return -1;
	}

	if (lf->libname && strcmp (lf->libname, lnh->libname)) {
		nocc_warning ("lib_mergeintolibrary(): library name conflict, keeping library-specified [%s]", lf->libname);
	} else if (!lf->libname) {
		lf->libname = string_dup (lnh->libname);
	}
	if (lf->namespace && strcmp (lf->namespace, lnh->namespace)) {
		nocc_warning ("lib_mergeintolibrary(): library namespace conflict, keeping library-specified [%s]", lf->namespace);
	} else if (!lf->namespace) {
		lf->namespace = string_dup (lnh->namespace);
	}

	/*}}}*/
	/*{{{  merge in auto-includes*/
	for (i=0; i<DA_CUR (lnh->autoinclude); i++) {
		char *ifile = DA_NTHITEM (lnh->autoinclude, i);
		int j;

		for (j=0; j<DA_CUR (lf->autoinclude); j++) {
			char *libifile = DA_NTHITEM (lf->autoinclude, j);

			if (!strcmp (libifile, ifile)) {
				break;		/* for() */
			}
		}
		if (j == DA_CUR (lf->autoinclude)) {
			/* add it */
			dynarray_add (lf->autoinclude, string_dup (ifile));
		}
	}


	/*}}}*/
	/*{{{  merge in auto-uses*/
	for (i=0; i<DA_CUR (lnh->autouse); i++) {
		char *lfile = DA_NTHITEM (lnh->autouse, i);
		int j;

		for (j=0; j<DA_CUR (lf->autouse); j++) {
			char *liblfile = DA_NTHITEM (lf->autouse, j);

			if (!strcmp (liblfile, lfile)) {
				break;		/* for() */
			}
		}
		if (j == DA_CUR (lf->autouse)) {
			/* add it */
			dynarray_add (lf->autouse, string_dup (lfile));
		}
	}


	/*}}}*/
	/*{{{  merge in native-library name*/
	if (lnh->nativelib) {
		if (lf->nativelib && strcmp (lf->nativelib, lnh->nativelib)) {
			nocc_warning ("lib_mergeintolibrary(): library nativelib conflict, keeping library-specified [%s]", lf->nativelib);
		} else if (!lf->nativelib) {
			lf->nativelib = string_dup (lnh->nativelib);
		}
	}

	/*}}}*/
	/*{{{  search for source unit in library*/
	for (i=0; i<DA_CUR (lf->srcs); i++) {
		lfsu = DA_NTHITEM (lf->srcs, i);
		if (!strcmp (lfsu->fname, lnh->lf->fnptr)) {
			break;		/* for() */
		}
	}
	if (i == DA_CUR (lf->srcs)) {
		/* create a new one */
		lfsu = lib_newlibfile_srcunit ();

		lfsu->fname = string_dup (lnh->lf->fnptr);
		dynarray_add (lf->srcs, lfsu);
	} else {
		/* clear out existing entries */
		for (i=0; i<DA_CUR (lfsu->entries); i++) {
			lib_freelibfile_entry (DA_NTHITEM (lfsu->entries, i));
		}
		dynarray_trash (lfsu->entries);
		dynarray_init (lfsu->entries);
		if (lfsu->hashalgo) {
			sfree (lfsu->hashalgo);
			lfsu->hashalgo = NULL;
		}
		if (lfsu->hashvalue) {
			sfree (lfsu->hashvalue);
			lfsu->hashvalue = NULL;
		}
		lfsu->issigned = 0;
	}
	if (lnh->hashalgo) {
		lfsu->hashalgo = string_dup (lnh->hashalgo);
	}
	if (lnh->hashvalue) {
		lfsu->hashvalue = string_dup (lnh->hashvalue);
	}
	lfsu->issigned = lnh->issigned;

	/*}}}*/
	/*{{{  merge in entries*/
	for (i=0; i<DA_CUR (lnh->entries); i++) {
		libtaghook_t *lth = DA_NTHITEM (lnh->entries, i);
		libfile_entry_t *lfe = lib_newlibfile_entry ();

		lfe->name = string_dup (lth->name);
		lfe->langname = string_dup (lnh->langname);
		lfe->targetname = string_dup (lnh->targetname);
		lfe->descriptor = lth->descriptor ? string_dup (lth->descriptor) : NULL;
		lfe->ws = lth->ws;
		lfe->vs = lth->vs;
		lfe->ms = lth->ms;
		lfe->adjust = lth->adjust;

		dynarray_add (lfsu->entries, lfe);
	}


	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int lib_parsedescriptors (lexfile_t *orglf, libusenodehook_t *lunh)*/
/*
 *	processes a library usage node, parsing the descriptors
 *	FIXME: only parses descriptors for the originating language at the moment
 *	returns 0 on success, non-zero on failure
 */
static int lib_parsedescriptors (lexfile_t *orglf, libusenodehook_t *lunh)
{
	char *dbuf = NULL;
	int dbuflen = 0;
	int i;
	libfile_t *lf = lunh->libdata;

	/*{{{  size up descriptors*/
	for (i=0; i<DA_CUR (lf->srcs); i++) {
		libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, i);
		int j;

		for (j=0; j<DA_CUR (lfsu->entries); j++) {
			libfile_entry_t *lfent = DA_NTHITEM (lfsu->entries, j);

			if (lfent && lfent->descriptor && lfent->langname && !strcmp (lfent->langname, orglf->parser->langname)) {
				dbuflen += strlen (lfent->descriptor) + 4;
			}
		}
	}
	/*}}}*/
	if (dbuflen) {
		lexfile_t *lexbuf;
		tnode_t *decltree = NULL;
		tnode_t *thisnode;

		dbuf = (char *)smalloc (dbuflen);

		dbuflen = 0;
		/*{{{  build buffer*/
		for (i=0; i<DA_CUR (lf->srcs); i++) {
			libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, i);
			int j;

			for (j=0; j<DA_CUR (lfsu->entries); j++) {
				libfile_entry_t *lfent = DA_NTHITEM (lfsu->entries, j);

				if (lfent && lfent->descriptor && lfent->langname && !strcmp (lfent->langname, orglf->parser->langname)) {
					strcpy (dbuf + dbuflen, lfent->descriptor);
					dbuflen += strlen (lfent->descriptor);
					strcpy (dbuf + dbuflen, "\n");
					dbuflen++;
				}
			}
		}
		/*}}}*/
		/*{{{  open buffer as a lexfile_t and parse it*/
		lexbuf = lexer_openbuf (NULL, orglf->parser->langname, dbuf);
		if (!lexbuf) {
			nocc_error ("lib_parsedescriptors(): failed to open buffer..");
			sfree (dbuf);
			return -1;
		}

		decltree = parser_descparse (lexbuf);
		lexer_close (lexbuf);

		if (!decltree) {
			sfree (dbuf);
			return -1;
		}

		/*}}}*/
		/*{{{  attach declaration tree to the hook (flatten later on)*/
		lunh->decltree = decltree;

		/*}}}*/
		/*{{{  walk both trees and attach libfile size data to declarations*/
		thisnode = lunh->decltree;

		for (i=0; thisnode && (i<DA_CUR (lf->srcs)); i++) {
			libfile_srcunit_t *lfsu = DA_NTHITEM (lf->srcs, i);
			int j;

			for (j=0; thisnode && (j<DA_CUR (lfsu->entries)); j++) {
				libfile_entry_t *lfent = DA_NTHITEM (lfsu->entries, j);

				if (lfent && lfent->descriptor && lfent->langname && !strcmp (lfent->langname, orglf->parser->langname)) {
					int tnflags = tnode_tnflagsof (thisnode);

					tnode_setchook (thisnode, uselinkchook, (void *)lfent);
					if (tnflags & TNF_LONGDECL) {
						thisnode = tnode_nthsubof (thisnode, 3);
					} else if (tnflags & TNF_SHORTDECL) {
						thisnode = tnode_nthsubof (thisnode, 2);
					} else if (tnflags & TNF_TRANSPARENT) {
						thisnode = tnode_nthsubof (thisnode, 0);
					} else {
						nocc_warning ("lib_parsedescriptors(): unhandled node in library declaration-tree [%s]", thisnode->tag->name);
						thisnode = NULL;
					}
				}
			}
		}
		/*}}}*/

		sfree (dbuf);
	} else {
		nocc_warning ("lib_parsedescriptors(): no usable descriptors in library..");
	}
	return 0;
}
/*}}}*/


/*{{{  static int lib_scopein_libnode (tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-in a library definition node (sets defining namespace)
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_scopein_libnode (tnode_t **nodep, scope_t *ss)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (*nodep, 0);
	namespace_t *ns;

	if (!lnh->namespace) {
		return 1;
	}
	ns = name_findnamespace (lnh->namespace);

	if (!ns) {
		ns = name_newnamespace (lnh->namespace);
	}

	scope_pushdefns (ss, ns);
	return 1;
}
/*}}}*/
/*{{{  static int lib_scopeout_libnode (tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-out a library definition node (clears defining namespace)
 *	returns 0 to stop walk, 1 to continue (defunct)
 */
static int lib_scopeout_libnode (tnode_t **nodep, scope_t *ss)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (*nodep, 0);
	namespace_t *ns;

	if (!lnh->namespace) {
		return 1;
	}
	ns = name_findnamespace (lnh->namespace);

	if (!ns) {
		nocc_internal ("lib_scopeout_libnode(): did not find my namyspace! [%s]", lnh->namespace);
	}

	scope_popdefns (ss, ns);
	return 1;
}
/*}}}*/
/*{{{  static int lib_betrans_libnode (tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transform on a library node
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_betrans_libnode (tnode_t **nodep, betrans_t *be)
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
	slen += be->target->tarch ? strlen (be->target->tarch) : 7;
	slen += be->target->tvendor ? strlen (be->target->tvendor) : 7;
	slen += be->target->tos ? strlen (be->target->tos) : 7;

	lnh->targetname = (char *)smalloc (slen + 4);
	sprintf (lnh->targetname, "%s-%s-%s", be->target->tarch ?: "unknown", be->target->tvendor ?: "unknown", be->target->tos ?: "unknown");

	dynarray_add (entrystack, lnh);
	betrans_subtree (tnode_nthsubaddr (*nodep, 0), be);
	dynarray_delitem (entrystack, DA_CUR (entrystack) - 1);

	return 0;
}
/*}}}*/
/*{{{  static void lib_codegen_libnode_pcall (codegen_t *cgen, void *arg)*/
/*
 *	this is called after code-generation has finished to write out a library -- will
 *	have access to the code-gen digest if generated.
 */
static void lib_codegen_libnode_pcall (codegen_t *cgen, void *arg)
{
	libnodehook_t *lnh = (libnodehook_t *)arg;
	libfile_t *lf;

	if (lnh->issepcomp) {
		/* re-create the file each time with this */
		lf = lib_newlibrary (lnh->libname);
	} else {
		/* try and open or create library */
		lf = lib_readlibrary (lnh->libname, 0);
	}
	if (!lf) {
		/* failed for some reason */
		codegen_warning (cgen, "failed to open/create library \"%s\"", lnh->libname);
		return;
	}

	if (cgen->digest) {
		int issigned = 0;
		char *tmp = crypto_readdigest (cgen->digest, &issigned);

		if (tmp) {
			if (lnh->hashalgo) {
				sfree (lnh->hashalgo);
			}
			lnh->hashalgo = string_dup (compopts.hashalgo);

			if (lnh->hashvalue) {
				sfree (lnh->hashvalue);
			}
			lnh->hashvalue = tmp;

			lnh->issigned = issigned;
		}
	}

	/* merge in information from this library node */
	if (lib_mergeintolibrary (lf, lnh)) {
		codegen_warning (cgen, "failed to merge library data for \"%s\"", lnh->libname);
		lib_freelibfile (lf);
		return;
	}

	/* write modified/new library info out and free */
	if (lib_writelibrary (lf)) {
		codegen_warning (cgen, "failed to write library \"%s\"", lnh->libname);
		lib_freelibfile (lf);
		return;
	}

	lib_freelibfile (lf);

	return;
}
/*}}}*/
/*{{{  static int lib_codegen_libnode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a library node (produces the library)
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_codegen_libnode (tnode_t *node, codegen_t *cgen)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (node, 0);

	/* mark for later generation -- so we can get the digest for the generated code */
	codegen_setpostcall (cgen, lib_codegen_libnode_pcall, (void *)lnh);

	return 1;
}
/*}}}*/

/*{{{  static int lib_betrans_libtag (tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transform on a library tag node
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_betrans_libtag (tnode_t **nodep, betrans_t *be)
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

	/* maybe set descriptor */
	if (tnode_getchook (tnode_nthsubof (*nodep, 0), descriptorchook)) {
		char *descr = (char *)tnode_getchook (tnode_nthsubof (*nodep, 0), descriptorchook);

		if ((*lthp)->descriptor) {
			nocc_warning ("lib_betrans_libtag(): tag already has descriptor, replacing");
			sfree ((*lthp)->descriptor);
		}
		(*lthp)->descriptor = string_dup (descr);
	}

	return 1;
}
/*}}}*/
/*{{{  static int lib_namemap_libtag (tnode_t **nodep, map_t *mdata)*/
/*
 *	does name-mapping on a library-tag
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_namemap_libtag (tnode_t **nodep, map_t *mdata)
{
	libtaghook_t *lthp = (libtaghook_t *)tnode_nthhookof (*nodep, 0);
	tnode_t *bnode;

	map_submapnames (tnode_nthsubaddr (*nodep, 0), mdata);
	/* find the back-end BLOCK */
	bnode = treeops_findintree (tnode_nthsubof (*nodep, 0), mdata->target->tag_BLOCK);
	if (!bnode) {
		nocc_warning ("lib_namemap_libtag(): could not find BLOCK inside PUBLIC node!");
		return 0;
	}

	lthp->bnode = bnode;

	return 0;
}
/*}}}*/
/*{{{  static int lib_precode_libtag (tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-code generation on a library-tag
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_precode_libtag (tnode_t **nodep, codegen_t *cgen)
{
	libtaghook_t *lthp = (libtaghook_t *)tnode_nthhookof (*nodep, 0);

	cgen->target->be_getblocksize (lthp->bnode, &(lthp->ws), NULL, &(lthp->vs), &(lthp->ms), &(lthp->adjust), NULL);
	return 1;
}
/*}}}*/

/*{{{  static int lib_prescope_libusenode (tnode_t **nodep, prescope_t *ps)*/
/*
 *	does pre-scoping on library usage nodes
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_prescope_libusenode (tnode_t **nodep, prescope_t *ps)
{
	libusenodehook_t *lunh = (libusenodehook_t *)tnode_nthhookof (*nodep, 0);

	/* still got a declaration tree in the hook here */
	prescope_subtree (&lunh->decltree, ps);

	return 1;
}
/*}}}*/
/*{{{  static int lib_scopein_libusenode (tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-in library usage nodes (puts declarations into scope)
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_scopein_libusenode (tnode_t **nodep, scope_t *ss)
{
	libusenodehook_t *lunh = (libusenodehook_t *)tnode_nthhookof (*nodep, 0);
	tnode_t *decltree = lunh->decltree;
	tnode_t **walkp;
	tnode_t *tempnode = NULL;
	namespace_t *ns = NULL;

	/*{{{  walk declaration tree to find innermost*/
	for (walkp = &lunh->decltree; *walkp; ) {
		int tnflags = tnode_tnflagsof (*walkp);

		if (tnflags & TNF_LONGDECL) {
			walkp = tnode_nthsubaddr (*walkp, 3);
		} else if (tnflags & TNF_SHORTDECL) {
			walkp = tnode_nthsubaddr (*walkp, 2);
		} else if (tnflags & TNF_TRANSPARENT) {
			walkp = tnode_nthsubaddr (*walkp, 0);
		} else {
			scope_error (*nodep, ss, "lib_parsedescriptors(): unexpected node [%s]", decltree->tag->name);
			return 0;
		}
	}

	/*}}}*/

	/* temporarily attach body of USE to innermost of declarations for scoping */
	tempnode = tnode_nthsubof (*nodep, 0);

	/* scope them */
	if (lunh->namespace && lunh->asnamespace && strlen (lunh->asnamespace)) {
		/*{{{  use namespace AS something else*/
		ns = name_findnamespace (lunh->asnamespace);
		if (ns) {
			scope_error (*nodep, ss, "namespace [%s] already in use", lunh->asnamespace);
			return 0;
		}

		ns = name_newnamespace (lunh->asnamespace);

		if (strcmp (lunh->namespace, lunh->asnamespace)) {
			/* real namespace is different from the one used */
			namespace_t *realns = name_findnamespace (lunh->namespace);

			if (realns) {
				scope_warning (*nodep, ss, "namespace [%s] already present", lunh->namespace);
			} else {
				realns = name_newnamespace (lunh->namespace);
			}
			ns->nextns = realns;

			name_hidenamespace (realns);
		}

		scope_pushdefns (ss, ns);
		/* TEMPLIBUSENODE pops this and re-pushes onto the usage-stack */

		tempnode = tnode_create (tag_templibusenode, NULL, tempnode, (void *)lunh);

		/*}}}*/
	} else if (lunh->namespace && strlen (lunh->namespace)) {
		/*{{{  using namespace plainly*/

		ns = name_findnamespace (lunh->namespace);
		if (!ns) {
			ns = name_newnamespace (lunh->namespace);
		}

#if 0
fprintf (stderr, "lib_scopein_libusenode(): pushing defining namespace [%s]\n", ns->nspace);
#endif
		scope_pushdefns (ss, ns);
		/* TEMPLIBUSENODE pops this and re-pushes onto the usage-stack */

		tempnode = tnode_create (tag_templibusenode, NULL, tempnode, (void *)lunh);
		/*}}}*/
	}

	*walkp = tempnode;
	tnode_setnthsub (*nodep, 0, NULL);

	tnode_modprepostwalktree (&lunh->decltree, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

	if (ns) {
		scope_popusens (ss, ns);
		ns = NULL;
	}

	/* put body back */
	if (lunh->namespace && strlen (lunh->namespace)) {
		tnode_setnthsub (*nodep, 0, tnode_nthsubof (tempnode, 0));
		tnode_setnthsub (tempnode, 0, NULL);
		tnode_free (tempnode);
	} else {
		tnode_setnthsub (*nodep, 0, tempnode);
	}
	*walkp = NULL;

	decltree = lunh->decltree;

	/*{{{  break up declarations and put in hook*/
	while (decltree) {
		tnode_t **nextp;
		int tnflags = tnode_tnflagsof (decltree);

		if (tnflags & TNF_LONGDECL) {
			nextp = tnode_nthsubaddr (decltree, 3);
		} else if (tnflags & TNF_SHORTDECL) {
			nextp = tnode_nthsubaddr (decltree, 2);
		} else if (tnflags & TNF_TRANSPARENT) {
			nextp = tnode_nthsubaddr (decltree, 0);
		} else {
			scope_error (*nodep, ss, "lib_parsedescriptors(): unexpected node [%s]", decltree->tag->name);
			break;		/* while() */
		}

		dynarray_add (lunh->decls, decltree);
		decltree = *nextp;
		*nextp = NULL;
	}
	lunh->decltree = NULL;

	/*}}}*/

	return 0;		/* already done */
}
/*}}}*/
/*{{{  static int lib_betrans_libusenode (tnode_t **nodep, betrans_t *be)*/
/*
 *	does back-end transforms for a library-usage node
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_betrans_libusenode (tnode_t **nodep, betrans_t *be)
{
	return 1;
}
/*}}}*/
/*{{{  static int lib_namemap_libusenode (tnode_t **nodep, map_t *mdata)*/
/*
 *	does name-mapping for library-usage nodes, generates back-end BLOCKs
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_namemap_libusenode (tnode_t **nodep, map_t *mdata)
{
	libusenodehook_t *lunh = (libusenodehook_t *)tnode_nthhookof (*nodep, 0);
	int i;

	for (i=0; i<DA_CUR (lunh->decls); i++) {
		tnode_t *decl = DA_NTHITEM (lunh->decls, i);
		tnode_t **bodyp;
		libfile_entry_t *lfent = decl ? (libfile_entry_t *)tnode_getchook (decl, uselinkchook) : NULL;
		int tnflags;

		if (!decl || !lfent) {
			tnode_error (*nodep, "lib_namemap_libusenode(): missing declaration or hook");
			continue;
		}
		map_submapnames (DA_NTHITEMADDR (lunh->decls, i), mdata);
		decl = DA_NTHITEM (lunh->decls, i);

		/* if anything was mapped out, bodies should be back-end blocks */
		tnflags = tnode_tnflagsof (decl);
		if (tnflags & TNF_LONGDECL) {
			bodyp = tnode_nthsubaddr (decl, 2);
		} else {
			tnode_error (*nodep, "lib_namemap_libusenode(): unhandled node [%s]", decl->tag->name);
			continue;		/* for() */
		}

		mdata->target->be_setblocksize (*bodyp, lfent->ws, 0, lfent->vs, lfent->ms, lfent->adjust);
#if 0
fprintf (stderr, "lib_namemap_libusenode(): mapped library declaration, now got:\n");
tnode_dumptree (decl, 1, stderr);
#endif
	}
	return 1;
}
/*}}}*/
/*{{{  static int lib_codegen_libusenode (tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a library-usage node
 *	returns 0 to stop walk, 1 to continue
 */
static int lib_codegen_libusenode (tnode_t *node, codegen_t *cgen)
{
	return 1;
}
/*}}}*/

/*{{{  static int lib_scopein_templibusenode (tnode_t **nodep, scope_t *ss)*/
/*
 *	scopes-in a TEMPLIBUSENODE -- this checks to see if the LIBUSENODE
 *	was defining a namespace;  if so, removes it from the scoper
 */
static int lib_scopein_templibusenode (tnode_t **nodep, scope_t *ss)
{
	libusenodehook_t *lunh = (libusenodehook_t *)tnode_nthhookof (*nodep, 0);

	if (lunh && lunh->namespace && lunh->asnamespace && strlen (lunh->asnamespace)) {
		/*{{{  using library AS something else*/
		namespace_t *ns = name_findnamespace (lunh->asnamespace);

		if (!ns) {
			nocc_error ("lib_scopein_templibusenode(): did not find namespace [%s]!", lunh->asnamespace);
			return 1;
		} else {
			scope_popdefns (ss, ns);
			/* and into in-use namespaces! */
			scope_pushusens (ss, ns);
		}
		/* clear hook -- prevents trying this again, though it shouldn't */
		tnode_setnthhook (*nodep, 0, NULL);
		/*}}}*/
	} else if (lunh && lunh->namespace && strlen (lunh->namespace)) {
		/*{{{  using library in its own namespace*/
		namespace_t *ns = name_findnamespace (lunh->namespace);

		if (!ns) {
			nocc_error ("lib_scopein_templibusenode(): did not find namespace [%s]!", lunh->namespace);
			return 1;
		} else {
			scope_popdefns (ss, ns);
			/* and into in-use namespaces! */
			scope_pushusens (ss, ns);
		}
		/* clear hook -- prevents trying this again, though it shouldn't */
		tnode_setnthhook (*nodep, 0, NULL);
		/*}}}*/
	}
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
	cops->scopein = lib_scopein_libnode;
	cops->scopeout = lib_scopeout_libnode;
	cops->betrans = lib_betrans_libnode;
	cops->codegen = lib_codegen_libnode;
	tnd_libnode->ops = cops;

	i = -1;
	tag_libnode = tnode_newnodetag ("LIBNODE", &i, tnd_libnode, NTF_NONE);
	/*}}}*/
	/*{{{  nocc:libusenode -- LIBUSENODE*/
	i = -1;
	tnd_libusenode = tnode_newnodetype ("nocc:libusenode", &i, 1, 0, 1, TNF_TRANSPARENT);
	tnd_libusenode->hook_free = lib_libusenodehook_free;
	tnd_libusenode->hook_copy = lib_libusenodehook_copy;
	tnd_libusenode->hook_dumptree = lib_libusenodehook_dumptree;
	cops = tnode_newcompops ();
	cops->prescope = lib_prescope_libusenode;
	cops->scopein = lib_scopein_libusenode;
	cops->betrans = lib_betrans_libusenode;
	cops->namemap = lib_namemap_libusenode;
	cops->codegen = lib_codegen_libusenode;
	tnd_libusenode->ops = cops;

	i = -1;
	tag_libusenode = tnode_newnodetag ("LIBUSENODE", &i, tnd_libusenode, NTF_NONE);
	/*}}}*/
	/*{{{  nocc:templibusenode -- TEMPLIBUSENODE*/
	i = -1;
	tnd_templibusenode = tnode_newnodetype ("nocc:templibusenode", &i, 1, 0, 1, TNF_NONE);
	cops = tnode_newcompops ();
	cops->scopein = lib_scopein_templibusenode;
	tnd_templibusenode->ops = cops;

	i = -1;
	tag_templibusenode = tnode_newnodetag ("TEMPLIBUSENODE", &i, tnd_templibusenode, NTF_NONE);
	/*}}}*/
	/*{{{  nocc:libtag -- PUBLICTAG*/
	i = -1;
	tnd_libtag = tnode_newnodetype ("nocc:libtag", &i, 1, 0, 1, TNF_TRANSPARENT);
	tnd_libtag->hook_free = lib_libtaghook_free;
	tnd_libtag->hook_copy = lib_libtaghook_copy;
	tnd_libtag->hook_dumptree = lib_libtaghook_dumptree;
	cops = tnode_newcompops ();
	cops->betrans = lib_betrans_libtag;
	cops->namemap = lib_namemap_libtag;
	cops->precode = lib_precode_libtag;
	tnd_libtag->ops = cops;

	i = -1;
	tag_publictag = tnode_newnodetag ("PUBLICTAG", &i, tnd_libtag, NTF_NONE);
	/*}}}*/

	/*{{{  command-line options: "--liboutpath <path>", "--liballpublic", "--scoutpath <path>"*/
	opts_add ("liboutpath", '\0', lib_opthandler, (void *)1, "0output directory for library info");
	opts_add ("liballpublic", '\0', lib_opthandler, (void *)2, "1all top-level entries public in library");
	opts_add ("scoutpath", '\0', lib_opthandler, (void *)3, "0output directory for .xlo files");

	/*}}}*/
	/*{{{  others*/
	dynarray_init (entrystack);

	libchook = tnode_lookupornewchook ("lib:mark");
	uselinkchook = tnode_lookupornewchook ("lib:uselink");
	descriptorchook = tnode_lookupornewchook ("fetrans:descriptor");

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
	libnodehook_t *lnh;

	if (!libname) {
		/* probably for separate compilation, choose source filename */
		if (!lf->filename) {
			nocc_warning ("library_newlibnode(): no default filename for separate compilation!");
			libname = string_dup ("unknown");
		} else {
			char *ch;

			if (scpath) {
				libname = string_dup (lf->fnptr);
			} else {
				libname = string_dup (lf->filename);
			}
			for (ch = libname + (strlen (libname) - 1); (ch > libname) && (*ch != '.'); ch--);
			if (*ch == '.') {
				/* chop off existing extension */
				*ch = '\0';
			}
		}

		lnh = lib_newlibnodehook (lf, libname, NULL);
		lnh->issepcomp = 1;
		sfree (libname);
	} else {
		lnh = lib_newlibnodehook (lf, libname, libname);
	}

	lnode = tnode_create (tag_libnode, lf, NULL, lnh);

	/* defining this file as part of a library */
	lf->islibrary = 1;

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
/*{{{  int library_setnativelib (tnode_t *libnode, char *lname)*/
/*
 *	sets the "nativelib" name for a library (i.e. what will eventually be linked in)
 *	return 0 on success, non-zero on failure
 */
int library_setnativelib (tnode_t *libnode, char *lname)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (libnode, 0);

	if (lnh->nativelib) {
		tnode_warning (libnode, "library_setnativelib(): removing previous native library name [%s]", lnh->nativelib);
		sfree (lnh->nativelib);
	}
	lnh->nativelib = string_dup (lname);

	return 0;
}
/*}}}*/
/*{{{  int library_setnamespace (tnode_t *libnode, char *nsname)*/
/*
 *	sets the namespace for a library (used when defining a library)
 *	returns 0 on success, non-zero on failure
 */
int library_setnamespace (tnode_t *libnode, char *nsname)
{
	libnodehook_t *lnh = (libnodehook_t *)tnode_nthhookof (libnode, 0);

	if (lnh->namespace) {
		sfree (lnh->namespace);
	}
	lnh->namespace = string_dup (nsname ? nsname : "");

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
	if (node) {
		tnode_t *pnode = library_newlibpublictag (NULL, NULL);

		tnode_setchook (node, libchook, (void *)pnode);
	}
	return 0;
}
/*}}}*/
/*{{{  int library_makepublic (tnode_t **nodep, char *name)*/
/*
 *	takes a library marker ("lib:mark") out of a node
 *	returns 0 if nothing changed, non-zero otherwise
 */
int library_makepublic (tnode_t **nodep, char *name)
{
	if (tnode_getchook (*nodep, libchook)) {
		tnode_t *xnode = (tnode_t *)tnode_getchook (*nodep, libchook);
		libtaghook_t *lth = (libtaghook_t *)tnode_nthhookof (xnode, 0);

		if (lth->name) {
			sfree (lth->name);
		}
		lth->name = string_dup (name);

		tnode_setnthsub (xnode, 0, *nodep);
		tnode_setchook (*nodep, libchook, NULL);
		*nodep = xnode;

		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  tnode_t *library_newusenode (lexfile_t *lf, char *libname)*/
/*
 *	creates a new library-usage node
 *	returns usage-node on success, NULL on failure
 */
tnode_t *library_newusenode (lexfile_t *lf, char *libname)
{
	tnode_t *unode;
	libusenodehook_t *lunh = lib_newlibusenodehook (lf, libname);

	/* try and read the library data here */
	lunh->libdata = lib_readlibrary (libname, 1);
	if (!lunh->libdata) {
		nocc_error ("failed to read library [%s]", libname);
		lib_libusenodehook_free (lunh);
		return NULL;
	}

	if (lunh->libdata->namespace) {
		if (lunh->namespace) {
			sfree (lunh->namespace);
		}
		lunh->namespace = string_dup (lunh->libdata->namespace);
	}

	/* parse descriptors */
	if (lib_parsedescriptors (lf, lunh)) {
		nocc_error ("failed to parse descriptors in library [%s]", libname);
		lib_libusenodehook_free (lunh);
		return NULL;
	}

	unode = tnode_create (tag_libusenode, lf, NULL, lunh);

	return unode;
}
/*}}}*/
/*{{{  int library_setusenamespace (tnode_t *libusenode, char *nsname)*/
/*
 *	sets the usage namespace on a library usage node
 *	returns 0 on success, non-zero on failure
 */
int library_setusenamespace (tnode_t *libusenode, char *nsname)
{
	libusenodehook_t *lunh = (libusenodehook_t *)tnode_nthhookof (libusenode, 0);

	if (lunh->asnamespace && strcmp (lunh->asnamespace, nsname)) {
		nocc_error ("already using library [%s] as [%s], wanted [%s]", lunh->namespace, lunh->asnamespace, nsname);
		return -1;
	} else if (!lunh->asnamespace) {
		lunh->asnamespace = string_dup (nsname);
	}

	return 0;
}
/*}}}*/



