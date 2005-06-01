/*
 *	xmlkeys.c -- XML keyword processing
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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
#include "xml.h"

#include "gperf_xmlkeys.h"

/*{{{  void xmlkeys_init (void)*/
/*
 *	initialises the xml keyword processing
 */
void xmlkeys_init (void)
{
	return;
}
/*}}}*/
/*{{{  xmlkey_t *xmlkeys_lookup (const char *keyname)*/
/*
 *	looks up an XML keyword
 */
xmlkey_t *xmlkeys_lookup (const char *keyname)
{
	int keylen = strlen (keyname);
	xmlkey_t *xk;

	xk = (xmlkey_t *)xmlkeys_lookup_byname (keyname, keylen);
	return xk;
}
/*}}}*/

