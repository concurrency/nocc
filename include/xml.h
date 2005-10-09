/*
 *	xml.h -- XML parser interface
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

#ifndef __XML_H
#define __XML_H

struct TAG_xmlkey;

#define XML_MAX_ATTRS	8

typedef struct TAG_xmlhandler {
	void *hook;
	void (*init)(struct TAG_xmlhandler *);
	void (*final)(struct TAG_xmlhandler *);
	void (*elem_start)(struct TAG_xmlhandler *, void *, struct TAG_xmlkey *,
				struct TAG_xmlkey **, const char **);
	void (*elem_end)(struct TAG_xmlhandler *, void *, struct TAG_xmlkey *);
	void (*comment)(struct TAG_xmlhandler *, void *, const char *);
	void (*data)(struct TAG_xmlhandler *, void *, const char *, int len);
	void *uhook;
} xmlhandler_t;

extern void xml_init (void);
extern xmlhandler_t *xml_new_handler (void);
extern void xml_del_handler (xmlhandler_t *xh);
extern int xml_parse_file (xmlhandler_t *xh, const char *fname);

/* below is largely for misc/xmlkeys.c */

typedef enum {
	XMLKEY_INVALID,
	XMLKEY_NOCC,
	XMLKEY_TARGET,
	XMLKEY_EPATH,
	XMLKEY_IPATH,
	XMLKEY_LPATH,
	XMLKEY_EXTENSION,
	XMLKEY_DESC,
	XMLKEY_AUTHOR,
	XMLKEY_MAINTAINER,
	XMLKEY_COMMENT,
	XMLKEY_LEX,
	XMLKEY_KEYWORD,
	XMLKEY_SYMBOL,
	XMLKEY_NAME,
	XMLKEY_TAG,
	XMLKEY_PARSER,
	XMLKEY_NODE,
	XMLKEY_MATCH,
	XMLKEY_CHECK,
	XMLKEY_TYPE,
	XMLKEY_OP,
	XMLKEY_TYPEOF,
	XMLKEY_ERROR,
	XMLKEY_TREE,
	XMLKEY_NEWNAME,
	XMLKEY_DECL,
	XMLKEY_CONSTR,
	XMLKEY_ACTION,
	XMLKEY_LIBRARY,
	XMLKEY_NATIVELIB,
	XMLKEY_SRCINCLUDE,
	XMLKEY_SRCUSE,
	XMLKEY_LIBUNIT,
	XMLKEY_PROC,
	XMLKEY_DESCRIPTOR,
	XMLKEY_BLOCKINFO,
	XMLKEY_NAMESPACE,
	XMLKEY_PATH,
	XMLKEY_LANGUAGE,
	XMLKEY_VALUE,
	XMLKEY_ALLOCWS,
	XMLKEY_ALLOCVS,
	XMLKEY_ALLOCMS,
	XMLKEY_ADJUST
} xmlkeytype_t;

typedef struct TAG_xmlkey {
	char *name;
	xmlkeytype_t type;
} xmlkey_t;

extern void xmlkeys_init (void);
extern xmlkey_t *xmlkeys_lookup (const char *keyname);


#endif	/* !__XML_H */

