/*
 *	dfaerror.h -- DFA error helper interface
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

#ifndef __DFAERROR_H
#define __DFAERROR_H

struct TAG_dfanode;
struct TAG_token;


typedef enum ENUM_dfaerrorreport {	/* bitfields */
	DFAERR_NONE = 0x0000,			/* no extra messages */
	DFAERR_INVALID = 0x0001,		/* invalid (unique) */
	DFAERR_EXPECTED = 0x0002,		/* say what we could have parsed here (expected) */
	DFAERR_CODE = 0x0004			/* dump the offending line of source code */
} dfaerrorreport_e;

typedef enum ENUM_dfaerrorsource {
	DFAERRSRC_INVALID = 0,
	DFAERRSRC_STUCK = 1			/* got stuck in the DFA somewhere */
} dfaerrorsource_e;

typedef struct TAG_dfaerrorhandler {
	char *inmsg;				/* "in blah" type string */
	void (*stuck)(struct TAG_dfaerrorhandler *, struct TAG_dfanode *, struct TAG_token *);
	dfaerrorreport_e report;
} dfaerrorhandler_t;


extern int dfaerror_init (void);
extern int dfaerror_shutdown (void);

extern dfaerrorhandler_t *dfaerror_newhandler (void);
extern dfaerrorhandler_t *dfaerror_newhandler_stuckfcn (void (*fcn)(struct TAG_dfaerrorhandler *, struct TAG_dfanode *, struct TAG_token *));
extern void dfaerror_freehandler (dfaerrorhandler_t *ehan);

extern dfaerrorsource_e dfaerror_decodesource (const char *str);
extern dfaerrorreport_e dfaerror_decodereport (const char *str);

extern int dfaerror_defaulthandler (const char *dfarule, const char *msg, dfaerrorsource_e src, dfaerrorreport_e rep);


#endif	/* !__DFAERROR_H */

