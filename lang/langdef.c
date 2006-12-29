/*
 *	langdef.c -- language definition handling for NOCC
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "tnode.h"
#include "parser.h"
#include "parsepriv.h"
#include "lexpriv.h"
#include "names.h"
#include "target.h"


/*}}}*/


/*{{{  int langdef_init (void)*/
/*
 *	called to initialise the language definition bits
 *	returns 0 on success, non-zero on failure
 */
int langdef_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int langdef_shutdown (void)*/
/*
 *	called to shut-down the language definition bits
 *	returns 0 on success, non-zero on failure
 */
int langdef_shutdown (void)
{
	return 0;
}
/*}}}*/


