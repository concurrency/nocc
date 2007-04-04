/*
 *	keywords.c -- keyword processing
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
#include "origin.h"
#include "lexer.h"
#include "lexpriv.h"
#include "keywords.h"

#include "gperf_keywords.h"

/* keywords by tag value */
STATICDYNARRAY (keyword_t *, ikeywords);

/* hash for added keywords */
STATICSTRINGHASH (keyword_t *, extrakeywords, 8);


/*{{{  void keywords_init (void)*/
/*
 *	initialises keyword processing
 */
void keywords_init (void)
{
	int i;
	int maxtag = 0;

	for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
		if (wordlist[i].name && (wordlist[i].tagval > maxtag)) {
			maxtag = wordlist[i].tagval;
		}
	}

	dynarray_init (ikeywords);
	dynarray_setsize (ikeywords, maxtag + 10);
	for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
		if (wordlist[i].name && (wordlist[i].tagval >= 0)) {
			DA_SETNTHITEM (ikeywords, wordlist[i].tagval, (keyword_t *)&(wordlist[i]));
		}
	}

	stringhash_init (extrakeywords);

	return;
}
/*}}}*/
/*{{{  keyword_t *keywords_lookup (const char *str, const int len, const unsigned int langtag)*/
/*
 *	looks up a keyword
 */
keyword_t *keywords_lookup (const char *str, const int len, const unsigned int langtag)
{
	keyword_t *kw;

	kw = (keyword_t *)keyword_lookup_byname (str, len);
	if (kw && langtag && ((kw->langtag & langtag) != langtag)) {
		kw = NULL;
	}

	if (!kw) {
		kw = stringhash_lookup (extrakeywords, str);
		if (kw && langtag && ((kw->langtag & langtag) != langtag)) {
			kw = NULL;
		}
	}
	return kw;
}
/*}}}*/
/*{{{  keyword_t *keywords_add (const char *str, const int tagval, const unsigned int langtag, origin_t *origin)*/
/*
 *	adds a keyword to the compiler
 */
keyword_t *keywords_add (const char *str, const int tagval, const unsigned int langtag, origin_t *origin)
{
	keyword_t *kw = (keyword_t *)smalloc (sizeof (keyword_t));

	kw->name = string_dup ((char *)str);
	kw->tagval = tagval;
	kw->langtag = langtag;
	kw->origin = origin;

	stringhash_insert (extrakeywords, kw, kw->name);
	return kw;
}
/*}}}*/


