/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'1,3,5-6' */

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
 *	xmlkeys.gperf -- XML keywords for nocc
 */
struct TAG_xmlkey;

#define TOTAL_KEYWORDS 72
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 14
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 171
/* maximum key range = 169, duplicates = 0 */

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
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172,   0, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172,  70, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172,   0,   0,   0,
        5,  15,  60,  15,   0,  45,  10,  25,  30,  35,
        0,  10,  45,  75,  50,   0,  20,  25,  30,   5,
        0,   5,   5, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
      172, 172, 172, 172, 172, 172, 172
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]+1];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

static const struct TAG_xmlkey wordlist[] =
  {
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"cow",			XMLKEY_COW},
    {"name",			XMLKEY_NAME},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"command",		XMLKEY_COMMAND},
    {(char*)0,XMLKEY_INVALID},
    {"nocc",			XMLKEY_NOCC},
    {"dhash",			XMLKEY_DHASH},
    {(char*)0,XMLKEY_INVALID},
    {"op",			XMLKEY_OP},
    {(char*)0,XMLKEY_INVALID},
    {"decl",			XMLKEY_DECL},
    {"srcinclude",		XMLKEY_SRCINCLUDE},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"gdb",			XMLKEY_GDB},
    {"node",			XMLKEY_NODE},
    {"epath",			XMLKEY_EPATH},
    {(char*)0,XMLKEY_INVALID},
    {"comment",		XMLKEY_COMMENT},
    {"tag",			XMLKEY_TAG},
    {"hash",			XMLKEY_HASH},
    {(char*)0,XMLKEY_INVALID},
    {"srcuse",			XMLKEY_SRCUSE},
    {(char*)0,XMLKEY_INVALID},
    {"nocchelp",		XMLKEY_NOCCHELP},
    {"desc",			XMLKEY_DESC},
    {"signedhash",		XMLKEY_SIGNEDHASH},
    {"signeddhash",		XMLKEY_SIGNEDDHASH},
    {(char*)0,XMLKEY_INVALID},
    {"cachedir",		XMLKEY_CACHEDIR},
    {"data",			XMLKEY_DATA},
    {"lpath",			XMLKEY_LPATH},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"lex",			XMLKEY_LEX},
    {"help",			XMLKEY_HELP},
    {(char*)0,XMLKEY_INVALID},
    {"action",			XMLKEY_ACTION},
    {"newname",		XMLKEY_NEWNAME},
    {(char*)0,XMLKEY_INVALID},
    {"extn",			XMLKEY_EXTN},
    {(char*)0,XMLKEY_INVALID},
    {"symbol",			XMLKEY_SYMBOL},
    {"allocws",		XMLKEY_ALLOCWS},
    {(char*)0,XMLKEY_INVALID},
    {"extension",		XMLKEY_EXTENSION},
    {"ipath",			XMLKEY_IPATH},
    {"adjust",			XMLKEY_ADJUST},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"namespace",		XMLKEY_NAMESPACE},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"helpset",		XMLKEY_HELPSET},
    {"hashalgo",		XMLKEY_HASHALGO},
    {"subtarget",		XMLKEY_SUBTARGET},
    {"cccsp-kroc",		XMLKEY_CCCSP_KROC},
    {"target",			XMLKEY_TARGET},
    {"gprolog",		XMLKEY_GPROLOG},
    {(char*)0,XMLKEY_INVALID},
    {"meta",			XMLKEY_META},
    {"match",			XMLKEY_MATCH},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"wget",			XMLKEY_WGET},
    {"error",			XMLKEY_ERROR},
    {(char*)0,XMLKEY_INVALID},
    {"allocvs",		XMLKEY_ALLOCVS},
    {"language",		XMLKEY_LANGUAGE},
    {"path",			XMLKEY_PATH},
    {"maintainer",		XMLKEY_MAINTAINER},
    {(char*)0,XMLKEY_INVALID},
    {"allocms",		XMLKEY_ALLOCMS},
    {(char*)0,XMLKEY_INVALID},
    {"nativelib",		XMLKEY_NATIVELIB},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"libunit",		XMLKEY_LIBUNIT},
    {(char*)0,XMLKEY_INVALID},
    {"tree",			XMLKEY_TREE},
    {"value",			XMLKEY_VALUE},
    {"constr",			XMLKEY_CONSTR},
    {"library",		XMLKEY_LIBRARY},
    {(char*)0,XMLKEY_INVALID},
    {"nocc:namespace",		XMLKEY_NOCC_NAMESPACE},
    {"check",			XMLKEY_CHECK},
    {"author",			XMLKEY_AUTHOR},
    {"version",		XMLKEY_VERSION},
    {(char*)0,XMLKEY_INVALID},
    {"proc",			XMLKEY_PROC},
    {"trustedkey",		XMLKEY_TRUSTEDKEY},
    {(char*)0,XMLKEY_INVALID},
    {"keyword",		XMLKEY_KEYWORD},
    {(char*)0,XMLKEY_INVALID},
    {"type",			XMLKEY_TYPE},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"privkey",		XMLKEY_PRIVKEY},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"pref",			XMLKEY_PREF},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"xmlns:nocc",		XMLKEY_XMLNS_NOCC},
    {"parser",			XMLKEY_PARSER},
    {"nocc:libinfo",		XMLKEY_NOCC_LIBINFO},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"blockinfo",		XMLKEY_BLOCKINFO},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"wgetopts",		XMLKEY_WGETOPTS},
    {(char*)0,XMLKEY_INVALID},
    {"descriptor",		XMLKEY_DESCRIPTOR},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"gperf",			XMLKEY_GPERF},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"typeof",			XMLKEY_TYPEOF}
  };

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct TAG_xmlkey *
xmlkeys_lookup_byname (register const char *str, register unsigned int len)
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


