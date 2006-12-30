/*
 *	fcnlib.c -- function library for NOCC
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*{{{  includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"

/*}}}*/
/*{{{  private types*/

typedef struct {
	char *name;		/* function name */
	void *fcnaddr;		/* function address */
	int ret;		/* non-zero if it returns something */
	int nargs;		/* number of arguments expected */
} fcnlib_t;


/*}}}*/
/*{{{  private data*/

STATICSTRINGHASH (fcnlib_t *, functions, 6);
STATICDYNARRAY (fcnlib_t *, afunctions);

/*}}}*/


/*{{{  static fcnlib_t *fcn_newfcnlib (void)*/
/*
 *	creates a new fcnlib_t structure
 */
static fcnlib_t *fcn_newfcnlib (void)
{
	fcnlib_t *fcnl = (fcnlib_t *)smalloc (sizeof (fcnlib_t));

	fcnl->name = NULL;
	fcnl->fcnaddr = NULL;
	fcnl->ret = 0;
	fcnl->nargs = 0;

	return fcnl;
}
/*}}}*/
/*{{{  static void fcn_freefcnlib (fcnlib_t *fcnl)*/
/*
 *	frees a fcnlib_t structure
 */
static void fcn_freefcnlib (fcnlib_t *fcnl)
{
	if (!fcnl) {
		nocc_warning ("fcn_freefcnlib(): NULL pointer!");
		return;
	}
	if (fcnl->name) {
		sfree (fcnl->name);
		fcnl->name = NULL;
	}
	fcnl->fcnaddr = NULL;
	fcnl->ret = 0;
	fcnl->nargs = 0;

	sfree (fcnl);
	return;
}
/*}}}*/




/*{{{  int fcnlib_init (void)*/
/*
 *	initialises the function library
 *	returns 0 on success, non-zero on failure
 */
int fcnlib_init (void)
{
	stringhash_init (functions);
	dynarray_init (afunctions);
	return 0;
}
/*}}}*/
/*{{{  int fcnlib_shutdown (void)*/
/*
 *	shuts-down the function library
 *	returns 0 on success, non-zero on failure
 */
int fcnlib_shutdown (void)
{
	int i;

	for (i=0; i<DA_CUR (afunctions); i++) {
		fcnlib_t *fcnl = DA_NTHITEM (afunctions, i);

		if (fcnl) {
			fcn_freefcnlib (fcnl);
		}
	}
	dynarray_trash (afunctions);
	stringhash_trash (functions);

	return 0;
}
/*}}}*/


/*{{{  int fcnlib_addfcn (const char *name, void *addr, int ret, int nargs)*/
/*
 *	adds an entry to the function library
 *	returns 0 on success, non-zero on failure
 */
int fcnlib_addfcn (const char *name, void *addr, int ret, int nargs)
{
	fcnlib_t *fcnl = stringhash_lookup (functions, name);

	if (fcnl) {
		/* already here */
		if ((addr == fcnl->fcnaddr) && (ret == fcnl->ret) && (nargs == fcnl->nargs)) {
			/* re-registering exact, ok */
			return 0;
		}
		nocc_warning ("fcnlib_addfcn(): function [%s] already registered with (0x%8.8x,%d,%d), but trying to set to (0x%8.8x,%d,%d)",
			name, (unsigned int)fcnl->fcnaddr, fcnl->ret, fcnl->nargs, (unsigned int)addr, ret, nargs);
		return -1;
	}

	fcnl = fcn_newfcnlib ();

	fcnl->name = string_dup (name);
	fcnl->fcnaddr = addr;
	fcnl->ret = ret;
	fcnl->nargs = nargs;

	stringhash_insert (functions, fcnl, fcnl->name);
	dynarray_add (afunctions, fcnl);

	return 0;
}
/*}}}*/
/*{{{  int fcnlib_havefunction (const char *name)*/
/*
 *	tests to see whether the named function is registered
 *	returns non-zero if it is, zero otherwise
 */
int fcnlib_havefunction (const char *name)
{
	fcnlib_t *fcnl = stringhash_lookup (functions, name);

	return fcnl ? 1 : 0;
}
/*}}}*/
/*{{{  void *fcnlib_findfunction (const char *name)*/
/*
 *	looks up a function by name
 *	returns address on success, NULL on failure
 */
void *fcnlib_findfunction (const char *name)
{
	fcnlib_t *fcnl = stringhash_lookup (functions, name);

	if (fcnl) {
		return fcnl->fcnaddr;
	}
	nocc_warning ("fcnlib_findfunction(): no such function [%s] registered", name);

	return NULL;
}
/*}}}*/
/*{{{  void *fcnlib_findfunction2 (const char *name, const int ret, const int nargs)*/
/*
 *	looks up a function by name, return and number of arguments
 *	returns address on success, NULL on failure
 */
void *fcnlib_findfunction2 (const char *name, const int ret, const int nargs)
{
	fcnlib_t *fcnl = stringhash_lookup (functions, name);

	if (fcnl) {
		if ((fcnl->ret == ret) && (fcnl->nargs == nargs)) {
			/* this one */
			return fcnl->fcnaddr;
		}
		nocc_warning ("fcnlib_findfunction2(): function [%s] registered but ret/args mismatch, looking for (%d,%d) found (%d,%d)", name, ret, nargs, fcnl->ret, fcnl->nargs);
		return NULL;
	}
	nocc_warning ("fcnlib_findfunction2(): no such function [%s] registered", name);

	return NULL;
}
/*}}}*/
/*{{{  void *fcnlib_findfunction3 (const char *name, int *n_ret, int *n_nargs)*/
/*
 *	looks up a function by name, but sets the return-flag and number-of-args passed (if non-NULL)
 *	returns address on success, NULL on failure
 */
void *fcnlib_findfunction3 (const char *name, int *n_ret, int *n_nargs)
{
	fcnlib_t *fcnl = stringhash_lookup (functions, name);

	if (fcnl) {
		if (n_ret) {
			*n_ret = fcnl->ret;
		}
		if (n_nargs) {
			*n_nargs = fcnl->nargs;
		}
		return fcnl->fcnaddr;
	}
	nocc_warning ("fcnlib_findfunction3(): no such function [%s] registered", name);

	return NULL;
}
/*}}}*/

