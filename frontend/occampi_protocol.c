/*
 *	occampi_protocol.c -- occam-pi protocol handling
 *	Copyright (C) 2008 Fred Barnes <frmb@kent.ac.uk>
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
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "origin.h"
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "fcnlib.h"
#include "dfa.h"
#include "dfaerror.h"
#include "parsepriv.h"
#include "occampi.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "typecheck.h"
#include "tracescheck.h"
#include "constprop.h"
#include "fetrans.h"
#include "precheck.h"
#include "usagecheck.h" 
#include "map.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "langops.h"

/*}}}*/


/*{{{  static int occampi_prescope_protocoldecl (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	called to prescope a protocol declaration (PROTOCOL ...)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_protocoldecl (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t *type = tnode_nthsubof (*nodep, 1);

	if (parser_islistnode (type)) {
		parser_cleanuplist (type);
	}

	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_protocoldecl (compops_t *cops, tnode_t **nodep, scope_t *ss)*/
/*
 *	called to scope-in a protocol declaration (PROTOCOL ...)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_protocoldecl (compops_t *cops, tnode_t **nodep, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*nodep, 0);
	tnode_t *type;
	char *rawname;
	name_t *sname = NULL;
	tnode_t *newname;
	void *namemark;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = (char*)tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "occampi_scopein_protocoldecl: here! rawname = \"%s\".  unscoped type=\n", rawname);
tnode_dumptree (tnode_nthsubof (*nodep, 1), 1, stderr);
#endif
	namemark = name_markscope ();

	/* scope subtype */
	if (scope_subtree (tnode_nthsubaddr (*nodep, 1), ss)) {
		return 0;
	}
	type = tnode_nthsubof (*nodep, 1);
#if 0
fprintf (stderr, "occampi_scopein_protocoldecl: here! rawname = \"%s\".  scoped type=\n", rawname);
tnode_dumptree (type, 1, stderr);
#endif

	/* if we have an intypedecl_scopein, do that here, followed by any scope-out */
#if 0
fprintf (stderr, "occampi_scopein_protocoldecl(): intypedecl_scopein cops = 0x%8.8x, compop? = %d\n", (unsigned int)cops,
		tnode_hascompop (cops, "intypedecl_scopein"));
#endif
	if (tnode_hascompop (cops, "intypedecl_scopein")) {
		tnode_callcompop (cops, "intypedecl_scopein", 2, nodep, ss);
	}

#if 0
fprintf (stderr, "occampi_scopein_protocoldecl(): here 1, in-scope:\n");
name_dumpnames (stderr);
#endif
	name_markdescope (namemark);

	sname = name_addscopename (rawname, *nodep, type, NULL);
	if ((*nodep)->tag == opi.tag_VARPROTOCOLDECL) {
		newname = tnode_createfrom (opi.tag_NVARPROTOCOLDECL, name, sname);
	} else if ((*nodep)->tag == opi.tag_SEQPROTOCOLDECL) {
		newname = tnode_createfrom (opi.tag_NSEQPROTOCOLDECL, name, sname);
	} else {
		scope_error (name, ss, "unknown node type! [%s]", (*nodep)->tag->name);
		return 0;
	}
	SetNameNode (sname, newname);
	tnode_setnthsub (*nodep, 0, newname);
#if 0
fprintf (stderr, "occampi_scopein_protocoldecl(): here 2, in-scope:\n");
name_dumpnames (stderr);
#endif

	/* free the old name */
	tnode_free (name);
	ss->scoped++;

	/* scope body */
	if (scope_subtree (tnode_nthsubaddr (*nodep, 2), ss)) {
		return 0;
	}

	name_descopename (sname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_typecheck_protocoldecl (compops_t *cops, tnode_t *node, typecheck_t *tc)*/
/*
 *	does type-checking for protocol declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typecheck_protocoldecl (compops_t *cops, tnode_t *node, typecheck_t *tc)
{
	tnode_t **typep = tnode_nthsubaddr (node, 1);


	if (node->tag == opi.tag_VARPROTOCOLDECL) {
		/*{{{  check variant PROTOCOL declaration*/
		if (!parser_islistnode (*typep)) {
			typecheck_error (node, tc, "expected list of protocols in variant protocol declaration");
		} else {
			tnode_t **taglines;
			int ntags, i;
			int did_error = 0;

			taglines = parser_getlistitems (*typep, &ntags);
			if (!ntags) {
				typecheck_warning (node, tc, "variant protocol has no tags");
			}
			for (i=0; i<ntags; i++) {
				if (!taglines[i]) {
					typecheck_error (node, tc, "missing variant %d in PROTOCOL declaration", i);
					did_error = 1;
				} else if (taglines[i]->tag != opi.tag_TAGDECL) {
					typecheck_error (node, tc, "variant %d in PROTOCOL declaration does not begin with a tag", i);
					did_error = 1;
				} else {
					tnode_t *tagname = tnode_nthsubof (taglines[i], 0);
					tnode_t *protocol = tnode_nthsubof (taglines[i], 1);
					tnode_t **valp = tnode_nthsubaddr (taglines[i], 2);

					if (tagname->tag != opi.tag_NTAG) {
						typecheck_error (taglines[i], tc, "variant %d in PROTOCOL declaration does not begin with a tag", i);
						did_error = 1;
					} else if (!parser_islistnode (protocol)) {
						typecheck_error (taglines[i], tc, "variant %d in PROTOCOL has a broken sequential protocol", i);
						did_error = 1;
					} else {
						tnode_t **pitems;
						int npitems, j;

						pitems = parser_getlistitems (protocol, &npitems);
						for (j=0; j<npitems; j++) {
							if (!langops_iscommunicable (pitems[j])) {
								typecheck_error (pitems[j], tc, "item %d in variant %d is non-communicable", j, i);
								did_error = 1;
							}
						}
					}

					if (*valp) {
						/* make sure value is an integer */
						tnode_t *definttype = tnode_create (opi.tag_INT, NULL);
						tnode_t *itype = typecheck_gettype (*valp, definttype);
						
						if (!itype) {
							typecheck_error (*valp, tc, "invalid type for enumeration on variant %d", i);
							did_error = 1;
						} else if (!typecheck_typeactual (definttype, itype, *valp, tc)) {
							typecheck_error (*valp, tc, "non-integer value for enumeration on variant %d", i);
							did_error = 1;
						}

						tnode_free (definttype);
					}
				}
			}
		}

		/*}}}*/
	} else if (node->tag == opi.tag_SEQPROTOCOLDECL) {
		/*{{{  check sequential PROTOCOL declaration*/
		/* FIXME! */

		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_typeresolve_protocoldecl (compops_t *cops, tnode_t **nodep, typecheck_t *tc)*/
/*
 *	does type-resolution for type declarations
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_typeresolve_protocoldecl (compops_t *cops, tnode_t **nodep, typecheck_t *tc)
{
	tnode_t *n = *nodep;

	if (n->tag == opi.tag_VARPROTOCOLDECL) {
		/*{{{  assign tag values to variant protocol tags*/
		tnode_t **taglines;
		int ntags;

		taglines = parser_getlistitems (tnode_nthsubof (*nodep, 1), &ntags);
		if (ntags > 0) {
			int i, left;
			POINTERHASH (tnode_t *, taghash, 4);
			int hval = -1;

			/* abuse a pointer-hash to do this -- really storing integer values */
			pointerhash_init (taghash, 4);
			left = 0;

			/* put each enumerated value already set into the hash, count remainder */
			for (i=0; i<ntags; i++) {
				tnode_t **valp = tnode_nthsubaddr (taglines[i], 2);

				if (!*valp) {
					left++;
				} else if (!constprop_isconst (*valp)) {
					typecheck_error (*valp, tc, "enumeration for tag on variant %d is non-constant", i);
				} else {
					int val = constprop_intvalof (*valp);
					tnode_t *other = pointerhash_lookup (taghash, val);

					if (other) {
						typecheck_error (*valp, tc, "duplicate enumeration value %d on variant %d", val, i);
					} else {
						pointerhash_insert (taghash, taglines[i], val);
						if (val > hval) {
							hval = val;		/* record highest seen tag value */
						}
					}
				}
			}
			
			/* if we have any left, fill in the blanks */
			if (left) {
				hval++;
				for (i=0; i<ntags; i++) {
					tnode_t **valp = tnode_nthsubaddr (taglines[i], 2);

					if (!*valp) {
						tnode_t *other;

						do {
							other = pointerhash_lookup (taghash, hval);
							hval++;
						} while (other);

						*valp = constprop_newconst (CONST_INT, NULL, tnode_create (opi.tag_INT, NULL), hval);
						pointerhash_insert (taghash, taglines[i], hval);
					}
				}
			}

			pointerhash_trash (taghash);
		}
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_namemap_protocoldecl (compops_t *cops, tnode_t **nodep, map_t *mdata)*/
/*
 *	does name-mapping for a protocol declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_protocoldecl (compops_t *cops, tnode_t **nodep, map_t *mdata)
{
	tnode_t **bodyp = tnode_nthsubaddr (*nodep, 2);

	/* nothing special to do here actually, just walk through body */
	map_submapnames (bodyp, mdata);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_precode_protocoldecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)*/
/*
 *	does pre-coding for a protocol declaration
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_precode_protocoldecl (compops_t *cops, tnode_t **nodep, codegen_t *cgen)
{
	/* FIXME: might actually want to precode things in the PROTOCOL declaration at some point.. */
	codegen_subprecode (tnode_nthsubaddr (*nodep, 2), cgen);
	return 0;
}
/*}}}*/
/*{{{  static int occampi_usagecheck_protocoldecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)*/
/*
 *	does usage-checking for a protocol declaration (dummy, because we don't want to check inside..)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_usagecheck_protocoldecl (langops_t *lops, tnode_t *node, uchk_state_t *ucstate)
{
	usagecheck_subtree (tnode_nthsubof (node, 2), ucstate);
	return 0;
}
/*}}}*/

/*{{{  static int occampi_namemap_nameprotocolnode (compops_t *cops, tnode_t **nodep, map_t *map)*/
/*
 *	transforms a given protocol-name into a back-end name
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_namemap_nameprotocolnode (compops_t *cops, tnode_t **nodep, map_t *map)
{
	tnode_t *bename = tnode_getchook (*nodep, map->mapchook);
	tnode_t *tname;

	if (bename) {
		tname = map->target->newnameref (bename, map);
		*nodep = tname;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_bytesfor_nameprotocolnode (langops_t *lops, tnode_t *node, target_t *target)*/
/*
 *	returns the number of bytes in a named protocol node
 */
static int occampi_bytesfor_nameprotocolnode (langops_t *lops, tnode_t *node, target_t *target)
{
	nocc_error ("occampi_bytesfor_nameprotocolnode(): no bytes for [%s]", node->tag->name);
	return -1;
}
/*}}}*/
/*{{{  static tnode_t *occampi_gettype_nameprotocolnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the type of a named type-node (trivial)
 */
static tnode_t *occampi_gettype_nameprotocolnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	name_t *name = tnode_nthnameof (node, 0);

	if (!name) {
		nocc_fatal ("occampi_gettype_nameprotocolnode(): NULL name!");
		return NULL;
	}
	if (NameTypeOf (name)) {
#if 0
fprintf (stderr, "occmpi_gettype_nameprotocolnode(): node = [%s], name:\n", node->tag->name);
name_dumpname (name, 1, stderr);
fprintf (stderr, "   \"   name->type:\n");
tnode_dumptree (name->type, 1, stderr);
#endif
		return NameTypeOf (name);
	}
#if 0
nocc_message ("occampi_gettype_nameprotocolnode(): null type on name, node was:");
tnode_dumptree (node, 4, stderr);
#endif
	nocc_fatal ("occampi_gettype_nameprotocolnode(): name has NULL type (FIXME!)");
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *occampi_getsubtype_nameprotocolnode (langops_t *lops, tnode_t *node, tnode_t *default_type)*/
/*
 *	returns the sub-type of a named type-node (only for N_TAG)
 */
static tnode_t *occampi_getsubtype_nameprotocolnode (langops_t *lops, tnode_t *node, tnode_t *default_type)
{
	if (node->tag == opi.tag_NTAG) {
		/* need to look in the declaration (inside a PROTOCOL definition) */
		name_t *name = tnode_nthnameof (node, 0);
		tnode_t *ndecl;

		if (!name) {
			nocc_fatal ("occampi_getsubtype_nameprotocolnode(): NULL name!");
			return NULL;
		}
		if (!NameDeclOf (name)) {
			nocc_fatal ("occampi_getsubtype_nameprotocolnode(): NULL declaration!");
			return NULL;
		}
		ndecl = NameDeclOf (name);

		return tnode_nthsubof (ndecl, 1);
	}
	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *occampi_typeactual_nameprotocolnode (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)*/
/*
 *	does actual-use type-checking on a named type-node (channel I/O for PROTOCOLs)
 *	returns actual type used, NULL on failure
 */
static tnode_t *occampi_typeactual_nameprotocolnode (langops_t *lops, tnode_t *formaltype, tnode_t *actualtype, tnode_t *node, typecheck_t *tc)
{
#if 0
fprintf (stderr, "occampi_typeactual_nameprotocolnode(): formaltype =\n");
tnode_dumptree (formaltype, 1, stderr);
fprintf (stderr, "occampi_typeactual_nameprotocolnode(): actualtype =\n");
tnode_dumptree (actualtype, 1, stderr);
#endif

	if (formaltype->tag == opi.tag_NSEQPROTOCOLDECL) {
		/*{{{  check actual usage on sequential protocol (input/output)*/
		name_t *name = tnode_nthnameof (formaltype, 0);
		tnode_t *slist = NameTypeOf (name);
		int nfitems, naitems, i;
		tnode_t **fitems, **aitems;
		tnode_t *atype;

		fitems = parser_getlistitems (slist, &nfitems);
		aitems = parser_getlistitems (actualtype, &naitems);

		if (nfitems != naitems) {
			typecheck_error (node, tc, "expected %d items in I/O list, found %d", nfitems, naitems);
			return NULL;
		}

		atype = parser_newlistnode (NULL);
		for (i=0; i<nfitems; i++) {
			tnode_t *rtype = typecheck_typeactual (fitems[i], aitems[i], node, tc);

			if (!rtype) {
				typecheck_error (node, tc, "invalid type for item %d in I/O list", i);
			}
			parser_addtolist (atype, rtype);
		}

#if 0
fprintf (stderr, "Occampi_typeactual_nameprotocolnode(): real actual type =\n");
tnode_dumptree (atype, 1, stderr);
#endif
		return atype;
		/*}}}*/
	} else if (formaltype->tag == opi.tag_NVARPROTOCOLDECL) {
		/*{{{  check actual usage on variant protocol (input/output)*/
		name_t *name = tnode_nthnameof (formaltype, 0);

		if (node->tag == opi.tag_OUTPUT) {
			/*{{{  check output, first item should be a tag*/
			int naitems, nfitems, nalist, i;
			tnode_t **aitems = parser_getlistitems (actualtype, &naitems);
			tnode_t **alist = parser_getlistitems (tnode_nthsubof (node, 1), &nalist);
			tnode_t *tag, *tagtype, *rtagtype;
			tnode_t *slist, **fitems;
			tnode_t *atype;

			if (!naitems) {
				typecheck_error (node, tc, "no tag specified in variant protocol output");
				return NULL;
			}
			if (!nalist) {
				typecheck_error (node, tc, "no tag specified in variant protocol output");
				return NULL;
			}
			tag = alist[0];
			
			if (tag->tag != opi.tag_NTAG) {
				typecheck_error (node, tc, "first item in variant protocol output is not a tag!");
				return NULL;
			}

			slist = typecheck_getsubtype (tag, NULL);
			if (!slist) {
				typecheck_error (node, tc, "variant protocol tag has no sub-type!");
				return NULL;
			}

			if (!parser_islistnode (slist)) {
				typecheck_error (node, tc, "variant protocol sub-type is not a list!");
				return NULL;
			}

			fitems = parser_getlistitems (slist, &nfitems);
			if ((naitems - 1) > nfitems) {
				typecheck_error (node, tc, "too many items in I/O list");
				return NULL;
			} else if ((naitems - 1) < nfitems) {
				typecheck_error (node, tc, "too few items in I/O list");
				return NULL;
			}

			atype = parser_newlistnode (NULL);
			for (i=0; i<nfitems; i++) {
				tnode_t *rtype = typecheck_typeactual (fitems[i], aitems[i+1], node, tc);

				if (!rtype) {
					typecheck_error (node, tc, "invalid type for item %d in I/O list", i);
				}
				parser_addtolist (atype, rtype);
			}

			/* last, add the TAG type to the front of the list */
			tagtype = typecheck_gettype (tag, NULL);
			rtagtype = typecheck_typeactual (tagtype, aitems[0], node, tc);
			if (!rtagtype) {
				typecheck_error (node, tc, "invalid type for tag in variant I/O list");
			}
#if 0
fprintf (stderr, "occampi_typeactual_nameprotocolnode(): rtagtype =\n");
tnode_dumptree (rtagtype, 1, stderr);
#endif
			parser_addtolist_front (atype, rtagtype);

			return atype;
			/*}}}*/
		}
		/*}}}*/
	}

	return NULL;
}
/*}}}*/
/*{{{  static tnode_t *occampi_protocoltotype_nameprotocolnode (langops_t *lops, tnode_t *prot, tnode_t *rhs)*/
/*
 *	gets the type associated with a named-protocol, 'rhs' is used to aid resolution in variant protocols
 *	returns the type on success, NULL on failure
 */
static tnode_t *occampi_protocoltotype_nameprotocolnode (langops_t *lops, tnode_t *prot, tnode_t *rhs)
{
	tnode_t *ntype = NULL;

	if (prot->tag == opi.tag_NSEQPROTOCOLDECL) {
		name_t *name = tnode_nthnameof (prot, 0);
		ntype = NameTypeOf (name);
	} else if (prot->tag == opi.tag_NVARPROTOCOLDECL) {
#if 0
fprintf (stderr, "occampi_protocoltotype_nameprotocolnode(): NVARPROTOCOLDECL: rhs =\n");
tnode_dumptree (rhs, 1, stderr);
#endif
		if (parser_islistnode (rhs)) {
			int nitems;
			tnode_t **items = parser_getlistitems (rhs, &nitems);

			if ((nitems > 0) && (items[0]->tag == opi.tag_NTAG)) {
				/* first item is the tag, find corresponding protocol */
				name_t *name = tnode_nthnameof (prot, 0);
				tnode_t *taglist = NameTypeOf (name);
				int ntags, i;
				tnode_t **tags = parser_getlistitems (taglist, &ntags);

				for (i=0; !ntype && (i<ntags); i++) {
					tnode_t *tagname = tnode_nthsubof (tags[i], 0);
					tnode_t *tagsprot = tnode_nthsubof (tags[i], 1);

					if (tagname == items[0]) {
						/* this one */
						ntype = tagsprot;
					}
				}
				if (ntype) {
					/* add a type for the tag at the start of the type-list */
					tnode_t *tlist = parser_newlistnode (NULL);
					int npitems;
					tnode_t **pbits = parser_getlistitems (ntype, &npitems);

					parser_addtolist (tlist, tnode_createfrom (opi.tag_INT, rhs));
					for (i=0; i<npitems; i++) {
						parser_addtolist (tlist, pbits[i]);
					}

					ntype = tlist;
				}
			}
		}
	}

	return ntype;
}
/*}}}*/
/*{{{  static int occampi_getname_nameprotocolnode (langops_t *lops, tnode_t *node, char **str)*/
/*
 *	gets the name of a named-type-node (data-type name, etc.)
 *	return 0 on success, -ve on failure
 */
static int occampi_getname_nameprotocolnode (langops_t *lops, tnode_t *node, char **str)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));

	if (*str) {
		sfree (*str);
	}
	*str = (char *)smalloc (strlen (pname) + 2);
	strcpy (*str, pname);

	return 0;
}
/*}}}*/
/*{{{  static int occampi_initialising_decl_nameprotocolnode (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)*/
/*
 *	does initialising declarations for user-defined types (DATA TYPE, CHAN TYPE, ...)
 *	returns 0 if nothing needed, non-zero otherwise
 */
static int occampi_initialising_decl_nameprotocolnode (langops_t *lops, tnode_t *t, tnode_t *benode, map_t *mdata)
{
	return 0;
}
/*}}}*/
/*{{{  static int occampi_istype_nameprotocolnode (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the specified node is a type
 */
static int occampi_istype_nameprotocolnode (langops_t *lops, tnode_t *node)
{
	if (node->tag == opi.tag_NTAG) {
		return 0;
	} else if (node->tag == opi.tag_NVARPROTOCOLDECL) {
		return 1;
	} else if (node->tag == opi.tag_NSEQPROTOCOLDECL) {
		return 1;
	}
	return 0;
}
/*}}}*/
/*{{{  static int occampi_typehash_nameprotocolnode (langops_t *lops, tnode_t *node, int hsize, void *ptr)*/
/*
 *	generates a type-hash for a named type
 *	returns 0 on success, non-zero on failure
 */
static int occampi_typehash_nameprotocolnode (langops_t *lops, tnode_t *node, int hsize, void *ptr)
{
	char *pname = NameNameOf (tnode_nthnameof (node, 0));
	unsigned int myhash = 0;
	tnode_t *subtype = NameTypeOf (tnode_nthnameof (node, 0));

	langops_typehash_blend (hsize, ptr, strlen (pname), (void *)pname);

	if (node->tag == opi.tag_NTAG) {
		myhash = 0x7a5;
	} else if (node->tag == opi.tag_NVARPROTOCOLDECL) {
		myhash = 0xef39c;
	} else if (node->tag == opi.tag_NSEQPROTOCOLDECL) {
		myhash = 0x125cdf;
	} else {
		nocc_serious ("occampi_typehash_nameprotocolnode(): unknown node (%s,%s)", node->tag->name, node->tag->ndef->name);
		return 1;
	}
#if 0
fprintf (stderr, "occampi_typehash_nameprotocolnode(): FIXME: subtype needs including, got:\n");
tnode_dumptree (subtype, 1, stderr);
#endif

	return 0;
}
/*}}}*/

/*{{{  static int occampi_prescope_tagdecl (compops_t *cops, tnode_t **nodep, prescope_t *ps)*/
/*
 *	called to pre-scope a TAGDECL
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_prescope_tagdecl (compops_t *cops, tnode_t **nodep, prescope_t *ps)
{
	tnode_t **tlistp = tnode_nthsubaddr (*nodep, 1);

	if (!*tlistp) {
		/* empty protocol, create empty list */
		*tlistp = parser_newlistnode (NULL);
	} else if (!parser_islistnode (*tlistp)) {
		/* singular, make list */
		*tlistp = parser_buildlistnode (NULL, *tlistp, NULL);
	}
	return 1;
}
/*}}}*/
/*{{{  static int occampi_scopein_tagdecl (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope in a tag name (inside a PROTOCOL)
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_scopein_tagdecl (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 0);
	char *rawname;
	tnode_t *type, *newname;
	name_t *sname = NULL;
	tnode_t *inttype;

	if (name->tag != opi.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

	scope_subtree (tnode_nthsubaddr (*node, 1), ss);		/* scope type */
	type = tnode_nthsubof (*node, 1);

	inttype = tnode_createfrom (opi.tag_INT, *node);
	sname = name_addscopename (rawname, *node, inttype, NULL);
	newname = tnode_createfrom (opi.tag_NTAG, name, sname);
	SetNameNode (sname, newname);
	tnode_setnthsub (*node, 0, newname);

	/* free old name */
	tnode_free (name);
	ss->scoped++;

	return 0;
}
/*}}}*/


/*{{{  static int occampi_fetrans_actionnode_forprotocol (compops_t *cops, tnode_t **nodep, fetrans_t *fe)*/
/*
 *	does front-end transforms on an action-node for PROTOCOL types
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_fetrans_actionnode_forprotocol (compops_t *cops, tnode_t **nodep, fetrans_t *fe)
{
	int v = 1;
	tnode_t *atype = tnode_nthsubof (*nodep, 2);

	if ((((*nodep)->tag == opi.tag_INPUT) || ((*nodep)->tag == opi.tag_OUTPUT)) &&
			atype && (parser_islistnode (atype))) {
		/*{{{  flatten out I/O structure into a new SEQ node*/
		tnode_t *lhs = tnode_nthsubof (*nodep, 0);
		tnode_t *rhs = tnode_nthsubof (*nodep, 1);
		int nitems, ntypes, i;
		tnode_t **items = parser_getlistitems (rhs, &nitems);
		tnode_t **types = parser_getlistitems (atype, &ntypes);

#if 0
fprintf (stderr, "occampi_fetrans_actionnode_forprotocol(): here! rhs = \n");
tnode_dumptree (rhs, 1, stderr);
fprintf (stderr, "occampi_fetrans_actionnode_forprotocol(): here! atype = \n");
tnode_dumptree (atype, 1, stderr);
#endif
		if (nitems != ntypes) {
			nocc_error ("occampi_fetrans_actionnode_forprotocol(): output list has %d items, type has %d", nitems, ntypes);
			return 0;
		}

		if (nitems == 1) {
			/* special case, just de-list */
			tnode_setnthsub (*nodep, 1, items[0]);
			tnode_setnthsub (*nodep, 2, types[0]);

			parser_trashlist (rhs);
			parser_trashlist (atype);
		} else {
			tnode_t *newseq = NULL;
			tnode_t *violist = parser_newlistnode (NULL);

			newseq = tnode_createfrom (opi.tag_SEQ, *nodep, NULL, violist);
			for (i=0; i<nitems; i++) {
				tnode_t *newio = tnode_createfrom ((*nodep)->tag, *nodep, lhs, items[i], types[i]);

				parser_addtolist (violist, newio);
			}
			
			/* replace existing node */
			*nodep = newseq;

			parser_trashlist (rhs);
			parser_trashlist (atype);

			/* do fetrans on subtree directly */
			fetrans_subtree (nodep, fe);
			return 0;
		}
		/*}}}*/
	}

	if (cops->next && tnode_hascompop (cops->next, "fetrans")) {
		v = tnode_callcompop (cops->next, "fetrans", 2, nodep, fe);
	}

	return v;
}
/*}}}*/
/*{{{  static int occampi_tracescheck_actionnode_forprotocol (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)*/
/*
 *	does traces-checking on an action-node for PROTOCOL types
 *	returns 0 to stop walk, 1 to continue
 */
static int occampi_tracescheck_actionnode_forprotocol (compops_t *cops, tnode_t *node, tchk_state_t *tcstate)
{
	chook_t *tchkhook = tracescheck_getnoderefchook ();
	int did_traces = 0;
	int v = 1;

	if (node->tag == opi.tag_OUTPUT) {
		tnode_t *lhs = tnode_nthsubof (node, 0);
		tnode_t *lhstype = typecheck_gettype (lhs, NULL);

		if (lhstype->tag != opi.tag_CHAN) {
			nocc_error ("occampi_tracescheck_actionnode_forprotocol(): confused, LHS-type of OUTPUT not CHAN!, got [%s]",
					lhstype->tag->name);
		} else {
			tnode_t *proto = typecheck_getsubtype (lhstype, NULL);

#if 1
fprintf (stderr, "occampi_tracescheck_actionnode_forprotocol(): got protocol:\n");
tnode_dumptree (proto, 1, stderr);
#endif
			if (proto->tag == opi.tag_NVARPROTOCOLDECL) {
				/* variant protocol, I/O list should begin with a tag */
				tnode_t *rhs = tnode_nthsubof (node, 1);

				if (!parser_islistnode (rhs)) {
					nocc_internal ("occampi_tracescheck_actionnode_forprotocol(): confused, RHS of VARPROTOCOL OUTPUT should be a list, got [%s]",
							rhs->tag->name);
				} else {
					int nitems;
					tnode_t **rhslist = parser_getlistitems (rhs, &nitems);

					if (nitems < 1) {
						nocc_internal ("occampi_tracescheck_actionnode_forprotocol(): too few RHS items in VARPROTOCOL OUTPUT!");
					} else {
						tnode_t *tag = rhslist[0];

						if (tag->tag != opi.tag_NTAG) {
							nocc_internal ("occampi_tracescheck_actionnode_forprotocol(): first RHS item is not a TAG!, got [%s]",
									tag->tag->name);
						} else {
							/* okay, build I/O list with this */
							tnode_t *baselhs = langops_getbasename (lhs);
							tnode_t *fieldlhs = langops_getfieldnamelist (lhs);
							tchknode_t *lhstcn, *tagtcn;

							if (baselhs && (baselhs != lhs)) {
								/* use the base name for now */
								/* FIXME: ... */
								lhs = baselhs;
							}

							lhstcn = (tchknode_t *)tnode_getchook (lhs, tchkhook);
							tagtcn = (tchknode_t *)tnode_getchook (tag, tchkhook);
#if 1
fprintf (stderr, "occampi_tracescheck_actionnode_forprotocol(): in here, tagtcn is:\n");
tracescheck_dumpnode (tagtcn, 1, stderr);
#endif

							if (lhstcn) {
								tchknode_t *newtcn = tracescheck_dupref (lhstcn);

								newtcn = tracescheck_createnode (TCN_OUTPUT, node, newtcn, tagtcn);
								tracescheck_addtobucket (tcstate, newtcn);
								did_traces = 1;
								v = 0;
							}
						}
					}
				}
			}
		}
	}

	if (!did_traces && cops->next && tnode_hascompop (cops->next, "tracescheck")) {
		v = tnode_callcompop (cops->next, "tracescheck", 2, node, tcstate);
	}

	return v;
}
/*}}}*/


/*{{{  static int occampi_protocol_init_nodes (void)*/
/*
 *	sets up protocol nodes for occam-pi
 *	returns 0 on success, non-zero on failure
 */
static int occampi_protocol_init_nodes (void)
{
	tndef_t *tnd;
	compops_t *cops;
	langops_t *lops;
	int i;

	/*{{{  occampi:protocoldecl -- SEQPROTOCOLDECL, VARPROTOCOLDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:protocoldecl", &i, 3, 0, 0, TNF_SHORTDECL);		/* subnotes: 0 = name; 1 = type; 2 = body */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_protocoldecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_protocoldecl));
	tnode_setcompop (cops, "typecheck", 2, COMPOPTYPE (occampi_typecheck_protocoldecl));
	tnode_setcompop (cops, "typeresolve", 2, COMPOPTYPE (occampi_typeresolve_protocoldecl));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_protocoldecl));
	tnode_setcompop (cops, "precode", 2, COMPOPTYPE (occampi_precode_protocoldecl));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "do_usagecheck", 2, LANGOPTYPE (occampi_usagecheck_protocoldecl));
	tnd->lops = lops;

	i = -1;
	opi.tag_VARPROTOCOLDECL = tnode_newnodetag ("VARPROTOCOLDECL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_SEQPROTOCOLDECL = tnode_newnodetag ("SEQPROTOCOLDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:tagdecl -- TAGDECL*/
	i = -1;
	tnd = tnode_newnodetype ("occampi:tagdecl", &i, 3, 0, 0, TNF_NONE);			/* subnodes: 0 = name, 1 = type, 2 = value-of */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "prescope", 2, COMPOPTYPE (occampi_prescope_tagdecl));
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (occampi_scopein_tagdecl));
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnd->lops = lops;

	i = -1;
	opi.tag_TAGDECL = tnode_newnodetag ("TAGDECL", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  occampi:nameprotocolnode -- N_SEQPROTOCOLDECL, N_VARPROTOCOLDECL, N_TAG*/
	i = -1;
	tnd = opi.node_NAMEPROTOCOLNODE = tnode_newnodetype ("occampi:nameprotocolnode", &i, 0, 1, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (occampi_namemap_nameprotocolnode));
	tnd->ops = cops;

	lops = tnode_newlangops ();
	tnode_setlangop (lops, "gettype", 2, LANGOPTYPE (occampi_gettype_nameprotocolnode));
	tnode_setlangop (lops, "getsubtype", 2, LANGOPTYPE (occampi_getsubtype_nameprotocolnode));
	tnode_setlangop (lops, "typeactual", 4, LANGOPTYPE (occampi_typeactual_nameprotocolnode));
	tnode_setlangop (lops, "bytesfor", 2, LANGOPTYPE (occampi_bytesfor_nameprotocolnode));
	tnode_setlangop (lops, "getname", 2, LANGOPTYPE (occampi_getname_nameprotocolnode));
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (occampi_initialising_decl_nameprotocolnode));
	tnode_setlangop (lops, "istype", 1, LANGOPTYPE (occampi_istype_nameprotocolnode));
	tnode_setlangop (lops, "typehash", 3, LANGOPTYPE (occampi_typehash_nameprotocolnode));
	tnode_setlangop (lops, "protocoltotype", 2, LANGOPTYPE (occampi_protocoltotype_nameprotocolnode));
	tnd->lops = lops;

	i = -1;
	opi.tag_NVARPROTOCOLDECL = tnode_newnodetag ("N_VARPROTOCOLDECL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_NSEQPROTOCOLDECL = tnode_newnodetag ("N_SEQPROTOCOLDECL", &i, tnd, NTF_NONE);
	i = -1;
	opi.tag_NTAG = tnode_newnodetag ("N_TAG", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int occampi_protocol_post_setup (void)*/
/*
 *	does post-setup for protocol nodes in occam-pi
 *	returns 0 on success, non-zero on failure/
 */
static int occampi_protocol_post_setup (void)
{
	tndef_t *tnd;
	compops_t *cops;

	/*{{{  intefere with action-nodes (occampi:actionnode) to flatten out protocol communications*/
	tnd = tnode_lookupnodetype ("occampi:actionnode");
	if (!tnd) {
		nocc_error ("occampi_dtype_post_setup(): failed to find \"occampi:actionnode\" node type");
		return -1;
	}

	cops = tnode_insertcompops (tnd->ops);

	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (occampi_fetrans_actionnode_forprotocol));
	tnode_setcompop (cops, "tracescheck", 2, COMPOPTYPE (occampi_tracescheck_actionnode_forprotocol));
	tnd->ops = cops;

	/*}}}*/

	return 0;
}
/*}}}*/


/*{{{  occampi_protocol_feunit (feunit_t)*/
feunit_t occampi_protocol_feunit = {
	init_nodes: occampi_protocol_init_nodes,
	reg_reducers: NULL,
	init_dfatrans: NULL,
	post_setup: occampi_protocol_post_setup,
	ident: "occampi-protocol"
};
/*}}}*/

