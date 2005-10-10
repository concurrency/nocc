/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf  */
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

#define TOTAL_KEYWORDS 46
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 12
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 91
/* maximum key range = 90, duplicates = 0 */

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
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92,  0,  5, 20,
       0, 10, 25, 92,  0, 15, 15, 15,  0, 10,
       5,  0, 10, 30, 20,  0, 30, 30, 45, 15,
       0,  0, 15, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
      92, 92, 92, 92, 92, 92, 92
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
    {"op",			XMLKEY_OP},
    {"lex",			XMLKEY_LEX},
    {"decl",			XMLKEY_DECL},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {(char*)0,XMLKEY_INVALID},
    {"language",		XMLKEY_LANGUAGE},
    {"nocc",			XMLKEY_NOCC},
    {"lpath",			XMLKEY_LPATH},
    {"symbol",			XMLKEY_SYMBOL},
    {(char*)0,XMLKEY_INVALID}, {(char*)0,XMLKEY_INVALID},
    {"name",			XMLKEY_NAME},
    {"error",			XMLKEY_ERROR},
    {"srcuse",			XMLKEY_SRCUSE},
    {"nocc:libinfo",		XMLKEY_NOCC_LIBINFO},
    {(char*)0,XMLKEY_INVALID},
    {"node",			XMLKEY_NODE},
    {"epath",			XMLKEY_EPATH},
    {(char*)0,XMLKEY_INVALID},
    {"newname",		XMLKEY_NEWNAME},
    {(char*)0,XMLKEY_INVALID},
    {"proc",			XMLKEY_PROC},
    {"ipath",			XMLKEY_IPATH},
    {(char*)0,XMLKEY_INVALID},
    {"allocms",		XMLKEY_ALLOCMS},
    {(char*)0,XMLKEY_INVALID},
    {"namespace",		XMLKEY_NAMESPACE},
    {"srcinclude",		XMLKEY_SRCINCLUDE},
    {(char*)0,XMLKEY_INVALID},
    {"allocws",		XMLKEY_ALLOCWS},
    {"tag",			XMLKEY_TAG},
    {"desc",			XMLKEY_DESC},
    {"maintainer",		XMLKEY_MAINTAINER},
    {"parser",			XMLKEY_PARSER},
    {"comment",		XMLKEY_COMMENT},
    {(char*)0,XMLKEY_INVALID},
    {"blockinfo",		XMLKEY_BLOCKINFO},
    {(char*)0,XMLKEY_INVALID},
    {"action",			XMLKEY_ACTION},
    {"libunit",		XMLKEY_LIBUNIT},
    {(char*)0,XMLKEY_INVALID},
    {"path",			XMLKEY_PATH},
    {"match",			XMLKEY_MATCH},
    {"constr",			XMLKEY_CONSTR},
    {"library",		XMLKEY_LIBRARY},
    {(char*)0,XMLKEY_INVALID},
    {"extension",		XMLKEY_EXTENSION},
    {"check",			XMLKEY_CHECK},
    {"adjust",			XMLKEY_ADJUST},
    {"version",		XMLKEY_VERSION},
    {(char*)0,XMLKEY_INVALID},
    {"nativelib",		XMLKEY_NATIVELIB},
    {"descriptor",		XMLKEY_DESCRIPTOR},
    {"author",			XMLKEY_AUTHOR},
    {"keyword",		XMLKEY_KEYWORD},
    {(char*)0,XMLKEY_INVALID},
    {"tree",			XMLKEY_TREE},
    {"value",			XMLKEY_VALUE},
    {(char*)0,XMLKEY_INVALID},
    {"allocvs",		XMLKEY_ALLOCVS},
    {(char*)0,XMLKEY_INVALID},
    {"type",			XMLKEY_TYPE},
    {(char*)0,XMLKEY_INVALID},
    {"target",			XMLKEY_TARGET},
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


