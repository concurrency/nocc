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
#include <stdarg.h>
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

typedef struct TAG_gtArray {
	void *ptr;			/* pointer to actual data */
	int ndim;			/* number of dimensions */
	int *dimsizes;			/* array of dimension sizes (0..(ndim-1)) */
	void *tdesc;			/* the suitably encoded type descriptor */
	int tsize;			/* subtype size (as per allocation for 'ptr', so sizeof(void *) for most pointers) */
} gtArray_t;

/*}}}*/

#if 0
/*{{{  debugging..*/
static inline void GuppyPrintString (Workspace wptr, const char *cll, gtString_t *str)
{
	ExternalCallN (fprintf, 9, stderr, "0x%8.8x: %s(): str @0x%8.8x, ptr=0x%8.8x, stype=%d, slen=%d, alen=%d\n", (unsigned int)wptr, cll,
			(unsigned int)str, str ? (unsigned int)str->ptr : -1, str ? str->stype : -1, str ? str->slen : -1, str ? str->alen : -1);
}
/*}}}*/
#endif

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
#if 0
GuppyPrintString (wptr, "GuppyStringInit", str);
#endif

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
#if 0
GuppyPrintString (wptr, "GuppyStringFree", str);
#endif
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
/*{{{  static inline void GuppyStringEmpty (Workspace wptr, gtString_t *str)*/
/*
 *	destroys the content of an existing string, but not the string structure itself.
 */
static inline void GuppyStringEmpty (Workspace wptr, gtString_t *str)
{
	if (str) {
#if 0
GuppyPrintString (wptr, "GuppyStringEmpty", str);
#endif
		if (str->alen) {
			MRelease (wptr, str->ptr);
			str->alen = 0;
		}
		str->ptr = NULL;
		str->slen = 0;
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

#if 0
GuppyPrintString (wptr, "GuppyStringConstInitialiser", str);
#endif
	return str;
}
/*}}}*/
/*{{{  static inline void GuppyStringAssign (Workspace wptr, gtString_t **dst, gtString_t *src)*/
/*
 *	does an assignment from one string to another
 */
static inline void GuppyStringAssign (Workspace wptr, gtString_t **dst, gtString_t *src)
{
#if 0
ExternalCallN (fprintf, 4, stderr, "GuppyStringAssign: src=%p *dst=%p\n", src, *dst);
//GuppyPrintString (wptr, "GuppyStringAssign(src)", src);
//GuppyPrintString (wptr, "GuppyStringAssign(*dst)", *dst);
#endif
	if (!*dst) {
		*dst = GuppyStringInit (wptr);
	} else if ((*dst)->alen) {
		/* something here, better free it nicely */
		MRelease (wptr, (*dst)->ptr);
		(*dst)->ptr = NULL;
		(*dst)->alen = 0;
	}

	if (!src->alen && src->ptr) {
		/* statically allocated string, alias it gleefully */
		(*dst)->ptr = src->ptr;
		(*dst)->alen = 0;
	} else if (src->alen && src->ptr) {
		/* dynamically allocated, duplicate it */
		int i;

		(*dst)->ptr = (char *)MAlloc (wptr, src->alen);
		(*dst)->alen = src->alen;
		for (i=0; i<src->slen; i++) {
			(*dst)->ptr[i] = src->ptr[i];
		}
		(*dst)->ptr[src->slen] = '\0';					/* self-sanity, debugging purposes */
	} else {
		/* empty string */
		(*dst)->ptr = NULL;
		(*dst)->alen = 0;
	}
	(*dst)->slen = src->slen;
	(*dst)->stype = src->stype;
#if 0
ExternalCallN (fprintf, 4, stderr, "GuppyStringAssign: src=%p *dst=%p\n", src, *dst);
// GuppyPrintString (wptr, "GuppyStringAssign(*dst)", *dst);
#endif
}
/*}}}*/
/*{{{  static inline void GuppyStringConcat (Workspace wptr, gtString_t *dst, gtString_t *src1, gtString_t *src2)*/
/*
 *	concatanates two strings
 */
static inline void GuppyStringConcat (Workspace wptr, gtString_t *dst, gtString_t *src1, gtString_t *src2)
{
	int slen, i;

#if 0
GuppyPrintString (wptr, "GuppyStringConcat(src1)", src1);
GuppyPrintString (wptr, "GuppyStringConcat(src2)", src2);
GuppyPrintString (wptr, "GuppyStringConcat(dst)", dst);
#endif
	GuppyStringEmpty (wptr, dst);

	slen = src1->slen + src2->slen;
	dst->alen = slen + 1;
	dst->ptr = (char *)MAlloc (wptr, dst->alen);
	dst->slen = slen;
	dst->stype = src1->stype;

	for (i=0; i<src1->slen; i++) {
		dst->ptr[i] = src1->ptr[i];
	}
	for (i=0; i<src2->slen; i++) {
		dst->ptr[i+src1->slen] = src2->ptr[i];
	}
	dst->ptr[dst->slen] = '\0';
#if 0
GuppyPrintString (wptr, "GuppyStringConcat(dst)", dst);
#endif
}
/*}}}*/
/*{{{  static inline void GuppyStringClear (Workspace wptr, gtString_t **str)*/
/*
 *	clears a string (after output)
 */
static inline void GuppyStringClear (Workspace wptr, gtString_t **str)
{
#if 0
GuppyPrintString (wptr, "GuppyStringClear(*str)", *str);
#endif
	*str = GuppyStringInit (wptr);
}
/*}}}*/

/*{{{  static inline gtArray_t *GuppyArrayInit (Workspace wptr)*/
/*
 *	initialises an array, no allocation (just returns NULL in practice).
 */
static inline gtArray_t *GuppyArrayInit (Workspace wptr)
{
	return NULL;
}
/*}}}*/
/*{{{  static inline gtArray_t *GuppyArrayInitAlloc (Workspace wptr, int ndim, int tsize, void *tdesc, ...)*/
/*
 *	initialises an array and allocates it.
 */
static inline gtArray_t *GuppyArrayInitAlloc (Workspace wptr, int ndim, int tsize, void *tdesc, ...)
{
	/* XXX: for efficiency, should tuck dimsizes behind array structure */
	gtArray_t *ary = (gtArray_t *)MAlloc (wptr, sizeof (gtArray_t));
	va_list ap;
	int talloc = tsize;
	int i;

	ary->ndim = ndim;
	ary->tsize = tsize;
	ary->tdesc = tdesc;
	ary->dimsizes = (int *)MAlloc (wptr, ndim * sizeof (int));

	va_start (ap, tdesc);
	for (i=0; i<ndim; i++) {
		int dsize = va_arg (ap, int);

		ary->dimsizes[i] = dsize;
		talloc *= dsize;
	}
	ary->ptr = MAlloc (wptr, talloc);
	memset (ary->ptr, 0, talloc);

	return ary;
}
/*}}}*/
/*{{{  static inline void GuppyArrayFree (Workspace wptr, gtArray_t *ary)*/
/*
 *	releases an array.
 */
static inline void GuppyArrayFree (Workspace wptr, gtArray_t *ary)
{
	if (!ary) {
		return;
	}
	if (ary->ptr) {
		MRelease (wptr, ary->ptr);
		ary->ptr = NULL;
	}
	if (ary->dimsizes) {
		MRelease (wptr, ary->dimsizes);
		ary->dimsizes = NULL;
	}
	MRelease (wptr, ary);
}
/*}}}*/

#define GUPPYTYPEDARRAYPTR(T,A) ((T*)((A)->ptr))


/* END verb-header.h */


