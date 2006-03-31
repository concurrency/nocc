/*
 *	krocvrt.c -- back-end routines for KRoC "virtual register transputer" target
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "tnode.h"
#include "opts.h"
#include "lexer.h"
#include "parser.h"
#include "treeops.h"
#include "names.h"
#include "target.h"
#include "map.h"
#include "transputer.h"
#include "codegen.h"
#include "allocate.h"

/*}}}*/
/*{{{  target_t for this target*/
target_t krocvrt_target = {
	initialised:	0,
	name:		"krocvrt",
	tarch:		"vrt",
	tvendor:	"kroc",
	tos:		NULL,
	desc:		"KRoC virtual register transputer code",
	extn:		"vrt",
	tcap: {
		can_do_fp: 1,
		can_do_dmem: 1
	},
	bws: {
		ds_min: 12,
		ds_io: 16,
		ds_altio: 16,
		ds_wait: 24,
		ds_max: 24
	},
	aws: {
		as_alt: 4,
		as_par: 12
	},
	chansize:	4,
	charsize:	1,
	intsize:	4,
	pointersize:	4,
	slotsize:	4,
	structalign:	4,
	maxfuncreturn:	3,

	tag_NAME:	NULL,
	tag_NAMEREF:	NULL,
	tag_BLOCK:	NULL,
	tag_CONST:	NULL,
	tag_INDEXED:	NULL,
	tag_BLOCKREF:	NULL,
	tag_STATICLINK:	NULL,
	tag_RESULT:	NULL,

	init:		NULL,
	newname:	NULL,
	newnameref:	NULL,
	newblock:	NULL,
	newconst:	NULL,
	newindexed:	NULL,
	newblockref:	NULL,
	newresult:	NULL,
	inresult:	NULL,

	be_blockbodyaddr:	NULL,
	be_allocsize:		NULL,
	be_typesize:		NULL,
	be_setoffsets:		NULL,
	be_getoffsets:		NULL,
	be_blocklexlevel:	NULL,
	be_setblocksize:	NULL,
	be_getblocksize:	NULL,
	be_codegen_init:	NULL,
	be_codegen_final:	NULL,
	
	be_precode_seenproc:	NULL,

	priv:		NULL
}
/*}}}*/


