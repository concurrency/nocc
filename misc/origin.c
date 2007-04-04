/*
 *	origin.c -- compiler origin routines
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
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

/*{{{  includes*/
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

/*}}}*/


/*{{{  int origin_init (void)*/
/*
 *	called to initialise origin handling
 *	returns 0 on success, non-zero on failure
 */
int origin_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int origin_shutdown (void)*/
/*
 *	called to shut-down origin handling
 *	returns 0 on success, non-zero on failure
 */
int origin_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  static origin_t *org_neworigin (void)*/
/*
 *	creates a blank origin_t structure
 */
static origin_t *org_neworigin (void)
{
	origin_t *org = (origin_t *)smalloc (sizeof (origin_t));

	org->type = ORG_INVALID;

	return org;
}
/*}}}*/
/*{{{  static void org_freeorigin (origin_t *org)*/
/*
 *	destroys an origin_t structure
 */
static void org_freeorigin (origin_t *org)
{
	if (!org) {
		nocc_warning ("org_freeorigin(): NULL origin!");
		return;
	}
	switch (org->type) {
	case ORG_INTERNAL:
		if (org->u.internal.file) {
			sfree (org->u.internal.file);
			org->u.internal.file = NULL;
		}
		break;
	default:
		break;
	}
	sfree (org);
	return;
}
/*}}}*/


/*{{{  origin_t *origin_internal (const char *filename, const int lineno)*/
/*
 *	creates a new internal origin (with filename and line-number)
 */
origin_t *origin_internal (const char *filename, const int lineno)
{
	origin_t *org = org_neworigin ();

	org->type = ORG_INTERNAL;
	org->u.internal.file = string_dup (filename);
	org->u.internal.line = lineno;

	return org;
}
/*}}}*/



