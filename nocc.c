/*
 *	nocc.c -- new occam-pi compiler (harness)
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

/*{{{  includes*/
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
#include "opts.h"
#include "fcnlib.h"
#include "crypto.h"
#include "keywords.h"
#include "symbols.h"
#include "lexer.h"
#include "parser.h"
#include "parsepriv.h"
#include "langdef.h"
#include "dfa.h"
#include "prescope.h"
#include "precheck.h"
#include "scope.h"
#include "typecheck.h"
#include "constprop.h"
#include "tnode.h"
#include "treecheck.h"
#include "names.h"
#include "treeops.h"
#include "feunit.h"
#include "aliascheck.h"
#include "usagecheck.h"
#include "defcheck.h"
#include "postcheck.h"
#include "library.h"
#include "fetrans.h"
#include "betrans.h"
#include "extn.h"
#include "xml.h"
#include "map.h"
#include "allocate.h"
#include "target.h"
#include "transputer.h"
#include "codegen.h"
#include "occampi_fe.h"
#include "mcsp_fe.h"
#include "rcxb_fe.h"
#include "hopp_fe.h"
#include "version.h"

/*}}}*/


/*{{{  global variables*/
char *progname = NULL;
compopts_t compopts = {
	progpath: NULL,
	verbose: 0,
	notmainmodule: 0,
	dohelp: NULL,
	dmemdump: 0,
	dumpspecs: 0,
	dumptree: 0,
	dumptreeto: NULL,
	dumpstree: 0,
	dumpstreeto: NULL,
	dumpgrammar: 0,
	dumpgrules: 0,
	dumpdfas: 0,
	dumpnames: 0,
	dumptargets: 0,
	dumpvarmaps: 0,
	dumpnodetypes: 0,
	dumpextns: 0,
	dumpfolded: 0,
	dumptracemem: 0,
	debugparser: 0,
	stoppoint: 0,
	tracetypecheck: 0,
	traceparser: 0,
	traceprecode: 0,
	treecheck: 0,
	doaliascheck: 1,
	dousagecheck: 1,
	dodefcheck: 1,
	specsfile: NULL,
	outfile: NULL,
	savenameddfa: {NULL, NULL},
	savealldfas: NULL,
	DA_CONSTINITIALISER(epath),
	DA_CONSTINITIALISER(ipath),
	DA_CONSTINITIALISER(lpath),
	DA_CONSTINITIALISER(eload),
	maintainer: NULL,
	target_str: NULL,
	target_cpu: NULL,
	target_os: NULL,
	target_vendor: NULL,
	hashalgo: NULL,
	privkey: NULL,
	gperf_p: NULL,
	gprolog_p: NULL
};

/*}}}*/
/*{{{  private types*/

typedef struct TAG_compilerpass {
	char *name;			/* of this pass */
	void *origin;
	int (*fcn)(void *);		/* pointer to pass function (arguments fudged) */
	comppassarg_t fargs;		/* bitfield describing the arguments required */
	int stoppoint;
	int *flagptr;			/* whether this pass is enabled */
} compilerpass_t;

typedef struct TAG_xmlnamespace {
	char *name;			/* of this namespace */
	char *uri;			/* associated URI (nice to have something valid on the end) */
} xmlnamespace_t;


/*}}}*/
/*{{{  private data*/
STATICDYNARRAY (char *, be_def_opts);
static int noccexitflag = 0;

STATICDYNARRAY (compilerpass_t *, cfepasses);
STATICDYNARRAY (compilerpass_t *, cbepasses);

STATICDYNARRAY (xmlnamespace_t *, xmlnamespaces);

/*}}}*/


/*{{{  static int nocc_shutdownrun (void)*/
/*
 *	called to call general shut-downs, on both success and error paths
 *	returns 0 on success, non-zero on failure
 */
static int nocc_shutdownrun (void)
{
	int v = 0;

	/* compiler framework shutdowns in reverse order from initialisations */
	if (crypto_shutdown ()) {
		v++;
	}
	if (target_shutdown ()) {
		v++;
	}
	if (codegen_shutdown ()) {
		v++;
	}
	if (allocate_shutdown ()) {
		v++;
	}
	if (map_shutdown ()) {
		v++;
	}
	if (betrans_shutdown ()) {
		v++;
	}
	if (fetrans_shutdown ()) {
		v++;
	}
	if (postcheck_shutdown ()) {
		v++;
	}
	if (defcheck_shutdown ()) {
		v++;
	}
	if (usagecheck_shutdown ()) {
		v++;
	}
	if (aliascheck_shutdown ()) {
		v++;
	}
	if (precheck_shutdown ()) {
		v++;
	}
	if (constprop_shutdown ()) {
		v++;
	}
	if (library_shutdown ()) {
		v++;
	}
	if (treeops_shutdown ()) {
		v++;
	}
	if (extn_shutdown ()) {
		v++;
	}
	if (name_shutdown ()) {
		v++;
	}
	if (scope_shutdown ()) {
		v++;
	}
	if (prescope_shutdown ()) {
		v++;
	}
	if (feunit_shutdown ()) {
		v++;
	}
	if (parser_shutdown ()) {
		v++;
	}
	if (dfa_shutdown ()) {
		v++;
	}
	if (langdef_shutdown ()) {
		v++;
	}
	if (tnode_shutdown ()) {
		v++;
	}
	if (treecheck_shutdown ()) {
		/* NOTE: do this after tnode cleanup */
		v++;
	}
	if (lexer_shutdown ()) {
		v++;
	}
	if (symbols_shutdown ()) {
		v++;
	}

	/* extra things which should be cleaned up properly, but initialised earlier in the compiler */
	if (fcnlib_shutdown ()) {
		v++;
	}

	return v;
}
/*}}}*/


/*{{{  global report routines*/
/*{{{  void nocc_xinternal (char *fmt, ...)*/
/*
 *	called to report a fatal internal error (compiler-wide)
 */
void nocc_xinternal (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: internal error: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	/* do a banner for these */
	fprintf (stderr, "\n");
	fprintf (stderr, "*******************************************************\n");
	fprintf (stderr, "  this is probably a compiler-bug;  please report to:  \n");
	fprintf (stderr, "      %s\n", compopts.maintainer);
	fprintf (stderr, "  with a copy of the code that caused the error.       \n");
	fprintf (stderr, "*******************************************************\n");
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	nocc_shutdownrun ();
	exit (EXIT_FAILURE);
	return;
}
/*}}}*/
/*{{{  void nocc_pinternal (char *fmt, const char *file, const int line, ...)*/
/*
 *	called to report a fatal internal error (compiler-wide)
 *	includes location in source where generated
 */
void nocc_pinternal (char *fmt, const char *file, const int line, ...)
{
	va_list ap;

	va_start (ap, line);
	fprintf (stderr, "%s: internal error at %s:%d : ", progname, file ? file : "<unknown>", file ? line : -1);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	/* do a banner for these */
	fprintf (stderr, "\n");
	fprintf (stderr, "*******************************************************\n");
	fprintf (stderr, "  this is probably a compiler-bug;  please report to:  \n");
	fprintf (stderr, "      %s\n", compopts.maintainer);
	fprintf (stderr, "  with a copy of the code that caused the error.       \n");
	fprintf (stderr, "*******************************************************\n");
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
	nocc_shutdownrun ();
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
	nocc_shutdownrun ();
	exit (EXIT_FAILURE);
	return;
}
/*}}}*/
/*{{{  void nocc_serious (char *fmt, ...)*/
/*
 *	called to report a serious, but not necessarily fatal, error (compiler-wide)
 */
void nocc_serious (char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "%s: **serious**: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (ap);
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
/*{{{  void nocc_cleanexit (void)*/
/*
 *	clean-exit, called by some option-handlers if they're just doing one-shot things
 */
void nocc_cleanexit (void)
{
	noccexitflag++;
	return;
}
/*}}}*/
/*}}}*/
/*{{{  specification file handling*/
STATICDYNARRAY (xmlkey_t *, specfilekeys);
STATICDYNARRAY (char *, specfiledata);

/*{{{  static void specfile_setcomptarget (char *target)*/
/*
 *	sets the compiler target
 */
static void specfile_setcomptarget (char *target)
{
	char *ch, *dh;
	int i;
	char **srefs[] = {&compopts.target_str, &compopts.target_cpu, &compopts.target_vendor, &compopts.target_os, NULL};

	for (i=0; srefs[i]; i++) {
		if (*(srefs[i])) {
			sfree (*(srefs[i]));
			*(srefs[i]) = NULL;
		}
	}
	if (compopts.target_str) {
		sfree (compopts.target_str);
	}
	compopts.target_str = string_dup (target);
#if 0
fprintf (stderr, "specfile_setcomptarget(): setting compiler target to [%s]\n", compopts.target_str);
#endif

	/* break up into bits */
	ch = compopts.target_str;
	for (dh = ch; (*dh != '-') && (*dh != '\0'); dh++);
	if (*dh == '\0') {
		nocc_warning ("badly formed target \"%s\" in specs file, ignoring", compopts.target_str);
		return;
	}
	compopts.target_cpu = string_ndup (ch, (int)(dh - ch));
	for (ch = ++dh; (*dh != '-') && (*dh != '\0'); dh++);
	if (*dh == '\0') {
		nocc_warning ("badly formed target \"%s\" in specs file, ignoring", compopts.target_str);
		return;
	}
	compopts.target_vendor = string_ndup (ch, (int)(dh - ch));
	dh++;
	if (*dh == '\0') {
		nocc_warning ("badly formed target \"%s\" in specs file, ignoring", compopts.target_str);
		return;
	}
	compopts.target_os = string_dup (dh);

#if 0
fprintf (stderr, "specfile_setcomptarget(): full target is [%s] [%s] [%s]\n", compopts.target_cpu, compopts.target_vendor, compopts.target_os);
#endif

	return;
}
/*}}}*/
/*{{{  static void specfile_setmaintainer (char *data)*/
/*
 *	sets the compiler maintainer
 */
static void specfile_setmaintainer (char *data)
{
	if (compopts.maintainer) {
		sfree (compopts.maintainer);
	}
	compopts.maintainer = string_dup (data);
	return;
}
/*}}}*/
/*{{{  static void specfile_sethashalgo (char *edata)*/
/*
 *	sets the output hashing algorithm (included in .xlb and .xlo files)
 */
static void specfile_sethashalgo (char *edata)
{
	if (compopts.hashalgo) {
		sfree (compopts.hashalgo);
	}
	compopts.hashalgo = string_dup (edata);
	return;
}
/*}}}*/
/*{{{  static void specfile_setprivkey (char *edata)*/
/*
 *	sets the private-key file used for signing
 */
static void specfile_setprivkey (char *edata)
{
	if (compopts.privkey) {
		sfree (compopts.privkey);
	}
	compopts.privkey = string_dup (edata);
	return;
}
/*}}}*/
/*{{{  static void specfile_setstring (char **target, char *edata)*/
/*
 *	sets an arbitrary string in the config file (copies argument)
 */
static void specfile_setstring (char **target, char *edata)
{
	if (*target) {
		sfree (*target);
	}
	*target = string_dup (edata);
	return;
}
/*}}}*/

/*{{{  static void specfile_init (xmlhandler_t *xh)*/
/*
 *	init callback
 */
static void specfile_init (xmlhandler_t *xh)
{
	dynarray_init (specfilekeys);
	dynarray_init (specfiledata);
	return;
}
/*}}}*/
/*{{{  static void specfile_final (xmlhandler_t *xh)*/
/*
 *	finalise callback
 */
static void specfile_final (xmlhandler_t *xh)
{
	dynarray_trash (specfilekeys);
	dynarray_trash (specfiledata);
	return;
}
/*}}}*/
/*{{{  static void specfile_elem_start (xmlhandler_t *xh, void *data, xmlkey_t *key, xmlkey_t **attrkeys, const char **attrvals)*/
/*
 *	element start callback
 */
static void specfile_elem_start (xmlhandler_t *xh, void *data, xmlkey_t *key, xmlkey_t **attrkeys, const char **attrvals)
{
	dynarray_add (specfilekeys, key);
	dynarray_add (specfiledata, NULL);
	return;
}
/*}}}*/
/*{{{  static void specfile_elem_end (xmlhandler_t *xh, void *data, xmlkey_t *key)*/
/*
 *	element end callback
 */
static void specfile_elem_end (xmlhandler_t *xh, void *data, xmlkey_t *key)
{
	char *edata;

	dynarray_delitem (specfilekeys, DA_CUR(specfilekeys) - 1);
	edata = DA_NTHITEM (specfiledata, DA_CUR (specfiledata) - 1);
	if (edata) {
		/* some data to process */
		switch (key->type) {
		case XMLKEY_TARGET:				/* setting compiler target */
			specfile_setcomptarget (edata);
			sfree (edata);
			break;
		case XMLKEY_EPATH:				/* adding an extension path to the compiler */
			dynarray_add (compopts.epath, edata);
			break;
		case XMLKEY_EXTN:				/* adding an extension name to load */
			dynarray_add (compopts.eload, edata);
			break;
		case XMLKEY_IPATH:				/* adding an include path to the compiler */
			dynarray_add (compopts.ipath, edata);
			break;
		case XMLKEY_LPATH:				/* adding a library path to the compiler */
			dynarray_add (compopts.lpath, edata);
			break;
		case XMLKEY_MAINTAINER:				/* setting compiler maintainer (email-address) */
			specfile_setmaintainer (edata);
			sfree (edata);
			break;
		case XMLKEY_HASHALGO:				/* setting the output hashing algorithm */
			specfile_sethashalgo (edata);
			sfree (edata);
			break;
		case XMLKEY_PRIVKEY:				/* setting the location of the private key */
			specfile_setprivkey (edata);
			sfree (edata);
			break;
		case XMLKEY_GPERF:
			specfile_setstring (&compopts.gperf_p, edata);
			sfree (edata);
			break;
		case XMLKEY_GPROLOG:
			specfile_setstring (&compopts.gprolog_p, edata);
			sfree (edata);
			break;
		default:
			nocc_warning ("unknown setting %s in specs file ignored", key->name);
			sfree (edata);
			break;
		}
	}
	dynarray_delitem (specfiledata, DA_CUR(specfiledata) - 1);
	return;
}
/*}}}*/
/*{{{  static void specfile_data (xmlhandler_t *xh, void *data, const char *text, int len)*/
/*
 *	character data callback
 */
static void specfile_data (xmlhandler_t *xh, void *data, const char *text, int len)
{
	if (!DA_CUR (specfiledata)) {
		nocc_warning ("top-level data in specs file ignored");
		return;
	}

	if (DA_NTHITEM (specfiledata, DA_CUR (specfiledata) - 1)) {
		/* adding to existing data */
		char *orig = DA_NTHITEM (specfiledata, DA_CUR (specfiledata) - 1);
		int olen = strlen (orig);
		char *dcopy = (char *)smalloc (olen + len + 1);

		memcpy (dcopy, orig, olen);
		memcpy (dcopy + olen, text, len);
		dcopy[len + olen] = '\0';
		DA_SETNTHITEM (specfiledata, DA_CUR (specfiledata) - 1, dcopy);
		sfree (orig);
	} else {
		/* fresh data */
		DA_SETNTHITEM (specfiledata, DA_CUR (specfiledata) - 1, string_ndup ((char *)text, len));
	}
	return;
}
/*}}}*/


/*}}}*/
/*{{{  extras*/
/*{{{  static void maybedumptrees (char **fnames, int nfnames, tnode_t **trees, int ntrees)*/
/*
 *	maybe dumps the parse tree to a file
 */
static void maybedumptrees (char **fnames, int nfnames, tnode_t **trees, int ntrees)
{
	int i;

	if (compopts.dumptree) {
		fprintf (stderr, "<nocc:treedump version=\"%s\">\n", version_string ());
		for (i=0; i<ntrees; i++) {
			fprintf (stderr, "    <nocc:parsetree src=\"%s\">\n", fnames[i]);
			tnode_dumptree (trees[i], 2, stderr);
			fprintf (stderr, "    </nocc:parsetree>\n");
		}
		fprintf (stderr, "</nocc:treedump>\n");
	} else if (compopts.dumptreeto) {
		FILE *stream;

		stream = fopen (compopts.dumptreeto, "w");
		if (!stream) {
			nocc_error ("failed to open %s for writing: %s", compopts.dumptreeto, strerror (errno));
		} else {
			int j;

			/* dump XML namespaces to start with */
			for (j=0; j<DA_CUR (xmlnamespaces); j++) {
				xmlnamespace_t *xmlns = DA_NTHITEM (xmlnamespaces, j);

				fprintf (stream, "<%s:namespace xmlns:%s=\"%s\">\n", xmlns->name, xmlns->name, xmlns->uri);
			}

			fprintf (stream, "<nocc:treedump version=\"%s\">\n", version_string ());
			for (i=0; i<ntrees; i++) {
				fprintf (stream, "    <nocc:parsetree src=\"%s\">\n", fnames[i]);
				tnode_dumptree (trees[i], 2, stream);
				fprintf (stream, "    </nocc:parsetree>\n");
			}
			fprintf (stream, "</nocc:treedump>\n");

			/* finish off XML namespaces */
			for (j--; j>=0; j--) {
				xmlnamespace_t *xmlns = DA_NTHITEM (xmlnamespaces, j);

				fprintf (stream, "</%s:namespace>\n", xmlns->name);
			}

			fclose (stream);
		}
	} else if (compopts.dumpstree) {
		fprintf (stderr, "(nocc:treedump (version \"%s\")\n", version_string ());
		for (i=0; i<ntrees; i++) {
			fprintf (stderr, "  (nocc:parsetree (src \"%s\")\n", fnames[i]);
			tnode_dumpstree (trees[i], 2, stderr);
			fprintf (stderr, "  )\n");
		}
		fprintf (stderr, ")\n");
	} else if (compopts.dumpstreeto) {
		FILE *stream;

		stream = fopen (compopts.dumpstreeto, "w");
		if (!stream) {
			nocc_error ("failed to open %s for writing: %s", compopts.dumpstreeto, strerror (errno));
		} else {
			fprintf (stderr, "(nocc:treedump (version \"%s\")\n", version_string ());
			for (i=0; i<ntrees; i++) {
				fprintf (stderr, "  (nocc:parsetree (src \"%s\")\n", fnames[i]);
				tnode_dumpstree (trees[i], 2, stderr);
				fprintf (stderr, "  )\n");
			}
			fprintf (stderr, ")\n");

			fclose (stream);
		}
	}
	return;
}
/*}}}*/
/*}}}*/


/*{{{  int nocc_dooption (char *optstr)*/
/*
 *	called to trigger an option (always assumed to be a long option)
 *	will add to back-end options if cannot process immediately
 *	returns 0 on success (called now), 1 if deferred, -1 on error
 */
int nocc_dooption (char *optstr)
{
	char **lclopts = (char **)smalloc (2 * sizeof (char *));
	cmd_option_t *opt = NULL;
	int left = 1;

#if 0
	nocc_message ("nocc_dooption(): optstr = [%s]", optstr);
#endif
	lclopts[0] = optstr;
	lclopts[1] = NULL;
	opt = opts_getlongopt (optstr);
	if (opt) {
		if (opts_process (opt, (char ***)(&lclopts), &left) < 0) {
			nocc_error ("failed while processing option \"%s\"", optstr);
			sfree (lclopts);
			return -1;
		}
	} else {
		/* defer for back-end processing */
		char *str = (char *)smalloc (strlen (optstr) + 3);

		strcpy (str + 2, optstr);
		str[0] = '-';
		str[1] = '-';
		dynarray_add (be_def_opts, str);
		sfree (lclopts);
		return 1;
	}
	sfree (lclopts);
	return 0;
}
/*}}}*/
/*{{{  int nocc_dooption_arg (char *optstr, void *arg)*/
/*
 *	called to trigger an option (always assumed to be a long option)
 *	will add to back-end options if cannot process immediately
 *	called internally, also passes a pointer argument (usually the lexfile_t from which it originated)
 *	returns 0 on success (called now), 1 if deferred, -1 on error
 */
int nocc_dooption_arg (char *optstr, void *arg)
{
	char **lclopts = (char **)smalloc (3 * sizeof (char *));
	char **lcbase = lclopts;
	cmd_option_t *opt = NULL;
	int left = 2;

	lclopts[0] = optstr;
	lclopts[1] = (char *)arg;
	lclopts[2] = NULL;

	opt = opts_getlongopt (optstr);
	if (opt) {
		if (opts_process (opt, &lclopts, &left) < 0) {
			nocc_error ("failed while processing option \"%s\"", optstr);
			sfree (lcbase);
			return -1;
		}
	} else {
		/* defer for back-end processing */
		char *str = (char *)smalloc (strlen (optstr) + 3);

		strcpy (str + 2, optstr);
		str[0] = '-';
		str[1] = '-';
		dynarray_add (be_def_opts, str);
		sfree (lcbase);
		return 1;
	}
	sfree (lcbase);
	return 0;
}
/*}}}*/


/*{{{  static compilerpass_t *nocc_new_compilerpass (const char *name, void *origin, int (*fcn)(void *), comppassarg_t fargs, int spoint, int *flagptr)*/
/*
 *	creates a new compilerpass_t structure (initialised)
 */
static compilerpass_t *nocc_new_compilerpass (const char *name, void *origin, int (*fcn)(void *), comppassarg_t fargs, int spoint, int *flagptr)
{
	compilerpass_t *cpass = (compilerpass_t *)smalloc (sizeof (compilerpass_t));

	cpass->name = string_dup (name);
	cpass->origin = origin;
	cpass->fcn = fcn;
	cpass->fargs = fargs;
	cpass->stoppoint = spoint;
	cpass->flagptr = flagptr;

	return cpass;
}
/*}}}*/
/*{{{  static int nocc_init_cpasses (void)*/
/*
 *	initialises the default passes in the compiler
 *	returns 0 on success, non-zero on failure
 */
static int nocc_init_cpasses (void)
{
	/* stock front-end passes */
	dynarray_add (cfepasses, nocc_new_compilerpass ("pre-scope", NULL, (int (*)(void *))prescope_tree, CPASS_TREEPTR | CPASS_LANGPARSER, 3, NULL));
	dynarray_add (cfepasses, nocc_new_compilerpass ("scope", NULL, (int (*)(void *))scope_tree, CPASS_TREE | CPASS_LANGPARSER, 4, NULL));
	dynarray_add (cfepasses, nocc_new_compilerpass ("type-check", NULL, (int (*)(void *))typecheck_tree, CPASS_TREE | CPASS_LANGPARSER, 5, NULL));
	dynarray_add (cfepasses, nocc_new_compilerpass ("const-prop", NULL, (int (*)(void *))constprop_tree, CPASS_TREEPTR, 6, NULL));
	dynarray_add (cfepasses, nocc_new_compilerpass ("pre-check", NULL, (int (*)(void *))precheck_tree, CPASS_TREE, 7, NULL));
	dynarray_add (cfepasses, nocc_new_compilerpass ("alias-check", NULL, (int (*)(void *))aliascheck_tree, CPASS_TREE | CPASS_LANGPARSER, 8, &(compopts.doaliascheck)));
	dynarray_add (cfepasses, nocc_new_compilerpass ("usage-check", NULL, (int (*)(void *))usagecheck_tree, CPASS_TREE | CPASS_LANGPARSER, 9, &(compopts.dousagecheck)));
	dynarray_add (cfepasses, nocc_new_compilerpass ("def-check", NULL, (int (*)(void *))defcheck_tree, CPASS_TREE | CPASS_LANGPARSER, 10, &(compopts.dodefcheck)));
	dynarray_add (cfepasses, nocc_new_compilerpass ("post-check", NULL, (int (*)(void *))postcheck_tree, CPASS_TREEPTR | CPASS_LANGPARSER, 11, NULL));
	dynarray_add (cfepasses, nocc_new_compilerpass ("fetrans", NULL, (int (*)(void *))fetrans_tree, CPASS_TREEPTR | CPASS_LANGPARSER, 12, NULL));
	nocc_addxmlnamespace ("fetrans", "http://www.cs.kent.ac.uk/projects/ofa/nocc/NAMESPACES/fetrans");

	/* stock back-end passes */
	dynarray_add (cbepasses, nocc_new_compilerpass ("betrans", NULL, (int (*)(void *))betrans_tree, CPASS_TREEPTR | CPASS_TARGET, 13, NULL));
	dynarray_add (cbepasses, nocc_new_compilerpass ("name-map", NULL, (int (*)(void *))map_mapnames, CPASS_TREEPTR | CPASS_TARGET, 14, NULL));
	dynarray_add (cbepasses, nocc_new_compilerpass ("pre-alloc", NULL, (int (*)(void *))preallocate_tree, CPASS_TREEPTR | CPASS_TARGET, 15, NULL));
	dynarray_add (cbepasses, nocc_new_compilerpass ("allocate", NULL, (int (*)(void *))allocate_tree, CPASS_TREEPTR | CPASS_TARGET, 16, NULL));
	dynarray_add (cbepasses, nocc_new_compilerpass ("codegen", NULL, (int (*)(void *))codegen_generate_code, CPASS_TREEPTR | CPASS_LEXFILE | CPASS_TARGET, 17, NULL));

	return 0;
}
/*}}}*/
/*{{{  int nocc_addcompilerpass (const char *name, void *origin, const char *other, int before, int (*pfcn)(void *), comppassarg_t parg, int stopat, int *eflagptr)*/
/*
 *	this can be called by extensions to add passes to the compiler at run-time
 *	returns 0 on success, non-zero on failure
 */
int nocc_addcompilerpass (const char *name, void *origin, const char *other, int before, int (*pfcn)(void *), comppassarg_t parg, int stopat, int *eflagptr)
{
	int i;
	compilerpass_t *cpass, **cpasses = NULL;
	int where = -1;

	if (!name || !origin || !other || !pfcn) {
		nocc_internal ("nocc_addcompilerpass(): bad parameters");
		return -1;
	}

	/*{{{  just check that we're not trying to add it already*/
	for (i=0; i<DA_CUR (cfepasses); i++) {
		cpass = DA_NTHITEM (cfepasses, i);
		if (!strcmp (cpass->name, name)) {
			nocc_error ("nocc_addcompilerpass(): [%s] already registered (front-end)", name);
			return -1;
		}
	}
	for (i=0; i<DA_CUR (cbepasses); i++) {
		cpass = DA_NTHITEM (cbepasses, i);
		if (!strcmp (cpass->name, name)) {
			nocc_error ("nocc_addcompilerpass(): [%s] already registered (back-end)", name);
			return -1;
		}
	}
	/*}}}*/
	/*{{{  find out where we're trying to add it*/

	for (i=0; i<DA_CUR (cfepasses); i++) {
		cpass = DA_NTHITEM (cfepasses, i);
		if (!strcmp (cpass->name, other)) {
			where = before ? i : i+1;
			cpasses = DA_PTR (cfepasses);
			break;		/* for() */
		}
	}
	for (i=0; i<DA_CUR (cbepasses); i++) {
		cpass = DA_NTHITEM (cbepasses, i);
		if (!strcmp (cpass->name, other)) {
			if (where > -1) {
				nocc_internal ("nocc_addcompilerpass(): confused..");
				return -1;
			}
			where = before ? i : i+1;
			cpasses = DA_PTR (cbepasses);
			break;		/* for() */
		}
	}

	if (where == -1) {
		nocc_error ("nocc_addcompilerpass(): [%s] cannot be registered, said %s [%s], but did not find that", name, before ? "before" : "after", other ?: "(unknown)");
		return -1;
	}

	/*}}}*/

	/* make compiler pass */
	cpass = nocc_new_compilerpass (name, origin, pfcn, parg, stopat, eflagptr);

	if (cpasses == DA_PTR (cfepasses)) {
		dynarray_insert (cfepasses, cpass, where);
	} else {
		dynarray_insert (cbepasses, cpass, where);
	}

	return 0;
}
/*}}}*/


/*{{{  int nocc_addxmlnamespace (const char *name, const char *uri)*/
/*
 *	used to add an XML namespace to the list of those generated when dumping trees
 *	returns 0 on success, non-zero on failure
 */
int nocc_addxmlnamespace (const char *name, const char *uri)
{
	xmlnamespace_t *xmlns = NULL;
	int i;

	if (!name || !uri) {
		return -1;
	}
	for (i=0; i<DA_CUR (xmlnamespaces); i++) {
		xmlns = DA_NTHITEM (xmlnamespaces, i);
		if (!strcmp (xmlns->name, name)) {
			if (xmlns->uri) {
				sfree (xmlns->uri);
			}
			xmlns->uri = string_dup (uri);		/* update URI */
			return 0;
		}
	}
	xmlns = (xmlnamespace_t *)smalloc (sizeof (xmlnamespace_t));
	xmlns->name = string_dup (name);
	xmlns->uri = string_dup (uri);
	dynarray_add (xmlnamespaces, xmlns);

	return 0;
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
	DYNARRAY (char *, fe_def_opts);
	DYNARRAY (char *, srcfiles);
	target_t *target;
	
	/*{{{  basic initialisation*/
	for (progname=*argv + (strlen (*argv) - 1); (progname > *argv) && (progname[-1] != '/'); progname--);
	compopts.progpath = *argv;

	dmem_init ();
	compopts.maintainer = string_dup ("kroc-bugs@kent.ac.uk");
#ifdef TARGET_CPU
	compopts.target_cpu = string_dup (TARGET_CPU);
#else
	compopts.target_cpu = string_dup ("unknown");
#endif
#ifdef TARGET_OS
	compopts.target_os = string_dup (TARGET_OS);
#else
	compopts.target_os = string_dup ("unknown");
#endif
#ifdef TARGET_VENDOR
	compopts.target_vendor = string_dup (TARGET_VENDOR);
#else
	compopts.target_vendor = string_dup ("unknown");
#endif
	compopts.target_str = (char *)smalloc (strlen (compopts.target_cpu) + strlen (compopts.target_os) + strlen (compopts.target_vendor) + 4);
	sprintf (compopts.target_str, "%s-%s-%s", compopts.target_cpu, compopts.target_os, compopts.target_vendor);

	dynarray_init (fe_def_opts);
	dynarray_init (be_def_opts);
	dynarray_init (srcfiles);

	dynarray_init (cfepasses);
	dynarray_init (cbepasses);

	dynarray_init (xmlnamespaces);

	/*}}}*/
	/*{{{  general initialisation*/
#ifdef DEBUG
	nocc_message ("DEBUG: compiler initialisation");
#endif
	opts_init ();
	fcnlib_init ();
	keywords_init ();
	transinstr_init ();
	xml_init ();
	xmlkeys_init ();

	nocc_init_cpasses ();

	nocc_addxmlnamespace ("nocc", "http://www.cs.kent.ac.uk/projects/ofa/nocc/NAMESPACES/nocc");

	/*}}}*/
	/*{{{  process command-line arguments*/
	errored = 0;

	for (walk = argv + 1, i = argc - 1; *walk && i; walk++, i--) {
		cmd_option_t *opt = NULL;

		switch (**walk) {
		case '-':
			if ((*walk)[1] == '-') {
				char *ch;

				for (ch=(*walk + 2); ((*ch >= 'a') && (*ch <= 'z')) || ((*ch >= 'A') && (*ch <= 'Z')) || (*ch == '-'); ch++);
				if (*ch == '=') {
					/* long option split with an equals sign */
					char *realopt = string_ndup (*walk + 2, (int)(ch - *walk) - 2);

					opt = opts_getlongopt (realopt);
					if (opt) {
						/* yes, have this option */
						if (nocc_dooption_arg (realopt, ch + 1) < 0) {
							errored++;
						}
					} else {
						/* defer for front-end */
						dynarray_add (fe_def_opts, string_dup (*walk));
					}
					sfree (realopt);
				} else {
					opt = opts_getlongopt (*walk + 2);
					if (opt) {
						if (opts_process (opt, &walk, &i) < 0) {
							errored++;
						}
					} else {
						/* defer for front-end */
						dynarray_add (fe_def_opts, string_dup (*walk));
					}
				}
			} else {
				char *ch;

				for (ch = *walk + 1; *ch != '\0'; ch++) {
					opt = opts_getshortopt (*ch);
					if (opt) {
						if (opts_process (opt, &walk, &i) < 0) {
							errored++;
						}
					} else {
						/* defer for front-end */
						char *stropt = string_dup ("-X");

						stropt[1] = *ch;
						dynarray_add (fe_def_opts, stropt);
					}
				}
			}
			break;
		default:
			/* it's a source filename */
			dynarray_add (srcfiles, string_dup (*walk));
			break;
		}
	}
	if (errored) {
		nocc_fatal ("error processing command-line options");
		exit (EXIT_FAILURE);
	}

	/*}}}*/
	/*{{{  find and read a specs file*/
	if (!compopts.specsfile) {
		static const char *builtinspecs[] = {
			"./nocc.specs.xml",
#if defined(SYSCONFDIR)
			SYSCONFDIR "/nocc.specs.xml",
			SYSCONFDIR "/nocc/nocc.specs.xml",
#endif
			"/etc/nocc.specs.xml",
			"/usr/local/etc/nocc.specs.xml",
			NULL
		};
		int i;

		/* try and find a specs file */
#ifdef DEBUG
		nocc_message ("DEBUG: no specs file given, searching..");
#endif
		for (i=0; builtinspecs[i]; i++) {
			if (!access (builtinspecs[i], R_OK)) {
				compopts.specsfile = string_dup ((char *)(builtinspecs[i]));
				break;
			}
		}
		if (!builtinspecs[i]) {
#ifdef HAVE_GETPWUID
			struct passwd *pwent = getpwuid (getuid ());
			char fname[FILENAME_MAX];

			if (pwent) {
				sprintf (fname, "%s/.nocc.specs.xml", pwent->pw_dir);
				if (!access (fname, R_OK)) {
					compopts.specsfile = string_dup (fname);
				}
			}
#endif
		}
	}
	if (!compopts.specsfile) {
		nocc_warning ("no specs file found, using defaults");
	} else if (access (compopts.specsfile, R_OK)) {
		nocc_warning ("cannot read specs file %s: %s, using defaults", compopts.specsfile, strerror (errno));
	} else {
		/* okay, open the specs file and process */
		xmlhandler_t *sfxh;

		if (compopts.verbose) {
			nocc_message ("using specs file: %s", compopts.specsfile);
		}
		sfxh = xml_new_handler ();
		sfxh->init = specfile_init;
		sfxh->final = specfile_final;
		sfxh->elem_start = specfile_elem_start;
		sfxh->elem_end = specfile_elem_end;
		sfxh->data = specfile_data;
		i = xml_parse_file (sfxh, compopts.specsfile);
		xml_del_handler (sfxh);

		if (i) {
			nocc_error ("failed to process specs file: %s", compopts.specsfile);
		}
	}


	/*}}}*/
	/*{{{  for each path listed in the config, check that we have at least execute permission on it*/
	for (i=0; i<DA_CUR (compopts.epath); i++) {
		char *epath = DA_NTHITEM (compopts.epath, i);

		if (access (epath, X_OK)) {
			nocc_warning ("ignoring invalid extension path [%s]", epath);
			sfree (epath);
			dynarray_delitem (compopts.epath, i);
			i--;
		}
	}
	for (i=0; i<DA_CUR (compopts.ipath); i++) {
		char *ipath = DA_NTHITEM (compopts.ipath, i);

		if (access (ipath, X_OK)) {
			nocc_warning ("ignoring invalid include path [%s]", ipath);
			sfree (ipath);
			dynarray_delitem (compopts.ipath, i);
			i--;
		}
	}
	for (i=0; i<DA_CUR (compopts.lpath); i++) {
		char *lpath = DA_NTHITEM (compopts.lpath, i);

		if (access (lpath, X_OK)) {
			nocc_warning ("ignoring invalid library path [%s]", lpath);
			sfree (lpath);
			dynarray_delitem (compopts.lpath, i);
			i--;
		}
	}

	/*}}}*/
	/*{{{  dump specs file if requested (after processing paths)*/
	if (compopts.dumpspecs) {
		nocc_message ("detailed compiler settings:");

		nocc_message ("    target:          %s", compopts.target_str ? compopts.target_str : "<unset>");
		nocc_message ("    target-cpu:      %s", compopts.target_cpu ? compopts.target_cpu : "<unset>");
		nocc_message ("    target-vendor:   %s", compopts.target_vendor ? compopts.target_vendor : "<unset>");
		nocc_message ("    target-os:       %s", compopts.target_os ? compopts.target_os : "<unset>");

		nocc_message ("    epaths:");
		for (i=0; i<DA_CUR (compopts.epath); i++) {
			nocc_message ("                     %s", DA_NTHITEM (compopts.epath, i));
		}
		nocc_message ("    ipaths:");
		for (i=0; i<DA_CUR (compopts.ipath); i++) {
			nocc_message ("                     %s", DA_NTHITEM (compopts.ipath, i));
		}
		nocc_message ("    lpaths:");
		for (i=0; i<DA_CUR (compopts.lpath); i++) {
			nocc_message ("                     %s", DA_NTHITEM (compopts.lpath, i));
		}
		nocc_message ("    eloads:");
		for (i=0; i<DA_CUR (compopts.eload); i++) {
			nocc_message ("                     %s", DA_NTHITEM (compopts.eload, i));
		}

		nocc_message ("    not-main-module: %s", compopts.notmainmodule ? "yes" : "no");
		nocc_message ("    verbose:         %s", compopts.verbose ? "yes" : "no");
		nocc_message ("    treecheck:       %s", compopts.treecheck ? "yes" : "no");
	}


	/*}}}*/
	/*{{{  initialise other parts of the compiler and the dynamic framework*/
	symbols_init ();
	lexer_init ();
	tnode_init ();
	treecheck_init ();
	langdef_init ();
	dfa_init ();
	parser_init ();
	feunit_init ();
	prescope_init ();
	scope_init ();
	name_init ();
	extn_init ();
	treeops_init ();
	library_init ();
	constprop_init ();
	precheck_init ();
	aliascheck_init ();
	usagecheck_init ();
	defcheck_init ();
	postcheck_init ();
	fetrans_init ();
	betrans_init ();
	map_init ();
	allocate_init ();
	codegen_init ();
	target_init ();
	crypto_init ();

	/*}}}*/
	/*{{{  initialise occam-pi language lexer and parser (just registers them)*/
	if (occampi_register_frontend ()) {
		nocc_error ("failed to initialise built-in occam-pi language frontend");
		exit (EXIT_FAILURE);
	}

	/*}}}*/
	/*{{{  initialise haskell occam-pi language lexer and parser (just registers)*/
	if (hopp_register_frontend ()) {
		nocc_error ("failed to initialise built-in hopp language frontend");
		exit (EXIT_FAILURE);
	}
	/*}}}*/
	/*{{{  initialise MCSP and RCX-BASIC language lexers and parsers (again, just registration)*/
	if (mcsp_register_frontend ()) {
		nocc_error ("failed to initialise built-in MCSP language frontend");
		exit (EXIT_FAILURE);
	}
	if (rcxb_register_frontend ()) {
		nocc_warning ("failed to initialise built-in RCX-BASIC language frontend");
		exit (EXIT_FAILURE);
	}

	/*}}}*/
	/*{{{  process left-over arguments for front-end (short options have been singularised by this point)*/
	for (walk = DA_PTR (fe_def_opts), i = DA_CUR (fe_def_opts); walk && *walk && i; walk++, i--) {
		cmd_option_t *opt = NULL;

		switch (**walk) {
		case '-':
			if ((*walk)[1] == '-') {
				opt = opts_getlongopt (*walk + 2);
				if (opt) {
					if (opts_process (opt, &walk, &i) < 0) {
						errored++;
					}
					sfree (*walk);
					*walk = NULL;
				} else {
					/* defer for back-end */
					dynarray_add (be_def_opts, *walk);
					*walk = NULL;
				}
			} else {
				char *ch = *walk + 1;

				opt = opts_getshortopt (*ch);
				if (opt) {
					if (opts_process (opt, &walk, &i) < 0) {
						errored++;
					}
					sfree (*walk);
					*walk = NULL;
				} else {
					/* defer for back-end */
					dynarray_add (be_def_opts, *walk);
					*walk = NULL;
				}
			}
			break;
		}
	}
	dynarray_trash (fe_def_opts);

	if (errored) {
		nocc_fatal ("error processing command-line options (%d error%s)", errored, (errored == 1) ? "" : "s");
		exit (EXIT_FAILURE);
	}
	if (DA_CUR (be_def_opts) && compopts.verbose) {
		nocc_message ("deferring %d options for back-end", DA_CUR (be_def_opts));
	}

	/*}}}*/
	/*{{{  dump supported targets if requested*/
	if (compopts.dumptargets) {
		target_dumptargets (stderr);
	}
	/*}}}*/
	/*{{{  maybe need to do a clean exit here*/
	if (noccexitflag) {
		if (compopts.verbose) {
			nocc_message ("exiting...");
		}
		goto main_out;
	}
	/*}}}*/
	/*{{{  load extensions*/
	for (i=0; i<DA_CUR (compopts.eload); i++) {
		if (extn_loadextn (DA_NTHITEM (compopts.eload, i))) {
			errored++;
		}
	}

	/*}}}*/
	/*{{{  dump extensions if requested*/
	if (compopts.dumpextns) {
		extn_dumpextns ();
	}
	/*}}}*/
	/*{{{  check that we're actually compiling something*/
	if (!DA_CUR (srcfiles) && !compopts.dohelp) {
		nocc_fatal ("no input files!");
		exit (EXIT_FAILURE);
	} else {
		if (compopts.verbose) {
			nocc_message ("source files are:");
			for (i=0; i<DA_CUR (srcfiles); i++) {
				nocc_message ("\t%s", DA_NTHITEM (srcfiles, i));
			}
		}
	}
	/*}}}*/
	/*{{{  initialise extensions*/
	extn_initialise ();

	/*}}}*/

	/*{{{  get hold of the desired target*/
	target = target_lookupbyspec (compopts.target_cpu, compopts.target_vendor, compopts.target_os);
	if (!target) {
		nocc_error ("no back-end for [%s] target, need to load ?", compopts.target_str ?: "(unspecified)");
		exit (EXIT_FAILURE);
	}

	/*}}}*/
	/*{{{  if help was requested, initialise target (try!) and do it here*/
	if (compopts.dohelp) {
		target_initialise (target);

		opt_do_help (compopts.dohelp, NULL, NULL);
		exit (EXIT_SUCCESS);
	}
	/*}}}*/

	/*{{{  compiler run*/
	{
		DYNARRAY (lexfile_t *, srclexers);
		DYNARRAY (tnode_t *, srctrees);

		dynarray_init (srclexers);
		dynarray_init (srctrees);

		/*{{{  open lexers for files given on the command-line*/
		for (i=0; i<DA_CUR (srcfiles); i++) {
			char *fname = DA_NTHITEM (srcfiles, i);
			lexfile_t *tmp;

			nocc_message ("lexing: %s", fname);
			tmp = lexer_open (fname);
			if (!tmp) {
				nocc_error ("failed to open %s", fname);
				exit (EXIT_FAILURE);
			}
			dynarray_add (srclexers, tmp);
		}
		/*}}}*/
		if (compopts.stoppoint == 1) {
			/*{{{  stop after tokenise -- just show the tokens*/
			token_t *tok;

			for (i=0; (i<DA_CUR (srclexers)) && (i<DA_CUR (srcfiles)); i++) {
				char *fname = DA_NTHITEM (srcfiles, i);
				lexfile_t *tmp = DA_NTHITEM (srclexers, i);

				nocc_message ("tokenising %s..", fname);
				for (tok = lexer_nexttoken (tmp); tok && (tok->type != END); tok = lexer_nexttoken (tmp)) {
					lexer_dumptoken (stderr, tok);
					lexer_freetoken (tok);
					tok = NULL;
				}
				if (tok) {
					lexer_dumptoken (stderr, tok);
					lexer_freetoken (tok);
					tok = NULL;
				}
			}
			/*}}}*/
			/*{{{  close lexers*/
			for (i=DA_CUR (srclexers) - 1; i >= 0; i--) {
				lexer_close (DA_NTHITEM (srclexers, i));
			}
			dynarray_trash (srclexers);
			/*}}}*/

			goto main_out;
		}
		/*{{{  parse source files into trees*/
		for (i=0; i<DA_CUR (srclexers); i++) {
			lexfile_t *lf = DA_NTHITEM (srclexers, i);
			tnode_t *tree;

			/* initialise lexfile flags */
			lf->toplevel = 1;
			if (compopts.notmainmodule) {
				lf->sepcomp = 1;
			}

			if (compopts.verbose) {
				nocc_message ("parsing ...");
			}
			tree = parser_parse (lf);
			if (!tree) {
				nocc_error ("failed to parse %s", DA_NTHITEM (srcfiles, i));
				errored = 1;
			}
			dynarray_add (srctrees, tree);
		}
		if (compopts.stoppoint == 2) {
			/*{{{  stop after parse*/
			maybedumptrees (DA_PTR (srcfiles), DA_CUR (srcfiles), DA_PTR (srctrees), DA_CUR (srctrees));
			goto main_out;
			/*}}}*/
		}
		if (errored) {
			goto main_out;
		}
		/*}}}*/
		/*{{{  dump node-types if requested*/
		if (compopts.dumpnodetypes) {
			tnode_dumpnodetypes (stderr);
			goto local_close_out;
		}
		/*}}}*/

		/*{{{  front-end compiler passes*/
		if (compopts.verbose) {
			nocc_message ("front-end passes:");
		}
		for (i=0; i<DA_CUR (cfepasses); i++) {
			compilerpass_t *cpass = DA_NTHITEM (cfepasses, i);
			int passenabled = (!cpass->flagptr || (*(cpass->flagptr) == 1));
			int j;
			
			for (j=0; j<DA_CUR (srctrees); j++) {
				if (compopts.treecheck) {
					/* do pre-pass checks */
					if (treecheck_prepass (DA_NTHITEM (srctrees, j), cpass->name, passenabled)) {
						nocc_error ("failed pre-pass check for %s in %s", cpass->name, DA_NTHITEM (srcfiles, j));
						errored = 1;
					}
				}

				if (passenabled) {
					int result;

					if (compopts.verbose) {
						nocc_message ("   %s ...", cpass->name);
					}

					/* switch on argument calling pattern */
					switch (cpass->fargs) {
						/*{{{  (tnode_t **, langparser_t *)*/
					case (CPASS_TREEPTR | CPASS_LANGPARSER):
						{
							int (*fcnptr)(tnode_t **, langparser_t *) = (int (*)(tnode_t **, langparser_t *))cpass->fcn;

							result = fcnptr (DA_NTHITEMADDR (srctrees, j), (DA_NTHITEM (srclexers, j))->parser);
						}
						break;
						/*}}}*/
						/*{{{  (tnode_t *, langparser_t *)*/
					case (CPASS_TREE | CPASS_LANGPARSER):
						{
							int (*fcnptr)(tnode_t *, langparser_t *) = (int (*)(tnode_t *, langparser_t *))cpass->fcn;

							result = fcnptr (DA_NTHITEM (srctrees, j), (DA_NTHITEM (srclexers, j))->parser);
						}
						break;
						/*}}}*/
						/*{{{  (tnode_t **)*/
					case CPASS_TREEPTR:
						{
							int (*fcnptr)(tnode_t **) = (int (*)(tnode_t **))cpass->fcn;

							result = fcnptr (DA_NTHITEMADDR (srctrees, j));
						}
						break;
						/*}}}*/
						/*{{{  (tnode_t *)*/
					case CPASS_TREE:
						{
							int (*fcnptr)(tnode_t *) = (int (*)(tnode_t *))cpass->fcn;

							result = fcnptr (DA_NTHITEM (srctrees, j));
						}
						break;
						/*}}}*/
					default:
						nocc_fatal ("unhandled compiler-pass argument combination 0x%8.8x for pass [%s]", (unsigned int)cpass->fargs, cpass->name);
						result = -1;
						break;
					}
					if (result) {
						nocc_error ("failed to %s %s", cpass->name, DA_NTHITEM (srcfiles, j));
						errored = 1;
					}
				}

				if (compopts.treecheck) {
					/* do post-pass checks */
					if (treecheck_postpass (DA_NTHITEM (srctrees, j), cpass->name, passenabled)) {
						nocc_error ("failed post-pass check for %s in %s", cpass->name, DA_NTHITEM (srcfiles, j));
						errored = 1;
					}
				}
			}

			/* can still stop even if pass not enabled */

			if (compopts.stoppoint == cpass->stoppoint) {
				/* stopping here */
				maybedumptrees (DA_PTR (srcfiles), DA_CUR (srcfiles), DA_PTR (srctrees), DA_CUR (srctrees));
				goto main_out;
			}
			if (errored) {
				goto main_out;
			}
		}
		/*}}}*/

		/*{{{  initialise back-end*/
		target_initialise (target);

		/*}}}*/
		/*{{{  process left-over arguments for back-end (short options have been singularised by this point)*/
		for (walk = DA_PTR (be_def_opts), i = DA_CUR (be_def_opts); walk && *walk && i; walk++, i--) {
			cmd_option_t *opt = NULL;

			switch (**walk) {
			case '-':
				if ((*walk)[1] == '-') {
					opt = opts_getlongopt (*walk + 2);
					if (opt) {
						if (opts_process (opt, &walk, &i) < 0) {
							errored++;
						}
						sfree (*walk);
						*walk = NULL;
					} else {
						nocc_error ("unsupported option: %s", *walk);
						errored++;
					}
				} else {
					char *ch = *walk + 1;

					opt = opts_getshortopt (*ch);
					if (opt) {
						if (opts_process (opt, &walk, &i) < 0) {
							errored++;
						}
						sfree (*walk);
						*walk = NULL;
					} else {
						nocc_error ("unsupported option: %s", *walk);
						errored++;
					}
				}
				break;
			}
		}
		dynarray_trash (be_def_opts);

		if (errored) {
			nocc_fatal ("error processing options for back-end (%d error%s)", errored, (errored == 1) ? "" : "s");
			exit (EXIT_FAILURE);
		}

		/*}}}*/

		/*{{{  back-end compiler passes*/
		if (compopts.verbose) {
			nocc_message ("back-end passes:");
		}
		for (i=0; i<DA_CUR (cbepasses); i++) {
			compilerpass_t *cpass = DA_NTHITEM (cbepasses, i);
			int passenabled = (!cpass->flagptr || (*(cpass->flagptr) == 1));
			int j;

			for (j=0; j<DA_CUR (srctrees); j++) {
				if (compopts.treecheck) {
					/* do pre-pass checks */
					if (treecheck_prepass (DA_NTHITEM (srctrees, j), cpass->name, passenabled)) {
						nocc_error ("failed pre-pass check for %s in %s", cpass->name, DA_NTHITEM (srcfiles, j));
						errored = 1;
					}
				}

				if (passenabled) {
					int result;

					if (compopts.verbose) {
						nocc_message ("   %s ...", cpass->name);
					}

					/* switch on argument calling pattern */
					switch (cpass->fargs) {
						/*{{{  (tnode_t **, target_t *)*/
					case (CPASS_TREEPTR | CPASS_TARGET):
						{
							int (*fcnptr)(tnode_t **, target_t *) = (int (*)(tnode_t **, target_t *))cpass->fcn;

							result = fcnptr (DA_NTHITEMADDR (srctrees, j), target);
						}
						break;
						/*}}}*/
						/*{{{  (tnode_t **, lexfile_t *, target_t *)*/
					case (CPASS_TREEPTR | CPASS_LEXFILE | CPASS_TARGET):
						{
							int (*fcnptr)(tnode_t **, lexfile_t *, target_t *) = (int (*)(tnode_t **, lexfile_t *, target_t *))cpass->fcn;

							result = fcnptr (DA_NTHITEMADDR (srctrees, j), DA_NTHITEM (srclexers, j), target);
						}
						break;
						/*}}}*/
					default:
						nocc_fatal ("unhandled compiler-pass argument combination 0x%8.8x for pass [%s]", (unsigned int)cpass->fargs, cpass->name);
						result = -1;
						break;
					}
					if (result) {
						nocc_error ("failed to %s %s", cpass->name, DA_NTHITEM (srcfiles, j));
						errored = 1;
					}
				}

				if (compopts.treecheck) {
					/* do post-pass checks */
					if (treecheck_postpass (DA_NTHITEM (srctrees, j), cpass->name, passenabled)) {
						nocc_error ("failed post-pass check for %s in %s", cpass->name, DA_NTHITEM (srcfiles, j));
						errored = 1;
					}
				}
			}
			/* can still stop even if pass not enabled */

			if (compopts.stoppoint == cpass->stoppoint) {
				/* stopping here */
				maybedumptrees (DA_PTR (srcfiles), DA_CUR (srcfiles), DA_PTR (srctrees), DA_CUR (srctrees));
				goto main_out;
			}
			if (errored) {
				goto main_out;
			}
		}
		/*}}}*/

local_close_out:
		/*{{{  close lexers*/
		for (i=DA_CUR (srclexers) - 1; i >= 0; i--) {
			lexer_close (DA_NTHITEM (srclexers, i));
		}
		dynarray_trash (srclexers);
		/*}}}*/

		/*{{{  maybe dump trees*/
		maybedumptrees (DA_PTR (srcfiles), DA_CUR (srcfiles), DA_PTR (srctrees), DA_CUR (srctrees));

		/*}}}*/
	}
	/*}}}*/

main_out:
	/*{{{  dump compiler hooks if requested*/
	if (compopts.dumpchooks) {
		tnode_dumpchooks (stderr);
	}
	/*}}}*/
	/*{{{  call any specific finalisers*/
	treecheck_finalise ();

	/*}}}*/
	/*{{{  shutdown/etc.*/
	for (i=0; i<DA_CUR (srcfiles); i++) {
		sfree (DA_NTHITEM (srcfiles, i));
	}
	dynarray_trash (srcfiles);

	/* shutdown compiler */
	nocc_shutdownrun ();

	if (compopts.dmemdump) {
		dmem_usagedump ();
	}
#ifdef TRACE_MEMORY
	if (compopts.dumptracemem) {
		ss_cleanup();
	}
#endif
	dmem_shutdown ();

	/*}}}*/
	return (errored ? EXIT_FAILURE : EXIT_SUCCESS);
}
/*}}}*/


