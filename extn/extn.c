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


STATICDYNARRAY (extn_t *, extensions);


/*{{{  void extn_init (void)*/
/*
 *	initialises the extension handler
 */
void extn_init (void)
{
	dynarray_init (extensions);

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
	/*
	 *	TODO: load library, see if it registers, add to list if not
	 */
	return 0;
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


