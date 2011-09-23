/*
 *	options.c -- command-line option processing
 *	Copyright (C) 2004-2006 Fred Barnes <frmb@kent.ac.uk>
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
#include <sys/types.h>
#include <unistd.h>

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "opts.h"

/*{{{  forward decls*/
static int opt_do_help_flag (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_do_version (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_setintflag (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_setintflagup (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_clearintflag (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_setstopflag (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_setstr (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_setsaveopt (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_addincludepath (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_addlibrarypath (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_addextnpath (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_addextn (cmd_option_t *opt, char ***argwalk, int *argleft);
static int opt_settarget (cmd_option_t *opt, char ***argwalk, int *argleft);


/*}}}*/

#include "gperf_options.h"

/*{{{  private vars*/
/* options array by short-option */
STATICDYNARRAY (cmd_option_t *, icharopts);

/* added options */
STATICSTRINGHASH (cmd_option_t *, extraopts, 3);

/* ordered help */
STATICDYNARRAY (cmd_option_t *, ordered_options);

/*}}}*/

/*{{{  built-in option processors*/
/*{{{  static int opt_do_help_flag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets the "dohelp" flag in compopts, allowing other bits of the compiler to add
 *	options before we print them out.
 */
static int opt_do_help_flag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	compopts.dohelp = opt;
	return 0;
}
/*}}}*/
/*{{{  static int opt_do_version (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	prints out the compiler version, and exits
 */
static int opt_do_version (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	FILE *outstream = (opt->arg) ? (FILE *)(opt->arg) : stderr;

	fprintf (outstream, "%s\n", version_string());
	fflush (outstream);

	exit (0);
	return 0;
}
/*}}}*/
/*{{{  static int opt_setintflag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets an integer flag
 */
static int opt_setintflag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int *flag = (int *)(opt->arg);

	if (!flag) {
		return -1;
	}
	*flag = 1;
	return 0;
}
/*}}}*/
/*{{{  static int opt_setintflagup (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets or increments an integer flag
 */
static int opt_setintflagup (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int *flag = (int *)(opt->arg);

	if (!flag) {
		return -1;
	}
	*flag = *flag + 1;
	return 0;
}
/*}}}*/
/*{{{  static int opt_clearintflag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	clears an integer flag
 */
static int opt_clearintflag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int *flag = (int *)(opt->arg);

	if (!flag) {
		return -1;
	}
	*flag = 0;
	return 0;
}
/*}}}*/
/*{{{  static int opt_setstopflag (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets a stop flag -- stop compiler at some pre-defined point
 */
static int opt_setstopflag (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	compopts.stoppoint = (int)(opt->arg);
	return 0;
}
/*}}}*/
/*{{{  static int opt_setstr (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets a string
 */
static int opt_setstr (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	char **starget = (char **)(opt->arg);
	char *ch;

	ch = strchr (**argwalk, '=');
	if (ch) {
		ch++;
	} else {
		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		ch = **argwalk;
	}
	ch = string_dup (ch);
	if (*starget) {
		sfree (*starget);
	}
	*starget = ch;
	return 0;
}
/*}}}*/
/*{{{  static int opt_setsaveopt (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets a string
 */
static int opt_setsaveopt (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int cmd = (int)(opt->arg);

	if (cmd == 1) {
		/* save named DFA to file */
		char *dfaname, *fname;

		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		dfaname = **argwalk;

		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-2]);
			return -1;
		}
		fname = **argwalk;

		if (compopts.savenameddfa[0]) {
			sfree (compopts.savenameddfa[0]);
		}
		if (compopts.savenameddfa[1]) {
			sfree (compopts.savenameddfa[1]);
		}
		compopts.savenameddfa[0] = string_dup (dfaname);
		compopts.savenameddfa[1] = string_dup (fname);
	} else {
		nocc_error ("don\'t know how to process option %s", (*argwalk)[-1]);
		return -1;
	}

	return 0;
}
/*}}}*/
/*{{{  static int opt_addincludepath (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	adds paths to the list of places to look for include files
 *	returns 0 on success, non-zero on failure
 */
static int opt_addincludepath (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	char *ch, *dh, *eh;

	ch = strchr (**argwalk, '=');
	if (ch) {
		ch++;
	} else {
		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		ch = **argwalk;
	}

	/* ch is now the argument */
	ch = string_dup (ch);

	for (dh = ch; dh; dh = eh) {
		eh = strchr (dh, ':');
		if (eh) {
			*eh = '\0';
			eh++;
		}
		dynarray_add (compopts.ipath, string_dup (dh));
	}

	sfree (ch);

	return 0;
}
/*}}}*/
/*{{{  static int opt_addlibrarypath (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	adds paths to the list of places to look for library files
 *	returns 0 on success, non-zero on faolure
 */
static int opt_addlibrarypath (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	char *ch, *dh, *eh;

	ch = strchr (**argwalk, '=');
	if (ch) {
		ch++;
	} else {
		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		ch = **argwalk;
	}

	/* ch is now the argument */
	ch = string_dup (ch);

	for (dh = ch; dh; dh = eh) {
		eh = strchr (dh, ':');
		if (eh) {
			*eh = '\0';
			eh++;
		}
		dynarray_add (compopts.lpath, string_dup (dh));
	}

	sfree (ch);

	return 0;
}
/*}}}*/
/*{{{  static int opt_addextnpath (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	adds paths to the list of places to look for compiler extensions
 *	returns 0 on success, non-zero on faolure
 */
static int opt_addextnpath (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	char *ch, *dh, *eh;

	ch = strchr (**argwalk, '=');
	if (ch) {
		ch++;
	} else {
		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		ch = **argwalk;
	}

	/* ch is now the argument */
	ch = string_dup (ch);

	for (dh = ch; dh; dh = eh) {
		eh = strchr (dh, ':');
		if (eh) {
			*eh = '\0';
			eh++;
		}
		dynarray_add (compopts.epath, string_dup (dh));
	}

	sfree (ch);

	return 0;
}
/*}}}*/
/*{{{  static int opt_addextn (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	adds an extension name to the list of extensions to load
 *	returns 0 on success, non-zero on faolure
 */
static int opt_addextn (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	char *ch, *dh, *eh;

	ch = strchr (**argwalk, '=');
	if (ch) {
		ch++;
	} else {
		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		ch = **argwalk;
	}

	/* ch is now the argument */
	ch = string_dup (ch);

	for (dh = ch; dh; dh = eh) {
		eh = strchr (dh, ':');
		if (eh) {
			*eh = '\0';
			eh++;
		}
		dynarray_add (compopts.eload, string_dup (dh));
	}

	sfree (ch);

	return 0;
}
/*}}}*/
/*{{{  static int opt_settarget (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	sets the compiler target
 */
static int opt_settarget (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	char *ch, *dh, *eh;

	ch = strchr (**argwalk, '=');
	if (ch) {
		ch++;
	} else {
		(*argwalk)++;
		(*argleft)--;
		if (!**argwalk || !*argleft) {
			nocc_error ("missing argument for option %s", (*argwalk)[-1]);
			return -1;
		}
		ch = **argwalk;
	}
	ch = string_dup (ch);

	dh = strchr (ch, '-');
	if (!dh) {
		nocc_error ("malformed target [%s]", ch);
		sfree (ch);
		return -1;
	}
	eh = strchr (dh + 1, '-');
	if (!eh) {
		nocc_error ("malformed target [%s]", ch);
		sfree (ch);
		return -1;
	}

	if (compopts.target_cpu) {
		sfree (compopts.target_cpu);
		compopts.target_cpu = NULL;
	}
	compopts.target_cpu = string_ndup (ch, (int)(dh - ch));

	if (compopts.target_vendor) {
		sfree (compopts.target_vendor);
		compopts.target_vendor = NULL;
	}
	compopts.target_vendor = string_ndup (dh + 1, (int)(eh - (dh + 1)));

	if (compopts.target_os) {
		sfree (compopts.target_os);
		compopts.target_os = NULL;
	}
	compopts.target_os = string_dup (eh + 1);

	if (compopts.target_str) {
		sfree (compopts.target_str);
		compopts.target_str = ch;
	}

	return 0;
}
/*}}}*/
/*}}}*/


/*{{{  void opt_do_help (cmd_option_t *opt, char ***argwalk, int *argleft)*/
int opt_do_help (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	FILE *outstream = (opt->arg) ? (FILE *)(opt->arg) : stderr;
	int i;

	fprintf (outstream, "nocc (%s) Version " VERSION " " HOST_CPU "-" HOST_VENDOR "-" HOST_OS " (targetting " TARGET_CPU "-" TARGET_VENDOR "-" TARGET_OS ")\n", progname);
	fprintf (outstream, "Copyright (C) 2004-2010 Fred Barnes, University of Kent\n");
	fprintf (outstream, "Released under the terms and conditions of the GNU GPL v2\n\n");
	fflush (outstream);
	fprintf (outstream, "usage:  %s [options] <source filename>\n", progname);
	fprintf (outstream, "options:\n");

	for (i = 0; i < DA_CUR (ordered_options); i++) {
		if (ordered_options[i] && ordered_options[i]->name && ordered_options[i]->help && (ordered_options[i]->help[0] <= opt->help[0])) {
			char *htext = ordered_options[i]->help + 1;

			fprintf (outstream, "    ");
			if (ordered_options[i]->sopt != '\0') {
				fprintf (outstream, "-%c  ", ordered_options[i]->sopt);
			} else {
				fprintf (outstream, "    ");
			}
			fprintf (outstream, "--%-32s%s\n", ordered_options[i]->name, htext);
		}
	}
	fprintf (outstream, "note: some options require --opt=arg for arguments\n");
	
	exit (0);
	return 0;
}
/*}}}*/
/*{{{  static int opts_compare_option (cmd_option_t *opt1, cmd_option_t *opt2)*/
/*
 *	compares two options for ordering (used when sorting)
 *	returns logical difference
 */
static int opts_compare_option (cmd_option_t *opt1, cmd_option_t *opt2)
{
	if (opt1 == opt2) {
		return 0;
	} else if (!opt1) {
		return -1;
	} else if (!opt2) {
		return 1;
	}
	return opt1->order - opt2->order;
}
/*}}}*/


/*{{{  void opts_init (void)*/
/*
 *	initialises option processing
 */
void opts_init (void)
{
	int i;
	int optsize = (MAX_HASH_VALUE - MIN_HASH_VALUE) + 1;
	int ordcount;

	dynarray_init (ordered_options);
	dynarray_setsize (ordered_options, optsize);
	dynarray_init (icharopts);
	dynarray_setsize (icharopts, 256);

	for (i=0; i<optsize; i++) {
		ordered_options[i] = NULL;
	}
	for (i = MIN_HASH_VALUE, ordcount = 0; i <= MAX_HASH_VALUE; i++, ordcount++) {
		if (wordlist[i].name && (wordlist[i].sopt != '\0')) {
			if (DA_NTHITEM (icharopts, (int)(wordlist[i].sopt))) {
				nocc_warning ("duplicate short option `%c\'", wordlist[i].sopt);
			} else {
				DA_SETNTHITEM (icharopts, (int)(wordlist[i].sopt), (cmd_option_t *)&(wordlist[i]));
			}
		}
		if (wordlist[i].order > -1) {
			ordered_options[ordcount] = (cmd_option_t *)&(wordlist[i]);
		}
	}

	dynarray_qsort (ordered_options, opts_compare_option);

	stringhash_sinit (extraopts);
	
	return;
}
/*}}}*/
/*{{{  cmd_option_t *opts_getlongopt (const char *optname)*/
/*
 *	returns option structure for a long-option (leading "--" not expected)
 */
cmd_option_t *opts_getlongopt (const char *optname)
{
	int optlen = strlen (optname);
	cmd_option_t *opt;
	char *ch;

	ch = strchr (optname, '=');
	if (ch) {
		optlen = (ch - optname);
	}
	opt = (cmd_option_t *)option_lookup_byname (optname, optlen);
	if (!opt) {
		ch = string_ndup ((char *)optname, optlen);

		opt = stringhash_lookup (extraopts, ch);
		sfree (ch);
	}
	return opt;
}
/*}}}*/
/*{{{  cmd_option_t *opts_getshortopt (const char optchar)*/
/*
 *	returns option structure for a short-option (any leading "-" not expected)
 */
cmd_option_t *opts_getshortopt (const char optchar)
{
	if (optchar < 0) {
		return NULL;
	}
	return DA_NTHITEM (icharopts, (int)optchar);
}
/*}}}*/
/*{{{  int opts_process (cmd_option_t *opt, char ***arg_walk, int *arg_left)*/
/*
 *	called to process an option
 */
int opts_process (cmd_option_t *opt, char ***arg_walk, int *arg_left)
{
	if (!opt || !opt->opthandler) {
		nocc_internal ("unhandled option (%s)", (arg_walk && **arg_walk) ? **arg_walk : "?");
		return 1;
	}
	return opt->opthandler (opt, arg_walk, arg_left);
}
/*}}}*/
/*{{{  void opts_add (const char *optname, const char optchar, int (*opthandler)(cmd_option_t *, char ***, int *), void *arg, char *help)*/
/*
 *	adds an option to the compiler
 */
void opts_add (const char *optname, const char optchar, int (*opthandler)(cmd_option_t *, char ***, int *), void *arg, char *help)
{
	cmd_option_t *tmpopt = (cmd_option_t *)smalloc (sizeof (cmd_option_t));

	tmpopt->name = string_dup ((char *)optname);
	tmpopt->sopt = optchar;
	tmpopt->opthandler = opthandler;
	tmpopt->arg = arg;
	tmpopt->help = string_dup (help);

	stringhash_insert (extraopts, tmpopt, tmpopt->name);

	/* add to ordered options */
	dynarray_add (ordered_options, tmpopt);

	return;
}
/*}}}*/


