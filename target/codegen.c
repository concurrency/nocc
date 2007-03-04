/*
 *	codegen.c -- top-level code-generator for nocc
 *	Copyright (C) 2005 Fred Barnes <frmb@kent.ac.uk>
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
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "lexer.h"
#include "parser.h"
#include "tnode.h"
#include "names.h"
#include "target.h"
#include "codegen.h"
#include "crypto.h"


/*}}}*/

/*{{{  private types*/
typedef struct TAG_codegeninithook {
	struct TAG_codegeninithook *next;
	void (*init)(tnode_t *, codegen_t *, void *);
	void *arg;
} codegeninithook_t;

typedef struct TAG_codegenfinalhook {
	struct TAG_codegenfinalhook *next;
	void (*final)(tnode_t *, codegen_t *, void *);
	void *arg;
} codegenfinalhook_t;

/*}}}*/
/*{{{  private data*/
static chook_t *codegeninithook = NULL;
static chook_t *codegenfinalhook = NULL;

/*}}}*/


/*{{{  static void codegen_isetindent (FILE *stream, int indent)*/
/*
 *	sets indentation (debugging)
 */
static void codegen_isetindent (FILE *stream, int indent)
{
	int i;

	for (i=0; i<indent; i++) {
		fprintf (stream, "    ");
	}
	return;
}
/*}}}*/


/*{{{  static void codegen_precode_chook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	dumps (debugging) extra variables for allocation attached to a node
 */
static void codegen_precode_chook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	tnode_t *evars = (tnode_t *)hook;

	codegen_isetindent (stream, indent);
	fprintf (stream, "<chook id=\"precode:vars\" addr=\"0x%8.8x\">\n", (unsigned int)hook);
	tnode_dumptree (evars, indent + 1, stream);
	codegen_isetindent (stream, indent);
	fprintf (stream, "</chook>\n");

	return;
}
/*}}}*/
/*{{{  static void codegen_inithook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	called to dump init-hook (debugging)
 */
static void codegen_inithook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	codegeninithook_t *cgih = (codegeninithook_t *)hook;

	codegen_isetindent (stream, indent);
	fprintf (stream, "<chook:codegen:initialiser init=\"0x%8.8x\" arg=\"0x%8.8x\" addr=\"0x%8.8x\"", (unsigned int)cgih->init, (unsigned int)cgih->arg, (unsigned int)cgih);
	if (cgih->next) {
		fprintf (stream, ">\n");
		codegen_inithook_dumptree (node, (void *)cgih->next, indent+1, stream);
		codegen_isetindent (stream, indent);
		fprintf (stream, "</chook:codegen:initialiser>\n");
	} else {
		fprintf (stream, " />\n");
	}

	return;
}
/*}}}*/
/*{{{  static void codegen_inithook_free (void *hook)*/
/*
 *	called to free an init-hook
 */
static void codegen_inithook_free (void *hook)
{
	codegeninithook_t *cgih = (codegeninithook_t *)hook;

	if (cgih) {
		codegen_inithook_free (cgih->next);
		sfree (cgih);
	}
	return;
}
/*}}}*/
/*{{{  static codegeninithook_t *codegen_inithook_create (void (*init)(tnode_t *, codegen_t *, void *), void *arg)*/
/*
 *	creates a new init-hook
 */
static codegeninithook_t *codegen_inithook_create (void (*init)(tnode_t *, codegen_t *, void *), void *arg)
{
	codegeninithook_t *cgih = (codegeninithook_t *)smalloc (sizeof (codegeninithook_t));

	cgih->next = NULL;
	cgih->init = init;
	cgih->arg = arg;

	return cgih;
}
/*}}}*/
/*{{{  static void *codegen_inithook_copy (void *hook)*/
/*
 *	called to copy an init-hook
 */
static void *codegen_inithook_copy (void *hook)
{
	codegeninithook_t *cgih = (codegeninithook_t *)hook;

	if (cgih) {
		codegeninithook_t *tmp;

		tmp = codegen_inithook_create (cgih->init, cgih->arg);
		tmp->next = codegen_inithook_copy (cgih->next);
		cgih = tmp;
	}
	return cgih;
}
/*}}}*/
/*{{{  static void codegen_finalhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)*/
/*
 *	called to dump final-hook (debugging)
 */
static void codegen_finalhook_dumptree (tnode_t *node, void *hook, int indent, FILE *stream)
{
	codegenfinalhook_t *cgih = (codegenfinalhook_t *)hook;

	codegen_isetindent (stream, indent);
	fprintf (stream, "<chook:codegen:finaliser final=\"0x%8.8x\" arg=\"0x%8.8x\" addr=\"0x%8.8x\"", (unsigned int)cgih->final, (unsigned int)cgih->arg, (unsigned int)cgih);
	if (cgih->next) {
		fprintf (stream, ">\n");
		codegen_finalhook_dumptree (node, (void *)cgih->next, indent+1, stream);
		codegen_isetindent (stream, indent);
		fprintf (stream, "</chook:codegen:finaliser>\n");
	} else {
		fprintf (stream, " />\n");
	}

	return;
}
/*}}}*/
/*{{{  static void codegen_finalhook_free (void *hook)*/
/*
 *	called to free an final-hook
 */
static void codegen_finalhook_free (void *hook)
{
	codegenfinalhook_t *cgih = (codegenfinalhook_t *)hook;

	if (cgih) {
		codegen_finalhook_free (cgih->next);
		sfree (cgih);
	}
	return;
}
/*}}}*/
/*{{{  static codegenfinalhook_t *codegen_finalhook_create (void (*final)(tnode_t *, codegen_t *, void *), void *arg)*/
/*
 *	creates a new final-hook
 */
static codegenfinalhook_t *codegen_finalhook_create (void (*final)(tnode_t *, codegen_t *, void *), void *arg)
{
	codegenfinalhook_t *cgih = (codegenfinalhook_t *)smalloc (sizeof (codegenfinalhook_t));

	cgih->next = NULL;
	cgih->final = final;
	cgih->arg = arg;

	return cgih;
}
/*}}}*/
/*{{{  static void *codegen_finalhook_copy (void *hook)*/
/*
 *	called to copy an final-hook
 */
static void *codegen_finalhook_copy (void *hook)
{
	codegenfinalhook_t *cgih = (codegenfinalhook_t *)hook;

	if (cgih) {
		codegenfinalhook_t *tmp;

		tmp = codegen_finalhook_create (cgih->final, cgih->arg);
		tmp->next = codegen_finalhook_copy (cgih->next);
		cgih = tmp;
	}
	return cgih;
}
/*}}}*/


/*{{{  void codegen_setinithook (tnode_t *node, void (*init)(tnode_t *, codegen_t *, void *), void *arg)*/
/*
 *	sets an initialisation hook for a node
 */
void codegen_setinithook (tnode_t *node, void (*init)(tnode_t *, codegen_t *, void *), void *arg)
{
	codegeninithook_t *cgih, *here;

	if (!codegeninithook) {
		nocc_internal ("codegen_setinithook(): no initialisation hook!");
		return;
	}
	here = (codegeninithook_t *)tnode_getchook (node, codegeninithook);
	cgih = codegen_inithook_create (init, arg);
	cgih->next = here;

	tnode_setchook (node, codegeninithook, cgih);
	return;
}
/*}}}*/
/*{{{  void codegen_setfinalhook (tnode_t *node, void (*final)(tnode_t *, codegen_t *, void *), void *arg)*/
/*
 *	sets an finialisation hook for a node
 */
void codegen_setfinalhook (tnode_t *node, void (*final)(tnode_t *, codegen_t *, void *), void *arg)
{
	codegenfinalhook_t *cgih, *here;

	if (!codegenfinalhook) {
		nocc_internal ("codegen_setfinalhook(): no finalisation hook!");
		return;
	}
	here = (codegenfinalhook_t *)tnode_getchook (node, codegenfinalhook);
	cgih = codegen_finalhook_create (final, arg);
	cgih->next = here;

	tnode_setchook (node, codegenfinalhook, cgih);
	return;
}
/*}}}*/
/*{{{  void codegen_setpostcall (codegen_t *cgen, void (*func)(codegen_t *, void *), void *arg)*/
/*
 *	sets up a post-call (routine called after code-generation has finished)
 */
void codegen_setpostcall (codegen_t *cgen, void (*func)(codegen_t *, void *), void *arg)
{
	codegen_pcall_t *pcall = (codegen_pcall_t *)smalloc (sizeof (codegen_pcall_t));

	if (!cgen) {
		nocc_internal ("codegen_setpostcall(): NULL cgen!");
		return;
	}
	pcall->fcn = func;
	pcall->arg = arg;
	dynarray_add (cgen->pcalls, pcall);
	return;
}
/*}}}*/
/*{{{  void codegen_clearpostcall (codegen_t *cgen, void (*func)(codegen_t *, void *), void *arg)*/
/*
 *	clears a post-call (before it has run and been removed automatically)
 */
void codegen_clearpostcall (codegen_t *cgen, void (*func)(codegen_t *, void *), void *arg)
{
	int i;

	if (!cgen) {
		nocc_internal ("codegen_setpostcall(): NULL cgen!");
		return;
	}
	for (i=0; i<DA_CUR (cgen->pcalls); i++) {
		codegen_pcall_t *pcall = DA_NTHITEM (cgen->pcalls, i);

		if (pcall && (pcall->fcn == func) && (pcall->arg == arg)) {
			dynarray_delitem (cgen->pcalls, i);
			sfree (pcall);
			return;
		}
	}
	return;
}
/*}}}*/


/*{{{  int codegen_write_bytes (codegen_t *cgen, const char *ptr, int bytes)*/
/*
 *	writes plain bytes to the output file -- this is, in fact, the only thing that writes bytes to the output file
 *	returns 0 on success, non-zero on error
 */
int codegen_write_bytes (codegen_t *cgen, const char *ptr, int bytes)
{
	int v = 0;
	int left = bytes;

	if (cgen->fd < 0) {
		nocc_internal ("codegen_write_bytes(): attempt to write to closed file!");
		return -1;
	}
	if (bytes && cgen->digest) {
		/* write into digest */
		crypto_writedigest (cgen->digest, (unsigned char *)ptr, bytes);
	}
	while (left) {
		int r = write (cgen->fd, ptr + v, left);

		if (r < 0) {
			nocc_error ("failed to write to %s: %s", cgen->fname, strerror (errno));
			return -1;
		}
		left -= r;
		v += r;
	}
	return 0;
}
/*}}}*/
/*{{{  int codegen_write_string (codegen_t *cgen, const char *str)*/
/*
 *	writes a string to the output file
 *	returns 0 on success, non-zero on error
 */
int codegen_write_string (codegen_t *cgen, const char *str)
{
	int i;

	i = codegen_write_bytes (cgen, str, strlen (str));

	return i;
}
/*}}}*/
/*{{{  int codegen_write_fmt (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	writes a formatted string to the output file
 *	returns 0 on success, non-zero on error
 */
int codegen_write_fmt (codegen_t *cgen, const char *fmt, ...)
{
	va_list ap;
	int i, r;
	char *buf = (char *)smalloc (1024);

	va_start (ap, fmt);
	i = vsnprintf (buf, 1023, fmt, ap);
	va_end (ap);

	r = codegen_write_bytes (cgen, buf, i);

	sfree (buf);

	return r;
}
/*}}}*/


/*{{{  void codegen_warning (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	throws a code-generator warning message
 */
void codegen_warning (codegen_t *cgen, const char *fmt, ...)
{
	va_list ap;
	int i;
	char *buf = (char *)smalloc (1024);

	va_start (ap, fmt);
	i = vsnprintf (buf, 1023, fmt, ap);
	va_end (ap);

	nocc_outerrmsg (buf);

	sfree (buf);
	return;
}
/*}}}*/
/*{{{  void codegen_error (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	throws a code-generator error message
 */
void codegen_error (codegen_t *cgen, const char *fmt, ...)
{
	va_list ap;
	int i;
	char *buf = (char *)smalloc (1024);

	va_start (ap, fmt);
	i = vsnprintf (buf, 1023, fmt, ap);
	va_end (ap);

	nocc_outerrmsg (buf);

	sfree (buf);
	cgen->error++;

	return;
}
/*}}}*/
/*{{{  void codegen_fatal (codegen_t *cgen, const char *fmt, ...)*/
/*
 *	throws a code-generator fatal error message
 */
void codegen_fatal (codegen_t *cgen, const char *fmt, ...)
{
	va_list ap;
	int i;
	char *buf = (char *)smalloc (1024);

	va_start (ap, fmt);
	i = vsnprintf (buf, 1023, fmt, ap);
	va_end (ap);

	nocc_outerrmsg (buf);

	sfree (buf);
	nocc_internal ("error in code-generation");
	return;
}
/*}}}*/


/*{{{  static int codegen_prewalktree_codegen (tnode_t *node, void *data)*/
/*
 *	prewalktree for code generation, calls comp-ops "codegen" routine where present
 *	returns 0 to stop walk, 1 to continue
 */
static int codegen_prewalktree_codegen (tnode_t *node, void *data)
{
	codegen_t *cgen = (codegen_t *)data;
	codegeninithook_t *cgih = (codegeninithook_t *)tnode_getchook (node, codegeninithook);
	codegenfinalhook_t *cgfh = (codegenfinalhook_t *)tnode_getchook (node, codegenfinalhook);
	int i = 1;

	/*{{{  do initialisers*/
	while (cgih) {
		if (cgih->init) {
			cgih->init (node, cgen, cgih->arg);
		}
		cgih = cgih->next;
	}
	/*}}}*/

	if (node->tag->ndef->ops && tnode_hascompop_i (node->tag->ndef->ops, (int)COPS_CODEGEN)) {
		i = tnode_callcompop_i (node->tag->ndef->ops, (int)COPS_CODEGEN, 2, node, cgen);
	}

	/*{{{  if finalisers, do subnodes then finalisers*/
	if (cgfh) {
		int nsnodes, j;
		tnode_t **snodes = tnode_subnodesof (node, &nsnodes);

		for (j=0; j<nsnodes; j++) {
			tnode_prewalktree (snodes[j], codegen_prewalktree_codegen, (void *)cgen);
		}

		i = 0;
		while (cgfh) {
			if (cgfh->final) {
				cgfh->final (node, cgen, cgfh->arg);
			}
			cgfh = cgfh->next;
		}
	}
	/*}}}*/

	return i;
}
/*}}}*/
/*{{{  static int codegen_modprewalk_precode (tnode_t **tptr, void *data)*/
/*
 *	modprewalktree for pre-codegen, calls comp-ops "precode" routine where present
 *	return 0 to stop walk, 1 to continue
 */
static int codegen_modprewalk_precode (tnode_t **tptr, void *data)
{
	codegen_t *cgen = (codegen_t *)data;
	tnode_t *evars;
	int i;

	if (!tptr || !*tptr) {
		nocc_internal ("codegen_modprewalk_precode(): NULL pointer or node!");
		return 0;
	}

	evars = (tnode_t *)tnode_getchook (*tptr, cgen->pc_chook);
	if (evars) {
		/* do pre-allocation on nodes with this hook */
#if 0
fprintf (stderr, "codegen_modprewalk_precode(): pre-coding pc_chook vars.\n");
#endif
		tnode_modprewalktree (&evars, codegen_modprewalk_precode, data);
		tnode_setchook (*tptr, cgen->pc_chook, evars);
	}

	i = 1;
	if ((*tptr)->tag->ndef->ops && tnode_hascompop_i ((*tptr)->tag->ndef->ops, (int)COPS_PRECODE)) {
		if (compopts.traceprecode) {
			tnode_message (*tptr, "calling precode on [%s:%s]", (*tptr)->tag->ndef->name, (*tptr)->tag->name);
		}
		i = tnode_callcompop_i ((*tptr)->tag->ndef->ops, (int)COPS_PRECODE, 2, tptr, cgen);
	}
	return i;
}
/*}}}*/
/*{{{  int codegen_subcodegen (tnode_t *tree, codegen_t *cgen)*/
/*
 *	generates code for a nested tree structure
 *	returns 0 on success, non-zero on failure
 */
int codegen_subcodegen (tnode_t *tree, codegen_t *cgen)
{
	tnode_prewalktree (tree, codegen_prewalktree_codegen, (void *)cgen);

	return cgen->error;
}
/*}}}*/
/*{{{  int codegen_subprecode (tnode_t **tptr, codegen_t *cgen)*/
/*
 *	pre-code for nested structures
 *	returns 0 on success, non-zero on failure
 */
int codegen_subprecode (tnode_t **tptr, codegen_t *cgen)
{
	tnode_modprewalktree (tptr, codegen_modprewalk_precode, (void *)cgen);

	return cgen->error;
}
/*}}}*/
/*{{{  int codegen_generate_code (tnode_t **tptr, lexfile_t *lf, target_t *target)*/
/*
 *	generates code for a top-level tree
 *	returns 0 on success, non-zero on failure
 */
int codegen_generate_code (tnode_t **tptr, lexfile_t *lf, target_t *target)
{
	codegen_t *cgen = (codegen_t *)smalloc (sizeof (codegen_t));
	/* tnode_t *tree = *tptr; */
	int i;

	cgen->target = target;
	cgen->error = 0;
	cgen->cops = NULL;
	cgen->labcount = 1;
	cgen->cinsertpoint = tptr;
	cgen->pc_chook = tnode_lookupornewchook ("precode:vars");
	cgen->pc_chook->chook_dumptree = codegen_precode_chook_dumptree;
	dynarray_init (cgen->be_blks);
	dynarray_init (cgen->tcgstates);
	cgen->digest = NULL;
	dynarray_init (cgen->pcalls);

	/*{{{  figure out the output filename*/
	if (compopts.outfile) {
		cgen->fname = string_dup (compopts.outfile);
	} else {
		if (lf->filename) {
			char *ch;
			int lflen = strlen (lf->filename);

			cgen->fname = (char *)smalloc (lflen + strlen (target->extn) + 2);
			strcpy (cgen->fname, lf->filename);
			for (ch = cgen->fname + (lflen - 1); (ch > cgen->fname) && (ch[-1] != '.'); ch--);
			if (ch > cgen->fname) {
				strcpy (ch, target->extn);
			} else {
				cgen->fname[lflen] = '.';
				strcpy (cgen->fname + lflen + 1, target->extn);
			}
		} else {
			cgen->fname = (char *)smalloc (16 + strlen (target->extn));
			sprintf (cgen->fname, "./default.%s", target->extn);
		}
	}

	/*}}}*/
	/*{{{  open output file*/
	cgen->fd = open (cgen->fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (cgen->fd < 0) {
		nocc_error ("failed to open %s for writing: %s", cgen->fname, strerror (errno));
		sfree (cgen->fname);
		sfree (cgen);
		return -1;
	}

	/*}}}*/
	/*{{{  initialise cryptographic stuffs*/
	if (compopts.hashalgo) {
		cgen->digest = crypto_newdigest ();
	}

	/*}}}*/
	/*{{{  initialise back-end code generation*/
	i = target->be_codegen_init (cgen, lf);

	if (i) {
		close (cgen->fd);
		sfree (cgen->fname);
		sfree (cgen);
		return i;
	}

	/*}}}*/
	/*{{{  do pre-codegen on the tree*/
	tnode_modprewalktree (cgen->cinsertpoint, codegen_modprewalk_precode, (void *)cgen);

	/*}}}*/
	/*{{{  generate code*/
	i = codegen_subcodegen (*(cgen->cinsertpoint), cgen);
	if (i) {
		nocc_error ("failed to generate code");
	}

	/*}}}*/
	/*{{{  shutdown back-end code generation*/
	target->be_codegen_final (cgen, lf);
	close (cgen->fd);
	cgen->fd = -1;

	/*{{{  now that we've written everything out, do postcalls*/
	{
		int j;

		for (j=0; j<DA_CUR (cgen->pcalls); j++) {
			codegen_pcall_t *pcall = DA_NTHITEM (cgen->pcalls, j);

			if (pcall) {
				if (!i && pcall->fcn) {			/* don't call if failed */
					pcall->fcn (cgen, pcall->arg);
				}
				sfree (pcall);
			}
		}
	}
	/*}}}*/

	dynarray_trash (cgen->tcgstates);
	dynarray_trash (cgen->be_blks);
	dynarray_trash (cgen->pcalls);
	sfree (cgen->fname);
	if (cgen->digest) {
		crypto_freedigest (cgen->digest);
		cgen->digest = NULL;
	}

	i = cgen->error;

#if 0
fprintf (stderr, "codegen_generate_code(): cgen->error = %d\n", cgen->error);
#endif
	sfree (cgen);
	/*}}}*/

	return i;
}
/*}}}*/
/*{{{  int precode_addtoprecodevars (tnode_t *tptr, tnode_t *node)*/
/*
 *	called to add entries to a node's 'precode:vars' list (before pre-coding takes place)
 *	returns 0 on success, non-zero on failure
 */
int precode_addtoprecodevars (tnode_t *tptr, tnode_t *node)
{
	chook_t *pchook = tnode_lookupornewchook ("precode:vars");
	tnode_t *hvars = (tnode_t *)tnode_getchook (tptr, pchook);

	if (hvars) {
		if (parser_islistnode (hvars)) {
			parser_addtolist (hvars, node);
		} else {
			tnode_t *tmp = parser_newlistnode (NULL);

			parser_addtolist (tmp, hvars);
			parser_addtolist (tmp, node);

			tnode_setchook (tptr, pchook, tmp);
		}
	} else {
		tnode_setchook (tptr, pchook, node);
	}
	return 0;
}
/*}}}*/
/*{{{  int precode_pullupprecodevars (tnode_t *dest_tptr, tnode_t *src_tptr)*/
/*
 *	called to pull-up 'precode:vars' from one node to another
 *	returns 0 on success, non-zero on failure
 */
int precode_pullupprecodevars (tnode_t *dest_tptr, tnode_t *src_tptr)
{
	chook_t *pchook = tnode_lookupornewchook ("precode:vars");
	tnode_t *hvars = (tnode_t *)tnode_getchook (src_tptr, pchook);

	if (hvars) {
		tnode_setchook (dest_tptr, pchook, hvars);
		tnode_setchook (src_tptr, pchook, NULL);
	}
	return 0;
}
/*}}}*/


/*{{{  int codegen_new_label (codegen_t *cgen)*/
/*
 *	returns a new label
 */
int codegen_new_label (codegen_t *cgen)
{
	return cgen->labcount++;
}
/*}}}*/


/*{{{  void codegen_nocoder (codegen_t *cgen, const char *op)*/
/*
 *	called when a coder-operation is used that doesn't actually exist
 */
void codegen_nocoder (codegen_t *cgen, const char *op)
{
	nocc_internal ("target %s does not support %s.", cgen->target->name, op);
	return;
}
/*}}}*/


/*{{{  static int codegen_icheck_node (tnode_t *node, ntdef_t *expected, codegen_t *cgen, int err)*/
/*
 *	checks to see if a given node is of the expected back-end type, if "err" is non-zero, throw
 *	an error if mismatch.  returns 0 on success (match), non-zero on error
 */
static int codegen_icheck_node (tnode_t *node, ntdef_t *expected, codegen_t *cgen, int err)
{
	if (!err) {
		return !(node->tag == expected);
	} else if (node->tag != expected) {
		codegen_error (cgen, "expected %s found %s", expected->name, node->tag->name);
	}
	return 0;
}
/*}}}*/
/*{{{  int codegen_check_beblock (tnode_t *node, codegen_t *cgen, int err)*/
/*
 *	checks to see if the given node is a back-end block
 *	returns 0 on success, non-zero on error
 */
int codegen_check_beblock (tnode_t *node, codegen_t *cgen, int err)
{
	return codegen_icheck_node (node, cgen->target->tag_BLOCK, cgen, err);
}
/*}}}*/
/*{{{  int codegen_check_beblockref (tnode_t *node, codegen_t *cgen, int err)*/
/*
 *	checks to see if the given node is a back-end block-reference
 *	returns 0 on success, non-zero on error
 */
int codegen_check_beblockref (tnode_t *node, codegen_t *cgen, int err)
{
	return codegen_icheck_node (node, cgen->target->tag_BLOCKREF, cgen, err);
}
/*}}}*/
/*{{{  int codegen_check_bename (tnode_t *node, codegen_t *cgen, int err)*/
/*
 *	checks to see if the given node is a back-end name
 *	returns 0 on success, non-zero on error
 */
int codegen_check_bename (tnode_t *node, codegen_t *cgen, int err)
{
	return codegen_icheck_node (node, cgen->target->tag_NAME, cgen, err);
}
/*}}}*/
/*{{{  int codegen_check_benameref (tnode_t *node, codegen_t *cgen, int err)*/
/*
 *	checks to see if the given node is a back-end name-reference
 *	returns 0 on success, non-zero on error
 */
int codegen_check_benameref (tnode_t *node, codegen_t *cgen, int err)
{
	return codegen_icheck_node (node, cgen->target->tag_NAMEREF, cgen, err);
}
/*}}}*/
/*{{{  int codegen_check_beconst (tnode_t *node, codegen_t *cgen, int err)*/
/*
 *	checks to see if the given node is a back-end constant
 *	returns 0 on success, non-zero on error
 */
int codegen_check_beconst (tnode_t *node, codegen_t *cgen, int err)
{
	return codegen_icheck_node (node, cgen->target->tag_CONST, cgen, err);
}
/*}}}*/
/*{{{  int codegen_check_beindexed (tnode_t *node, codegen_t *cgen, int err)*/
/*
 *	checks to see if the given node is a back-end indexed node
 *	returns 0 on success, non-zero on error
 */
int codegen_check_beindexed (tnode_t *node, codegen_t *cgen, int err)
{
	return codegen_icheck_node (node, cgen->target->tag_INDEXED, cgen, err);
}
/*}}}*/


/*{{{  void codegen_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node)*/
/*
 *	called in pre-code to set the name of the outermost PROC,
 *	called multiple times during the traversal
 */
void codegen_precode_seenproc (codegen_t *cgen, name_t *name, tnode_t *node)
{
	if (cgen->target && cgen->target->be_precode_seenproc) {
		cgen->target->be_precode_seenproc (cgen, name, node);
	}
	return;
}
/*}}}*/


/*{{{  int codegen_shutdown (void)*/
/*
 *	shuts-down the code-generator
 *	returns 0 on success, non-zero on failure
 */
int codegen_shutdown (void)
{
	return 0;
}
/*}}}*/
/*{{{  int codegen_init (void)*/
/*
 *	initialises the code-generator
 *	returns 0 on success, non-zero on failure
 */
int codegen_init (void)
{
	codegeninithook = tnode_lookupornewchook ("codegen:initialiser");
	codegeninithook->chook_copy = codegen_inithook_copy;
	codegeninithook->chook_free = codegen_inithook_free;
	codegeninithook->chook_dumptree = codegen_inithook_dumptree;
	codegenfinalhook = tnode_lookupornewchook ("codegen:finaliser");
	codegenfinalhook->chook_copy = codegen_finalhook_copy;
	codegenfinalhook->chook_free = codegen_finalhook_free;
	codegenfinalhook->chook_dumptree = codegen_finalhook_dumptree;

	return 0;
}
/*}}}*/


