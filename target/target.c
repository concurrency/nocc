/*
 *	target.c -- general back-end target handling routines for nocc
 *	Copyright (C) 2005-2015 Fred Barnes <frmb@kent.ac.uk>
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
#include "fhandle.h"
#include "tnode.h"
#include "lexer.h"
#include "names.h"
#include "typecheck.h"
#include "target.h"
#include "map.h"

#include "krocetc.h"
#include "krocllvm.h"
#include "kroccifccsp.h"
#include "cccsp.h"
#include "atmelavr.h"

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

/*{{{  void target_dumptargets (fhandle_t *stream)*/
/*
 *	displays a list of supported targets (debugging/info)
 */
void target_dumptargets (fhandle_t *stream)
{
	int i;

	for (i=0; i<DA_CUR (atargets); i++) {
		target_t *xt = DA_NTHITEM (atargets, i);

		fhandle_printf (stream, "target: %s (%s-%s-%s)\n", xt->name, xt->tarch ?: "*", xt->tvendor ?: "*", xt->tos ?: "*");
		fhandle_printf (stream, "        %s\n", xt->desc ?: "(no description)");
		fhandle_printf (stream, "        capabilities: ");
		/*{{{  tcap flags*/
		if (xt->tcap.can_do_fp) {
			fhandle_printf (stream, "FP ");
		}
		if (xt->tcap.can_do_dmem) {
			fhandle_printf (stream, "DMEM ");
		}
		/*}}}*/
		fhandle_printf (stream, "\n");
		fhandle_printf (stream, "        sizes: char=%d  int=%d  pointer=%d\n", xt->charsize, xt->intsize, xt->pointersize);
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

/*{{{  int target_error (target_t *target, tnode_t *orgnode, const char *fmt, ...)*/
/*
 *	generic error reporting for back-end
 *	returns number of bytes written on success, < 0 on error
 */
int target_error (target_t *target, tnode_t *orgnode, const char *fmt, ...)
{
	va_list ap;
	static char errbuf[512];
	int n;
	lexfile_t *lf;
	int line;

	if (!orgnode) {
		lf = NULL;
		line = -1;
	} else {
		lf = orgnode->org ? orgnode->org->org_file : NULL;
		line = orgnode->org ? orgnode->org->org_line : -1;
	}
	if (!lf) {
		n = sprintf (errbuf, "%s: (error) ", target->name);
	} else {
		n = sprintf (errbuf, "%s: %s:%d (error) ", target->name, lf->fnptr, line);
	}
	va_start (ap, fmt);
	n += vsnprintf (errbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->errcount++;
	}

	nocc_outerrmsg (errbuf);

	return n;
}
/*}}}*/
/*{{{  int target_warning (target_t *target, tnode_t *orgnode, const char *fmt, ...)*/
/*
 *	generic warning reporting for back-end
 *	returns number of bytes written on success, < 0 on error
 */
int target_warning (target_t *target, tnode_t *orgnode, const char *fmt, ...)
{
	va_list ap;
	static char errbuf[512];
	int n;
	lexfile_t *lf;
	int line;

	if (!orgnode) {
		lf = NULL;
		line = -1;
	} else {
		lf = orgnode->org ? orgnode->org->org_file : NULL;
		line = orgnode->org ? orgnode->org->org_line : -1;
	}
	if (!lf) {
		n = sprintf (errbuf, "%s: (warning) ", target->name);
	} else {
		n = sprintf (errbuf, "%s: %s:%d (warning) ", target->name, lf->fnptr, line);
	}
	va_start (ap, fmt);
	n += vsnprintf (errbuf + n, 512 - n, fmt, ap);
	va_end (ap);

	if (lf) {
		lf->warncount++;
	}

	nocc_outwarnmsg (errbuf);

	return n;
}
/*}}}*/


/*{{{  int target_init (void)*/
/*
 *	initialises general target handling
 *	returns 0 on success, non-zero on error
 */
int target_init (void)
{
	stringhash_sinit (targets);
	dynarray_init (atargets);

	/* initialise the built-in KRoC/ETC target */
	if (krocetc_init ()) {
		return 1;
	}
	/* initialise the built-in KRoC/CIF/CCSP target */
	if (kroccifccsp_init ()) {
		return 1;
	}
	/* initialise the built-in KRoC/LLVM target */
	if (krocllvm_init ()) {
		return 1;
	}
	/* initialise the built-in CCSP C target */
	if (cccsp_init ()) {
		return 1;
	}
	/* initialise the built-in AVR target */
	if (atmelavr_init ()) {
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
	/* shutdown the built-in AVR target */
	if (atmelavr_shutdown ()) {
		return 1;
	}
	/* shutdown the built-in CCSP C target */
	if (cccsp_shutdown ()) {
		return 1;
	}
	/* shutdown the built-in KRoC/LLVM target */
	if (krocllvm_shutdown ()) {
		return 1;
	}
	/* shutdown the built-in KRoC/CIF/CCSP target */
	if (kroccifccsp_shutdown ()) {
		return 1;
	}
	/* shutdown the built-in KRoC/ETC target */
	if (krocetc_shutdown ()) {
		return 1;
	}
	return 0;
}
/*}}}*/



