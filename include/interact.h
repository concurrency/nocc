/*
 *	interact.h -- interactive things for NOCC
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

#ifndef __INTERACT_H
#define __INTERACT_H

/*
 * Note: functions are defined in nocc.c at the top-level.
 */

typedef enum ENUM_ihandlerflags {
	IHF_NONE = 0x0000,			/* nothing */
	IHF_LINE = 0x0001,			/* use line_callback */
	IHF_BITS = 0x0002			/* use bits_callback */
} ihandlerflags_t;

typedef struct TAG_ihandler {
	char *id;				/* identifier */
	char *prompt;				/* prompt addition in mode */
	ihandlerflags_t flags;			/* handling flags */
	int enabled;				/* dynamic switch for on/off */
	int (*line_callback)(char *);		/* callback for line handling */
	int (*bits_callback)(char **, int);	/* callback for line handling (already in bits) */
} ihandler_t;

/* return values for callback handlers */
#define IHR_HANDLED	0			/* all handled, next line please */
#define IHR_PHANDLED	1			/* partially handled, try some more */
#define IHR_UNHANDLED	2			/* unhandled, pass on */

extern ihandler_t *nocc_newihandler (void);
extern void nocc_freeihandler (ihandler_t *);

extern int nocc_register_ihandler (ihandler_t *);
extern int nocc_unregister_ihandler (ihandler_t *);


#endif	/* !__INTERACT_H */

