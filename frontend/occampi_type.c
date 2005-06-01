/*
 *	occampi_type.c -- occam-pi type handling for nocc
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
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "occampi.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
/*}}}*/


/*{{{  static tnode_t *occampi_type_gettype (tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a type-node (typically the sub-type)
 */
static tnode_t *occampi_type_gettype (tnode_t *node, tnode_t *default_type)
{
	tnode_t *type;

	type = tnode_nthsubof (node, 0);
	if (!type) {
		nocc_internal ("occampi_type_gettype(): no subtype ?");
		return NULL;
	}
	return type;
}
/*}}}*/
/*{{{  static tnode_t *occampi_type_typeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type compatibility on a type-node, returns the actual type used by the operation
 */
static tnode_t *occampi_type_typeactual (tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
	tnode_t *atype;

	if ((formaltype->tag == opi.tag_CHAN) && ((node->tag == opi.tag_INPUT) || (node->tag == opi.tag_OUTPUT))) {
		/* becomes a protocol-check in effect */
		atype = tnode_nthsubof (formaltype, 0);

#if 0
fprintf (stderr, "occampi_type_typeactual(): channel: node->tag = [%s]\n", node->tag->name);
#endif
		atype = typecheck_typeactual (atype, actualtype, node, tc);
	} else if (formaltype->tag == opi.tag_CHAN) {
		/* must be two channels then */
		if (actualtype->tag != opi.tag_CHAN) {
			typecheck_error (node, tc, "expected channel, found [%s]", actualtype->tag->name);
		}
		atype = actualtype;

		typecheck_typeactual (tnode_nthsubof (formaltype, 0), tnode_nthsubof (actualtype, 0), node, tc);
	} else {
		nocc_fatal ("occampi_type_typeactual(): don't know how to handle a non-channel here (yet)");
		atype = NULL;
	}

	return atype;
}
/*}}}*/
/*{{{  static int occampi_type_bytesfor (tnode_t *t)*/
/*
 *	returns the number of bytes required by this type (or -1 if not known)
 */
static int occampi_type_bytesfor (tnode_t *t)
{
	return -1;
}
/*}}}*/


/*{{{  static int occampi_leaftype_bytesfor (tnode_t *t)*/
/*
 *	returns the number of bytes required by a basic type
 */
static int occampi_leaftype_bytesfor (tnode_t *t)
{
	if (t->tag == opi.tag_BYTE) {
		return 1;
	} else if (t->tag == opi.tag_INT) {
		return 4;
	}
	return -1;
}
/*}}}*/


/*{{{  int occampi_type_nodes_init (void)*/
/*
 *	initialises type nodes for occam-pi
 *	return 0 on success, non-zero on error
 */
int occampi_type_nodes_init (void)
{
	int i;
	tndef_t *tnd;
	compops_t *cops;

	/*{{{  occampi:typenode -- CHAN*/
	i = -1;
	tnd = opi.node_TYPENODE = tnode_newnodetype ("occampi:typenode", &i, 1, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->gettype = occampi_type_gettype;
	cops->typeactual = occampi_type_typeactual;
	cops->bytesfor = occampi_type_bytesfor;
	tnd->ops = cops;

	i = -1;
	opi.tag_CHAN = tnode_newnodetag ("CHAN", &i, tnd, NTF_NONE);
	/*}}}*/
	/*{{{  occampi:leaftype -- INT, BYTE*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:leaftype", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	cops->bytesfor = occampi_leaftype_bytesfor;
	tnd->ops = cops;

	i = -1;
	opi.tag_INT = tnode_newnodetag ("INT", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_BYTE = tnode_newnodetag ("BYTE", &i, tnd, NTF_NONE);
	/*}}}*/

	return 0;
}
/*}}}*/

