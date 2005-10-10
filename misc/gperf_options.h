/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf  */
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

#define TOTAL_KEYWORDS 38
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 15
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 72
/* maximum key range = 67, duplicates = 0 */

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
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73,  0,  5, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 20,  5, 35,
      10, 45,  0, 20, 40, 73, 73, 10,  5, 35,
      10,  5, 15,  5, 73, 15,  0,  0, 15, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
      73, 73, 73, 73, 73, 73, 73
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[8]+1];
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
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"output",			'o',	opt_setstr,		&(compopts.outfile),		"0output file-name",					37},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-tree",		'\0',	opt_setintflag,		&(compopts.dumptree),		"1print parse tree",					3},
    {"stop-token",		'\0',	opt_setstopflag,	(void *)1,			"1stop after tokenise (and print tokens)",		13},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-tree-to",		'\0',	opt_setstr,		&(compopts.dumptreeto),		"1print parse tree to file",				4},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-typecheck",		'\0',	opt_setstopflag,	(void *)5,			"1stop after type check",				17},
    {"stop-undefcheck",	'\0',	opt_setstopflag,	(void *)9,			"1stop after undefined-usage check",			21},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"compile",		'c',	opt_setintflag,		&(compopts.notmainmodule),	"0compile for separate compilation",			34},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-dfas",		'\0',	opt_setintflag,		&(compopts.dumpdfas),		"1print named DFAs after parser init",			6},
    {"dump-names",		'\0',	opt_setintflag,		&(compopts.dumpnames),		"1print names after scope",				7},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-namemap",		'\0',	opt_setstopflag,	(void *)12,			"1stop after name-map",					24},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-nodetypes",		'\0',	opt_setintflag,		&(compopts.dumpnodetypes),	"1print node types after initialisation",		10},
    {"stop-parse",		'\0',	opt_setstopflag,	(void *)2,			"1stop after parse",					14},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-fetrans",		'\0',	opt_setstopflag,	(void *)10,			"1stop after front-end tree transform",			22},
    {"stop-prescope",		'\0',	opt_setstopflag,	(void *)3,			"1stop after pre-scope",				15},
    {"dump-dmem",		'\0',	opt_setintflag,		&(compopts.dmemdump),		"1display dynamic memory pool information",		35},
    {"stop-scope",		'\0',	opt_setstopflag,	(void *)4,			"1stop after scope",					16},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-betrans",		'\0',	opt_setstopflag,	(void *)11,			"1stop after back-end tree transform",			23},
    {"stop-prealloc",		'\0',	opt_setstopflag,	(void *)13,			"1stop after pre-allocation",				25},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-specs",		'\0',	opt_setintflag,		&(compopts.dumpspecs),		"1print compiler specs",				2},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-varmaps",		'\0',	opt_setintflag,		&(compopts.dumpvarmaps),	"1print variable maps after block allocation",		9},
    {"stop-precheck",		'\0',	opt_setstopflag,	(void *)6,			"1stop after pre-check",				18},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-aliascheck",	'\0',	opt_setstopflag,	(void *)7,			"1stop after alias check",				19},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-grammar",		'\0',	opt_setintflag,		&(compopts.dumpgrammar),	"1print grammars during parser init",			5},
    {"skip-defcheck",		'\0',	opt_clearintflag,	&(compopts.dodefcheck),		"1skip undefinedness checks",				30},
    {"save-named-dfa",		'\0',	opt_setsaveopt,		(void *)1,			"1save named DFA to file",				11},
    {"stop-alloc",		'\0',	opt_setstopflag,	(void *)14,			"1stop after variable allocation",			26},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-codegen",		'\0',	opt_setstopflag,	(void *)15,			"1stop after code-generation",				27},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"help",			'h',	opt_do_help,		NULL,				"0display standard usage information",			0},
    {"skip-aliascheck",	'\0',	opt_clearintflag,	&(compopts.doaliascheck),	"1skip alias checks",					28},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"dump-targets",		'\0',	opt_setintflag,		&(compopts.dumptargets),	"1print supported targets after initialisation",	8},
    {"help-ful",		'\0',	opt_do_help,		NULL,				"1display full usage information",			1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"stop-usagecheck",	'\0',	opt_setstopflag,	(void *)8,			"1stop after parallel-usage check",			20},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"version",		'V',	opt_do_version,		NULL,				"0print version and exit",				33},
    {"save-all-dfas",		'\0',	opt_setstr,		&(compopts.savealldfas),	"1save all DFAs to file",				12},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"specs-file",		'\0',	opt_setstr,		&(compopts.specsfile),		"0path to compiler specs file",				36},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"skip-usagecheck",	'\0',	opt_clearintflag,	&(compopts.dousagecheck),	"1skip parallel usage checks",				29},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"verbose",		'v',	opt_setintflag,		&(compopts.verbose),		"0verbose compilation",					31},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {(char*)0,'\0',NULL,NULL,NULL,-1},
    {"debug-parser",		'\0',	opt_setintflag,		&(compopts.debugparser),	"1debug parser",					32}
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


