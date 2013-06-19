/* ANSI-C code produced by gperf version 3.0.3 */
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

#define TOTAL_KEYWORDS 71
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 14
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 146
/* maximum key range = 143, duplicates = 0 */

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
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147,   0, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147,  70, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147,   0,   0,   0,
        5,  15,  35,  35,   0,   0,   5,  50,  20,  35,
        0,  10,  45,  75,  15,   0,  20,  25,  20,  40,
        5,  20,   5, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
      147, 147, 147, 147, 147, 147, 147
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
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"name",			XMLKEY_NAME},
    {"ipath",			XMLKEY_IPATH},
    {(char*)0,XMLKEY_INVALID},
    {"command",		XMLKEY_COMMAND},
    {"cow",			XMLKEY_COW},
    {"nocc",			XMLKEY_NOCC},
    {"dhash",			XMLKEY_DHASH},
    {(char*)0,XMLKEY_INVALID},
    {"op",			XMLKEY_OP},
    {(char*)0,XMLKEY_INVALID},
    {"decl",			XMLKEY_DECL},
    {"srcinclude",		XMLKEY_SRCINCLUDE},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"node",			XMLKEY_NODE},
    {"epath",			XMLKEY_EPATH},
    {(char*)0,XMLKEY_INVALID},
    {"comment",		XMLKEY_COMMENT},
    {"tag",			XMLKEY_TAG},
    {"hash",			XMLKEY_HASH},
    {"lpath",			XMLKEY_LPATH},
    {"srcuse",			XMLKEY_SRCUSE},
    {"libunit",		XMLKEY_LIBUNIT},
    {"nocchelp",		XMLKEY_NOCCHELP},
    {"desc",			XMLKEY_DESC},
    {"signedhash",		XMLKEY_SIGNEDHASH},
    {"signeddhash",		XMLKEY_SIGNEDDHASH},
    {(char*)0,XMLKEY_INVALID},
    {"cachedir",		XMLKEY_CACHEDIR},
    {"data",			XMLKEY_DATA},
    {"error",			XMLKEY_ERROR},
    {"symbol",			XMLKEY_SYMBOL},
    {"version",		XMLKEY_VERSION},
    {"gdb",			XMLKEY_GDB},
    {"help",			XMLKEY_HELP},
    {(char*)0,XMLKEY_INVALID},
    {"action",			XMLKEY_ACTION},
    {"library",		XMLKEY_LIBRARY},
    {"lex",			XMLKEY_LEX},
    {"extn",			XMLKEY_EXTN},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"newname",		XMLKEY_NEWNAME},
    {"hashalgo",		XMLKEY_HASHALGO},
    {"extension",		XMLKEY_EXTENSION},
    {"descriptor",		XMLKEY_DESCRIPTOR},
    {"constr",			XMLKEY_CONSTR},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"namespace",		XMLKEY_NAMESPACE},
    {(char*)0,XMLKEY_INVALID},
    {"author",			XMLKEY_AUTHOR},
    {"helpset",		XMLKEY_HELPSET},
    {(char*)0,XMLKEY_INVALID},
    {"tree",			XMLKEY_TREE},
    {"cccsp-kroc",		XMLKEY_CCCSP_KROC},
    {"target",			XMLKEY_TARGET},
    {"allocvs",		XMLKEY_ALLOCVS},
    {"language",		XMLKEY_LANGUAGE},
    {"meta",			XMLKEY_META},
    {"match",			XMLKEY_MATCH},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"nativelib",		XMLKEY_NATIVELIB},
    {"maintainer",		XMLKEY_MAINTAINER},
    {(char*)0,XMLKEY_INVALID},
    {"gprolog",		XMLKEY_GPROLOG},
    {(char*)0,XMLKEY_INVALID},
    {"path",			XMLKEY_PATH},
    {"value",			XMLKEY_VALUE},
    {"adjust",			XMLKEY_ADJUST},
    {"allocms",		XMLKEY_ALLOCMS},
    {(char*)0,XMLKEY_INVALID},
    {"wget",			XMLKEY_WGET},
    {(char*)0,XMLKEY_INVALID},
    {"parser",			XMLKEY_PARSER},
    {"allocws",		XMLKEY_ALLOCWS},
    {(char*)0,XMLKEY_INVALID},
    {"pref",			XMLKEY_PREF},
    {"trustedkey",		XMLKEY_TRUSTEDKEY},
    {(char*)0,XMLKEY_INVALID},
    {"keyword",		XMLKEY_KEYWORD},
    {(char*)0,XMLKEY_INVALID},
    {"nocc:namespace",		XMLKEY_NOCC_NAMESPACE},
    {"check",			XMLKEY_CHECK},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"proc",			XMLKEY_PROC},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"type",			XMLKEY_TYPE},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"blockinfo",		XMLKEY_BLOCKINFO},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"nocc:libinfo",		XMLKEY_NOCC_LIBINFO},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"gperf",			XMLKEY_GPERF},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"xmlns:nocc",		XMLKEY_XMLNS_NOCC},
    {(char*)0,XMLKEY_INVALID},
    {"privkey",		XMLKEY_PRIVKEY},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"wgetopts",		XMLKEY_WGETOPTS},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"typeof",			XMLKEY_TYPEOF}
  };

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
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


