/*
 *	guppy_cccsp_lib.c -- routines for Guppy and CIF/CCSP
 *	Fred Barnes, April 2013.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <cif.h>


void process_screen (Workspace wptr)
{
	Channel *link = ProcGetParam (wptr, 0, Channel *);

	for (;;) {
		char ch;

		ChanIn (wptr, link, &ch, 1);
		ExternalCallN (printf, 2, "%c", ch);
	}
}



