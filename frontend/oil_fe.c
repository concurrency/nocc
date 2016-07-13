/*
 *	oil_fe.c -- oil front-end (initialisation/registration)
 *	Copyright (C) 2016 Fred Barnes <frmb@kent.ac.uk>
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
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parsepriv.h"
#include "oil.h"
#include "opts.h"
#include "oil_fe.h"

#include "mwsync.h"

/*}}}*/


/*{{{  int oil_register_frontend (void)*/
/*
 *	registers the oil lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int oil_register_frontend (void)
{
	oil_lexer.parser = &oil_parser;
	oil_parser.lexer = &oil_lexer;

	if (lexer_registerlang (&oil_lexer)) {
		return -1;
	}
	if (mwsync_init (&oil_parser)) {
		return -1;
	}

	nocc_addxmlnamespace ("oil", "http://www.cs.kent.ac.uk/projects/ofa/nocc/NAMESPACES/oil");

	return 0;
}
/*}}}*/
/*{{{  int oil_unregister_frontend (void)*/
/*
 *	unregisters the oil lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int oil_unregister_frontend (void)
{
	mwsync_shutdown ();
	if (lexer_unregisterlang (&oil_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/

