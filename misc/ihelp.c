/*
 *	ihelp.c -- interactive help for NOCC
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
#include "ihelp.h"
#include "xml.h"


/*}}}*/
/*{{{  private data*/

STATICDYNARRAY (ihelpset_t *, helpsets);

static ihelpset_t *parser_helpset = NULL;
static ihelpentry_t *parser_helpentry = NULL;

/*}}}*/


/*{{{  static ihelpentry_t *ihelp_newhelpentry (void)*/
/*
 *	creates a new ihelpentry_t structure
 */
static ihelpentry_t *ihelp_newhelpentry (void)
{
	ihelpentry_t *ihe = (ihelpentry_t *)smalloc (sizeof (ihelpentry_t));

	ihe->id = NULL;
	ihe->text = NULL;

	return ihe;
}
/*}}}*/
/*{{{  static ihelpset_t *ihelp_newhelpset (void)*/
/*
 *	creates a new ihelpset_t structure
 */
static ihelpset_t *ihelp_newhelpset (void)
{
	ihelpset_t *ihs = (ihelpset_t *)smalloc (sizeof (ihelpset_t));

	ihs->lang = NULL;
	ihs->tag = NULL;
	stringhash_init (ihs->entries, IHELPSET_BITSIZE);

	return ihs;
}
/*}}}*/
/*{{{  static void ihelp_freehelpentry (ihelpentry_t *ihe)*/
/*
 *	frees an ihelpentry_t structure
 */
static void ihelp_freehelpentry (ihelpentry_t *ihe)
{
	if (ihe->id) {
		sfree (ihe->id);
		ihe->id = NULL;
	}
	if (ihe->text) {
		sfree (ihe->text);
		ihe->text = NULL;
	}
	sfree (ihe);
	return;
}
/*}}}*/
/*{{{  static void ihelp_freehelpset_entry (ihelpentry_t *entry, char *key, void *ptr)*/
/*
 *	frees helpset entries (stringhash walk)
 */
static void ihelp_freehelpset_entry (ihelpentry_t *entry, char *key, void *ptr)
{
	if (entry) {
		ihelp_freehelpentry (entry);
	}
	return;
}
/*}}}*/
/*{{{  static void ihelp_freehelpset (ihelpset_t *ihs)*/
/*
 *	frees an ihelpset_t structure (and any lingering entries)
 */
static void ihelp_freehelpset (ihelpset_t *ihs)
{
	if (ihs->lang) {
		sfree (ihs->lang);
		ihs->lang = NULL;
	}
	if (ihs->tag) {
		sfree (ihs->tag);
		ihs->tag = NULL;
	}
	stringhash_walk (ihs->entries, ihelp_freehelpset_entry, NULL);
	stringhash_trash (ihs->entries);
	sfree (ihs);
	return;
}
/*}}}*/


/*{{{  static void ihelpload_init (xmlhandler_t *xh)*/
/*
 *	XML parsing initialisation callback
 */
static void ihelpload_init (xmlhandler_t *xh)
{
	parser_helpset = NULL;
	parser_helpentry = NULL;
	return;
}
/*}}}*/
/*{{{  static void ihelpload_final (xmlhandler_t *xh)*/
/*
 *	XML parsing shutdown callback
 */
static void ihelpload_final (xmlhandler_t *xh)
{
	if (parser_helpentry) {
		nocc_warning ("in help file: leftover helpentry!");
		ihelp_freehelpentry (parser_helpentry);
		parser_helpentry = NULL;
	}
	if (parser_helpset) {
		nocc_warning ("in help file: leftover helpset!");
		ihelp_freehelpset (parser_helpset);
		parser_helpset = NULL;
	}
	return;
}
/*}}}*/
/*{{{  static void ihelpload_elem_start (xmlhandler_t *xh, void *data, xmlkey_t *key, xmlkey_t **attrkeys, const char **attrvals)*/
/*
 *	XML parsing element start
 */
static void ihelpload_elem_start (xmlhandler_t *xh, void *data, xmlkey_t *key, xmlkey_t **attrkeys, const char **attrvals)
{
	if (key->type == XMLKEY_NOCCHELP) {
		/* top-level, ignorable! */
	} else if (key->type == XMLKEY_HELPSET) {
		int i;
		char *the_lang = NULL;
		char *the_tag = NULL;

		for (i=0; attrkeys[i]; i++) {
			if (attrkeys[i]->type == XMLKEY_LANGUAGE) {
				the_lang = (char *)attrvals[i];
			} else if (attrkeys[i]->type == XMLKEY_TAG) {
				the_tag = (char *)attrvals[i];
			} else {
				nocc_warning ("in help file: unknown key [%s]", attrkeys[i]->name);
			}
		}

		if (parser_helpset) {
			nocc_warning ("in help file: nested <helpset>, ignoring..");
		} else {
			parser_helpset = ihelp_newhelpset ();
			parser_helpset->lang = the_lang ? string_dup (the_lang) : NULL;
			parser_helpset->tag = the_tag ? string_dup (the_tag) : NULL;
		}
	} else if (key->type == XMLKEY_HELP) {
		int i;
		char *the_id = NULL;

		for (i=0; attrkeys[i]; i++) {
			if (attrkeys[i]->type == XMLKEY_COMMAND) {
				the_id = (char *)attrvals[i];
			} else {
				nocc_warning ("in help file: unknown key [%s]", attrkeys[i]->name);
			}
		}

		if (parser_helpentry) {
			nocc_warning ("in help file: nested <help>, ignoring..");
		} else {
			parser_helpentry = ihelp_newhelpentry ();
			parser_helpentry->id = the_id ? string_dup (the_id) : NULL;
		}
	} else {
		nocc_warning ("in help file: unknown tag [%s]", key->name);
	}
}
/*}}}*/
/*{{{  static void ihelpload_elem_end (xmlhandler_t *xh, void *data, xmlkey_t *key)*/
/*
 *	XML parsing element end
 */
static void ihelpload_elem_end (xmlhandler_t *xh, void *data, xmlkey_t *key)
{
	if (key->type == XMLKEY_NOCCHELP) {
		/* top-level, ignorable */
	} else if (key->type == XMLKEY_HELPSET) {
		if (!parser_helpset) {
			/* not here for a reason.. */
		} else {
			dynarray_add (helpsets, parser_helpset);
			parser_helpset = NULL;
		}
	} else if (key->type == XMLKEY_HELP) {
		if (!parser_helpentry) {
			/* not here for a reason.. */
		} else if (!parser_helpset) {
			nocc_warning ("in help file: <help> outside of <helpset>");
			ihelp_freehelpentry (parser_helpentry);
			parser_helpentry = NULL;
		} else {
			/* fixup the text, if present */
			if (!parser_helpentry->text) {
				/* remove this */
				nocc_warning ("in help file: empty help for [%s]", parser_helpentry->id ?: "(null)");
				ihelp_freehelpentry (parser_helpentry);
				parser_helpentry = NULL;
			} else {
				strstrip (parser_helpentry->text);
				stringhash_insert (parser_helpset->entries, parser_helpentry, parser_helpentry->id);
				parser_helpentry = NULL;
			}
		}
	}
}
/*}}}*/
/*{{{  static void ihelpload_data (xmlhandler_t *xh, void *data, const char *text, int len)*/
/*
 *	XML parsing some data
 */
static void ihelpload_data (xmlhandler_t *xh, void *data, const char *text, int len)
{
	if (parser_helpentry) {
		if (!parser_helpentry->text) {
			/* new text */
			parser_helpentry->text = string_ndup (text, len);
		} else {
			/* add text */
			int olen = strlen (parser_helpentry->text);
			char *newtext = (char *)smalloc (olen + len + 1);

			memcpy (newtext, parser_helpentry->text, olen);
			memcpy (newtext + olen, text, len);
			newtext[olen + len] = '\0';
			sfree (parser_helpentry->text);
			parser_helpentry->text = newtext;
		}
	}
}
/*}}}*/


/*{{{  char *ihelp_getentry (const char *lang, const char *tag, const char *entry)*/
/*
 *	looks-up a specific help entry.  If 'lang' or 'tag' is NULL, looks up first matching entry.
 *	returns entry text on success, NULL on failure.
 */
char *ihelp_getentry (const char *lang, const char *tag, const char *entry)
{
	int i;

	if (!DA_CUR (helpsets)) {
		return NULL;
	}
	for (i=0; i<DA_CUR (helpsets); i++) {
		ihelpset_t *hs = DA_NTHITEM (helpsets, i);

		if ((!lang || (hs->lang && !strcmp (hs->lang, lang))) && (!tag || (hs->tag && !strcmp (hs->tag, tag)))) {
			/* matching set */
			ihelpentry_t *hent;

			hent = stringhash_lookup (hs->entries, entry);
			if (hent) {
				return hent->text;
			}
		}
	}
	return NULL;
}
/*}}}*/


/*{{{  int ihelp_init (void)*/
/*
 *	initialises interactive help
 *	returns 0 on success, non-zero on failure
 */
int ihelp_init (void)
{
	char *helpfile = NULL;
	int i;
	xmlhandler_t *xmlh;

	dynarray_init (helpsets);

	/* find a help file! */
	for (i=0; i<DA_CUR (compopts.epath); i++) {
		helpfile = (char *)smalloc (strlen (DA_NTHITEM (compopts.epath, i)) + 32);
		sprintf (helpfile, "%s/help.xml", DA_NTHITEM (compopts.epath, i));

		if (!access (helpfile, R_OK)) {
			/* found a readable one! */
			break;		/* for() */
		}
		sfree (helpfile);
		helpfile = NULL;
	}
	if (!helpfile) {
		/* didn't find one */
		nocc_message ("no help file found, help will be unavailable");
		return -1;
	}

	xmlh = xml_new_handler ();
	xmlh->init = ihelpload_init;
	xmlh->final = ihelpload_final;
	xmlh->elem_start = ihelpload_elem_start;
	xmlh->elem_end = ihelpload_elem_end;
	xmlh->data = ihelpload_data;
	xmlh->ws_data = 1;

	i = xml_parse_file (xmlh, helpfile);
	xml_del_handler (xmlh);

	if (i) {
		nocc_message ("failed to parse %s, help will be unavailable", helpfile);
		sfree (helpfile);
		return -2;
	} else {
		if (compopts.verbose) {
			nocc_message ("loaded help from %s, %d set(s)", helpfile, DA_CUR (helpsets));
		}
	}

	sfree (helpfile);

	return 0;
}
/*}}}*/
/*{{{  */
/*
 *	shuts-down interactive help
 *	returns 0 on success, non-zero on failure
 */
int ihelp_shutdown (void)
{
	return 0;
}
/*}}}*/

