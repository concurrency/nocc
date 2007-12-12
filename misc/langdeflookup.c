/*
 *	langdeflookup.c -- lookups for language definition constants
 *	Copyright (C) 2007 Fred Barnes <frmb@kent.ac.uk>
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
#include "origin.h"
#include "version.h"
#include "langdef.h"
#include "langdeflookup.h"

#include "gperf_langdeflookup.h"

/* language definitions by constant LDL_.. value */
STATICDYNARRAY (langdeflookup_t *, ilangdeflookups);

/* hash for added language definition items */
STATICSTRINGHASH (langdeflookup_t *, extralangdeflookups, 8);


/*{{{  int langdeflookup_init (void)*/
/*
 *	initialises language definition lookup routines
 *	returns 0 on success, non-zero on failure
 */
int langdeflookup_init (void)
{
	int i;
	int maxtag = 0;

	for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
		if (wordlist[i].name && (wordlist[i].ldl > maxtag)) {
			maxtag = wordlist[i].ldl;
		}
	}

	dynarray_init (ilangdeflookups);
	dynarray_setsize (ilangdeflookups, maxtag + 10);
	for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
		if (wordlist[i].name && (wordlist[i].ldl >= 0)) {
			DA_SETNTHITEM (ilangdeflookups, wordlist[i].ldl, (langdeflookup_t *)&(wordlist[i]));
		}
	}

	stringhash_sinit (extralangdeflookups);

	return 0;
}
/*}}}*/
/*{{{  int langdeflookup_shutdown (void)*/
/*
 *	shuts-down language definition lookup routines
 *	returns 0 on success, non-zero on failure
 */
int langdeflookup_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  langdeflookup_t *langdeflookup_lookup (const char *str, const int len)*/
/*
 *	looks up a language definition constant
 */
langdeflookup_t *langdeflookup_lookup (const char *str, const int len)
{
	langdeflookup_t *ti;

	ti = (langdeflookup_t *)langdeflookup_lookup_byname (str, len);
	if (!ti) {
		ti = stringhash_lookup (extralangdeflookups, str);
	}
	return ti;
}
/*}}}*/
/*{{{   int langdeflookup_add (const char *str, origin_t *origin)*/
/*
 *	adds a language-definition keyword to the compiler.  returns the assigned value
 */
int langdeflookup_add (const char *str, origin_t *origin)
{
	langdeflookup_t *ti = (langdeflookup_t *)smalloc (sizeof (langdeflookup_t));
	int i = DA_CUR (ilangdeflookups);

	ti->name = string_dup ((char *)str);
	ti->ldl = (langdeflookup_e)i;
	ti->origin = origin;

	stringhash_insert (extralangdeflookups, ti, ti->name);
	dynarray_add (ilangdeflookups, ti);

	return i;
}
/*}}}*/



