/*
 *	occampi.h -- occam-pi language interface for nocc
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

#ifndef __OCCAMPI_H
#define __OCCAMPI_H

struct TAG_langlexer;
struct TAG_langparser;

extern struct TAG_langlexer occampi_lexer;
extern struct TAG_langparser occampi_parser;

/* node-type and node-tag flag values */
#define TNF_NONE	0x0000
#define TNF_LONGPROC	0x0001
#define TNF_LONGDECL	0x0002
#define TNF_SHORTDECL	0x0004

#define NTF_NONE	0x0000

struct TAG_tndef;
struct TAG_ntdef;
struct TAG_token;


typedef struct {
	struct TAG_tndef *node_NAMENODE;
	struct TAG_tndef *node_LEAFNODE;
	struct TAG_tndef *node_TYPENODE;

	struct TAG_ntdef *tag_BYTE;
	struct TAG_ntdef *tag_INT;
	struct TAG_ntdef *tag_INT16;
	struct TAG_ntdef *tag_INT32;
	struct TAG_ntdef *tag_INT64;
	struct TAG_ntdef *tag_REAL32;
	struct TAG_ntdef *tag_REAL64;
	struct TAG_ntdef *tag_CHAR;
	struct TAG_ntdef *tag_CHAN;
	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_VARDECL;
	struct TAG_ntdef *tag_FPARAM;
	struct TAG_ntdef *tag_PROCDECL;
	struct TAG_ntdef *tag_SKIP;
	struct TAG_ntdef *tag_STOP;
	struct TAG_ntdef *tag_SEQ;
	struct TAG_ntdef *tag_PAR;
	struct TAG_ntdef *tag_ASSIGN;
	struct TAG_ntdef *tag_INPUT;
	struct TAG_ntdef *tag_OUTPUT;
	struct TAG_ntdef *tag_HIDDENPARAM;
	struct TAG_ntdef *tag_RETURNADDRESS;

	struct TAG_ntdef *tag_LITBYTE;
	struct TAG_ntdef *tag_LITINT;
	struct TAG_ntdef *tag_LITREAL;
	struct TAG_ntdef *tag_LITARRAY;

	struct TAG_ntdef *tag_NDECL;
	struct TAG_ntdef *tag_NPARAM;
	struct TAG_ntdef *tag_NPROCDEF;

	struct TAG_ntdef *tag_PINSTANCE;
	struct TAG_ntdef *tag_FINSTANCE;

	struct TAG_token *tok_COLON;
} occampi_pset_t;

extern occampi_pset_t opi;


struct TAG_tnode;
struct TAG_prescope;
struct TAG_scope;
struct TAG_feunit;


typedef struct {
	struct TAG_tnode *last_type;
} occampi_prescope_t;

typedef struct {
	void *data;
	int bytes;
} occampi_litdata_t;

extern void occampi_isetindent (FILE *stream, int indent);


extern struct TAG_feunit occampi_primproc_feunit;	/* occampi_primproc.c */
extern struct TAG_feunit occampi_cnode_feunit;		/* occampi_cnode.c */
extern struct TAG_feunit occampi_decl_feunit;		/* occampi_decl.c */
extern struct TAG_feunit occampi_action_feunit;		/* occampi_action.c */
extern struct TAG_feunit occampi_lit_feunit;		/* occampi_lit.c */
extern struct TAG_feunit occampi_type_feunit;		/* occampi_type.c */
extern struct TAG_feunit occampi_instance_feunit;	/* occampi_instance.c */


#endif	/* !__OCCAMPI_H */

