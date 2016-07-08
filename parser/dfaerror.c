/*
 *	dfaerror.c -- DFA error handling helper routines
 *	Copyright (C) 2007-2016 Fred Barnes <frmb@kent.ac.uk>
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
#include "parsepriv.h"
#include "names.h"
#include "dfa.h"
#include "dfaerror.h"

/*}}}*/


/*{{{  static dfaerrorhandler_t *dfaerr_newerrorhandler (void)*/
/*
 *	creates a new dfaerrorhandler_t structure
 */
static dfaerrorhandler_t *dfaerr_newerrorhandler (void)
{
	dfaerrorhandler_t *ehan = (dfaerrorhandler_t *)smalloc (sizeof (dfaerrorhandler_t));

	ehan->inmsg = NULL;
	ehan->stuck = NULL;
	ehan->report = DFAERR_NONE;

	return ehan;
}
/*}}}*/
/*{{{  static void dfaerr_freeerrorhandler (dfaerrorhandler_t *ehan)*/
/*
 *	destroys a dfaerrorhandler_t structure
 */
static void dfaerr_freeerrorhandler (dfaerrorhandler_t *ehan)
{
	if (!ehan) {
		nocc_warning ("dfaerr_freeerrorhandler(): NULL pointer!");
		return;
	}
	if (ehan->inmsg) {
		sfree (ehan->inmsg);
		ehan->inmsg = NULL;
	}
	sfree (ehan);
	return;
}
/*}}}*/


/*{{{  static int dfaerr_stuck (dfaerrorhandler_t *ehan, dfanode_t *dfanode, token_t *tok)*/
/*
 *	default handler for 'stuck-in-DFA' parse errors.  invokes parser_error() to handle it
 */
static void dfaerr_stuck (dfaerrorhandler_t *ehan, dfanode_t *dfanode, token_t *tok)
{
	char *msgbuf = (char *)smalloc (2048);
	int gone = 0;
	int max = 2047;

	gone += snprintf (msgbuf + gone, max - gone, "parse error");
	if (tok) {
		gone += snprintf (msgbuf + gone, max - gone, " at %s", lexer_stokenstr (tok));
	}
	if (ehan->inmsg) {
		gone += snprintf (msgbuf + gone, max - gone, " %s", ehan->inmsg);
	}
	if (ehan->report & DFAERR_EXPECTED) {
		if (DA_CUR (dfanode->match)) {
			int n;

			gone += snprintf (msgbuf + gone, max - gone, ", expected ");
			for (n=0; n<DA_CUR (dfanode->match); n++) {
				token_t *match = DA_NTHITEM (dfanode->match, n);

				gone += snprintf (msgbuf + gone, max - gone, "%s%s", !n ? "" : ((n == DA_CUR (dfanode->match) - 1) ? " or " : ", "), lexer_stokenstr (match));
			}
		} else {
			gone += snprintf (msgbuf + gone, max - gone, ", expected nothing!");
		}
	}
	if (ehan->report & DFAERR_CODE) {
		char *lbuf = NULL;

		if (lexer_getcodeline (tok->origin, &lbuf)) {
			/* skip */
		} else {
			parser_error (SLOCN (tok->origin), lbuf);
			sfree (lbuf);
		}
	}
	parser_error (SLOCN (tok->origin), msgbuf);
	return;
}
/*}}}*/


/*{{{  dfaerrorhandler_t *dfaerror_newhandler (void)*/
/*
 *	called to create a new, blank, dfaerrorhandler_t structure
 *	returns structure on success, NULL on failure
 */
dfaerrorhandler_t *dfaerror_newhandler (void)
{
	return dfaerr_newerrorhandler ();
}
/*}}}*/
/*{{{  dfaerrorhandler_t *dfaerror_newhandler_stuckfcn (void (*fcn)(dfaerrorhandler_t *, dfanode_t *, token_t *))*/
/*
 *	called to create a new dfaerrorhandler_t structure with its 'stuck' function set
 *	returns structure on success, NULL on failure
 */
dfaerrorhandler_t *dfaerror_newhandler_stuckfcn (void (*fcn)(dfaerrorhandler_t *, dfanode_t *, token_t *))
{
	dfaerrorhandler_t *ehan = dfaerr_newerrorhandler ();

	ehan->stuck = fcn;

	return ehan;
}
/*}}}*/
/*{{{  void dfaerror_freehandler (dfaerrorhandler_t *ehan)*/
/*
 *	called to destroy a dfaerrorhandler_t structure
 */
void dfaerror_freehandler (dfaerrorhandler_t *ehan)
{
	if (ehan) {
		dfaerr_freeerrorhandler (ehan);
	}
	return;
}
/*}}}*/


/*{{{  dfaerrorsource_e dfaerror_decodesource (const char *str)*/
/*
 *	de-strings a DFA error source
 *	returns DFAERRSRC_.. constant
 */
dfaerrorsource_e dfaerror_decodesource (const char *str)
{
	if (!str) {
		return DFAERRSRC_INVALID;
	}
	if (!strcmp (str, "STUCK")) {
		return DFAERRSRC_STUCK;
	}
	return DFAERRSRC_INVALID;
}
/*}}}*/
/*{{{  dfaerrorreport_e dfaerror_decodereport (const char *str)*/
/*
 *	de-strings DFA error report flags, expects comma-separated stuff
 *	returns DFAERR_.. bitfields
 */
dfaerrorreport_e dfaerror_decodereport (const char *str)
{
	char *local;
	char **bits;
	int i;
	dfaerrorreport_e ee = DFAERR_NONE;
	
	if (!str) {
		return DFAERR_INVALID;
	}

	local = string_dup (str);
	bits = split_string2 (local, ',', ' ');

	for (i=0; bits[i]; i++) {
		if (!strcmp (bits[i], "NONE")) {
			ee = DFAERR_NONE;
		} else if (!strcmp (bits[i], "EXPECTED")) {
			ee |= DFAERR_EXPECTED;
		} else if (!strcmp (bits[i], "CODE")) {
			ee |= DFAERR_CODE;
		} else {
			ee = DFAERR_INVALID;
			break;			/* for() */
		}
	}

	sfree (bits);
	sfree (local);

	return ee;
}
/*}}}*/


/*{{{  int dfaerror_defaulthandler (const char *dfarule, const char *msg, dfaerrorsource_e src, dfaerrorreport_e rep)*/
/*
 *	sets up a default handler for DFA errors and attaches to the specified 'dfarule'
 *	returns 0 on success, non-zero on failure
 */
int dfaerror_defaulthandler (const char *dfarule, const char *msg, dfaerrorsource_e src, dfaerrorreport_e rep)
{
	dfaerrorhandler_t *ehan;

	if (!dfa_lookupbyname ((char *)dfarule)) {
		nocc_error ("dfaerror_defaulthandler(): cannot set handler for non-existant DFA rule [%s]!", dfarule);
		return -1;
	}

	ehan = dfa_geterrorhandler ((char *)dfarule);
	if (!ehan) {
		/* nothing yet */
		ehan = dfaerror_newhandler ();
		dfa_seterrorhandler ((char *)dfarule, ehan);
	}

	switch (src) {
	case DFAERRSRC_INVALID:
		break;
	case DFAERRSRC_STUCK:
		if (ehan->stuck && (ehan->stuck != dfaerr_stuck)) {
			nocc_warning ("dfaerror_defaulthandler(): already got handler for DFA stuck, rule [%s]", dfarule);
		} else {
			ehan->stuck = dfaerr_stuck;
		}
		break;
	}

	ehan->report = rep;
	if (msg) {
		if (ehan->inmsg && strcmp (ehan->inmsg, msg)) {
			nocc_warning ("dfaerror_defaulthandler(): already got message for DFA error, rule [%s]", dfarule);
		} else if (!ehan->inmsg) {
			ehan->inmsg = string_dup (msg);
		}
	}

	return 0;
}
/*}}}*/


/*{{{  int dfaerror_init (void)*/
/*
 *	initialises DFA error handling routines
 *	returns 0 on success, non-zero on failure
 */
int dfaerror_init (void)
{
	return 0;
}
/*}}}*/
/*{{{  int dfaerror_shutdown (void)*/
/*
 *	shuts-down DFA error handling routines
 *	returns 0 on success, non-zero on failure
 */
int dfaerror_shutdown (void)
{
	return 0;
}
/*}}}*/


