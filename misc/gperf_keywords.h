/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf  */
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

#define TOTAL_KEYWORDS 71
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 9
#define MAX_HASH_VALUE 167
/* maximum key range = 159, duplicates = 0 */

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
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
       35, 168,  25, 168,  30, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168,  20,  30,  40,  55,   0,
       50,  25, 168,   5, 168,   5,  45,   5,  35,  50,
       10,  30,   0,  10,   5,  25,  10,   0, 168,  20,
       30, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
      168, 168, 168, 168, 168, 168
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
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL},
    {"WORKSPACE",	63,	NULL},
    {"WHILE",		50,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"PAR",		2,	NULL},
    {"ELSE",		30,	NULL},
    {"TIMER",		31,	NULL},
    {(char*)0,-1,NULL},
    {"IS",		13,	NULL},
    {"INT",		7,	NULL},
    {"TYPE",		18,	NULL},
    {"RESCHEDULE",	69,	NULL},
    {"RESULT",		37,	NULL},
    {"RETYPES",	38,	NULL},
    {"PRI",		48,	NULL},
    {"STEP",		45,	NULL},
    {"TIMES",		56,	NULL},
    {"SETPRI",		70,	NULL},
    {"AT",		19,	NULL},
    {(char*)0,-1,NULL},
    {"SKIP",		0,	NULL},
    {"AFTER",		49,	NULL},
    {(char*)0,-1,NULL},
    {"MOSTPOS",	41,	NULL},
    {"ALT",		47,	NULL},
    {"TRUE",		28,	NULL},
    {"PLACE",		61,	NULL},
    {(char*)0,-1,NULL},
    {"BARRIER",	24,	NULL},
    {(char*)0,-1,NULL},
    {"BYTE",		5,	NULL},
    {"INT64",		10,	NULL},
    {"MOBILE",		23,	NULL},
    {"IN",		62,	NULL},
    {"MOBSPACE",	65,	NULL},
    {"SIZE",		33,	NULL},
    {"INT16",		8,	NULL},
    {(char*)0,-1,NULL},
    {"MOSTNEG",	42,	NULL},
    {"NOT",		55,	NULL},
    {"PLUS",		53,	NULL},
    {"INT32",		9,	NULL},
    {"REAL64",		12,	NULL},
    {"OR",		52,	NULL},
    {"FOR",		35,	NULL},
    {"CASE",		27,	NULL},
    {"MINUS",		54,	NULL},
    {"INLINE",		21,	NULL},
    {"IF",		32,	NULL},
    {"VECSPACE",	64,	NULL},
    {"FORK",		22,	NULL},
    {(char*)0,-1,NULL},
    {"REAL32",		11,	NULL},
    {"INITIAL",	44,	NULL},
    {"ANY",		60,	NULL},
    {"CHAR",		68,	NULL},
    {(char*)0,-1,NULL},
    {"MOBILE.DATA",	58,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"SEQ",		3,	NULL},
    {"STOP",		1,	NULL},
    {"TRUNC",		39,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL},
    {"MOBILE.CHAN",	57,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"DATA",		17,	NULL},
    {"ROUND",		40,	NULL},
    {"MOBILE.PROC",	59,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"SYNC",		25,	NULL},
    {(char*)0,-1,NULL},
    {"SHARED",		67,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL},
    {"CLONE",		43,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL},
    {"CHAN",		16,	NULL},
    {"FALSE",		29,	NULL},
    {"RECORD",		46,	NULL},
    {"OF",		14,	NULL},
    {"VAL",		20,	NULL},
    {"PROC",		4,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"FROM",		34,	NULL},
    {"VALOF",		36,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"PROTOCOL",	26,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {"FUNCTION",	15,	NULL},
    {"BOOL",		6,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL},
    {"AND",		51,	NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL}, {(char*)0,-1,NULL},
    {(char*)0,-1,NULL},
    {"DEFINED",	66,	NULL}
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


