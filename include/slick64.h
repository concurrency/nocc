/*
 *	slick64.h -- top-level interface to the x86-64 target and "slick" scheduler
 *	Copyright (C) 2016 Fred Barnes, University of Kent <frmb@kent.ac.uk>
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

#ifndef __SLICK64_H
#define __SLICK64_H

extern int slick64_init (void);
extern int slick64_shutdown (void);

struct TAG_tnode;
struct TAG_map;
struct TAG_target;
struct TAG_srclocn;
struct TAG_name;
struct TAG_fhandle;
struct TAG_chook;

extern struct TAG_chook *slick64_parinfochook;

typedef enum ENUM_slick64_apicall {
	NOAPI = 0,
	CHAN_IN = 1,
	CHAN_OUT = 2,
	RUNP = 3,
	STOPP = 4,
	STARTP = 5,
	ENDP = 6
} slick64_apicall_e;

#define SLICK64_APICALL_LAST ENDP

#endif	/* !__SLICK64_H */

