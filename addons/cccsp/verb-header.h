/* BEGIN verb-header.h */

/*
 *	This is the stock verbatim header that gets included in NOCC-generated
 *	CCCSP code for Guppy.  Exposes some bits of CIF and provides inline
 *	low-level string handling.
 *
 *	Copyright (C) 2013 Fred Barnes, University of Kent <frmb@kent.ac.uk>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <cif.h>

/* wrapper for free-if-not-null (generic dynamically allocated type handling, part of) */
#define MReleaseChk(Wptr,P) do { if ((P) != NULL) { MRelease (Wptr, P); P = NULL; } } while (0)

/*{{{  gtString_t: string type wrapper*/

#define STYPE_ASCII	0		/* ASCII string (plain bytes) */
#define STYPE_UTF8	1		/* UTF8 strings (plain bytes) */
#define STYPE_UTF16	2		/* UTF16 strings (word-ordered 16-bit) */
#define STYPE_UNICODE	3		/* 32-bit UNICODE characters */

typedef struct TAG_gtString {
	char *ptr;			/* pointer to actual string data */
	int stype;			/* string character type */
	int slen;			/* string length (bytes) */
	int alen;			/* allocation length (bytes) */
} gtString_t;


/*}}}*/

/*{{{  static inline gtString_t *GuppyStringInit (Workspace wptr)*/
/*
 *	creates a new/blank string.
 */
static inline gtString_t *GuppyStringInit (Workspace wptr)
{
	gtString_t *str = (gtString_t *)MAlloc (wptr, sizeof (gtString_t));

	str->ptr = NULL;
	str->stype = STYPE_ASCII;
	str->slen = 0;
	str->alen = 0;

	return str;
}
/*}}}*/
/*{{{  static inline void GuppyStringFree (Workspace wptr, gtString_t *str)*/
/*
 *	destroys an existing string, frees if necessary.
 */
static inline void GuppyStringFree (Workspace wptr, gtString_t *str)
{
	if (str) {
		if (str->alen) {
			MRelease (wptr, str->ptr);
			str->alen = 0;
		}
		str->ptr = NULL;
		str->slen = 0;
		MRelease (wptr, str);
	}
}
/*}}}*/
/*{{{  static inline gtString_t *GuppyStringConstInitialiser (Workspace wptr, const char *text, const int slen)*/
/*
 *	constant string initialiser, puts the 'text' pointer into a new string (doesn't copy contents, but won't try and free either).
 */
static inline gtString_t *GuppyStringConstInitialiser (Workspace wptr, const char *text, const int slen)
{
	gtString_t *str = (gtString_t *)MAlloc (wptr, sizeof (gtString_t));

	str->ptr = (char *)text;
	str->stype = STYPE_ASCII;
	str->slen = slen;
	str->alen = 0;

	return str;
}
/*}}}*/
/*{{{  static inline void GuppyStringAssign (Workspace wptr, gtString_t **dst, gtString_t *src)*/
/*
 *	does an assignment from one string to another
 */
static inline void GuppyStringAssign (Workspace wptr, gtString_t **dst, gtString_t *src)
{
	if (!*dst) {
		*dst = GuppyStringInit (wptr);
	}
	if ((*dst)->alen) {
		/* something here, better free it nicely */
		MRelease (wptr, (*dst)->ptr);
		(*dst)->alen = 0;
	}
	if (!src->alen && src->ptr) {
		/* statically allocated string, alias it gleefully */
		(*dst)->ptr = src->ptr;
	} else if (src->alen && src->ptr) {
		/* dynamically allocated, duplicate it */
		(*dst)->ptr = (char *)MAlloc (wptr, src->alen);
		memcpy ((*dst)->ptr, src->ptr, src->slen);			/* specifically just the ones we care about */
		(*dst)->ptr[src->slen] = '\0';					/* self-sanity, debugging purposes */
	} else {
		/* empty string */
		(*dst)->ptr = NULL;
	}
	(*dst)->slen = src->slen;
	(*dst)->stype = src->stype;
}
/*}}}*/

/* END verb-header.h */


