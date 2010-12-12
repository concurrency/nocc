/*
 *	guppy.h -- Guppy language interface for nocc
 *	Copyright (C) 2010 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __GUPPY_H
#define __GUPPY_H

struct TAG_langlexer;
struct TAG_langparser;
struct TAG_langdef;

extern struct TAG_langlexer guppy_lexer;
extern struct TAG_langparser guppy_parser;

/* node-type and node-tag flag values */
#define NTF_BOOLOP			0x0010		/* boolean operator flag */
#define NTF_SYNCTYPE			0x0020		/* synchronisation type */
#define NTF_INDENTED_PROC_LIST		0x0040		/* for TNF_LONGPROCs, parse a list of indented processes into subnode 1 */
#define NTF_INDENTED_PROC		0x0080		/* for TNF_LONGPROCs, parse an indented process into subnode 1 */
							/* for TNF_LONGDECLs, parse an indented process into subnode 2 */

/* implementation-specific language-tag bits */
#define LANGTAG_STYPE			0x00010000	/* sized type (e.g. int8) */


struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;
struct TAG_chook;

typedef struct {
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_NAMETYPENODE;
	struct TAG_tndef *node_NAMEPROTOCOLNODE;
	struct TAG_tndef *node_LEAFNODE;
	struct TAG_tndef *node_TYPENODE;

	struct TAG_ntdef *tag_BOOL;
	struct TAG_ntdef *tag_BYTE;
	struct TAG_ntdef *tag_INT;			/* caters for all integer types (sizes and signedness) */
	struct TAG_ntdef *tag_REAL;			/* caters for all real types (sizes) */
	struct TAG_ntdef *tag_CHAR;
	struct TAG_ntdef *tag_STRING;

	struct TAG_ntdef *tag_CHAN;
	struct TAG_ntdef *tag_NAME;

	struct TAG_ntdef *tag_FCNDEF;
	struct TAG_ntdef *tag_VARDECL;
	struct TAG_ntdef *tag_FPARAM;

	struct TAG_ntdef *tag_SKIP;
	struct TAG_ntdef *tag_STOP;

	struct TAG_token *tok_ATSIGN;
	struct TAG_token *tok_STRING;

} guppy_pset_t;

extern guppy_pset_t gup;

typedef struct {
	struct TAG_tnode *last_type;			/* used when handling things like "int a, b, c" */
	int procdepth;					/* procedure/function nesting depth for public/non-public names */
} guppy_prescope_t;

typedef struct {
	int procdepth;					/* procedure/function nesting depth */
	struct TAG_tnode **insertpoint;			/* where nested procedures get unwound to */
} guppy_betrans_t;

typedef struct {
	void *data;
	int bytes;
} guppy_litdata_t;


extern void guppy_isetindent (FILE *stream, int indent);
extern struct TAG_langdef *guppy_getlangdef (void);

/* front-end units */
extern struct TAG_feunit guppy_primproc_feunit;		/* guppy_primproc.c */
extern struct TAG_feunit guppy_fcndef_feunit;		/* guppy_fcndef.c */
extern struct TAG_feunit guppy_decls_feunit;		/* guppy_decls.c */
extern struct TAG_feunit guppy_types_feunit;		/* guppy_types.c */

/* these are for language units to use in reductions */
extern void *guppy_nametoken_to_hook (void *ntok);
extern void *guppy_stringtoken_to_namehook (void *ntok);

/* option handlers inside front-end */
struct TAG_cmd_option;
extern int guppy_lexer_opthandler_flag (struct TAG_cmd_option *opt, char ***argwalk, int *argleft);


#endif	/* !__GUPPY_H */
