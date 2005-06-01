/*
 *	target.c -- general back-end target handling routines for nocc
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
#include "tnode.h"
#include "names.h"
#include "target.h"
#include "map.h"

#include "krocetc.h"
/*}}}*/


/*{{{  private vars*/
STATICSTRINGHASH (target_t *, targets, 4);
STATICDYNARRAY (target_t *, atargets);

/*}}}*/


/*{{{  int target_register (target_t *target)*/
/*
 *	called to register a target
 *	returns 0 on success, non-zero on error
 */
int target_register (target_t *target)
{
	target_t *xt;

	xt = stringhash_lookup (targets, target->name);
	if (xt == target) {
		nocc_warning ("target_register(): target [%s] is already registered", target->name);
		return 0;
	}
	if (xt) {
		nocc_error ("target_register(): different target [%s] is already registered", target->name);
		return 1;
	}

	stringhash_insert (targets, target, target->name);
	dynarray_add (atargets, target);

	return 0;
}
/*}}}*/
/*{{{  int target_unregister (target_t *target)*/
/*
 *	called to unregister a target
 *	returns 0 on success, non-zero on error
 */
int target_unregister (target_t *target)
{
	target_t *xt;

	xt = stringhash_lookup (targets, target->name);
	if (xt == target) {
		/* good-o */
		dynarray_rmitem (atargets, xt);
		stringhash_remove (targets, xt, xt->name);
		return 0;
	} else if (xt) {
		nocc_error ("target_unregister(): a different target is registered under name [%s]", target->name);
		return 1;
	}
	nocc_error ("target_unregister(): target [%s] is not registered", target->name);
	return 1;
}
/*}}}*/
/*{{{  target_t *target_lookupbyspec (char *tarch, char *tvendor, char *tos)*/
/*
 *	finds a target matching the given specification
 *	returns the target definition if found, NULL otherwise
 */
target_t *target_lookupbyspec (char *tarch, char *tvendor, char *tos)
{
	int i;

	for (i=0; i<DA_CUR (atargets); i++) {
		target_t *xt = DA_NTHITEM (atargets, i);

		if ((!xt->tarch || !tarch || !strcmp (tarch, xt->tarch)) && (!xt->tvendor || !tvendor || !strcmp (tvendor, xt->tvendor)) &&
				(!xt->tos || !tos || !strcmp (tos, xt->tos))) {
			/* this one matches */
			return xt;
		}
	}
	return NULL;
}
/*}}}*/


/*{{{  void target_dumptargets (FILE *stream)*/
/*
 *	displays a list of supported targets (debugging/info)
 */
void target_dumptargets (FILE *stream)
{
	int i;

	for (i=0; i<DA_CUR (atargets); i++) {
		target_t *xt = DA_NTHITEM (atargets, i);

		fprintf (stream, "target: %s (%s-%s-%s)\n", xt->name, xt->tarch ?: "*", xt->tvendor ?: "*", xt->tos ?: "*");
		fprintf (stream, "        %s\n", xt->desc ?: "(no description)");
		fprintf (stream, "        capabilities: ");
		/*{{{  tcap flags*/
		if (xt->tcap.can_do_fp) {
			fprintf (stream, "FP ");
		}
		if (xt->tcap.can_do_dmem) {
			fprintf (stream, "DMEM ");
		}
		/*}}}*/
		fprintf (stream, "\n");
		fprintf (stream, "        sizes: char=%d  int=%d  pointer=%d\n", xt->charsize, xt->intsize, xt->pointersize);
	}
	return;
}
/*}}}*/


/*{{{  int target_initialise (target_t *target)*/
/*
 *	initialises the given target
 *	returns 0 on success, non-zero on failure
 */
int target_initialise (target_t *target)
{
	int i;

	if (!target || !target->init) {
		nocc_error ("cannot initialise target [%s]", target ? target->name : "(null)");
		return 1;
	}
	if (target->initialised) {
		return 0;	/* already initialised */
	}

	i = target->init (target);

	return i;
}
/*}}}*/


/*{{{  int target_init (void)*/
/*
 *	initialises general target handling
 *	returns 0 on success, non-zero on error
 */
int target_init (void)
{
	stringhash_init (targets);
	dynarray_init (atargets);

	/* initialise the built-in KRoC/ETC target */
	if (krocetc_init ()) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  int target_shutdown (void)*/
/*
 *	shuts-down general target handling
 *	return 0 on success, non-zero on error
 */
int target_shutdown (void)
{
	/* shutdown the built-in KRoC/ETC target */
	if (krocetc_shutdown ()) {
		return 1;
	}
	return 0;
}
/*}}}*/



