/*
 *	mcsp_process.c -- handling for MCSP processes
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
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "symbols.h"
#include "keywords.h"
#include "lexer.h"
#include "lexpriv.h"
#include "tnode.h"
#include "parser.h"
#include "dfa.h"
#include "parsepriv.h"
#include "mcsp.h"
#include "feunit.h"
#include "names.h"
#include "scope.h"
#include "prescope.h"
#include "constprop.h"
#include "typecheck.h"
#include "usagecheck.h"
#include "fetrans.h"
#include "betrans.h"
#include "map.h"
#include "codegen.h"
#include "target.h"
#include "transputer.h"


/*}}}*/

/*{{{  private types/data*/
typedef struct TAG_mcsp_consthook {
	char *data;
	int length;
	int valtype;		/* whether 'data' should be interpreted as a number */
} mcsp_consthook_t;


/*}}}*/

/*{{{  static void *mcsp_nametoken_to_hook (void *ntok)*/
/*
 *	turns a name token into a hooknode for a tag_NAME
 */
static void *mcsp_nametoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *rawname;

	rawname = tok->u.name;
	tok->u.name = NULL;

	lexer_freetoken (tok);

	return (void *)rawname;
}
/*}}}*/
/*{{{  static void *mcsp_stringtoken_to_hook (void *ntok)*/
/*
 *	turns a string token into a hooknode for a tag_STRING
 */
static void *mcsp_stringtoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	mcsp_consthook_t *ch;

	ch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));
	ch->data = string_ndup (tok->u.str.ptr, tok->u.str.len);
	ch->length = tok->u.str.len;
	ch->valtype = 0;

	lexer_freetoken (tok);
	return (void *)ch;
}
/*}}}*/
/*{{{  static void *mcsp_integertoken_to_hook (void *ntok)*/
/*
 *	turns an integer token into a hooknode for a tag_INTEGER
 */
static void *mcsp_integertoken_to_hook (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	mcsp_consthook_t *ch;

	ch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));
	ch->data = mem_ndup (&(tok->u.ival), sizeof (int));
	ch->length = sizeof (int);
	ch->valtype = 1;

	lexer_freetoken (tok);
	return (void *)ch;
}
/*}}}*/
/*{{{  static void *mcsp_pptoken_to_node (void *ntok)*/
/*
 *	turns a keyword token for a primitive process into a mcsp:leafproc node
 */
static void *mcsp_pptoken_to_node (void *ntok)
{
	token_t *tok = (token_t *)ntok;
	char *sbuf;
	tnode_t *node;
	ntdef_t *tag;

	if (tok->type != KEYWORD) {
		nocc_error ("mcsp_pptoken_to_node(): token not keyword: [%s]", lexer_stokenstr (tok));
		lexer_freetoken (tok);
		return NULL;
	}
	sbuf = (char *)smalloc (128);
	snprintf (sbuf, 127, "MCSP%s", tok->u.kw->name);

	tag = tnode_lookupnodetag (sbuf);
	if (!tag) {
		nocc_error ("mcsp_pptoken_to_node(): keyword not node-tag: [%s]", sbuf);
		sfree (sbuf);
		lexer_freetoken (tok);
		return NULL;
	}
	sfree (sbuf);

	node = tnode_create (tag, tok->origin);
	lexer_freetoken (tok);

	return node;
}
/*}}}*/


/*{{{  static void mcsp_rawnamenode_hook_free (void *hook)*/
/*
 *	frees a rawnamenode hook (name-bytes)
 */
static void mcsp_rawnamenode_hook_free (void *hook)
{
	if (hook) {
		sfree (hook);
	}
	return;
}
/*}}}*/
/*{{{  static void *mcsp_rawnamenode_hook_copy (void *hook)*/
/*
 *	copies a rawnamenode hook (name-bytes)
 */
static void *mcsp_rawnamenode_hook_copy (void *hook)
{
	char *rawname = (char *)hook;

	if (rawname) {
		return string_dup (rawname);
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mcsp_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for rawnamenode hook (name-bytes)
 */
static void mcsp_rawnamenode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	mcsp_isetindent (stream, indent);
	fprintf (stream, "<mcsprawnamenode value=\"%s\" />\n", hook ? (char *)hook : "(null)");
	return;
}
/*}}}*/


/*{{{  static void mcsp_constnode_hook_free (void *hook)*/
/*
 *	frees a constnode hook (bytes)
 */
static void mcsp_constnode_hook_free (void *hook)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	if (ch) {
		if (ch->data) {
			sfree (ch->data);
		}
		sfree (ch);
	}

	return;
}
/*}}}*/
/*{{{  static void *mcsp_constnode_hook_copy (void *hook)*/
/*
 *	copies a constnode hook (name-bytes)
 */
static void *mcsp_constnode_hook_copy (void *hook)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	if (ch) {
		mcsp_consthook_t *newch = (mcsp_consthook_t *)smalloc (sizeof (mcsp_consthook_t));

		if (ch->valtype) {
			newch->data = ch->data ? mem_ndup (ch->data, ch->length) : NULL;
		} else {
			newch->data = ch->data ? string_dup (ch->data) : NULL;
		}
		newch->length = ch->length;
		newch->valtype = ch->valtype;

		return (void *)newch;
	}
	return NULL;
}
/*}}}*/
/*{{{  static void mcsp_constnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dump-tree for constnode hook (name-bytes)
 */
static void mcsp_constnode_hook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)hook;

	mcsp_isetindent (stream, indent);
	if (!ch) {
		fprintf (stream, "<mcspconsthook length=\"0\" value=\"(null)\" />\n");
	} else if (!ch->valtype) {
		fprintf (stream, "<mcspconsthook length=\"%d\" value=\"%s\" />\n", ch->length, ch->data ? ch->data : "(null)");
	} else if (ch->length == 4) {
		fprintf (stream, "<mcspconsthook length=\"%d\" value=\"%d\" />\n", ch->length, ch->data ? *(int *)ch->data : 0);
	} else {
		char *vstr = mkhexbuf ((unsigned char *)ch->data, ch->length);

		fprintf (stream, "<mcspconsthook length=\"%d\" hexvalue=\"%s\" />\n", ch->length, vstr);
		sfree (vstr);
	}
	return;
}
/*}}}*/


/*{{{  static int mcsp_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	scopes in a free-floating name
 */
static int mcsp_scopein_rawname (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = *node;
	char *rawname;
	name_t *sname = NULL;
	mcsp_lex_t *lmp;
	mcsp_scope_t *mss = (mcsp_scope_t *)ss->langpriv;

	if ((*node)->org_file && (*node)->org_file->priv) {
		lexpriv_t *lp = (lexpriv_t *)(*node)->org_file->priv;
		
		lmp = (mcsp_lex_t *)lp->langpriv;
	} else {
		lmp = NULL;
	}

	if (name->tag != mcsp.tag_NAME) {
		scope_error (name, ss, "name not raw-name!");
		return 0;
	}
	rawname = tnode_nthhookof (name, 0);

#if 0
fprintf (stderr, "mcsp_scopein_rawname: here! rawname = \"%s\"\n", rawname);
#endif
	sname = name_lookupss (rawname, ss);
	if (sname) {
		/* resolved */
		*node = NameNodeOf (sname);

		/* if it looks like a PROCDEF, turn into an INSTANCE -- if we're not already in an instance!*/
		if (!mss->inamescope && ((*node)->tag == mcsp.tag_PROCDEF)) {
			*node = tnode_createfrom (mcsp.tag_INSTANCE, name, *node, parser_newlistnode (NULL));
		}

		tnode_free (name);
	} else {
#if 0
fprintf (stderr, "mcsp_scopein_rawname(): unresolved name \"%s\", unbound-events = %d, mss->uvinsertlist = 0x%8.8x\n", rawname, lmp ? lmp->unboundvars : -1, (unsigned int)mss->uvinsertlist);
#endif
		if (lmp && lmp->unboundvars) {
			if (mss && mss->uvinsertlist) {
				/*{{{  add the name manually*/
				tnode_t *decl = tnode_create (mcsp.tag_UPARAM, NULL, NULL);
				tnode_t *newname;

				sname = name_addsubscopenamess (rawname, mss->uvscopemark, decl, NULL, name, ss);
				parser_addtolist (mss->uvinsertlist, decl);
				newname = tnode_createfrom (mcsp.tag_EVENT, decl, sname);
				SetNameNode (sname, newname);
				tnode_setnthsub (decl, 0, newname);

				/* and replace local node! */
				*node = newname;
				tnode_free (name);

				ss->scoped++;
				/*}}}*/
			} else {
				scope_error (name, ss, "unresolved name \"%s\" cannot be captured", rawname);
			}
		} else {
			scope_error (name, ss, "unresolved name \"%s\"", rawname);
		}
	}

	return 1;
}
/*}}}*/


/*{{{  static int mcsp_namemap_leafproc (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a leaf-process
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_leafproc (compops_t *cops, tnode_t **node, map_t *map)
{
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_leafproc (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a leaf-process
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_leafproc (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SKIP) {
		/* nothing :) */
		return 0;
	} else if (node->tag == mcsp.tag_STOP) {
		codegen_callops (cgen, tsecondary, I_SETERR);
		return 0;
	} else if ((node->tag == mcsp.tag_DIV) || (node->tag == mcsp.tag_CHAOS)) {
		/* just stop for now -- maybe get creative later ;) */
		codegen_callops (cgen, tsecondary, I_SETERR);
		return 0;
	}
	codegen_warning (cgen, "don\'t know how to generate code for [%s]", node->tag->name);
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_namenode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on an EVENT
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_namenode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		if (t->tag == mcsp.tag_EVENT) {
			/*{{{  turn event into SYNC -- not a declaration occurence*/
			*node = tnode_create (mcsp.tag_SYNC, NULL, *node, NULL);
			return 0;
			/*}}}*/
		}
		break;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for namenodes (unbound references)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_namenode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *bename = (tnode_t *)tnode_getchook (*node, map->mapchook);
	tnode_t *tname;

	if (bename) {
		tname = map->target->newnameref (bename, map);
		*node = tname;
	}

	return 0;
}
/*}}}*/
/*{{{  static void mcsp_namenode_initevent (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to initialise an EVENT
 */
static void mcsp_namenode_initevent (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_offs;

	cgen->target->be_getoffsets (node, &ws_offs, NULL, NULL, NULL);
	/* FIXME: this needs tidying */
	codegen_callops (cgen, loadconst, 5 * cgen->target->slotsize);
	codegen_callops (cgen, tsecondary, I_MALLOC);
	codegen_callops (cgen, storelocal, ws_offs);

	/* initialise */
	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 0);		/* ref-count */

	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 4);		/* enrolled-count */

	codegen_callops (cgen, loadconst, 1);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 8);		/* count-down */

	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 12);		/* fptr */

	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, storenonlocal, 16);		/* bptr */

	return;
}
/*}}}*/
/*{{{  static void mcsp_namenode_finalevent (tnode_t *node, codegen_t *cgen, void *arg)*/
/*
 *	generates code to destroy an EVENT
 */
static void mcsp_namenode_finalevent (tnode_t *node, codegen_t *cgen, void *arg)
{
	int ws_offs;

	cgen->target->be_getoffsets (node, &ws_offs, NULL, NULL, NULL);

	codegen_callops (cgen, loadlocal, ws_offs);
	codegen_callops (cgen, tsecondary, I_MRELEASE);
	codegen_callops (cgen, tsecondary, I_NULL);
	codegen_callops (cgen, storelocal, ws_offs);

	return;
}
/*}}}*/
/*{{{  static int mcsp_namenode_initialising_decl (langops_t *lops, tnode_t *node, tnode_t *bename, map_t *map)*/
/*
 *	used to initialise EVENTs
 */
static int mcsp_namenode_initialising_decl (langops_t *lops, tnode_t *node, tnode_t *bename, map_t *map)
{
	if (node->tag == mcsp.tag_EVENT) {
		codegen_setinithook (bename, mcsp_namenode_initevent, (void *)node);
		codegen_setfinalhook (bename, mcsp_namenode_finalevent, (void *)node);
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_cnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms on SEQ/PAR nodes (pass 1+ only)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_cnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
		/* nothing in this pass! */
		break;
	case 1:
		if (t->tag == mcsp.tag_SEQCODE) {
			/*{{{  flatten SEQCODEs*/
			tnode_t *slist = tnode_nthsubof (t, 1);
			tnode_t **procs;
			int nprocs, i;
			tnode_t *newlist = parser_newlistnode (NULL);

			procs = parser_getlistitems (slist, &nprocs);
			for (i=0; i<nprocs; i++) {
				if (procs[i] && (procs[i]->tag == mcsp.tag_SEQCODE)) {
					tnode_t *xslist;
					tnode_t **xprocs;
					int nxprocs, j;

					/* flatten */
					fetrans_subtree (&procs[i], fe);

					xslist = tnode_nthsubof (procs[i], 1);
					xprocs = parser_getlistitems (xslist, &nxprocs);
					for (j=0; j<nxprocs; j++) {
						parser_addtolist (newlist, xprocs[j]);
						xprocs[j] = NULL;
					}

					/* assume we got them all */
					tnode_free (procs[i]);
					procs[i] = NULL;
				} else {
					/* move over to new list after sub-fetrans (preserves order) */
					fetrans_subtree (&procs[i], fe);

					parser_addtolist (newlist, procs[i]);
					procs[i] = NULL;
				}
			}

			/* park new list and free old */
			tnode_setnthsub (t, 1, newlist);
			tnode_free (slist);

			return 0;
			/*}}}*/
		}
		break;
	case 2:
		if (t->tag == mcsp.tag_PARCODE) {
			/*{{{  build the alphabet for this node, store unbound ones in parent*/
			mcsp_alpha_t *savedalpha = mfe->curalpha;
			mcsp_alpha_t *a_lhs, *a_rhs;
			mcsp_alpha_t *paralpha;
			tnode_t **subnodes;
			int nsnodes;

			/* always two subtrees */
			subnodes = parser_getlistitems (tnode_nthsubof (t, 1), &nsnodes);
			if (nsnodes != 2) {
				nocc_internal ("mcsp_fetrans_dopnode(): pass2 for PARCODE: have %d items", nsnodes);
				return 0;
			}

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (&subnodes[0], fe);
			a_lhs = mfe->curalpha;

			mfe->curalpha = mcsp_newalpha ();
			fetrans_subtree (&subnodes[1], fe);
			a_rhs = mfe->curalpha;

			mfe->curalpha = savedalpha;
			paralpha = mcsp_newalpha ();

			mcsp_sortandmergealpha (a_lhs, a_rhs, paralpha, mfe->curalpha);

			mcsp_freealpha (a_lhs);
			mcsp_freealpha (a_rhs);

			tnode_setnthhook (t, 0, (void *)paralpha);
			return 0;
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does mapping for a constructor node (SEQ/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_cnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == mcsp.tag_SEQCODE) {
		/* nothing special */
		return 1;
	} else if ((*node)->tag == mcsp.tag_PARCODE) {
		/*{{{  map PAR bodies*/
		tnode_t *body = tnode_nthsubof (*node, 1);
		tnode_t **bodies;
		int nbodies, i;
		tnode_t *parnode = *node;
		mcsp_alpha_t *alpha = (mcsp_alpha_t *)tnode_nthhookof (*node, 0);		/* events the _two_ processes synchronise on */

		if (!parser_islistnode (body)) {
			nocc_internal ("mcsp_namemap_cnode(): body of PARCODE not list");
			return 1;
		}

		/* if we have an alphabet, map these first (done in PAR context) */
		if (alpha) {
			map_submapnames (&(alpha->elist), map);
#if 0
fprintf (stderr, "mcsp_namemap_cnode(): mapped alphabet, got list:\n");
tnode_dumptree (alpha->elist, 1, stderr);
#endif
		}

		bodies = parser_getlistitems (body, &nbodies);
		for (i=0; i<nbodies; i++) {
			/*{{{  turn body into back-end block*/
			tnode_t *blk, *parbodyspace;
			/* tnode_t *saved_blk = map->thisblock;
			tnode_t **saved_params = map->thisprocparams; */

			blk = map->target->newblock (bodies[i], map, NULL, map->lexlevel + 1);
			map_pushlexlevel (map, blk, NULL);
			/* map->thisblock = blk;
			 * map->thisprocparams = NULL;
			 * map->lexlevel++; */

			/* map body */
			map_submapnames (&(bodies[i]), map);
			parbodyspace = map->target->newname (tnode_create (mcsp.tag_PARSPACE, NULL), bodies[i], map, 0, 16, 0, 0, 0, 0);	/* FIXME! */
			*(map->target->be_blockbodyaddr (blk)) = parbodyspace;

			map_poplexlevel (map);
			/* map->lexlevel--;
			 * map->thisblock = saved_blk;
			 * map->thisprocparams = saved_params; */

			/* make block node the individual PAR process */
			bodies[i] = blk;
			/*}}}*/
		}

		if (nbodies > 1) {
			/*{{{  make space for PAR*/
			tnode_t *bename, *bodyref, *blist;
			tnode_t *fename = tnode_create (mcsp.tag_PARSPACE, NULL);

			blist = parser_newlistnode (NULL);
			for (i=0; i<nbodies; i++) {
				parser_addtolist (blist, bodies[i]);
			}
			bodyref = map->target->newblockref (blist, *node, map);
			bename = map->target->newname (fename, bodyref, map, map->target->aws.as_par, 0, 0, 0, 0, 0);
			tnode_setchook (fename, map->mapchook, (void *)bename);

			*node = bename;

			tnode_setnthsub (parnode, 0, map->target->newnameref (bename, map));
			/*}}}*/
			/*{{{  if we have an alphabet, link in with alloc:extravars*/
			if (alpha) {
				tnode_setchook (bename, map->allocevhook, alpha->elist);
			}
			/*}}}*/
		}

		/*}}}*/
	} else {
		nocc_internal ("mcsp_namemap_cnode(): don\'t know how to handle tag [%s]", (*node)->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a constructor node (SEQ/PAR)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_cnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SEQCODE) {
		return 1;
	} else if (node->tag == mcsp.tag_PARCODE) {
		/*{{{  generate code for PAR*/
		tnode_t *body = tnode_nthsubof (node, 1);
		tnode_t **bodies;
		int nbodies, i;
		int joinlab = codegen_new_label (cgen);
		int pp_wsoffs = 0;
		tnode_t *parspaceref = tnode_nthsubof (node, 0);
		mcsp_alpha_t *alpha = (mcsp_alpha_t *)tnode_nthhookof (node, 0);

		bodies = parser_getlistitems (body, &nbodies);
		/*{{{  if we've got an alphabet, up ref-counts by (nbodies), enroll-counts by (nbodies - 1), down-counts by (nbodies - 1)*/
		if (alpha) {
			tnode_t **events;
			int nevents, j;

			codegen_callops (cgen, comment, "start barrier enrolls");
			events = parser_getlistitems (alpha->elist, &nevents);
			for (j=0; j<nevents; j++) {
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 0);		/* refcount */
				codegen_callops (cgen, loadconst, nbodies);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 0);		/* refcount */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 4);		/* enroll-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 4);		/* enroll-count */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 8);		/* down-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_SUM);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 8);		/* down-count */
			}
			codegen_callops (cgen, comment, "finish barrier enrolls");
		}
		/*}}}*/
		/*{{{  PAR setup*/
		codegen_callops (cgen, comment, "BEGIN PAR SETUP");
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;
			tnode_t *statics = tnode_nthsubof (bodies[i], 1);

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  setup statics in workspace of PAR process*/
			if (statics && parser_islistnode (statics)) {
				int nitems, p, wsoff;
				tnode_t **items = parser_getlistitems (statics, &nitems);

				for (p=nitems - 1, wsoff = pp_wsoffs-4; p>=0; p--, wsoff -= 4) {
					codegen_callops (cgen, loadparam, items[p], PARAM_REF);
					codegen_callops (cgen, storelocal, wsoff);
				}
			} else if (statics) {
				codegen_callops (cgen, loadparam, statics, PARAM_REF);
				codegen_callops (cgen, storelocal, pp_wsoffs-4);
			}
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		/*{{{  setup local PAR workspace*/
		codegen_callops (cgen, loadconst, nbodies + 1);		/* par-count */
		codegen_callops (cgen, storename, parspaceref, 4);
		codegen_callops (cgen, loadconst, 0);			/* priority */
		codegen_callops (cgen, storename, parspaceref, 8);
		codegen_callops (cgen, loadlabaddr, joinlab);		/* join-lab */
		codegen_callops (cgen, storename, parspaceref, 0);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			codegen_check_beblock (bodies[i], cgen, 1);
			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);

			/*{{{  start PAR process*/
			codegen_callops (cgen, loadlabaddr, elab);
			codegen_callops (cgen, loadlocalpointer, pp_wsoffs - adjust);
			codegen_callops (cgen, tsecondary, I_STARTP);
			/*}}}*/

			pp_wsoffs -= ws_size;
		}
		codegen_callops (cgen, comment, "END PAR SETUP");
		/*}}}*/
		/*{{{  end process doing PAR*/
		codegen_callops (cgen, loadpointer, parspaceref, 0);
		codegen_callops (cgen, tsecondary, I_ENDP);
		/*}}}*/
		pp_wsoffs = 0;
		for (i=0; i<nbodies; i++) {
			/*{{{  PAR body*/
			int ws_size, vs_size, ms_size;
			int ws_offset, adjust, elab;

			cgen->target->be_getblocksize (bodies[i], &ws_size, &ws_offset, &vs_size, &ms_size, &adjust, &elab);
			codegen_callops (cgen, comment, "PAR = %d,%d,%d,%d,%d", ws_size, ws_offset, vs_size, ms_size, adjust);

			codegen_subcodegen (bodies[i], cgen);

			codegen_callops (cgen, loadpointer, parspaceref, pp_wsoffs + adjust);
			codegen_callops (cgen, tsecondary, I_ENDP);
			/*}}}*/

			pp_wsoffs += ws_size;
		}
		/*}}}*/
		/*{{{  PAR cleanup*/
		codegen_callops (cgen, comment, "END PAR BODIES");
		codegen_callops (cgen, setlabel, joinlab);
		/*}}}*/
		/*{{{  if we've got an alphabet, down ref-counts by (nbodies), enroll-counts by (nbodies - 1), down-counts by (nbodies - 1)*/
		if (alpha) {
			tnode_t **events;
			int nevents, j;

			codegen_callops (cgen, comment, "start barrier resigns");
			events = parser_getlistitems (alpha->elist, &nevents);
			for (j=0; j<nevents; j++) {
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 0);		/* refcount */
				codegen_callops (cgen, loadconst, nbodies);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 0);		/* refcount */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 4);		/* enroll-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 4);		/* enroll-count */

				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, loadnonlocal, 8);		/* down-count */
				codegen_callops (cgen, loadconst, nbodies - 1);
				codegen_callops (cgen, tsecondary, I_DIFF);
				codegen_callops (cgen, loadpointer, events[j], 0);
				codegen_callops (cgen, storenonlocal, 8);		/* down-count */
			}
			codegen_callops (cgen, comment, "finish barrier resigns");
		}
		/*}}}*/
	} else {
		codegen_error (cgen, "mcsp_codegen_cnode(): how to handle [%s] ?", node->tag->name);
	}
	return 0;
}
/*}}}*/

/*{{{  static int mcsp_fetrans_actionnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transforms for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_actionnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;
	tnode_t *t = *node;

	switch (mfe->parse) {
	case 0:
	case 1:
		/* nothing in these passes! */
		break;
	case 2:
		if (t->tag == mcsp.tag_SYNC) {
			/*{{{  add event to list*/
			if (mfe->curalpha) {
				mcsp_addtoalpha (mfe->curalpha, tnode_nthsubof (t, 0));
			}
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_betrans_actionnode (compops_t *cops, tnode_t **node, betrans_t *be)*/
/*
 *	does back-end transforms for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_betrans_actionnode (compops_t *cops, tnode_t **node, betrans_t *be)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_SYNC) {
		/* we need to turn this into a single-guard ALT */
		tnode_t *altnode, *guard, *glist, *event;

		event = tnode_nthsubof (t, 0);
		guard = tnode_createfrom (mcsp.tag_GUARD, t, event, tnode_create (mcsp.tag_SKIP, NULL));
		glist = parser_newlistnode (NULL);
		parser_addtolist (glist, guard);
		altnode = tnode_createfrom (mcsp.tag_ALT, t, glist, NULL);

		tnode_setnthsub (t, 0, NULL);
		tnode_free (t);
		*node = altnode;

		betrans_subtree (node, be);
		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_namemap_actionnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_actionnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_SYNC) {
		nocc_internal ("mcsp_namemap_actionnode(): should not see SYNC here!");
		return 0;
	} else if (t->tag == mcsp.tag_CHANWRITE) {
		tnode_t *bename;
		tnode_t **opp;
		int aslots = 0;		/* above workspace slots that we might need */

		/* do channel (and argument if not an event) */
		map_submapnames (tnode_nthsubaddr (*node, 0), map);

		opp = tnode_nthsubaddr (*node, 1);
		if (*opp && ((*opp)->tag != mcsp.tag_EVENT)) {
			map_submapnames (opp, map);
		} else if (*opp) {
			/* slightly crafty -- turn operand into a back-end constant containing the EVENT's name */
			name_t *evname = tnode_nthnameof (*opp, 0);
			char *thename = NameNameOf (evname);
			char *localname = (char *)smalloc (strlen (thename) + 3);

			sprintf (localname, "%s\n", thename);
			*opp = map->target->newconst (*opp, map, (void *)localname, strlen (localname) + 1);		/* null terminator for free */
			sfree (localname);
			aslots = 1;											/* need a counter for output */
		}

		/* make space for output */
		bename = map->target->newname (*node, NULL, map, aslots * map->target->slotsize, map->target->bws.ds_io, 0, 0, 0, 0);
		*node = bename;

		return 0;
	}

	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_actionnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for an action-node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_actionnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_SYNC) {
		codegen_error (cgen, "mcsp_codegen_actionnode(): cannot generate code for SYNC!");
		return 0;
	} else if (node->tag == mcsp.tag_CHANWRITE) {
		tnode_t *chan = tnode_nthsubof (node, 0);
		tnode_t *data = tnode_nthsubof (node, 1);
		int tempslot = 0;				/* reserved a slot -- should really be_getoffsets for it */
		int looplab, skiplab;

		/* FIXME: this is crude.. */
		looplab = codegen_new_label (cgen);
		skiplab = codegen_new_label (cgen);

		codegen_callops (cgen, loadconst, 0);
		codegen_callops (cgen, storelocal, tempslot);

		codegen_callops (cgen, setlabel, looplab);
		codegen_callops (cgen, loadpointer, data, 0);
		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, tsecondary, I_LB);
		codegen_callops (cgen, branch, I_CJ, skiplab);		/* if we saw the NULL terminator */
		codegen_callops (cgen, trashistack);

		codegen_callops (cgen, loadpointer, data, 0);
		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, tsecondary, I_SUM);

		codegen_callops (cgen, loadpointer, chan, 0);
		codegen_callops (cgen, loadconst, 1);			/* BYTE output only! */
		codegen_callops (cgen, tsecondary, I_OUT);

		codegen_callops (cgen, loadlocal, tempslot);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, storelocal, tempslot);

		codegen_callops (cgen, branch, I_J, looplab);

		codegen_callops (cgen, setlabel, skiplab);

		return 0;
	}
	codegen_warning (cgen, "don\'t know how to generate code for [%s]", node->tag->name);
	return 1;
}
/*}}}*/

/*{{{  static int mcsp_namemap_loopnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a loopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_loopnode (compops_t *cops, tnode_t **node, map_t *map)
{
	tnode_t *t = *node;

	if (t->tag == mcsp.tag_ILOOP) {
		/*{{{  loop*/
		tnode_t **condp = tnode_nthsubaddr (t, 1);

		if (*condp) {
			map_submapnames (condp, map);
		}
		map_submapnames (tnode_nthsubaddr (t, 0), map);

		return 0;
		/*}}}*/
	} else if (t->tag == mcsp.tag_PRIDROP) {
		/*{{{  drop priority*/
		/* allocate a temporary anyway, not used yet */
		*node = map->target->newname (t, NULL, map, map->target->slotsize, map->target->bws.ds_min, 0, 0, 0, 0);

		/* map body */
		map_submapnames (tnode_nthsubaddr (t, 0), map);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_loopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a loopnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_loopnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_ILOOP) {
		/*{{{  infinite loop!*/
		int looplab = codegen_new_label (cgen);

		codegen_callops (cgen, setlabel, looplab);
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);
		codegen_callops (cgen, branch, I_J, looplab);

		return 0;
		/*}}}*/
	} else if (node->tag == mcsp.tag_PRIDROP) {
		/*{{{  drop priority*/
		codegen_callops (cgen, tsecondary, I_GETPRI);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUM);
		codegen_callops (cgen, tsecondary, I_SETPRI);

		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);

		codegen_callops (cgen, tsecondary, I_GETPRI);
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_DIFF);
		codegen_callops (cgen, tsecondary, I_SETPRI);

		return 0;
		/*}}}*/
	}
	return 1;
}
/*}}}*/

/*{{{  static int mcsp_scopein_replnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-in a replicator node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopein_replnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	tnode_t *name = tnode_nthsubof (*node, 1);
	tnode_t **bodyptr = tnode_nthsubaddr (*node, 0);
	void *nsmark;
	char *rawname;
	name_t *replname;
	tnode_t *newname;

	if (!name && !tnode_nthsubof (*node, 2)) {
		/* just have a length expression, nothing to scope in */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);
	} else {
		/* scope the start and end expressions */
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 2), scope_modprewalktree, scope_modpostwalktree, (void *)ss);
		tnode_modprepostwalktree (tnode_nthsubaddr (*node, 3), scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		nsmark = name_markscope ();
		/* scope in replicator name and walk body */
		rawname = (char *)tnode_nthhookof (name, 0);
		replname = name_addscopenamess (rawname, *node, NULL, NULL, ss);
		newname = tnode_createfrom (mcsp.tag_VAR, name, replname);
		SetNameNode (replname, newname);
		tnode_setnthsub (*node, 1, newname);

		/* free old name, scope body */
		tnode_free (name);

		tnode_modprepostwalktree (bodyptr, scope_modprewalktree, scope_modpostwalktree, (void *)ss);

		name_markdescope (nsmark);
	}

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_scopeout_replnode (compops_t *cops, tnode_t **node, scope_t *ss)*/
/*
 *	called to scope-out a replicator node
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_scopeout_replnode (compops_t *cops, tnode_t **node, scope_t *ss)
{
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_fetrans_replnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transform for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_replnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	tnode_t *rname = tnode_nthsubof (*node, 1);
	tnode_t **rstartptr = tnode_nthsubaddr (*node, 2);
	tnode_t **rendptr = tnode_nthsubaddr (*node, 3);

	fetrans_subtree (tnode_nthsubaddr (*node, 0), fe);			/* fetrans body */
	if (!rname && !(*rstartptr)) {
		/* only got replicator length, start start to constant 1 */
		*rstartptr = constprop_newconst (CONST_INT, NULL, NULL, 1);
	}
	if (rname) {
		fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);		/* fetrans name */
	}
	/* trans start/end expressions */
	fetrans_subtree (rstartptr, fe);
	fetrans_subtree (rendptr, fe);

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_replnode (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_replnode (compops_t *cops, tnode_t **node, map_t *map)
{
	if ((*node)->tag == mcsp.tag_REPLSEQ) {
		tnode_t **rptr = tnode_nthsubaddr (*node, 1);
		tnode_t *fename;
		tnode_t *nameref = NULL;

		map_submapnames (tnode_nthsubaddr (*node, 0), map);		/* map body */
		map_submapnames (tnode_nthsubaddr (*node, 2), map);		/* map start expression */
		map_submapnames (tnode_nthsubaddr (*node, 3), map);		/* map end expression */

		/* reserve two words for replicator */
		if (*rptr) {
			fename = *rptr;
			*rptr = NULL;
		} else {
			fename = tnode_create (mcsp.tag_LOOPSPACE, NULL);
		}

		*node = map->target->newname (fename, *node, map, 2 * map->target->intsize, 0, 0, 0, map->target->intsize, 0);
		tnode_setchook (fename, map->mapchook, (void *)*node);

		/* transform replicator name into a reference to the space just reserved */
		nameref = map->target->newnameref (*node, map);
		/* *rptr = nameref; */
#if 0
fprintf (stderr, "got nameref for REPLSEQ space:\n");
tnode_dumptree (nameref, 1, stderr);
fprintf (stderr, "*rptr is:\n");
tnode_dumptree (*rptr, 1, stderr);
#endif
		/* *rptr = map->target->newnameref (*node, map); */
		if (*rptr) {
			tnode_free (*rptr);
		}
		*rptr = nameref;

		return 0;
	}
	return 1;
}
/*}}}*/
/*{{{  static int mcsp_codegen_replnode (compops_t *cops, tnode_t *node, codegen_t *cgen)*/
/*
 *	does code-generation for a replnode
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_codegen_replnode (compops_t *cops, tnode_t *node, codegen_t *cgen)
{
	if (node->tag == mcsp.tag_REPLSEQ) {
		int toplab = codegen_new_label (cgen);
		int botlab = codegen_new_label (cgen);
		tnode_t *rref = tnode_nthsubof (node, 1);

		/*{{{  loop-head*/
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 2), 0);		/* start */
		codegen_callops (cgen, storename, rref, 0);				/* => value */
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 3), 0);		/* end */
		codegen_callops (cgen, loadname, tnode_nthsubof (node, 2), 0);		/* start */
		codegen_callops (cgen, tsecondary, I_SUB);
		codegen_callops (cgen, storename, rref, cgen->target->intsize);		/* => count */

		codegen_callops (cgen, setlabel, toplab);

		/*}}}*/
		/*{{{  loop-body*/
		codegen_subcodegen (tnode_nthsubof (node, 0), cgen);

		/*}}}*/
		/*{{{  loop-end*/

		codegen_callops (cgen, loadname, rref, cgen->target->intsize);		/* count */
		codegen_callops (cgen, branch, I_CJ, botlab);
		codegen_callops (cgen, loadname, rref, 0);				/* value */
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_ADD);
		codegen_callops (cgen, storename, rref, 0);				/* value = value - 1*/
		codegen_callops (cgen, loadname, rref, cgen->target->intsize);		/* count */
		codegen_callops (cgen, loadconst, 1);
		codegen_callops (cgen, tsecondary, I_SUB);
		codegen_callops (cgen, storename, rref, cgen->target->intsize);		/* count = count - 1*/
		codegen_callops (cgen, branch, I_J, toplab);

		codegen_callops (cgen, setlabel, botlab);
		/*}}}*/
		return 0;
	}
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_fetrans_guardnode (compops_t *cops, tnode_t **node, fetrans_t *fe)*/
/*
 *	does front-end transformation on a GUARD
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_fetrans_guardnode (compops_t *cops, tnode_t **node, fetrans_t *fe)
{
	mcsp_fetrans_t *mfe = (mcsp_fetrans_t *)fe->langpriv;

	switch (mfe->parse) {
	case 0:
		if ((*node)->tag == mcsp.tag_GUARD) {
			/* don't walk LHS */
			fetrans_subtree (tnode_nthsubaddr (*node, 1), fe);
			return 0;
		}
		break;
	case 1:
		/* nothing in this pass! */
		break;
	case 2:
		if ((*node)->tag == mcsp.tag_GUARD) {
			/*{{{  add event to current alphabet*/
			tnode_t *event = tnode_nthsubof (*node, 0);

			if (mfe->curalpha && (event->tag == mcsp.tag_EVENT)) {
				mcsp_addtoalpha (mfe->curalpha, event);
			}
			/*}}}*/
		}
		break;
	}
	return 1;
}
/*}}}*/


/*{{{  static int mcsp_constprop_const (compops_t *cops, tnode_t **node)*/
/*
 *	does constant propagation on a constant node (post walk)
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_constprop_const (compops_t *cops, tnode_t **node)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (*node, 0);

	if ((*node)->tag == mcsp.tag_INTEGER) {
		*node = constprop_newconst (CONST_INT, *node, NULL, *(int *)(ch->data));
	}
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_namemap_const (compops_t *cops, tnode_t **node, map_t *map)*/
/*
 *	does name-mapping for a constant
 *	returns 0 to stop walk, 1 to continue
 */
static int mcsp_namemap_const (compops_t *cops, tnode_t **node, map_t *map)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (*node, 0);

	*node = map->target->newconst (*node, map, ch->data, ch->length);
	return 0;
}
/*}}}*/
/*{{{  static int mcsp_isconst_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if the node is a constant (returns width)
 */
static int mcsp_isconst_const (langops_t *lops, tnode_t *node)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (node, 0);

	return ch->length;
}
/*}}}*/
/*{{{  static int mcsp_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)*/
/*
 *	returns the constant value of a constant node (assigns to pointer if non-null)
 */
static int mcsp_constvalof_const (langops_t *lops, tnode_t *node, void *ptr)
{
	mcsp_consthook_t *ch = (mcsp_consthook_t *)tnode_nthhookof (node, 0);
	int r = 0;

	if (node->tag == mcsp.tag_INTEGER) {
		if (ptr) {
			*(int *)ptr = *(int *)(ch->data);
		}
		r = *(int *)(ch->data);
	}
	return r;
}
/*}}}*/
/*{{{  static int mcsp_valbyref_const (langops_t *lops, tnode_t *node)*/
/*
 *	returns non-zero if value-references of this constant get passed by reference
 */
static int mcsp_valbyref_const (langops_t *lops, tnode_t *node)
{
	if (node->tag == mcsp.tag_STRING) {
		return 1;
	}
	return 0;
}
/*}}}*/


/*{{{  static int mcsp_process_init_nodes (void)*/
/*
 *	initialises MCSP process nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_process_init_nodes (void)
{
	tndef_t *tnd;
	int i;
	compops_t *cops;
	langops_t *lops;

	/*{{{  mcsp:rawnamenode -- NAME*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:rawnamenode", &i, 0, 0, 1, TNF_NONE);				/* hooks: raw-name */
	tnd->hook_free = mcsp_rawnamenode_hook_free;
	tnd->hook_copy = mcsp_rawnamenode_hook_copy;
	tnd->hook_dumptree = mcsp_rawnamenode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_rawname));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_NAME = tnode_newnodetag ("MCSPNAME", &i, tnd, NTF_NONE);

#if 0
fprintf (stderr, "mcsp_process_init_nodes(): tnd->name = [%s], mcsp.tag_NAME->name = [%s], mcsp.tag_NAME->ndef->name = [%s]\n", tnd->name, mcsp.tag_NAME->name, mcsp.tag_NAME->ndef->name);
#endif
	/*}}}*/
	/*{{{  mcsp:leafnode -- PARSPACE, LOOPSPACE*/
	i = -1;
	tnd = tnode_lookupornewnodetype ("mcsp:leafnode", &i, 0, 0, 0, TNF_NONE);

	i = -1;
	mcsp.tag_PARSPACE = tnode_newnodetag ("MCSPPARSPACE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_LOOPSPACE = tnode_newnodetag ("MCSPLOOPSPACE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:leafproc -- SKIP, STOP, DIV, CHAOS*/
	i = -1;
	tnd = mcsp.node_LEAFPROC = tnode_newnodetype ("mcsp:leafproc", &i, 0, 0, 0, TNF_NONE);
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_leafproc));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_leafproc));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SKIP = tnode_newnodetag ("MCSPSKIP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_STOP = tnode_newnodetag ("MCSPSTOP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_DIV = tnode_newnodetag ("MCSPDIV", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHAOS = tnode_newnodetag ("MCSPCHAOS", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:namenode -- EVENT, PROCDEF, CHAN, VAR*/
	i = -1;
	tnd = mcsp.node_NAMENODE = tnode_newnodetype ("mcsp:namenode", &i, 0, 1, 0, TNF_NONE);		/* subnames: 0 = name */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_namenode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_namenode));
/*	cops->gettype = mcsp_gettype_namenode; */
	tnd->ops = cops;
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "initialising_decl", 3, LANGOPTYPE (mcsp_namenode_initialising_decl));
	tnd->lops = lops;

	i = -1;
	mcsp.tag_EVENT = tnode_newnodetag ("MCSPEVENT", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PROCDEF = tnode_newnodetag ("MCSPPROCDEF", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHAN = tnode_newnodetag ("MCSPCHAN", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_VAR = tnode_newnodetag ("MCSPVAR", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:actionnode -- SYNC, CHANWRITE*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:actionnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = event(s), 1 = data/null */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_actionnode));
	tnode_setcompop (cops, "betrans", 2, COMPOPTYPE (mcsp_betrans_actionnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_actionnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_actionnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SYNC = tnode_newnodetag ("MCSPSYNC", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_CHANWRITE = tnode_newnodetag ("MCSPCHANWRITE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:cnode -- SEQCODE, PARCODE*/
	i = -1;
	tnd = mcsp.node_CNODE = tnode_newnodetype ("mcsp:cnode", &i, 2, 0, 1, TNF_NONE);		/* subnodes: 0 = back-end space reference, 1 = list of processes;  hooks: 0 = mcsp_alpha_t */
	tnd->hook_free = mcsp_alpha_hook_free;
	tnd->hook_copy = mcsp_alpha_hook_copy;
	tnd->hook_dumptree = mcsp_alpha_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_cnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_cnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_cnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_SEQCODE = tnode_newnodetag ("MCSPSEQNODE", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PARCODE = tnode_newnodetag ("MCSPPARNODE", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:loopnode -- ILOOP, PRIDROP*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:loopnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = body; 1 = condition */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_loopnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_loopnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_ILOOP = tnode_newnodetag ("MCSPILOOP", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_PRIDROP = tnode_newnodetag ("MCSPPRIDROP", &i, tnd, NTF_NONE);		/* maybe not a loopnode as such, but will do for now */

	/*}}}*/
	/*{{{  mcsp:replnode -- REPLSEQ, REPLPAR, REPLILEAVE*/
	i = -1;
	tnd = mcsp.node_REPLNODE = tnode_newnodetype ("mcsp:replnode", &i, 4, 0, 0, TNF_NONE);		/* subnodes: 0 = body; 1 = repl-var, 2 = start, 3 = end */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "scopein", 2, COMPOPTYPE (mcsp_scopein_replnode));
	tnode_setcompop (cops, "scopeout", 2, COMPOPTYPE (mcsp_scopeout_replnode));
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_replnode));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_replnode));
	tnode_setcompop (cops, "codegen", 2, COMPOPTYPE (mcsp_codegen_replnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_REPLSEQ = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_REPLPAR = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_REPLILEAVE = tnode_newnodetag ("MCSPREPLSEQ", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:guardnode -- GUARD*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:guardnode", &i, 2, 0, 0, TNF_NONE);				/* subnodes: 0 = guard, 1 = process */
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "fetrans", 2, COMPOPTYPE (mcsp_fetrans_guardnode));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_GUARD = tnode_newnodetag ("MCSPGUARD", &i, tnd, NTF_NONE);

	/*}}}*/
	/*{{{  mcsp:constnode -- STRING, INTEGER*/
	i = -1;
	tnd = tnode_newnodetype ("mcsp:constnode", &i, 0, 0, 1, TNF_NONE);				/* hooks: data */
	tnd->hook_free = mcsp_constnode_hook_free;
	tnd->hook_copy = mcsp_constnode_hook_copy;
	tnd->hook_dumptree = mcsp_constnode_hook_dumptree;
	cops = tnode_newcompops ();
	tnode_setcompop (cops, "constprop", 1, COMPOPTYPE (mcsp_constprop_const));
	tnode_setcompop (cops, "namemap", 2, COMPOPTYPE (mcsp_namemap_const));
	lops = tnode_newlangops ();
	tnode_setlangop (lops, "isconst", 1, LANGOPTYPE (mcsp_isconst_const));
	tnode_setlangop (lops, "constvalof", 2, LANGOPTYPE (mcsp_constvalof_const));
	tnode_setlangop (lops, "valbyref", 1, LANGOPTYPE (mcsp_valbyref_const));
	tnd->ops = cops;

	i = -1;
	mcsp.tag_STRING = tnode_newnodetag ("MCSPSTRING", &i, tnd, NTF_NONE);
	i = -1;
	mcsp.tag_INTEGER = tnode_newnodetag ("MCSPINTEGER", &i, tnd, NTF_NONE);

	/*}}}*/

	return 0;
}
/*}}}*/
/*{{{  static int mcsp_process_reg_reducers (void)*/
/*
 *	registers reducers for MCSP process nodes
 *	returns 0 on success, non-zero on failure
 */
static int mcsp_process_reg_reducers (void)
{
	parser_register_grule ("mcsp:namereduce", parser_decode_grule ("T+St0XC1R-", mcsp_nametoken_to_hook, mcsp.tag_NAME));
	parser_register_grule ("mcsp:namepush", parser_decode_grule ("T+St0XC1N-", mcsp_nametoken_to_hook, mcsp.tag_NAME));
	parser_register_grule ("mcsp:ppreduce", parser_decode_grule ("ST0T+XR-", mcsp_pptoken_to_node));
	parser_register_grule ("mcsp:subevent", parser_decode_grule ("SN0N+N+V00C4R-", mcsp.tag_SUBEVENT));
	parser_register_grule ("mcsp:integerreduce", parser_decode_grule ("ST0T+XC1R-", mcsp_integertoken_to_hook, mcsp.tag_INTEGER));
	parser_register_grule ("mcsp:stringreduce", parser_decode_grule ("ST0T+XC1R-", mcsp_stringtoken_to_hook, mcsp.tag_STRING));
	parser_register_grule ("mcsp:replseqreduce", parser_decode_grule ("N+N+N+VN+VN-VN+C4R-", mcsp.tag_REPLSEQ));
	parser_register_grule ("mcsp:replseqlreduce", parser_decode_grule ("N+00N+C4R-", mcsp.tag_REPLSEQ));

	return 0;
}
/*}}}*/
/*{{{  static dfattbl_t **mcsp_process_init_dfatrans (int *ntrans)*/
/*
 *	creates and returns DFA transition tables for MCSP process nodes
 */
static dfattbl_t **mcsp_process_init_dfatrans (int *ntrans)
{
	DYNARRAY (dfattbl_t *, transtbl);

	dynarray_init (transtbl);
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:name ::= [ 0 +Name 1 ] [ 1 {<mcsp:namereduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:string ::= [ 0 +String 1 ] [ 1 {<mcsp:stringreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:integer ::= [ 0 +Integer 1 ] [ 1 {<mcsp:integerreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:expr ::= [ 0 mcsp:name 1 ] [ 0 mcsp:string 1 ] [ 0 mcsp:integer 1 ] [ 1 {<mcsp:nullreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:event ::= [ 0 mcsp:name 1 ] [ 1 @@. 3 ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:expr 4 ] [ 4 {<mcsp:subevent>} -* ]"));
	dynarray_add (transtbl, dfa_bnftotbl ("mcsp:eventset ::= ( mcsp:event | @@{ { mcsp:event @@, 1 } @@} )"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:leafproc ::= [ 0 +@SKIP 1 ] [ 0 +@STOP 1 ] [ 0 +@DIV 1 ] [ 0 +@CHAOS 1 ] [ 1 {<mcsp:ppreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:replseq ::= [ 0 @@; 1 ] [ 1 @@[ 2 ] [ 2 mcsp:expr 3 ] [ 3 @@= 4 ] [ 3 @@] 10 ] [ 4 mcsp:expr 5 ] [ 5 @@, 6 ] [ 6 mcsp:expr 7 ] " \
				"[ 7 @@] 8 ] [ 8 mcsp:process 9 ] [ 9 {<mcsp:replseqreduce>} -* ] [ 10 mcsp:process 11 ] [ 11 {<mcsp:replseqlreduce>} -* ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:restofprocess ::= [ 0 mcsp:dop 1 ] [ 1 mcsp:process 2 ] [ 2 {Rmcsp:folddop} -* ] " \
				"[ 0 %mcsp:hide <mcsp:hide> ]"));
	dynarray_add (transtbl, dfa_transtotbl ("mcsp:process ::= [ 0 +Name 7 ] [ 0 mcsp:leafproc 2 ] [ 0 mcsp:fixpoint 2 ] [ 0 @@( 3 ] [ 0 -@@; 12 ] " \
				"[ 1 %mcsp:restofprocess <mcsp:restofprocess> ] [ 1 -* 2 ] [ 2 {<mcsp:nullreduce>} -* ] " \
				"[ 3 mcsp:process 4 ] [ 4 @@) 5 ] [ 5 %mcsp:restofprocess <mcsp:restofprocess> ] [ 5 -* 6 ] [ 6 {<mcsp:nullreduce>} -* ] " \
				"[ 7 -@@( 8 ] [ 7 -* 10 ] [ 8 {<parser:rewindtokens>} -* 9 ] [ 9 mcsp:instance 1 ] [ 10 {<parser:rewindtokens>} -* 11 ] [ 11 mcsp:event 1 ] " \
				"[ 12 mcsp:replseq 13 ] [ 13 {<mcsp:nullreduce>} -* ]"));

	*ntrans = DA_CUR (transtbl);
	return DA_PTR (transtbl);
}
/*}}}*/


/*{{{  mcsp_process_feunit (feunit_t)*/
feunit_t mcsp_process_feunit = {
	init_nodes: mcsp_process_init_nodes,
	reg_reducers: mcsp_process_reg_reducers,
	init_dfatrans: mcsp_process_init_dfatrans,
	post_setup: NULL
};
/*}}}*/

