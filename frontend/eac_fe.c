/*
 *	eac_fe.c -- escape analysis code front-end (initialisation/registration)
 *	Copyright (C) 2011 Fred Barnes <frmb@kent.ac.uk>
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
#include "eac.h"
#include "opts.h"
#include "eac_fe.h"

/*}}}*/


/*{{{  int eac_register_frontend (void)*/
/*
 *	registers the EAC lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int eac_register_frontend (void)
{
	eac_lexer.parser = &eac_parser;
	eac_parser.lexer = &eac_lexer;

	if (lexer_registerlang (&eac_lexer)) {
		return -1;
	}
	
	nocc_addxmlnamespace ("eac", "http://www.cs.kent.ac.uk/projects/ofa/nocc/NAMESPACES/eac");

	return 0;
}
/*}}}*/
/*{{{  int eac_unregister_frontend (void)*/
/*
 *	unregisters the EAC lexer and parser
 *	returns 0 on success, non-zero on failure
 */
int eac_unregister_frontend (void)
{
	if (lexer_unregisterlang (&eac_lexer)) {
		return -1;
	}
	return 0;
}
/*}}}*/

