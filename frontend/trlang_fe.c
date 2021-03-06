/*
 *	trlang_fe.c -- tree-rewriting language front-end
 *	Copyright (C) 2006-2016 Fred Barnes <frmb@kent.ac.uk>
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

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parsepriv.h"
#include "occampi.h"
#include "opts.h"
#include "occampi_fe.h"
#include "trlang.h"
#include "trlang_fe.h"


/*}}}*/


/*{{{  int trlang_register_frontend (void)*/
/*
 *	registers the trlang lexer
 *	returns 0 on success, non-zero on failure
 */
int trlang_register_frontend (void)
{
	trlang_lexer.parser = &trlang_parser;
	trlang_parser.lexer = &trlang_lexer;

	if (lexer_registerlang (&trlang_lexer)) {
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  int trlang_unregister_frontend (void)*/
/*
 *	unregisters the trlang lexer
 *	returns 0 on success, non-zero on failure
 */
int trlang_unregister_frontend (void)
{
	if (lexer_unregisterlang (&trlang_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/


