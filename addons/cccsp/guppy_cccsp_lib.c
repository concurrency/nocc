/*
 *	guppy_cccsp_lib.c -- routines for Guppy and CIF/CCSP
 *	Fred Barnes, 2013-2015.
 */

#include <cccsp/verb-header.h>


/*{{{  void gcf_guppy_screen_process (Workspace wptr, Channel *in)*/
/* @APICALLCHAIN: gcf_guppy_screen_process: =? */
/*
 *	dummy (never actually used directly) but present to shut nocc up
 */
void gcf_guppy_screen_process (Workspace wptr, Channel *in)
{
	return;
}
/*}}}*/
/*{{{  void gproc_guppy_screen_process (Workspace wptr)*/
/* @APICALLCHAIN: gproc_guppy_screen_process: =?, ProcGetParam, ChanIn, ExternalCallN, GuppyStringFree */
/*
 *	the top-level screen process: receives strings and printf's them.
 */
void gproc_guppy_screen_process (Workspace wptr)
{
	Channel *link = ProcGetParam (wptr, 0, Channel *);

	for (;;) {
		gtString_t *s = NULL;

		ChanIn (wptr, link, &s, 4);
		ExternalCallN (printf, 2, "%s", s->ptr);
		GuppyStringFree (wptr, s);
	}
}
/*}}}*/
/*{{{  void gcf_int_to_str (Workspace wptr, gtString_t **strp, const int n)*/
/* @APICALLCHAIN: gcf_int_to_str: =?, GuppyStringFree, GuppyStringInit, MAlloc, ExternalCallN */
/*
 *	converts an integer to a string
 */
void gcf_int_to_str (Workspace wptr, gtString_t **strp, const int n)
{
	char *ch;

	GuppyStringFree (wptr, *strp);
	*strp = GuppyStringInit (wptr);
	(*strp)->alen = 32;
	(*strp)->ptr = (char *)MAlloc (wptr, (*strp)->alen);

	ExternalCallN (snprintf, 4, (*strp)->ptr, 31, "%d", n);
	for ((*strp)->slen = 0, ch = (*strp)->ptr; *ch != '\0'; (*strp)->slen++, ch++);
	// (*strp)->slen = strlen ((*strp)->ptr);
}
/*}}}*/
/*{{{  void gcf_debug_printf (Workspace wptr, gtString_t *s)*/
/* @APICALLCHAIN: gcf_debug_printf: =?, ExternalCallN */
/*
 *	simple debugging for programs.
 */
void gcf_debug_printf (Workspace wptr, gtString_t *s)
{
	ExternalCallN (printf, 2, "%s", s->ptr);
}
/*}}}*/


/*{{{  void guppy_cccsp_lib_dummy_str (Workspace wptr)*/
/*
 *	this is some dead-code used to force generation of stack sizing for inlined code in verb-header.h
 */
void guppy_cccsp_lib_dummy_str (Workspace wptr)
{
	gtString_t *s1, *s2, *s3;

	s1 = GuppyStringInit (wptr);
	s3 = GuppyStringInit (wptr);
	GuppyStringEmpty (wptr, s1);
	s2 = GuppyStringConstInitialiser (wptr, "foo", 3);
	GuppyStringAssign (wptr, &s3, s1);
	GuppyStringConcat (wptr, s3, s1, s2);
	GuppyStringClear (wptr, &s2);
	GuppyStringFree (wptr, s1);
}
/*}}}*/
/*{{{  void guppy_cccsp_lib_dummy_array (Workspace wptr)*/
/*
 *	some more dead-code for arrays
 */
void guppy_cccsp_lib_dummy_array (Workspace wptr)
{
	gtArray_t *ary;

	ary = GuppyArrayInit (wptr);
	ary = GuppyArrayInitAlloc (wptr, 1, 4, (void *)NULL, 42);
	GuppyArrayFree (wptr, ary);
}
/*}}}*/

