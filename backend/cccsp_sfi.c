/*
 *	cccsp_sfi.c -- stack-frame-info for cccsp back-end
 *	Copyright (C) 2013-2016 Fred Barnes <frmb@kent.ac.uk>
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


/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif 
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "origin.h"
#include "fhandle.h"
#include "tnode.h"
#include "opts.h"
#include "lexer.h"
#include "parser.h"
#include "constprop.h"
#include "treeops.h"
#include "langops.h"
#include "names.h"
#include "typecheck.h"
#include "target.h"
#include "betrans.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"
#include "allocate.h"
#include "cccsp.h"
#include "parsepriv.h"

/*}}}*/
/*{{{  private data*/

static cccsp_sfi_t *sfitable = NULL;
static int sfierror = 0;

/*}}}*/
/*{{{  global data*/

chook_t *cccsp_sfi_entrychook = NULL;

/*}}}*/

/*{{{  static cccsp_sfi_entry_t *cccsp_sfi_newentry (void)*/
/*
 *	creates a new cccsp_sfi_entry_t structure
 */
static cccsp_sfi_entry_t *cccsp_sfi_newentry (void)
{
	cccsp_sfi_entry_t *sfient = (cccsp_sfi_entry_t *)smalloc (sizeof (cccsp_sfi_entry_t));

	sfient->name = NULL;
	dynarray_init (sfient->children);
	sfient->framesize = 0;
	sfient->allocsize = 0;

	sfient->parfixup = 0;

	return sfient;
}
/*}}}*/
/*{{{  static void cccsp_sfi_freeentry (cccsp_sfi_entry_t *sfient)*/
/*
 *	destroys a cccsp_sfi_entry_t structure
 */
static void cccsp_sfi_freeentry (cccsp_sfi_entry_t *sfient)
{
	if (!sfient) {
		nocc_serious ("cccsp_sfi_freeentry(): NULL pointer!");
		return;
	}

	if (sfient->name) {
		sfree (sfient->name);
	}
	dynarray_trash (sfient->children);
	sfree (sfient);

	return;
}
/*}}}*/

/*{{{  static void cccsp_sfi_dumptable_walk (cccsp_sfi_entry_t *sfient, char *name, fhandle_t *stream)*/
/*
 *	used when walking entries for printing
 */
static void cccsp_sfi_dumptable_walk (cccsp_sfi_entry_t *sfient, char *name, fhandle_t *stream)
{
	int i;

	fhandle_printf (stream, "%-40s%d\t%d\t", sfient->name, sfient->framesize, sfient->allocsize);
	for (i=0; i<DA_CUR (sfient->children); i++) {
		cccsp_sfi_entry_t *sfiref = DA_NTHITEM (sfient->children, i);

		fhandle_printf (stream, "%s%s", i ? ", " : "", sfiref->name);
	}
	fhandle_printf (stream, "\n");
	return;
}
/*}}}*/
/*{{{  static void cccsp_sfi_clearalloc_walk (cccsp_sfi_entry_t *sfient, char *name, void *arg)*/
/*
 *	clears allocations in all entries by setting to -1
 */
static void cccsp_sfi_clearalloc_walk (cccsp_sfi_entry_t *sfient, char *name, void *arg)
{
	sfient->allocsize = -1;
	return;
}
/*}}}*/
/*{{{  static void cccsp_sfi_calcalloc_entry (cccsp_sfi_entry_t *sfient)*/
/*
 *	recursively works out the allocation size for a single entry
 */
static void cccsp_sfi_calcalloc_entry (cccsp_sfi_entry_t *sfient)
{
	int i;
	int submax = 0;

	if (sfient->allocsize >= 0) {
		return;			/* done already */
	}
	if (sfient->allocsize == -2) {
		nocc_internal ("cccsp_sfi_calcalloc_entry(): stuck in a loop..\n");
		return;
	}
	sfient->allocsize = -2;		/* make note that we're doing it now */

	for (i=0; i<DA_CUR (sfient->children); i++) {
		cccsp_sfi_entry_t *child = DA_NTHITEM (sfient->children, i);

		cccsp_sfi_calcalloc_entry (child);
		if (child->allocsize > submax) {
			submax = child->allocsize;
		}
	}
	sfient->allocsize = sfient->framesize + submax;
	return;
}
/*}}}*/
/*{{{  static void cccsp_sfi_calcalloc_walk (cccsp_sfi_entry_t *sfient, char *name, void *arg)*/
/*
 *	calculates the allocation size for a particular entry
 */
static void cccsp_sfi_calcalloc_walk (cccsp_sfi_entry_t *sfient, char *name, void *arg)
{
	cccsp_sfi_calcalloc_entry (sfient);
	return;
}
/*}}}*/
/*{{{  static void cccsp_sfi_checkalloc_walk (cccsp_sfi_entry_t *sfient, char *name, int *err)*/
/*
 *	does a final pass over the entries to make sure all were allocated okay
 */
static void cccsp_sfi_checkalloc_walk (cccsp_sfi_entry_t *sfient, char *name, int *err)
{
	if (sfient->allocsize < 0) {
		*err = 1;
	}
}
/*}}}*/
/*{{{  static void cccsp_sfi_entrychook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)*/
/*
 *	dump-tree for a cccsp:sfi:entry compiler hook
 */
static void cccsp_sfi_entrychook_dumptree (tnode_t *node, void *hook, int indent, fhandle_t *stream)
{
	cccsp_sfi_entry_t *sfient = (cccsp_sfi_entry_t *)hook;

	cccsp_isetindent (stream, indent);
	fhandle_ppxml (stream, "<cccsp:sfi:entry name=\"%s\" framesize=\"%d\" allocsize=\"%d\" />\n", sfient->name, sfient->framesize, sfient->allocsize);

	return;
}
/*}}}*/

/*{{{  int cccsp_sfi_init (void)*/
/*
 *	initialises SFI handling
 *	returns 0 on success, non-zero on failure
 */
int cccsp_sfi_init (void)
{
	sfitable = (cccsp_sfi_t *)smalloc (sizeof (cccsp_sfi_t));
	stringhash_init (sfitable->entries, SFIENTRIES_BITSIZE);

	cccsp_sfi_entrychook = tnode_lookupornewchook ("cccsp:sfi:entry");
	cccsp_sfi_entrychook->chook_dumptree = cccsp_sfi_entrychook_dumptree;

	sfierror = 0;

	return 0;
}
/*}}}*/
/*{{{  int cccsp_sfi_shutdown (void)*/
/*
 *	shuts-down SFI handling
 *	returns 0 on success, non-zero on failure
 */
int cccsp_sfi_shutdown (void)
{
	/* let-go rather than free, since bits probably peppered around the tree */
	sfitable = NULL;
	return 0;
}
/*}}}*/

/*{{{  cccsp_sfi_entry_t *cccsp_sfi_lookupornew (char *name)*/
/*
 *	looks-up or creates anew a cccsp_sfi_entry_t
 */
cccsp_sfi_entry_t *cccsp_sfi_lookupornew (char *name)
{
	cccsp_sfi_entry_t *sfient;
	
	if (!sfitable) {
		nocc_internal ("cccsp_sfi_lookupornew(%s): no table!", name);
		return NULL;
	}
	sfient = stringhash_lookup (sfitable->entries, name);
	if (!sfient) {
		sfient = cccsp_sfi_newentry ();
		sfient->name = string_dup (name);

		stringhash_insert (sfitable->entries, sfient, sfient->name);
	}

	return sfient;
}
/*}}}*/
/*{{{  cccsp_sfi_entry_t *cccsp_sfi_copyof (cccsp_sfi_entry_t *ent)*/
/*
 *	creates a copy of an existing entry (shallow, no children)
 */
cccsp_sfi_entry_t *cccsp_sfi_copyof (cccsp_sfi_entry_t *ent)
{
	cccsp_sfi_entry_t *sfient = cccsp_sfi_newentry ();

	sfient->name = string_dup (ent->name);
	sfient->framesize = ent->framesize;
	sfient->allocsize = ent->allocsize;

	return sfient;
}
/*}}}*/
/*{{{  void cccsp_sfi_addchild (cccsp_sfi_entry_t *parent, cccsp_sfi_entry_t *child)*/
/*
 *	adds one reference to another
 */
void cccsp_sfi_addchild (cccsp_sfi_entry_t *parent, cccsp_sfi_entry_t *child)
{
	int j;

	for (j=0; j<DA_CUR (parent->children); j++) {
		if (DA_NTHITEM (parent->children, j) == child) {
			break;			/* already linked */
		}
	}
	if (j == DA_CUR (parent->children)) {
		dynarray_add (parent->children, child);
	}
	return;
}
/*}}}*/
/*{{{  int cccsp_sfi_loadcalls (const char *fname)*/
/*
 *	loads call-chain definitions from a file (used for the CIF/CCCSP/Guppy APIs)
 *	returns 0 on success, non-zero on error
 */
int cccsp_sfi_loadcalls (const char *fname)
{
	fhandle_t *fhan;
	int rval = 0;
	char rbuf[1024];
	int lineno = 0;

	fhan = fhandle_fopen (fname, "r");
	if (!fhan) {
		nocc_error ("cccsp_sfi_loadcalls(): failed to open [%s]", fname);
		return -1;
	}

	while (fhandle_gets (fhan, rbuf, 1024) > 0) {
		int sidx, i;
		char *lstart, *dstart;
		cccsp_sfi_entry_t *sfient;

		lineno++;
		for (sidx = 0; (rbuf[sidx] != '\0') && ((rbuf[sidx] == ' ') || (rbuf[sidx] == '\t')); sidx++);
		if ((rbuf[sidx] == '\0') || (rbuf[sidx] == '#')) {
			continue;
		}
		lstart = &(rbuf[sidx]);

		for (i=0; (lstart[i] != '\0') && (lstart[i] != '\n') && (lstart[i] != '\r'); i++);
		lstart[i] = '\0';

		if (*lstart == '\0') {
			continue;
		}

		/* expecting something that looks like: "FcnName: dep-list, ..." or "FcnName: =NNN" */
		for (dstart=lstart; (*dstart != '\0') && (*dstart != ':'); dstart++);
		if (*dstart == '\0') {
			nocc_error ("cccsp_sfi_loadcalls(): knackered line %d [%s] in [%s]", lineno, lstart, fname);
			rval = -1;
			goto out_cleanup;
		}

		*dstart = '\0';
		for (dstart++; (*dstart == ' ') || (*dstart == '\t'); dstart++);	/* skip whitespace */

		sfient = cccsp_sfi_lookupornew (lstart);

		if (*dstart == '\0') {
			/* empty line, just keep for future reference */
			continue;
		}
		if (*dstart == '=') {
			/* fixed allocation size being specified */
			int sz = 0;
			char *ch;

			dstart++;
			for (ch = dstart; (*ch >= '0') && (*ch <= '9'); ch++);		/* should run to whitespace or EOL */
			if (*ch == '\0') {
				/* EOL */
			} else {
				*ch = '\0';
				for (ch++; (*ch == ' ') || (*ch == '\t'); ch++);	/* next thing */
			}

			if (sscanf (dstart, "%d", &sz) != 1) {
				nocc_error ("cccsp_sfi_loadcalls(): knackered line %d in [%s]", lineno, fname);
				rval = -1;
				goto out_cleanup;
			}

			if (sfient->framesize > 0) {
				nocc_warning ("cccsp_sfi_loadcalls(): already got frame-size for [%s]..", sfient->name);
				if (sz > sfient->framesize) {
					sfient->framesize = sz;
				}
			} else {
				sfient->framesize = sz;
			}

			dstart = ch;
		}
		if (*dstart != '\0') {
			char *ch;

			/* expecting comma-separated list of names */
			for (ch=dstart; *ch != '\0'; ) {
				char *dh;
				cccsp_sfi_entry_t *refsfi;
				int j;

				for (dh=ch; (*dh != '\0') && (*dh != ' ') && (*dh != '\t') && (*dh != ','); dh++);
				if (*dh == '\0') {
					/* EOL */
				} else {
					*dh = '\0';
					for (dh++; (*dh == ' ') || (*dh == '\t') || (*dh == ','); dh++);
				}

				refsfi = cccsp_sfi_lookupornew (ch);
				cccsp_sfi_addchild (sfient, refsfi);

				ch = dh;
			}
		}

#if 0
fhandle_printf (FHAN_STDERR, "cccsp_sfi_loadcalls(): got line [%s]\n", lstart);
#endif
	}

out_cleanup:
	fhandle_close (fhan);
	return rval;
}
/*}}}*/
/*{{{  int cccsp_sfi_loadusage (const char *fname)*/
/*
 *	loads stack-usage information from a file (one dropped by gcc)
 *	returns 0 on success, non-zero on error
 */
int cccsp_sfi_loadusage (const char *fname)
{
	fhandle_t *fhan;
	int rval = 0;
	char rbuf[1024];
	int lineno = 0;

	fhan = fhandle_fopen (fname, "r");
	if (!fhan) {
		nocc_error ("cccsp_sfi_loadusage(): failed to open [%s]", fname);
		return -1;
	}

	while (fhandle_gets (fhan, rbuf, 1024) > 0) {
		int i;
		char *nstart, *lstart, *tstart;
		int sz;

		lineno++;
		for (i=0; (rbuf[i] != '\0') && (rbuf[i] != '\n') && (rbuf[i] != '\r'); i++);
		rbuf[i] = '\0';			/* remove trailing newline */

		/* scan forward for tab or space */
		for (nstart = (char *)rbuf; (*nstart != '\0') && (*nstart != '\t') && (*nstart != ' '); nstart++);
		if (*nstart == '\0') {
			/* knackered */
			nocc_error ("cccsp_sfi_loadusage(): damaged line %d in [%s]", lineno, fname);
			continue;
		}
		lstart = nstart;
		*nstart = '\0';
		for (nstart++; (*nstart == ' ') || (*nstart == '\t'); nstart++);
		for (lstart--; (lstart > (char *)rbuf) && (lstart[-1] != ':'); lstart--);

		/* oki: lstart should be function name, nstart is value-field */

		for (tstart = nstart; (*tstart >= '0') && (*tstart <= '9'); tstart++);
		if (*tstart == '\0') {
			/* knackered */
			nocc_error ("cccsp_sfi_loadusage(): damaged line %d in [%s]", lineno, fname);
			continue;
		}
		*tstart = '\0';
		for (tstart++; (*tstart == ' ') || (*tstart == '\t'); tstart++);
		if (sscanf (nstart, "%d", &sz) != 1) {
			/* knackered */
			nocc_error ("cccsp_sfi_loadusage(): damaged line %d (size field) in [%s]", lineno, fname);
			continue;
		}

		/* should be some qualifiers/flags -- look for very specific combinations */
		if (!strcmp (tstart, "static") || !strcmp (tstart, "dynamic,bounded")) {
			cccsp_sfi_entry_t *sfient = cccsp_sfi_lookupornew (lstart);

			if (sz > sfient->framesize) {
				sfient->framesize = sz;
			}
		} else {
			nocc_warning ("cccsp_sfi_loadusage(): ignoring function [%s] on line %d in [%s]", lstart, lineno, fname);
		}
	}

out_cleanup:
	fhandle_close (fhan);
	return rval;
}
/*}}}*/
/*{{{  int cccsp_sfi_calc_alloc (void)*/
/*
 *	calculates allocations required based on call graph
 *	returns 0 on success, non-zero on error
 */
int cccsp_sfi_calc_alloc (void)
{
	int err = 0;

	if (!sfitable) {
		nocc_error ("cccsp_sfi_calc_alloc(): no table!");
		return -1;
	}
	stringhash_walk (sfitable->entries, cccsp_sfi_clearalloc_walk, NULL);
	stringhash_walk (sfitable->entries, cccsp_sfi_calcalloc_walk, NULL);
	stringhash_walk (sfitable->entries, cccsp_sfi_checkalloc_walk, &err);

	return err;
}
/*}}}*/
/*{{{  void cccsp_sfi_dumptable (fhandle_t *stream)*/
/*
 *	dumps the SFI table (debugging)
 */
void cccsp_sfi_dumptable (fhandle_t *stream)
{
	if (!sfitable) {
		return;
	}
	stringhash_walk (sfitable->entries, cccsp_sfi_dumptable_walk, (void *)stream);
}
/*}}}*/

/*{{{  int cccsp_sfi_error (tnode_t *t, const char *fmt, ...)*/
/*
 *	error-reporting for SFI handling things
 *	returns number of bytes written on success, < 0 on error
 */
int cccsp_sfi_error (tnode_t *t, const char *fmt, ...)
{
	va_list ap;
	static char errbuf[512];
	int n;
	lexfile_t *lf = t->org ? t->org->org_file : NULL;

	va_start (ap, fmt);
	n = sprintf (errbuf, "%s:%d (SFI error) ", lf ? lf->fnptr : "(unknown)", t->org ? t->org->org_line : -1);
	n += vsnprintf (errbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->errcount++;
	}
	sfierror++;

	nocc_outerrmsg (errbuf);

	return n;
}
/*}}}*/
/*{{{  int cccsp_sfi_geterror (void)*/
/*
 *	returns the SFI error counter (used to fail the whole compiler pass)
 */
int cccsp_sfi_geterror (void)
{
	return sfierror;
}
/*}}}*/

