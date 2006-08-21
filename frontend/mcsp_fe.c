/*
 *	mcsp_fe.c -- MCSP front-end (initialisation/registration)
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
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "version.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parsepriv.h"
#include "mcsp.h"
#include "opts.h"
#include "mcsp_fe.h"
#include "mwsync.h"

/*}}}*/


/*{{{  int mcsp_register_frontend (void)*/
/*
 *	registers the MCSP lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int mcsp_register_frontend (void)
{
	mcsp_lexer.parser = &mcsp_parser;
	mcsp_parser.lexer = &mcsp_lexer;

	if (lexer_registerlang (&mcsp_lexer)) {
		return -1;
	}

	opts_add ("unbound-events", '\0', mcsp_lexer_opthandler_flag, (void *)1, "1permit the use of unbounded events in MCSP");

	if (mwsync_init (1)) {
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  int mcsp_unregister_frontend (void)*/
/*
 *	unregisters the MCSP lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int mcsp_unregister_frontend (void)
{
	mwsync_shutdown ();

	if (lexer_unregisterlang (&mcsp_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/



