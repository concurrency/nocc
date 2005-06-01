/*
 *	xml.c -- XML handling functions for nocc
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

/* note: only libexpat is supported at the moment */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

#include <expat.h>


#include "nocc.h"
#include "support.h"
#include "extn.h"
#include "xml.h"

/* private state */
static xmlhandler_t *curhandler;


/*{{{  void xml_init (void)*/
/*
 *	initialises the XML parser
 */
void xml_init (void)
{
	curhandler = NULL;
	return;
}
/*}}}*/
/*{{{  xmlhandler_t *xml_new_handler (void)*/
/*
 *	creates a blank handler structure
 */
xmlhandler_t *xml_new_handler (void)
{
	xmlhandler_t *xh = (xmlhandler_t *)smalloc (sizeof (xmlhandler_t));

	xh->hook = NULL;
	xh->init = NULL;
	xh->elem_start = NULL;
	xh->elem_end = NULL;
	xh->comment = NULL;
	xh->data = NULL;

	return xh;
}
/*}}}*/
/*{{{  void xml_del_handler (xmlhandler_t *xh)*/
/*
 *	frees a handler structure
 */
void xml_del_handler (xmlhandler_t *xh)
{
	if (!xh) {
		return;
	}
	sfree (xh);
	return;
}
/*}}}*/


#ifdef USE_LIBEXPAT
/* private state for the parser */
typedef struct {
	XML_Parser parser;
	int errored;
	DYNARRAY (xmlkey_t *, nodestack);
	int nsdepth;
	char *filename;
	int flen, fd;
	xmlhandler_t *prevhandler;
} xmlstate_t;


/*{{{  static void xml_element_begin (void *data, const XML_Char *name, const XML_Char **attr)*/
/*
 *	called by the XML parser when a new element is encountered
 */
static void xml_element_begin (void *data, const XML_Char *name, const XML_Char **attr)
{
	xmlstate_t *xmls = (xmlstate_t *)(curhandler->hook);
	xmlkey_t *xk;
	int i, j;

	xk = xmlkeys_lookup (name);
	if (!xk) {
		nocc_warning ("unknown XML element %s at %s:%d", name, xmls->filename, XML_GetCurrentLineNumber (xmls->parser));
		xmls->errored = 1;
	} else {
		xmlkey_t *attrkeys[XML_MAX_ATTRS];
		char *attrvals[XML_MAX_ATTRS];

		/* add to our own stack and call specific processing */
		xmls->nsdepth++;
		dynarray_add (xmls->nodestack, xk);
		if (DA_NTHITEM (xmls->nodestack, xmls->nsdepth) != xk) {
			nocc_internal ("XML nodestack error on element %s at %s:%d", name, xmls->filename, XML_GetCurrentLineNumber (xmls->parser));
			xmls->errored = 1;
		}
		/* search for attributes */
		j = 0;
		for (i=0; attr[i]; i+=2) {
			attrkeys[j] = xmlkeys_lookup (attr[i]);
			if (!attrkeys[j]) {
				nocc_warning ("unknown XML attribute %s at %s:%d", attr[i], xmls->filename, XML_GetCurrentLineNumber (xmls->parser));
				xmls->errored = 1;
			} else {
				attrvals[j] = (char *)(attr[i+1]);
				j++;
			}
		}
		attrkeys[j] = NULL;
		attrvals[j] = NULL;
		if (curhandler->elem_start) {
			curhandler->elem_start (curhandler, data, xk, attrkeys, (const char **)attrvals);
		}
	}
	return;
}
/*}}}*/
/*{{{  static void xml_element_end (void *data, const XML_Char *name)*/
/*
 *	called by the XML parser when an element is ended
 */
static void xml_element_end (void *data, const XML_Char *name)
{
	xmlstate_t *xmls = (xmlstate_t *)(curhandler->hook);
	xmlkey_t *xk;

	xk = xmlkeys_lookup (name);
	if (!xk) {
		xmls->errored = 1;
	} else {
		if (xmls->nsdepth < 0) {
			nocc_warning ("XML stack underflow on %s at %s:%d", name, xmls->filename, XML_GetCurrentLineNumber (xmls->parser));
			xmls->errored = 1;
		} else {
			xmlkey_t *tos = DA_NTHITEM (xmls->nodestack, xmls->nsdepth);

			if (tos != xk) {
				nocc_warning ("expected XML closing tag for %s, found %s at %s:%d", tos->name, name, xmls->filename, XML_GetCurrentLineNumber (xmls->parser));
				xmls->errored = 1;
			} else {
				/* call specific processing */
				if (curhandler->elem_end) {
					curhandler->elem_end (curhandler, data, xk);
				}
				dynarray_delitem (xmls->nodestack, xmls->nsdepth);
				xmls->nsdepth--;
			}
		}
	}
	return;
}
/*}}}*/
/*{{{  static void xml_comment (void *data, const XML_Char *comment)*/
/*
 *	called by the XML parser when a comment is encountered
 */
static void xml_comment (void *data, const XML_Char *comment)
{
	if (curhandler->comment) {
		curhandler->comment (curhandler, data, comment);
	}
	return;
}
/*}}}*/
/*{{{  static void xml_data (void *data, const XML_Char *data, int len)*/
/*
 *	called by the XML parser when data is encountered
 */
static void xml_data (void *data, const XML_Char *text, int len)
{
	int left;
	char *ch;

	/* see if it's all whitespace first */
	for (left = len, ch = (char *)text; len && ((*ch == '\n') || (*ch == '\r') || (*ch == '\t') || (*ch == ' ')); ch++, len--);
	if (len && curhandler->data) {
		curhandler->data (curhandler, data, text, len);
	}
	return;
}
/*}}}*/
/*{{{  static int xml_parse_buffer (xmlhandler_t *xh, const char *buffer, int buflen)*/
/*
 *	parses an XML file in a buffer using the given handler.
 *	return 0 on success, non-zero on failure.
 */
static int xml_parse_buffer (xmlhandler_t *xh, const char *buffer, int buflen)
{
	xmlstate_t *xmls = (xmlstate_t *)(xh->hook);
	int pres;

	if (!xmls || !buffer || !buflen) {
		nocc_internal ("xml_parse_buffer(): invalid params");
		xmls->errored = 1;
		return -1;
	}
	xmls->parser = XML_ParserCreate (NULL);
	if (!xmls->parser) {
		nocc_warning ("failed to create XML parser instance");
		xmls->errored = 1;
		return -1;
	}

	XML_SetElementHandler (xmls->parser, xml_element_begin, xml_element_end);
	XML_SetCommentHandler (xmls->parser, xml_comment);
	XML_SetCharacterDataHandler (xmls->parser, xml_data);

	xmls->nsdepth = -1;
	if (curhandler) {
		xmls->prevhandler = curhandler;
	} else {
		xmls->prevhandler = NULL;
	}
	curhandler = xh;
	if (curhandler->init) {
		/* call initialiser if present */
		curhandler->init (curhandler);
	}
	pres = XML_Parse (xmls->parser, buffer, buflen, 1);
	curhandler = xmls->prevhandler;

	if (pres == XML_STATUS_ERROR) {
		nocc_warning ("XML parse error at %s:%d: %s", xmls->filename, XML_GetCurrentLineNumber (xmls->parser),
				XML_ErrorString (XML_GetErrorCode (xmls->parser)));
		xmls->errored = 1;
	}

	XML_ParserFree (xmls->parser);
	xmls->parser = NULL;

	return (xmls->errored ? -1 : 0);
}
/*}}}*/
/*{{{  int xml_parse_file (xmlhandler_t *xh, const char *fname)*/
/*
 *	parses an XML file using the given handler.
 *	returns 0 on success, non-zero on failure.
 */
int xml_parse_file (xmlhandler_t *xh, const char *fname)
{
	xmlstate_t *xmls = (xmlstate_t *)smalloc (sizeof (xmlstate_t));
	struct stat stbuf;
	char *buffer;
	int result;

	xmls->parser = NULL;
	xmls->errored = 0;
	dynarray_init (xmls->nodestack);
	xmls->nsdepth = 0;
	xmls->filename = string_dup ((char *)fname);
	xmls->flen = 0;
	xmls->fd = -1;

	if (stat (xmls->filename, &stbuf) < 0) {
		nocc_warning ("unable to stat %s: %s", xmls->filename, strerror (errno));
		goto out_error;
	}
	if (!S_ISREG (stbuf.st_mode)) {
		nocc_warning ("non-regular file %s", xmls->filename);
	}
	if (stbuf.st_size == 0) {
		nocc_warning ("empty file %s", xmls->filename);
		goto out_error;
	}
	xmls->flen = stbuf.st_size;
	xmls->fd = open (xmls->filename, O_RDONLY);
	if (xmls->fd < 0) {
		nocc_warning ("unable to open %s for reading: %s", xmls->filename, strerror (errno));
		goto out_error;
	}
	buffer = (char *)mmap ((void *)0, xmls->flen, PROT_READ, MAP_SHARED, xmls->fd, 0);
	if (buffer == ((char *)-1)) {
		nocc_warning ("unable to map %s: %s", xmls->filename, strerror (errno));
		goto out_error2;
	}

	xh->hook = (void *)xmls;
	result = xml_parse_buffer (xh, buffer, xmls->flen);

	if (!result && xmls->errored) {
		result = -1;
	}
	munmap ((void *)buffer, xmls->flen);
	close (xmls->fd);
	sfree (xmls->filename);
	dynarray_trash (xmls->nodestack);
	sfree (xmls);

	return result;
out_error2:
	close (xmls->fd);
out_error:
	sfree (xmls->filename);
	dynarray_trash (xmls->nodestack);
	sfree (xmls);
	return -1;
}
/*}}}*/


#else	/* !USE_LIBEXPAT */
#warning stand-along XML support unsupported!



#endif	/* !USE_LIBEXPAT */

