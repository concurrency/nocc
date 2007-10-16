/*
 *	traceslang_fe.c -- traces language front-end
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
#include "version.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parsepriv.h"
#include "occampi.h"
#include "opts.h"
#include "occampi_fe.h"
#include "traceslang.h"
#include "traceslang_fe.h"


/*}}}*/


/*{{{  int traceslang_register_frontend (void)*/
/*
 *	registers the traceslang lexer
 *	returns 0 on success, non-zero on failure
 */
int traceslang_register_frontend (void)
{
	traceslang_lexer.parser = &traceslang_parser;
	traceslang_parser.lexer = &traceslang_lexer;

	if (lexer_registerlang (&traceslang_lexer)) {
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  int traceslang_unregister_frontend (void)*/
/*
 *	unregisters the traceslang lexer
 *	returns 0 on success, non-zero on failure
 */
int traceslang_unregister_frontend (void)
{
	if (lexer_unregisterlang (&traceslang_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/


