/* ANSI-C code produced by gperf version 3.0.2 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'1-2' */

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
 *	langdeflookup.gperf -- language definition keywords for NOCC
 *	Fred Barnes, 2007  <frmb@kent.ac.uk>
 */
struct TAG_langdeflookup;

#define TOTAL_KEYWORDS 16
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 10
#define MIN_HASH_VALUE 5
#define MAX_HASH_VALUE 36
/* maximum key range = 32, duplicates = 0 */

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
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37,  5,  5, 37, 20,  0,
      10, 15, 37,  0, 37, 10, 37,  0,  0, 37,
      37, 37, 15,  5,  0, 37, 37, 37, 37,  5,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
      37, 37, 37, 37, 37, 37
    };
  return len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]];
}

static const struct TAG_langdeflookup wordlist[] =
  {
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {(char*)0,LDL_INVALID,NULL},
    {"TNODE",		LDL_TNODE,	NULL},
    {"IMPORT",		LDL_IMPORT,	NULL},
    {"INVALID",	LDL_KINVALID,	NULL},
    {"BNF",		LDL_BNF,	NULL},
    {(char*)0,LDL_INVALID,NULL},
    {"TABLE",		LDL_TABLE,	NULL},
    {"BEFORE",		LDL_BEFORE,	NULL},
    {"SECTION",	LDL_SECTION,	NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {"MAINTAINER",	LDL_MAINTAINER,	NULL},
    {"SYMBOL",		LDL_SYMBOL,	NULL},
    {"KEYWORD",	LDL_KEYWORD,	NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {"AFTER",		LDL_AFTER,	NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {(char*)0,LDL_INVALID,NULL},
    {"DESC",		LDL_DESC,	NULL},
    {"IDENT",		LDL_IDENT,	NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {"RFUNC",		LDL_RFUNC,	NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {(char*)0,LDL_INVALID,NULL}, {(char*)0,LDL_INVALID,NULL},
    {"GRULE",		LDL_GRULE,	NULL},
    {"DFAERR",		LDL_DFAERR,	NULL}
  };

#ifdef __GNUC__
__inline
#endif
const struct TAG_langdeflookup *
langdeflookup_lookup_byname (register const char *str, register unsigned int len)
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


