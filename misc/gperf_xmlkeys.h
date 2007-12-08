/* ANSI-C code produced by gperf version 3.0.2 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'1,3,6' */

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

#define TOTAL_KEYWORDS 61
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 14
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 117
/* maximum key range = 115, duplicates = 0 */

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
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118,   5, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118,   0,   0,   5,
        5,  30,  46,   0,  10,  45,  35,   0,   0,  10,
        0,  30,  45,  30,  25,   5,  15,  30,   5,  50,
       45,   0,  20, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
      118, 118, 118, 118, 118, 118, 118
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
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
    {"lex",			XMLKEY_LEX},
    {"name",			XMLKEY_NAME},
    {"lpath",			XMLKEY_LPATH},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"gdb",			XMLKEY_GDB},
    {"nocc",			XMLKEY_NOCC},
    {"dhash",			XMLKEY_DHASH},
    {"symbol",			XMLKEY_SYMBOL},
    {"comment",		XMLKEY_COMMENT},
    {(char*)0,XMLKEY_INVALID},
    {"decl",			XMLKEY_DECL},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"nocc:libinfo",		XMLKEY_NOCC_LIBINFO},
    {(char*)0,XMLKEY_INVALID},
    {"nocc:namespace",		XMLKEY_NOCC_NAMESPACE},
    {"value",			XMLKEY_VALUE},
    {"adjust",			XMLKEY_ADJUST},
    {"allocvs",		XMLKEY_ALLOCVS},
    {(char*)0,XMLKEY_INVALID},
    {"desc",			XMLKEY_DESC},
    {"srcinclude",		XMLKEY_SRCINCLUDE},
    {(char*)0,XMLKEY_INVALID},
    {"allocms",		XMLKEY_ALLOCMS},
    {"tag",			XMLKEY_TAG},
    {"hash",			XMLKEY_HASH},
    {"signedhash",		XMLKEY_SIGNEDHASH},
    {"signeddhash",		XMLKEY_SIGNEDDHASH},
    {"op",			XMLKEY_OP},
    {"hashalgo",		XMLKEY_HASHALGO},
    {"node",			XMLKEY_NODE},
    {"epath",			XMLKEY_EPATH},
    {"action",			XMLKEY_ACTION},
    {"library",		XMLKEY_LIBRARY},
    {"language",		XMLKEY_LANGUAGE},
    {"data",			XMLKEY_DATA},
    {"error",			XMLKEY_ERROR},
    {"target",			XMLKEY_TARGET},
    {"gprolog",		XMLKEY_GPROLOG},
    {(char*)0,XMLKEY_INVALID},
    {"meta",			XMLKEY_META},
    {"match",			XMLKEY_MATCH},
    {"srcuse",			XMLKEY_SRCUSE},
    {"version",		XMLKEY_VERSION},
    {(char*)0,XMLKEY_INVALID},
    {"type",			XMLKEY_TYPE},
    {"ipath",			XMLKEY_IPATH},
    {"gperf",			XMLKEY_GPERF},
    {"keyword",		XMLKEY_KEYWORD},
    {(char*)0,XMLKEY_INVALID},
    {"namespace",		XMLKEY_NAMESPACE},
    {"maintainer",		XMLKEY_MAINTAINER},
    {"check",			XMLKEY_CHECK},
    {"libunit",		XMLKEY_LIBUNIT},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"trustedkey",		XMLKEY_TRUSTEDKEY},
    {"author",			XMLKEY_AUTHOR},
    {"newname",		XMLKEY_NEWNAME},
    {(char*)0,XMLKEY_INVALID},
    {"extn",			XMLKEY_EXTN},
    {"tree",			XMLKEY_TREE},
    {"constr",			XMLKEY_CONSTR},
    {"allocws",		XMLKEY_ALLOCWS},
    {(char*)0,XMLKEY_INVALID},
    {"nativelib",		XMLKEY_NATIVELIB},
    {"xmlns:nocc",		XMLKEY_XMLNS_NOCC},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"extension",		XMLKEY_EXTENSION},
    {"descriptor",		XMLKEY_DESCRIPTOR},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"path",			XMLKEY_PATH},
    {(char*)0,XMLKEY_INVALID},
    {"parser",			XMLKEY_PARSER},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"proc",			XMLKEY_PROC},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"typeof",			XMLKEY_TYPEOF},
    {(char*)0,XMLKEY_INVALID},
    {"blockinfo",		XMLKEY_BLOCKINFO},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"privkey",		XMLKEY_PRIVKEY}
  };

#ifdef __GNUC__
__inline
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


