/* ANSI-C code produced by gperf version 3.0.2 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'2,6,9' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif


/*
 *	options.gperf -- command-line options for nocc
 */
struct TAG_cmd_option;

#define TOTAL_KEYWORDS 64
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 16
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 137
/* maximum key range = 134, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138,   0, 138,  35, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138,  10,  15,  30,
       60,  20,  10,   0,  15,  70,  15,  20,   0,  10,
       60,  40,  10,  15,   5,  35,   0,   0,   5, 138,
        0, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
      138, 138, 138, 138, 138, 138, 138, 138
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[8]+2];
      /*FALLTHROUGH*/
      case 8:
      case 7:
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
        break;
    }
  return hval;
}

static const struct TAG_cmd_option wordlist[] =
  {
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"extn",			'e',	opt_addextn,		NULL,				"0compiler extension to load",				400},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"output",			'o',	opt_setstr,		&(compopts.outfile),		"0output file-name",					304},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-tree",		'\0',	opt_setintflag,		&(compopts.dumptree),		"1print parse tree",					3},
    {"stop-token",		'\0',	opt_setstopflag,	(void *)1,			"1stop after tokenise (and print tokens)",		100},
    {"dump-lexers",		'\0',	opt_setintflag,		&(compopts.dumplexers),		"1print registered languages (lexers)",			7},
    {"dump-tree-to",		'\0',	opt_setstr,		&(compopts.dumptreeto),		"1print parse tree to file",				4},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-typecheck",		'\0',	opt_setstopflag,	(void *)5,			"1stop after type check",				104},
    {"stop-undefcheck",	'\0',	opt_setstopflag,	(void *)11,			"1stop after undefined-usage check",			110},
    {"stop-typeresolve",	'\0',	opt_setstopflag,	(void *)7,			"1stop after type resolve",				106},
    {"trace-parser",		'\0',	opt_setintflag,		&(compopts.traceparser),	"1trace parser (debugging)",				200},
    {"trace-precode",		'\0',	opt_setintflag,		&(compopts.traceprecode),	"1trace pre-code (debugging)",				203},
    {"dump-fcns",		'\0',	opt_setintflag,		&(compopts.dumpfcns),		"1print registered functions",				11},
    {"stop-parse",		'\0',	opt_setstopflag,	(void *)2,			"1stop after parse",					101},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-fetrans",		'\0',	opt_setstopflag,	(void *)13,			"1stop after front-end tree transform",			112},
    {"stop-prescope",		'\0',	opt_setstopflag,	(void *)3,			"1stop after pre-scope",				102},
    {"help",			'h',	opt_do_help_flag,	NULL,				"0display standard usage information",			0},
    {"trace-typecheck",	'\0',	opt_setintflag,		&(compopts.tracetypecheck),	"1trace type-check (debugging)",			201},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-betrans",		'\0',	opt_setstopflag,	(void *)14,			"1stop after back-end tree transform",			113},
    {"trace-langops",		'\0',	opt_setstr,		&(compopts.tracelangops),	"1trace language operations (debugging)",		205},
    {"stop-postcheck",		'\0',	opt_setstopflag,	(void *)12,			"1stop after post-check",				111},
    {"trace-constprop",	'\0',	opt_setintflag,		&(compopts.traceconstprop),	"1trace constant-propagation (debugging)",		202},
    {"dump-folded",		'\0',	opt_setintflag,		&(compopts.dumpfolded),		"1include folds in parse tree dumps",			20},
    {"debug-parser",		'\0',	opt_setintflag,		&(compopts.debugparser),	"1debug parser",					252},
    {"dump-tracemem",		'\0',	opt_setintflag,		&(compopts.dumptracemem),	"1display left-over memory blocks (if compiled)",	450},
    {"extn-path",		'E',	opt_addextnpath,	NULL,				"1add paths for compiler extensions",			352},
    {"stop-alloc",		'\0',	opt_setstopflag,	(void *)17,			"1stop after variable allocation",			116},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"help-ful",		'\0',	opt_do_help_flag,	NULL,				"1display full usage information",			1},
    {"treecheck",		'\0',	opt_setintflag,		&(compopts.treecheck),		"1enable run-time parse tree checking",			250},
    {"dump-extns",		'\0',	opt_setintflag,		&(compopts.dumpextns),		"1print detailed information about loaded extensions",	19},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-codegen",		'\0',	opt_setstopflag,	(void *)18,			"1stop after code-generation",				117},
    {"stop-precheck",		'\0',	opt_setstopflag,	(void *)8,			"1stop after pre-check",				107},
    {"stop-constprop",		'\0',	opt_setstopflag,	(void *)6,			"1stop after constant propagation",			105},
    {"dump-stree",		'\0',	opt_setintflag,		&(compopts.dumpstree),		"1print parse tree in s-record format",			5},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"compile",		'c',	opt_setintflag,		&(compopts.notmainmodule),	"0compile for separate compilation",			300},
    {"dump-stree-to",		'\0',	opt_setstr,		&(compopts.dumpstreeto),	"1print parse tree in s-record format to file",		6},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-scope",		'\0',	opt_setstopflag,	(void *)4,			"1stop after scope",					103},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-grammar",		'\0',	opt_setintflag,		&(compopts.dumpgrammar),	"1print grammars during parser init",			8},
    {"stop-prealloc",		'\0',	opt_setstopflag,	(void *)16,			"1stop after pre-allocation",				115},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-aliascheck",	'\0',	opt_setstopflag,	(void *)9,			"1stop after alias check",				108},
    {"dump-chooks",		'\0',	opt_setintflag,		&(compopts.dumpchooks),		"1print compiler hooks",				18},
    {"dump-varmaps",		'\0',	opt_setintflag,		&(compopts.dumpvarmaps),	"1print variable maps after block allocation",		14},
    {"trace-compops",		'\0',	opt_setstr,		&(compopts.tracecompops),	"1trace compiler operations (debugging)",		204},
    {"dump-snodetags",		'\0',	opt_setintflag,		&(compopts.dumpsnodetags),	"1print node tags after initialisation (short form)",	17},
    {"dump-snodetypes",	'\0',	opt_setintflag,		&(compopts.dumpsnodetypes),	"1print node types after initialisation (short form)",	16},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"verbose",		'v',	opt_setintflag,		&(compopts.verbose),		"0verbose compilation",					251},
    {"gdb",			'\0',	opt_setintflag,		&(compopts.fatalgdb),		"1launch GDB on fatal error",				302},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-specs",		'\0',	opt_setintflag,		&(compopts.dumpspecs),		"1print compiler specs",				2},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"version",		'V',	opt_do_version,		NULL,				"0print version and exit",				253},
    {"save-all-dfas",		'\0',	opt_setstr,		&(compopts.savealldfas),	"1save all DFAs to file",				51},
    {"dump-dfas",		'\0',	opt_setintflag,		&(compopts.dumpdfas),		"1print named DFAs after parser init",			10},
    {"dump-names",		'\0',	opt_setintflag,		&(compopts.dumpnames),		"1print names after scope",				12},
    {"dump-grules",		'\0',	opt_setintflag,		&(compopts.dumpgrules),		"1print generic reduction rules after parser init",	9},
    {"stop-namemap",		'\0',	opt_setstopflag,	(void *)15,			"1stop after name-map",					114},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-nodetypes",		'\0',	opt_setintflag,		&(compopts.dumpnodetypes),	"1print node types after initialisation",		15},
    {"skip-aliascheck",	'\0',	opt_clearintflag,	&(compopts.doaliascheck),	"1skip alias checks",					150},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"specs-file",		'\0',	opt_setstr,		&(compopts.specsfile),		"0path to compiler specs file",				303},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-targets",		'\0',	opt_setintflag,		&(compopts.dumptargets),	"1print supported targets after initialisation",	13},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"save-named-dfa",		'\0',	opt_setsaveopt,		(void *)1,			"1save named DFA to file",				50},
    {"stop-usagecheck",	'\0',	opt_setstopflag,	(void *)10,			"1stop after parallel-usage check",			109},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"library-path",		'L',	opt_addlibrarypath,	NULL,				"0add paths for library files",				351},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"skip-usagecheck",	'\0',	opt_clearintflag,	&(compopts.dousagecheck),	"1skip parallel usage checks",				151},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-dmem",		'\0',	opt_setintflag,		&(compopts.dmemdump),		"1display dynamic memory pool information",		301},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"skip-defcheck",		'\0',	opt_clearintflag,	&(compopts.dodefcheck),		"1skip undefinedness checks",				152},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"include-path",		'I',	opt_addincludepath,	NULL,				"0add paths for include files",				350}
  };

#ifdef __GNUC__
__inline
#endif
const struct TAG_cmd_option *
option_lookup_byname (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (s && *str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}


