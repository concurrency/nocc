/*
 *	names.c -- name stuff (note: names can exist globally)
 *	Copyright (C) 2004-2013 Fred Barnes <frmb@kent.ac.uk>
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

/*{{{  includes, etc.*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "fhandle.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "names.h"
#include "scope.h"

/*}}}*/
/*{{{  private stuff*/
STATICSTRINGHASH (namelist_t *, names, 7);
STATICDYNARRAY (name_t *, namestack);
STATICSTRINGHASH (namespace_t *, namespaces, 4);

static int tempnamecounter = 1;

/*}}}*/


/*{{{  int name_init (void)*/
/*
 *	initialises the name handling bits
 *	returns 0 on success, non-zero on failure
 */
int name_init (void)
{
	stringhash_sinit (names);
	dynarray_init (namestack);
	stringhash_sinit (namespaces);

	return 0;
}
/*}}}*/
/*{{{  int name_shutdown (void)*/
/*
 *	shuts down name handling bits
 *	returns 0 on success, non-zero on failure
 */
int name_shutdown (void)
{
	return 0;
}
/*}}}*/


/*{{{  name_t *name_lookup (char *str)*/
/*
 *	looks up a name -- returns it at the current scoping level (or last if unset)
 */
name_t *name_lookup (char *str)
{
	namelist_t *nl;
	name_t *name;

	nl = stringhash_lookup (names, str);
#if 0
fprintf (stderr, "name_lookup(): str=[%s], nl=0x%8.8x, nl->curscope = %d, DA_CUR (nl->scopes) = %d\n", str, (unsigned int)nl, (unsigned int)nl->curscope, DA_CUR (nl->scopes));
#endif
	if (!nl) {
		name = NULL;
	} else if ((nl->curscope < 0) && !DA_CUR (nl->scopes)) {
		name = NULL;
	} else if (nl->curscope < 0) {
		name = DA_NTHITEM (nl->scopes, DA_CUR (nl->scopes) - 1);
	} else {
		name = DA_NTHITEM (nl->scopes, nl->curscope);
	}
	return name;
}
/*}}}*/
/*{{{  name_t *name_lookupss (char *str, scope_t *ss)*/
/*
 *	looks up a name -- returns it at the current scoping level (or last if unset)
 *	this one is name-space aware
 */
name_t *name_lookupss (char *str, scope_t *ss)
{
	namelist_t *nl = NULL;
	name_t *name = NULL;
	namespace_t *ns;

#if 0
fprintf (stderr, "name_lookupss(): str=[%s], nl=0x%8.8x, nl->curscope = %d, DA_CUR (nl->scopes) = %d, DA_CUR (ss->usens) = %d\n", str, (unsigned int)nl, (unsigned int)nl->curscope, DA_CUR (nl->scopes), DA_CUR (ss->usens));
#endif
#if 0
fprintf (stderr, "name_lookupss(): here 1! str=[%s] ss = 0x%8.8x, DA_CUR (usens) = %d\n", str, (unsigned int)ss, DA_CUR (ss->usens));
#endif
	/* see if it's namespace-flavoured */
	if ((ns = name_findnamespacepfx (str)) != NULL) {
		/* yes, lookup part missing the namespace -- after checking it's visible */
		int i;

		for (i=0; (i<DA_CUR (ss->usens)) && (ns != DA_NTHITEM (ss->usens, i)); i++);
		if (i == DA_CUR (ss->usens)) {
			nocc_warning ("namespace [%s] is not visible", ns->nspace);		/* shortly followed by a scope error probably */
			/* namespace selected not in use */
		} else {
			nl = stringhash_lookup (names, str + strlen (ns->nspace) + 1);

			if (!nl) {
				/* no such name */
			} else {
				int top = (nl->curscope < 0) ? DA_CUR (nl->scopes) - 1: nl->curscope;

				/* find an in-scope one that matches the namespace given */
				for (i=top; i >= 0; i--) {
					name_t *tname = DA_NTHITEM (nl->scopes, i);

					if (tname && (tname->ns == ns)) {
						/* this one */
						name = tname;
						break;			/* for() */
					}
				}
			}
		}
	}

	if (!name) {
		/* plain, find the first one in scope that has no namespace (i.e. local), failing that, the latest in-use namespace */
		nl = stringhash_lookup (names, str);
		if (!nl) {
			/* no such name */
		} else {
			int top = (nl->curscope < 0) ? DA_CUR (nl->scopes) - 1: nl->curscope;
			int i;

#if 0
fprintf (stderr, "name_lookupss(): found stack for [%s], looking for namespace-less one..  namestack is:\n", str);
name_dumpnames (stderr);
#endif
			/* look for a namespace-less one (local) */
			for (i=top; i >= 0; i--) {
				name_t *tname = DA_NTHITEM (nl->scopes, i);

#if 0
fprintf (stderr, "tname->me->name = [%s], tname->ns->nspace = [%s]\n", tname->me->name, tname->ns ? tname->ns->nspace : "<empty>");
#endif
				if (tname && (!tname->ns || !strlen (tname->ns->nspace))) {
					/* this one */
					name = tname;
					break;			/* for() */
				}
			}

			if (!name) {
#if 0
fprintf (stderr, "name_lookupss(): found stack for [%s], looking for one in a visible namespace.. (top = %d), DA_CUR (ss->usens) = %d\n",
		str, top, DA_CUR (ss->usens));
#endif
				/* look for one in a visible namespace */
				for (i=top; i >= 0; i--) {
					name_t *tname = DA_NTHITEM (nl->scopes, i);
					int j;

					if (!tname || !tname->ns || !strlen (tname->ns->nspace)) {
						continue;		/* for() */
					}
					for (j = (DA_CUR (ss->usens) - 1); (j >= 0) && (tname->ns != DA_NTHITEM (ss->usens, j)); j--);
					if (j >= 0) {
						/* this one */
						name = tname;
						break;			/* for() */
					}
				}
			}
		}
	}

#if 0
	if (!nl) {
		name = NULL;
	} else if ((nl->curscope < 0) && !DA_CUR (nl->scopes)) {
		name = NULL;
	} else if (nl->curscope < 0) {
		name = DA_NTHITEM (nl->scopes, DA_CUR (nl->scopes) - 1);
	} else {
		name = DA_NTHITEM (nl->scopes, nl->curscope);
	}
#endif

	return name;
}
/*}}}*/
/*{{{  name_t *name_addname (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode)*/
/*
 *	adds a name and returns it, not scoped
 */
name_t *name_addname (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode)
{
	name_t *name;
	namelist_t *nl;

#if 0
fprintf (stderr, "name_addname(): here! str=\"%s\"\n", str);
#endif
	name = (name_t *)smalloc (sizeof (name_t));
	name->decl = decl;
	name->type = type;
	name->namenode = namenode;
	name->refc = 1;				/* because it won't ever be looked up really */
	name->ns = NULL;

	nl = stringhash_lookup (names, str);
	if (!nl) {
		nl = (namelist_t *)smalloc (sizeof (namelist_t));
		nl->name = string_dup (str);
		dynarray_init (nl->scopes);
		nl->curscope = -1;
		stringhash_insert (names, nl, nl->name);
	}
	name->me = nl;

	return name;
}
/*}}}*/
/*{{{  name_t *name_addscopenamess (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode, scope_t *ss)*/
/*
 *	adds a name -- and returns it, after putting it in scope.
 *	scope state is used to get hold of namespaces.
 */
name_t *name_addscopenamess (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode, scope_t *ss)
{
	name_t *name;
	namelist_t *nl;

#if 0
fprintf (stderr, "name_addscopenamess(): here! str=\"%s\", default-namespace: \"%s\"\n", str, (ss && DA_CUR (ss->defns)) ? (DA_NTHITEM (ss->defns, DA_CUR (ss->defns) - 1)->nspace) : "(none)");
#endif
	name = (name_t *)smalloc (sizeof (name_t));
	name->decl = decl;
	name->type = type;
	name->namenode = namenode;
	name->refc = 0;
	if (ss && DA_CUR (ss->defns)) {
		name->ns = DA_NTHITEM (ss->defns, DA_CUR (ss->defns) - 1);
	} else {
		name->ns = NULL;
	}
#if 0
fprintf (stderr, "name_addscopename(): adding name [%s] type:\n", str);
tnode_dumptree (type, 1, stderr);
#endif

	nl = stringhash_lookup (names, str);
	if (!nl) {
		nl = (namelist_t *)smalloc (sizeof (namelist_t));
		nl->name = string_dup (str);
		dynarray_init (nl->scopes);
		nl->curscope = -1;
		stringhash_insert (names, nl, nl->name);
	}
	dynarray_add (nl->scopes, name);
	nl->curscope = DA_CUR (nl->scopes) - 1;
	name->me = nl;

	dynarray_add (namestack, name);
	
	return name;
}
/*}}}*/
/*{{{  name_t *name_addscopename (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode)*/
/*
 *	adds a name and returns it, after putting in scope
 */
name_t *name_addscopename (char *str, tnode_t *decl, tnode_t *type, tnode_t *namenode)
{
	return name_addscopenamess (str, decl, type, namenode, NULL);
}
/*}}}*/
/*{{{  name_t *name_addsubscopenamess (char *str, void *scopemark, tnode_t *decl, tnode_t *type, tnode_t *namenode, scope_t *ss)*/
/*
 *	adds a name and returns it, after putting it in scope.
 *	Unlike the others, this inserts the name in the scope stack at the point of the given marker
 */
name_t *name_addsubscopenamess (char *str, void *scopemark, tnode_t *decl, tnode_t *type, tnode_t *namenode, scope_t *ss)
{
	name_t *name;
	namelist_t *nl;

	name = (name_t *)smalloc (sizeof (name_t));
	name->decl = decl;
	name->type = type;
	name->namenode = namenode;
	name->refc = 0;
	if (ss && DA_CUR (ss->defns)) {
		name->ns = DA_NTHITEM (ss->defns, DA_CUR (ss->defns) - 1);
	} else {
		name->ns = NULL;
	}

	nl = stringhash_lookup (names, str);
	if (!nl) {
		nl = (namelist_t *)smalloc (sizeof (namelist_t));
		nl->name = string_dup (str);
		dynarray_init (nl->scopes);
		nl->curscope = -1;
		stringhash_insert (names, nl, nl->name);
	}

#if 0
fprintf (stderr, "name_addsubscopenamess(): str=\"%s\", scopemark=0x%8.8x, nl->curscope=%d\n", str, (unsigned int)scopemark, nl->curscope);
#endif
	if (!scopemark) {
		/* nothing was in scope, add at front */
		dynarray_insert (nl->scopes, name, 0);
		nl->curscope++;					/* whatever it was got pushed along */
		name->me = nl;

		dynarray_insert (namestack, name, 0);
	} else {
		int i = 0;

		/* something was in scope */
		if (nl->curscope < 0) {
			/* created this fresh, put name in */
			dynarray_add (nl->scopes, name);
			nl->curscope = DA_CUR (nl->scopes) - 1;

			/* find out where in namestack it goes */
			for (i=DA_CUR (namestack) - 1; (i >= 0) && ((void *)DA_NTHITEM (namestack, i) != scopemark); i--);
		} else {
			/* something may have been in scope here, need to search */
			int j;

			j = nl->curscope;
			for (i=DA_CUR (namestack) - 1; (i >= 0) && ((void *)DA_NTHITEM (namestack, i) != scopemark); i--) {
				if (DA_NTHITEM (nl->scopes, j) == DA_NTHITEM (namestack, i)) {
					/* before this one */
					j--;
				}
			}
			/* add after element j */
			dynarray_insert (nl->scopes, name, j+1);
			nl->curscope++;			/* pushes active one along */
		}
		name->me = nl;

		/* add to namestack after i */
		dynarray_insert (namestack, name, i+1);
	}

	return name;
}
/*}}}*/
/*{{{  void name_scopename (name_t *name)*/
/*
 *	scopes in a name
 */
void name_scopename (name_t *name)
{
	namelist_t *nl = name->me;
	int i;

	for (i=0; i<DA_CUR (nl->scopes); i++) {
		if (DA_NTHITEM (nl->scopes, i) == name) {
			nocc_internal ("name_scopename(): name [%s] already in scope", nl->name);
		}
	}
	dynarray_add (nl->scopes, name);
	nl->curscope = DA_CUR (nl->scopes) - 1;
	dynarray_add (namestack, name);

	return;
}
/*}}}*/
/*{{{  void name_descopename (name_t *name)*/
/*
 *	descopes a name
 */
void name_descopename (name_t *name)
{
	namelist_t *nl = name->me;
	int i;
	int found = 0;

	for (i=0; i<DA_CUR (nl->scopes); i++) {
		if (DA_NTHITEM (nl->scopes, i) == name) {
			dynarray_delitem (nl->scopes, i);
			found = 1;
			break;		/* for() */
		}
	}
	if (!found) {
		nocc_internal ("name_descopename(): name [%s] not in scope", nl->name);
	}
#if 0
fprintf (stderr, "name_descopename(): removing name [%s]\n", nl->name);
#endif
	/* find and remove from the namestack */
	for (i=DA_CUR (namestack) - 1; i >= 0; i--) {
		if (DA_NTHITEM (namestack, i) == name) {
			if (i != (DA_CUR (namestack) - 1)) {
				nocc_warning ("name_descopename(): name [%s] not top of scope", nl->name);
			}
			dynarray_delitem (namestack, i);
			for (; i < DA_CUR (namestack); ) {
				dynarray_delitem (namestack, i);
			}

			break;		/* for() */
		}
	}
		
	if (nl->curscope >= DA_CUR (nl->scopes)) {
		nl->curscope = DA_CUR (nl->scopes) - 1;
	}
	return;
}
/*}}}*/
/*{{{  void name_delname (name_t *name)*/
/*
 *	deletes a name
 */
void name_delname (name_t *name)
{
	namelist_t *nl = name->me;

	dynarray_rmitem (nl->scopes, name);
	if (nl->curscope >= DA_CUR (nl->scopes)) {
		nl->curscope = DA_CUR (nl->scopes) - 1;
	}
	sfree (name);

	return;
}
/*}}}*/
/*{{{  name_t *name_addtempname (tnode_t *decl, tnode_t *type, ntdef_t *nametag, tnode_t **namenode)*/
/*
 *	creates a temporary name and returns it, not scoped (assumed to be good)
 *	if "nametag" and "namenode" are given, creates a NAMENODE too
 */
name_t *name_addtempname (tnode_t *decl, tnode_t *type, ntdef_t *nametag, tnode_t **namenode)
{
	name_t *name;
	namelist_t *nl;
	char *str;

	str = (char *)smalloc (32);
	sprintf (str, "$tmp.%d", tempnamecounter++);

	name = (name_t *)smalloc (sizeof (name_t));
	name->decl = decl;
	name->type = type;
	name->namenode = namenode ? *namenode : NULL;
	name->refc = 0;
	name->ns = NULL;

#if 0
fprintf (stderr, "name_addtempname(): adding name [%s] type:\n", str);
tnode_dumptree (type, 1, stderr);
#endif

	nl = stringhash_lookup (names, str);
	if (!nl) {
		nl = (namelist_t *)smalloc (sizeof (namelist_t));
		nl->name = string_dup (str);
		dynarray_init (nl->scopes);
		nl->curscope = -1;
		stringhash_insert (names, nl, nl->name);
	}
	name->me = nl;

	if (namenode && !*namenode) {
		*namenode = tnode_createfrom (nametag, type, name);
		SetNameNode (name, *namenode);
	}

	return name;
}
/*}}}*/


/*{{{  namespace_t *name_findnamespace (char *nsname)*/
/*
 *	looks up a whole namespace by name
 */
namespace_t *name_findnamespace (char *nsname)
{
	return stringhash_lookup (namespaces, nsname);
}
/*}}}*/
/*{{{  namespace_t *name_findnamespacepfx (char *nsname)*/
/*
 *	looks up a namespace from a name prefix (e.g. <namespace>.<name>)
 */
namespace_t *name_findnamespacepfx (char *nsname)
{
	char *lname = string_dup (nsname);
	char *ch;
	namespace_t *ns;

	for (ch=lname; (*ch != '\0') && (*ch != '.'); ch++);
	*ch = '\0';

	ns = name_findnamespace (lname);

	sfree (lname);
	return ns;
}
/*}}}*/
/*{{{  namespace_t *name_newnamespace (char *nsname)*/
/*
 *	creates a new namespace and returns it
 */
namespace_t *name_newnamespace (char *nsname)
{
	namespace_t *ns = (namespace_t *)smalloc (sizeof (namespace_t));

	ns->nspace = string_dup (nsname);
	ns->nextns = NULL;

	stringhash_insert (namespaces, ns, ns->nspace);

	return ns;
}
/*}}}*/
/*{{{  int name_hidenamespace (namespace_t *ns)*/
/*
 *	hides a namespace -- removing it from the stringhash
 *	returns 0 on success, non-zero on failure
 */
int name_hidenamespace (namespace_t *ns)
{
	namespace_t *xns = stringhash_lookup (namespaces, ns->nspace);

	if (xns != ns) {
		return -1;
	}
#if 0
fprintf (stderr, "name_hidenamespace(): hiding namespace [%s]\n", ns->nspace);
#endif
	stringhash_remove (namespaces, ns, ns->nspace);
	return 0;
}
/*}}}*/
/*{{{  char *name_newwholename (name_t *name)*/
/*
 *	returns a new string that is the whole name (including namespace)
 */
char *name_newwholename (name_t *name)
{
	char *str;
	namespace_t *ns;
	
	for (ns = name->ns; ns && (ns->nextns); ns = ns->nextns);
	if (ns && strlen (ns->nspace)) {
		str = (char *)smalloc (strlen (name->me->name) + strlen (ns->nspace) + 2);
		sprintf (str, "%s.%s", ns->nspace, name->me->name);
	} else {
		str = string_dup (name->me->name);
	}

	return str;
}
/*}}}*/


/*{{{  void *name_markscope (void)*/
/*
 *	this "marks" the name-stack, such that it can be restored with
 *	name_markdescope() below
 */
void *name_markscope (void)
{
	if (!DA_CUR (namestack)) {
		return NULL;
	}
	return DA_NTHITEM (namestack, DA_CUR (namestack) - 1);
}
/*}}}*/
/*{{{  void name_markdescope (void *mark)*/
/*
 *	this descopes names above some mark
 */
void name_markdescope (void *mark)
{
	int i;

	if (!mark) {
		/* means we're descoping everything! */
		for (i=0; i<DA_CUR (namestack);) {
			name_descopename (DA_NTHITEM (namestack, DA_CUR (namestack) - 1));
		}
	} else {
		for (i=DA_CUR (namestack) - 1; i >= 0; i--) {
			if (DA_NTHITEM (namestack, i) == mark) {
				break;		/* for() */
			}
		}
		if (i < 0) {
			nocc_internal ("name_markdescope(): mark not found!");
			return;
		}
		for (i++; i<DA_CUR (namestack);) {
			name_descopename (DA_NTHITEM (namestack, DA_CUR (namestack) - 1));
		}
	}
	return;
}
/*}}}*/


/*{{{  void name_dumpname (name_t *name, int indent, fhandle_t *stream)*/
/*
 *	dumps a single name (global call)
 */
void name_dumpname (name_t *name, int indent, fhandle_t *stream)
{
	int i;
	tnode_t *type;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "    ");
	}
	type = NameTypeOf (name);
	fhandle_printf (stream, "<name name=\"%s\" type=\"%s\" decladdr=\"0x%8.8x\" namespace=\"%s\" addr=\"0x%8.8x\" />\n", name->me->name,
			type ? type->tag->name : "(null)", (unsigned int)(NameDeclOf (name)), name->ns ? name->ns->nspace : "",
			(unsigned int)name);

	return;
}
/*}}}*/
/*{{{  void name_dumpsname (name_t *name, int indent, fhandle_t *stream)*/
/*
 *	dumps a name in s-record format (global call)
 */
void name_dumpsname (name_t *name, int indent, fhandle_t *stream)
{
	int i;
	tnode_t *type;

	for (i=0; i<indent; i++) {
		fhandle_printf (stream, "  ");
	}
	type = NameTypeOf (name);
	fhandle_printf (stream, "(name (name \"%s\") (type \"%s\") (namespace \"%s\") (addr \"0x%8.8x\"))\n", name->me->name,
			type ? type->tag->name : "(null)", name->ns ? name->ns->nspace : "", (unsigned int)name);

	return;
}
/*}}}*/
/*{{{  static void name_walkdumpname (namelist_t *nl, char *key, void *ptr)*/
/*
 *	dumps a single name
 */
static void name_walkdumpname (namelist_t *nl, char *key, void *ptr)
{
	fhandle_t *stream = ptr ? (fhandle_t *)ptr : FHAN_STDERR;
	int i;

	fhandle_printf (stream, "name [%s] curscope = %d\n", key, nl->curscope);
	for (i=0; i<DA_CUR (nl->scopes); i++) {
		name_t *name = DA_NTHITEM (nl->scopes, i);
		tnode_t *declnode = name->decl;

		fhandle_printf (stream, "\t%d\trefc = %-3d  decl = 0x%8.8x, (%s,%s):\n", i, name->refc, (unsigned int)declnode,
			declnode->tag->ndef->name, declnode->tag->name);
	}
	return;
}
/*}}}*/
/*{{{  void name_dumpnames (fhandle_t *stream)*/
/*
 *	dumps the names
 */
void name_dumpnames (fhandle_t *stream)
{
	stringhash_walk (names, name_walkdumpname, (void *)stream);
	return;
}
/*}}}*/


