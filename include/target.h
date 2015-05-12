/*
 *	target.h -- target interface/description for NOCC
 *	Copyright (C) 2005-2015 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __TARGET_H
#define __TARGET_H

struct TAG_tnode;
struct TAG_ntdef;
struct TAG_map;
struct TAG_codegen;
struct TAG_lexfile;
struct TAG_name;
struct TAG_betrans;
struct TAG_fhandle;


typedef struct TAG_target {
	int initialised;									/* set once! */
	char *name;
	char *tarch, *tvendor, *tos;								/* for target matching */
	char *desc;
	char *extn;										/* extension */

	struct {										/* target capabilities (bitfield) */
		unsigned int can_do_fp:1;							/* can do floating-point */
		unsigned int can_do_dmem:1;							/* can do dynamic memory */

	} tcap;

	struct {										/* below workspace sizes (for scheduler state) */
		unsigned int ds_min:8;								/* minimum below-workspace bytes for deschedule */
		unsigned int ds_io:8;								/* below-workspace bytes for channel I/O */
		unsigned int ds_altio:8;							/* below-workspace bytes for ALTs */
		unsigned int ds_wait:8;								/* below-workspace bytes for processes doing timeouts */
		unsigned int ds_max:8;								/* maximum below-workspace bytes for any process */
	} bws;

	struct {
		unsigned int as_alt:8;								/* above workspace bytes for ALT */
		unsigned int as_par:8;								/* above workspace bytes for PAR */
	} aws;

	int chansize;										/* number of bytes for a CHAN */
	int charsize;										/* number of bytes for a CHAR */
	int intsize;										/* number of bytes for an INT */
	int pointersize;									/* number of bytes for a pointer */
	int slotsize;										/* byte alignment for workspace slots */
	int structalign;									/* byte alignment for structured types */
	int maxfuncreturn;									/* maximum function return size (in slots) */
	int skipallocate;									/* whether allocation pass should be skipped */

	struct TAG_ntdef *tag_NAME;
	struct TAG_ntdef *tag_NAMEREF;
	struct TAG_ntdef *tag_BLOCK;
	struct TAG_ntdef *tag_CONST;
	struct TAG_ntdef *tag_INDEXED;
	struct TAG_ntdef *tag_BLOCKREF;
	struct TAG_ntdef *tag_STATICLINK;
	struct TAG_ntdef *tag_RESULT;


	int (*init)(struct TAG_target *target);							/* initialisation routine */

				/* creates a new back-end name, populated (fe-name, body, map-data, alloc-ws-high, alloc-ws-low, alloc-vs, alloc-ms, type-size, indirection) */
	struct TAG_tnode *(*newname)(struct TAG_tnode *, struct TAG_tnode *, struct TAG_map *, int, int, int, int, int, int);
				/* creates a new back-end name-ref, populated (be-name, map-data) */
	struct TAG_tnode *(*newnameref)(struct TAG_tnode *, struct TAG_map *);
				/* creates a back-end block, populated (body, map-data, statics-list, lexlevel) */
	struct TAG_tnode *(*newblock)(struct TAG_tnode *, struct TAG_map *, struct TAG_tnode *, int);
				/* creates a back-end constant, populated (body, map-data, data, size, typecat) */
	struct TAG_tnode *(*newconst)(struct TAG_tnode *, struct TAG_map *, void *, int, typecat_e);
				/* creates a back-end indexed node, populated (base, index, isize, offset) */
	struct TAG_tnode *(*newindexed)(struct TAG_tnode *, struct TAG_tnode *, int, int);
				/* creates a back-end block reference, populated (block, body, map-data) */
	struct TAG_tnode *(*newblockref)(struct TAG_tnode *, struct TAG_tnode *, struct TAG_map *);
				/* creates a back-end result node, semi-populated (expression, map-data) */
	struct TAG_tnode *(*newresult)(struct TAG_tnode *, struct TAG_map *);


				/* adds some back-end node to the sub-list of a result (node-ref, map-data) */
	void (*inresult)(struct TAG_tnode **, struct TAG_map *);


				/* extract the originating node associated with a back-end constant */
	struct TAG_tnode *(*be_getorgnode)(struct TAG_tnode *);
				/* return a pointer to the body within a back-end block */
	struct TAG_tnode **(*be_blockbodyaddr)(struct TAG_tnode *);
				/* back-end space requirements (node, wsh, wsl, vs, ms) */
	int (*be_allocsize)(struct TAG_tnode *, int *, int *, int *, int*);
				/* back-end type-size (stored in NAMEs and NAMEREFs), (typesize, indir) */
	int (*be_typesize)(struct TAG_tnode *, int *, int *);
				/* back-end type-category (node, typecat) */
	void (*be_settypecat)(struct TAG_tnode *, typecat_e);
				/* back-end type-category (node, typecat-ptr) */
	void (*be_gettypecat)(struct TAG_tnode *, typecat_e *);
				/* back-end offsets (node, ws-offset, vs-offset, ms-offset, ms-shadow) */
	void (*be_setoffsets)(struct TAG_tnode *, int, int, int, int);
				/* back-end offsets (node, ws-offset-ptr, vs-offset-ptr, ms-offset-ptr, ms-shadow-ptr) */
	void (*be_getoffsets)(struct TAG_tnode *, int *, int *, int *, int *);
				/* back-end lexlevel for a block or name (node) */
	int (*be_blocklexlevel)(struct TAG_tnode *);
				/* back-end set block size (node, ws-size, ws-offset, vs-size, ms-size, static-adjust) */
	void (*be_setblocksize)(struct TAG_tnode *, int, int, int, int, int);
				/* back-end get block size (node, ws-size-ptr, ws-offset-ptr, vs-size-ptr, ms-size-ptr, static-adjust-ptr, entry-lab-ptr) */
	void (*be_getblocksize)(struct TAG_tnode *, int *, int *, int *, int *, int *, int *);
				/* back-end code-generate initialise */
	int (*be_codegen_init)(struct TAG_codegen *, struct TAG_lexfile *);
				/* back-end code-generate finalise */
	int (*be_codegen_final)(struct TAG_codegen *, struct TAG_lexfile *);

				/* pre-code visiting top-level processes */
	void (*be_precode_seenproc)(struct TAG_codegen *, struct TAG_name *, struct TAG_tnode *);

				/* compiler-wide hook for back-end transformation (BETRANS) pass */
	void (*be_do_betrans)(struct TAG_tnode **, struct TAG_betrans *);
				/* compiler-wide hook for pre-mapping (PREMAP) pass */
	void (*be_do_premap)(struct TAG_tnode **, struct TAG_map *);
				/* compiler-wide hook for name-mapping (NAMEMAP) pass */
	void (*be_do_namemap)(struct TAG_tnode **, struct TAG_map *);
				/* compiler-wide hook for back-end mapping (BEMAP) pass */
	void (*be_do_bemap)(struct TAG_tnode **, struct TAG_map *);
				/* compiler-wide hook for pre-allocation (PREALLOCATE) pass */
	void (*be_do_preallocate)(struct TAG_tnode *, struct TAG_target *);
				/* compiler-wide hook for pre-codegen (PRECODE) pass */
	void (*be_do_precode)(struct TAG_tnode **, struct TAG_codegen *);
				/* compiler-wide hook for codegen (CODEGEN) pass */
	void (*be_do_codegen)(struct TAG_tnode *, struct TAG_codegen *);

	void *priv;
} target_t;


extern int target_register (target_t *target);
extern int target_unregister (target_t *target);
extern target_t *target_lookupbyspec (char *tarch, char *tvendor, char *tos);
extern void target_dumptargets (struct TAG_fhandle *stream);

extern int target_initialise (target_t *target);

extern int target_init (void);
extern int target_shutdown (void);

#endif	/* !__TARGET_H */

