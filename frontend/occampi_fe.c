/*
 *	occampi_fe.c -- occam-pi front-end (initialisation/registration)
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
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "lexer.h"
#include "lexpriv.h"
#include "parsepriv.h"
#include "occampi.h"
#include "occampi_fe.h"

/*}}}*/


/*{{{  int occampi_register_frontend (void)*/
/*
 *	registers the occam-pi lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int occampi_register_frontend (void)
{
	occampi_lexer.parser = &occampi_parser;
	occampi_parser.lexer = &occampi_lexer;

	if (lexer_registerlang (&occampi_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  int occampi_unregister_frontend (void)*/
/*
 *	unregisters the occam-pi lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int occampi_unregister_frontend (void)
{
	if (lexer_unregisterlang (&occampi_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/



