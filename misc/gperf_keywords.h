/* ANSI-C code produced by gperf version 3.0.2 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'1,3,$' */

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
 *	keywords.gperf -- standard keywords for nocc
 *	Fred Barnes, 2005  <frmb@kent.ac.uk>
 */
struct TAG_keyword;

#define TOTAL_KEYWORDS 79
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 212
/* maximum key range = 211, duplicates = 0 */

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
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
       70, 213,  10, 213,   5, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213,  40,  30,  30,  60,  20,
       85,  10, 213,  10, 213,   0,  65,   5,  45,   0,
        0,   0,   0,   5,   0,  55,   0,  15, 213,  20,
       10, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
      213, 213, 213, 213, 213, 213
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const struct TAG_keyword wordlist[] =
  {
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"OR",		52,	LANGTAG_OCCAMPI,	NULL},
    {"PAR",		2,	LANGTAG_OCCAMPI,	NULL},
    {"PORT",		77,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"SEQ",		3,	LANGTAG_OCCAMPI,	NULL},
    {"STOP",		1,	LANGTAG_OCCAMPI,	NULL},
    {"TIMER",		31,	LANGTAG_OCCAMPI,	NULL},
    {"RESULT",		37,	LANGTAG_OCCAMPI,	NULL},
    {"RETYPES",	38,	LANGTAG_OCCAMPI,	NULL},
    {"INT",		7,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"TIMES",		56,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"IS",		13,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"SKIP",		0,	LANGTAG_OCCAMPI,	NULL},
    {"INT16",		8,	LANGTAG_OCCAMPI,	NULL},
    {"SETPRI",		70,	LANGTAG_OCCAMPI,	NULL},
    {"MOSTPOS",	41,	LANGTAG_OCCAMPI,	NULL},
    {"PRI",		48,	LANGTAG_OCCAMPI,	NULL},
    {"TYPE",		18,	LANGTAG_OCCAMPI,	NULL},
    {"INT64",		10,	LANGTAG_OCCAMPI,	NULL},
    {"GETPRI",		74,	LANGTAG_OCCAMPI,	NULL},
    {"MOSTNEG",	42,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"STEP",		45,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"PROC",		4,	LANGTAG_OCCAMPI,	NULL},
    {"RESCHEDULE",	69,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"BARRIER",	24,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"SIZE",		33,	LANGTAG_OCCAMPI,	NULL},
    {"INTERLEAVE",	76,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"AT",		19,	LANGTAG_OCCAMPI,	NULL},
    {"ALT",		47,	LANGTAG_OCCAMPI,	NULL},
    {"WORKSPACE",	63,	LANGTAG_OCCAMPI,	NULL},
    {"AFTER",		49,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"NOT",		55,	LANGTAG_OCCAMPI,	NULL},
    {"ELSE",		30,	LANGTAG_OCCAMPI,	NULL},
    {"WHILE",		50,	LANGTAG_OCCAMPI,	NULL},
    {"TRACES",		75,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"ASM",		72,	LANGTAG_OCCAMPI,	NULL},
    {"BYTE",		5,	LANGTAG_OCCAMPI,	NULL},
    {"CLONE",		43,	LANGTAG_OCCAMPI,	NULL},
    {"REAL64",		12,	LANGTAG_OCCAMPI,	NULL},
    {"IN",		62,	LANGTAG_OCCAMPI,	NULL},
    {"VECSPACE",	64,	LANGTAG_OCCAMPI,	NULL},
    {"CASE",		27,	LANGTAG_OCCAMPI,	NULL},
    {"MINUS",		54,	LANGTAG_OCCAMPI,	NULL},
    {"MOBILE",		23,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"MOBSPACE",	65,	LANGTAG_OCCAMPI,	NULL},
    {"PLUS",		53,	LANGTAG_OCCAMPI,	NULL},
    {"PLACE",		61,	LANGTAG_OCCAMPI,	NULL},
    {"PUBLIC",		71,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"PROTOCOL",	26,	LANGTAG_OCCAMPI,	NULL},
    {"CHAR",		68,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"MOBILE.PROC",	59,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"TRUE",		28,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"BYTESIN",	73,	LANGTAG_OCCAMPI,	NULL},
    {"ANY",		60,	LANGTAG_OCCAMPI,	NULL},
    {"SYNC",		25,	LANGTAG_OCCAMPI,	NULL},
    {"INT32",		9,	LANGTAG_OCCAMPI,	NULL},
    {"MOBILE.DATA",	58,	LANGTAG_OCCAMPI,	NULL},
    {"OF",		14,	LANGTAG_OCCAMPI,	NULL},
    {"FOR",		35,	LANGTAG_OCCAMPI,	NULL},
    {"FORK",		22,	LANGTAG_OCCAMPI,	NULL},
    {"TRUNC",		39,	LANGTAG_OCCAMPI,	NULL},
    {"MOBILE.CHAN",	57,	LANGTAG_OCCAMPI,	NULL},
    {"INITIAL",	44,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"FROM",		34,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"RECORD",		46,	LANGTAG_OCCAMPI,	NULL},
    {"IF",		32,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"BOOL",		6,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"INLINE",		21,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"DATA",		17,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"PLACED",		78,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"SHARED",		67,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"REAL32",		11,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"CHAN",		16,	LANGTAG_OCCAMPI,	NULL},
    {"ROUND",		40,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"VAL",		20,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"VALOF",		36,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"AND",		51,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"FALSE",		29,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"FUNCTION",	15,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"DEFINED",	66,	LANGTAG_OCCAMPI,	NULL}
  };

#ifdef __GNUC__
__inline
#endif
const struct TAG_keyword *
keyword_lookup_byname (register const char *str, register unsigned int len)
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


