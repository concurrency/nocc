/*
 *	transisntr.c -- transputer instructions (lookup mainly)
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
#include "transputer.h"

#include "gperf_transinstr.h"

/* instructions by "ins" value */
STATICDYNARRAY (transinstr_t *, itransinstrs);

/* hash for added instructions */
STATICSTRINGHASH (transinstr_t *, extratransinstrs, 8);


/*{{{  void transinstr_init (void)*/
/*
 *	initialises transputer-instruction processing
 */
void transinstr_init (void)
{
	int i;
	int maxtag = 0;

	for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
		if (wordlist[i].name && (wordlist[i].ins > maxtag)) {
			maxtag = wordlist[i].ins;
		}
	}

	dynarray_init (itransinstrs);
	dynarray_setsize (itransinstrs, maxtag + 10);
	for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
		if (wordlist[i].name && (wordlist[i].ins >= 0)) {
			DA_SETNTHITEM (itransinstrs, wordlist[i].ins, (transinstr_t *)&(wordlist[i]));
		}
	}

	stringhash_init (extratransinstrs);

	return;
}
/*}}}*/
/*{{{  transinstr_t *transinstr_lookup (const char *str, const int len)*/
/*
 *	looks up a transputer instruction
 */
transinstr_t *transinstr_lookup (const char *str, const int len)
{
	transinstr_t *ti;

	ti = (transinstr_t *)transinstr_lookup_byname (str, len);
	if (!ti) {
		ti = stringhash_lookup (extratransinstrs, str);
	}
	return ti;
}
/*}}}*/
/*{{{   int transinstr_add (const char *str, instrlevel_e level, void *origin)*/
/*
 *	adds an instruction to the compiler.  returns the assigned value
 */
int transinstr_add (const char *str, instrlevel_e level, void *origin)
{
	transinstr_t *ti = (transinstr_t *)smalloc (sizeof (transinstr_t));
	int i = DA_CUR (itransinstrs);

	ti->name = string_dup ((char *)str);
	ti->level = level;
	ti->ins = (transinstr_e)i;
	ti->origin = origin;

	stringhash_insert (extratransinstrs, ti, ti->name);
	dynarray_add (itransinstrs, ti);

	return i;
}
/*}}}*/


