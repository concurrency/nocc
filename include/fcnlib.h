/*
 *	fcnlib.h -- function library for NOCC
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

#ifndef __FCNLIB_H
#define __FCNLIB_H


extern int fcnlib_init (void);
extern int fcnlib_shutdown (void);


extern int fcnlib_addfcn (const char *name, void *addr, int ret, int nargs);
extern int fcnlib_havefunction (const char *name);
extern void *fcnlib_findfunction (const char *name);
extern void *fcnlib_findfunction2 (const char *name, const int ret, const int nargs);
extern void *fcnlib_findfunction3 (const char *name, int *n_ret, int *n_nargs);


#endif	/* !__FCNLIB_H */

