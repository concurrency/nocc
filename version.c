/*
 *	version.c -- version information for nocc
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

#include "version.h"

/*{{{  version strings*/
const char *versionstr = "nocc " VERSION;
const char *versionstrl = "nocc " VERSION " on " HOST_CPU "-" HOST_VENDOR "-" HOST_OS " targetting " TARGET_CPU "-" TARGET_VENDOR "-" TARGET_OS;

/*}}}*/
/*{{{  const char *version_string (void)*/
/*
 *	returns the version-string
 */
const char *version_string (void)
{
	return versionstr;
}
/*}}}*/
/*{{{  const char *version_string_long (void)*/
/*
 *	returns the long version-string
 */
const char *version_string_long (void)
{
	return versionstrl;
}
/*}}}*/
