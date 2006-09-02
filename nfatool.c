/*
 *	nfatool.c -- occam-compiler NFA tool (generates DFAs for the compiler)
 *	Copyright (C) 2004-2005 Fred Barnes <frmb@kent.ac.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#include "nocc.h"
#include "support.h"
#include "keywords.h"
#include "symbols.h"
#include "tnode.h"
#include "xml.h"


/*{{{  global variables*/
char *progname = NULL;

compopts_t compopts = {
	verbose: 0,
	notmainmodule: 0,
	dmemdump: 0,
	dumpspecs: 0,
	dumptree: 0,
	specsfile: NULL,
	DA_CONSTINITIALISER(epath),
	DA_CONSTINITIALISER(ipath),
	DA_CONSTINITIALISER(lpath),
	target_str: NULL,
	target_cpu: NULL,
	target_os: NULL,
	target_vendor: NULL
};

/*}}}*/
/*{{{  private variables for nfatool*/
static char *outfile = NULL;

/*}}}*/


/*{{{  global report routines*/
/*{{{  void nocc_internal (char *fmt, ...)*/
/*
 *	called to report a fatal internal error (compiler-wide)
 */
void nocc_internal (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: internal error: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	/* do a banner for these */
	fprintf (stderr, "\n");
	fprintf (stderr, "***************************************************\n");
	fprintf (stderr, "  this is probably a compiler-bug;  please report  \n");
	fprintf (stderr, "  to:  ofa-bugs@kent.ac.uk  with a copy of the     \n");
	fprintf (stderr, "  code that caused the error.                      \n");
	fprintf (stderr, "***************************************************\n");
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	exit (EXIT_FAILURE);
	return;
}
/*}}}*/
/*{{{  void nocc_fatal (char *fmt, ...)*/
/*
 *	called to report a fatal error (compiler-wide)
 */
void nocc_fatal (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: fatal error: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	exit (EXIT_FAILURE);
	return;
}
/*}}}*/
/*{{{  void nocc_error (char *fmt, ...)*/
/*
 *	called to report an error (compiler-wide)
 */
void nocc_error (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: error: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	return;
}
/*}}}*/
/*{{{  void nocc_warning (char *fmt, ...)*/
/*
 *	called to report a warning (compiler-wide)
 */
void nocc_warning (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: warning: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	return;
}
/*}}}*/
/*{{{  void nocc_message (char *fmt, ...)*/
/*
 *	called to report a message (compiler-wide)
 */
void nocc_message (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	return;
}
/*}}}*/
/*{{{  void nocc_outerrmsg (char *string)*/
/*
 *	called to output errors/warnings/etc. for other compiler parts
 */
void nocc_outerrmsg (char *string)
{
	fprintf (stderr, "%s\n", string);
	return;
}
/*}}}*/
/*}}}*/


/*{{{  static void nfatool_do_help (FILE *stream)*/
/*
 *	show usage information for nfatool
 */
static void nfatool_do_help (FILE *stream)
{
	fprintf (stream, "%s (nfatool) Version " VERSION ": NFA tool for nocc\n", progname);
	fprintf (stream, "Usage:\n    %s [options] <file1> [file2 ...]\n", progname);
	fprintf (stream, "where options are:\n");
	fprintf (stream, "  --help | -h                show help and exit\n");
	fprintf (stream, "  --version | -V             show version and exit\n");
	fprintf (stream, "  --output | -o  <file>      send output to <file>\n");
	fprintf (stream, "  --verbose | -v             verbose operation\n");

	exit (EXIT_SUCCESS);
	return;
}
/*}}}*/
/*{{{  static void nfatool_do_version (FILE *stream)*/
/*
 *	show version information for nfatool
 */
static void nfatool_do_version (FILE *stream)
{
	fprintf (stream, "%s (nfatool) " VERSION "\n", progname);

	exit (EXIT_SUCCESS);
	return;
}
/*}}}*/
/*{{{  static void nfatool_do_setstring (char ***walk, int *i, char **target)*/
/*
 *	sets a string option from command-line params
 */
static void nfatool_do_setstring (char ***walk, int *i, char **target)
{
	(*walk)++;
	(*i)--;
	if (!*i || !**walk) {
		nocc_error ("missing argument for option %s", (*walk)[-1]);
		exit (EXIT_FAILURE);
	}
	if (*target) {
		sfree (*target);
	}
	*target = string_dup (**walk);
	return;
}
/*}}}*/


/*{{{  int main (int argc, char **argv)*/
/*
 *	start here
 */
int main (int argc, char **argv)
{
	char **walk;
	int i;
	int errored;

	char **srcfiles = NULL;
	int nsrcfiles = 0;
	
	/*{{{  basic initialisation*/
	for (progname=*argv + (strlen (*argv) - 1); (progname > *argv) && (progname[-1] != '/'); progname--);
	dmem_init ();

	/*}}}*/
	/*{{{  general initialisation*/
#ifdef DEBUG
	nocc_message ("DEBUG: compiler initialisation");
#endif
	keywords_init ();
	symbols_init ();
	tnode_init ();
	xml_init ();
	xmlkeys_init ();

	/*}}}*/
	/*{{{  process command-line arguments*/
	errored = 0;

	for (walk = argv + 1, i = argc - 1; *walk && i; walk++, i--) {
		switch (**walk) {
		case '-':
			if ((*walk)[1] == '-') {
				char *opt = *walk + 2;

				if (!strcmp (opt, "help")) {
					nfatool_do_help (stdout);
				} else if (!strcmp (opt, "version")) {
					nfatool_do_version (stdout);
				} else if (!strcmp (opt, "output")) {
					nfatool_do_setstring (&walk, &i, &outfile);
				} else if (!strcmp (opt, "verbose")) {
					compopts.verbose++;
				} else if (!strcmp (opt, "dump-dmem")) {
					compopts.dmemdump = 1;
				} else {
					nocc_error ("unknown option: %s", *walk);
					errored++;
				}
			} else {
				char *ch;

				for (ch = *walk + 1; *ch != '\0'; ch++) {
					switch (*ch) {
					case 'h':
						nfatool_do_help (stdout);
						break;
					case 'V':
						nfatool_do_version (stdout);
						break;
					case 'v':
						compopts.verbose++;
						break;
					case 'o':
						nfatool_do_setstring (&walk, &i, &outfile);
						break;
					default:
						nocc_error ("unknown option: -%c (in: %s)", *ch, *walk);
						errored++;
						break;
					}
				}
			}
			break;
		default:
			/* it's something else -- filename probably */
			if (!srcfiles) {
				srcfiles = (char **)smalloc (argc * sizeof (char *));
			}
			srcfiles[nsrcfiles++] = *walk;
			break;
		}
	}

	if (errored) {
		nocc_fatal ("error processing command-line options");
		exit (EXIT_FAILURE);
	}


	/*}}}*/
	/*{{{  look for filenames given on the command-line*/
	if (!nsrcfiles) {
		nocc_fatal ("no input files!");
		exit (EXIT_FAILURE);
	}
	/*}}}*/

	/*{{{  process file(s) */
	for (i=0; i<nsrcfiles; i++) {
		char *fname = srcfiles[i];

		nocc_message ("processing: %s", fname);
	}
	/*}}}*/

	/*{{{  shutdown/etc.*/
	if (compopts.dmemdump) {
		dmem_usagedump ();
	}
	dmem_shutdown ();

	/*}}}*/
	return EXIT_SUCCESS;
}
/*}}}*/


