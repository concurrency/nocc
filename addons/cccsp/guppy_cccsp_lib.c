/*
 *	guppy_cccsp_lib.c -- routines for Guppy and CIF/CCSP
 *	Fred Barnes, 2013.
 */

#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <cif.h>
#endif

#include <cccsp/verb-header.h>

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

void gcf_debug_printf (Workspace wptr, gtString_t *s)
{
	ExternalCallN (printf, 2, "%s", s->ptr);
}


