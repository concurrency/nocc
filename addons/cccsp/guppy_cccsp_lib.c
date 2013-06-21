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



