/*
 *	extn.c -- dynamic extension handler
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "extn.h"
#include "parser.h"
#include "parsepriv.h"
#include "dfa.h"

#if defined(LIBDL)

#include <dlfcn.h>


STATICDYNARRAY (extn_t *, extensions);


/*{{{  void extn_init (void)*/
/*
 *	initialises the extension handler
 */
void extn_init (void)
{
	dynarray_init (extensions);

	if (!dlsym ((void *)0, "nocc_error")) {
		nocc_error ("extn_init(): could not find nocc_error symbol");
	}

	return;
}
/*}}}*/
/*{{{  int extn_loadextn (const char *fname)*/
/*
 *	loads an extension into the compiler
 *	searches via the "epath" config component
 *	return 0 on success, non-zero on failure
 */
int extn_loadextn (const char *fname)
{
	int curecount = DA_CUR (extensions);
	int i;
	char *fnbuf = NULL;
	void *libhandle;
	extn_t *theextn = NULL;

	fnbuf = (char *)smalloc (FILENAME_MAX);

	for (i=0; i<DA_CUR (compopts.epath); i++) {
		char *epath = DA_NTHITEM (compopts.epath, i);
		int eplen = strlen (epath);

		snprintf (fnbuf, FILENAME_MAX-1, "%s%slib%s.so", epath, (epath[eplen - 1] == '/') ? "" : "/", fname);
		if (!access (fnbuf, R_OK)) {
			break;		/* for() */
		}
	}
	if (i == DA_CUR (compopts.epath)) {
		nocc_error ("extn_loadextn(): failed to find library %s", fname);
		sfree (fnbuf);
		return -1;
	}

	/* open library */
	libhandle = dlopen (fnbuf, RTLD_NOW | RTLD_LOCAL);
	if (!libhandle) {
		nocc_error ("extn_loadextn(): failed to open library %s (from %s): %s", fname, fnbuf, dlerror ());
		sfree (fnbuf);
		return -1;
	}

	/* if the library had a constructor, it would have called register already */
	if (curecount == DA_CUR (extensions)) {
		/*{{{  didn't register yet, prod it manually*/
		int (*initfcn)(void);
		char *derr;

		dlerror ();		/* clear any error message */
		initfcn = (int (*)(void))dlsym (libhandle, "nocc_extn_init");
		derr = dlerror ();
		if (derr) {
			nocc_error ("extn_loadextn(): failed to resolve \"nocc_extn_init\" in %s (file %s): %s", fname, fnbuf, derr);
			sfree (fnbuf);
			dlclose (libhandle);
			return -1;
		}

		/* call init function -- should register it */
		if (initfcn ()) {
			nocc_error ("extn_loadextn(): failed to initialise %s (file %s)", fname, fnbuf);
			sfree (fnbuf);
			dlclose (libhandle);
			return -1;
		}
		if (curecount == DA_CUR (extensions)) {
			nocc_error ("extn_loadextn(): extension %s (file %s) did not register", fname, fnbuf);
			sfree (fnbuf);
			dlclose (libhandle);
			return -1;
		}

		/*}}}*/
	}

	/* should have registered as the last one, set filename */
	theextn = DA_NTHITEM (extensions, DA_CUR (extensions) - 1);
	theextn->filename = string_dup (fnbuf);

	/* leave library open and forget about it */

	sfree (fnbuf);

	return 0;
}
/*}}}*/
/*{{{  void extn_dumpextns (void)*/
/*
 *	dumps loaded extensions (debugging)
 */
void extn_dumpextns (void)
{
	int i;

	nocc_message ("%d loaded extensions:", DA_CUR (extensions));
	for (i=0; i<DA_CUR (extensions); i++) {
		extn_t *extn = DA_NTHITEM (extensions, i);

		nocc_message ("    %-16s %-8s %-8s (%s)", extn->name, extn->cversionstr, extn->version, extn->desc);
	}

	return;
}
/*}}}*/
/*{{{  int extn_register (extn_t *extn)*/
/*
 *	called to register an extension
 *	return 0 on success, non-zero on failure
 */
int extn_register (extn_t *extn)
{
	int i;

	for (i=0; i<DA_CUR (extensions); i++) {
		if (DA_NTHITEM (extensions, i) == extn) {
			nocc_error ("extn_register(): extension [%s] is already registered", extn->name);
			return 1;
		} else if (!strcmp (DA_NTHITEM (extensions, i)->name, extn->name)) {
			nocc_error ("extn_register(): an extension called [%s] is already registered", extn->name);
			return -1;
		}
	}
	dynarray_add (extensions, extn);
	return 0;
}
/*}}}*/


/*{{{  int extn_initialise (void)*/
/*
 *	called to initialise extensions (calling the extn_t init routine)
 *	returns 0 on success, non-zero on failure
 */
int extn_initialise (void)
{
	int i;

	for (i=0; i<DA_CUR (extensions); i++) {
		extn_t *extn = DA_NTHITEM (extensions, i);

		if (extn->init && extn->init (extn)) {
			nocc_error ("extn_initialise(): failed to initialise extension [%s]", extn->name);
			return -1;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  int extn_preloadgrammar (langparser_t *lang, dfattbl_t ***ttblptr, int *ttblcur, int *ttblmax)*/
/*
 *	called to pre-load grammars for extensions, language involved is passed
 *	returns 0 on success, non-zero on failure
 */
int extn_preloadgrammar (langparser_t *lang, dfattbl_t ***ttblptr, int *ttblcur, int *ttblmax)
{
	int i;

	for (i=0; i<DA_CUR (extensions); i++) {
		extn_t *extn = DA_NTHITEM (extensions, i);

		if (extn->preloadgrammar && extn->preloadgrammar (extn, lang, ttblptr, ttblcur, ttblmax)) {
			nocc_error ("extn_preloadgrammar(): failed to load for language [%s] extension [%s]", lang->langname ?: "(unknown)", extn->name);
			return -1;
		}
	}
	return 0;
}
/*}}}*/
/*{{{  int extn_postloadgrammar (langparser_t *lang)*/
/*
 *	called to post-load grammars for extensions, language involved is passed
 *	return 0 on success, non-zero on failure
 */
int extn_postloadgrammar (langparser_t *lang)
{
	int i;

	for (i=0; i<DA_CUR (extensions); i++) {
		extn_t *extn = DA_NTHITEM (extensions, i);

		if (extn->postloadgrammar && extn->postloadgrammar (extn, lang)) {
			nocc_error ("extn_postloadgrammar(): failed to load for language [%s] extension [%s]", lang->langname ?: "(unknown)", extn->name);
			return -1;
		}
	}
	return 0;
}
/*}}}*/

#else	/* !defined(LIBDL) */

/*{{{  void extn_init (void)*/
/*
 *	dummy initialisation routine
 */
void extn_init (void)
{
	return;
}
/*}}}*/
/*{{{  int extn_loadextn (const char *fname)*/
/*
 *	dummy load extension
 *	returns 0 on success, non-zero on failure
 */
int extn_loadextn (const char *fname)
{
	nocc_warning ("cannot load extension [%s], no dynamic library support", fname);
	return 0;
}
/*}}}*/
/*{{{  int extn_initialise (void)*/
/*
 *	dummy initialise extensions
 *	returns 0 on success, non-zero on failure
 */
int extn_initialise (void)
{
	return 0;
}
/*}}}*/
/*{{{  int extn_preloadgrammar (langparser_t *lang, dfattbl_t ***ttblptr, int *ttblcur, int *ttblmax)*/
/*
 *	dummy preloadgrammar
 *	returns 0 on success, non-zero on failure
 */
int extn_preloadgrammar (langparser_t *lang, dfattbl_t ***ttblptr, int *ttblcur, int *ttblmax)
{
	return 0;
}
/*}}}*/
/*{{{  int extn_postloadgrammar (langparser_t *lang)*/
/*
 *	dummy postloadgrammar
 *	return 0 on success, non-zero on failure
 */
int extn_postloadgrammar (langparser_t *lang)
{
	return 0;
}
/*}}}*/

#endif	/* !defined(LIBDL) */

