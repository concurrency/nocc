/* ANSI-C code produced by gperf version 3.0.4 */
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
 *	Fred Barnes, 2005-2014  <frmb@kent.ac.uk>
 */
struct TAG_keyword;

#define TOTAL_KEYWORDS 131
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 12
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 206
/* maximum key range = 204, duplicates = 0 */

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
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
       45, 207,  35, 207,  40, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207,  40,  15,   5,  50,   0,
       45,  25,  20,   0,  65, 115,  15,   5,  20,  75,
        5, 150,  25,  15,   0,  35,  60, 125,  20,   0,
        0, 150, 207, 207, 207, 207, 207,  50,  10,  60,
       20,  70,  10,   0,   5,  40,  60, 207,   0,  50,
       35, 100,   0,  45,  20, 100,   0,  55,   0,  15,
      207, 207, 207,   0, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
      207, 207, 207, 207, 207, 207, 207
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[2]+1];
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
    {(char*)0,-1,0,NULL},
    {"TRY",		83,	LANGTAG_OCCAMPI,	NULL},
    {"ELSE",		30,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"protocol",	117,	LANGTAG_GUPPY,		NULL},
    {"CASE",		27,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"INLINE",		21,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"bool",		102,	LANGTAG_GUPPY,		NULL},
    {"CLONE",		43,	LANGTAG_OCCAMPI,	NULL},
    {"MOBILE",		23,	LANGTAG_OCCAMPI,	NULL},
    {"IS",		13,	LANGTAG_OCCAMPI,	NULL},
    {"MOBSPACE",	65,	LANGTAG_OCCAMPI,	NULL},
    {"PROC",		4,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"PUBLIC",		71,	LANGTAG_OCCAMPI,	NULL},
    {"IN",		62,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"PORT",		77,	LANGTAG_OCCAMPI,	NULL},
    {"PLACE",		61,	LANGTAG_OCCAMPI,	NULL},
    {"MOBILE.PROC",	59,	LANGTAG_OCCAMPI,	NULL},
    {"MOSTPOS",	41,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"STOP",		1,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"RESULT",		37,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"PROTOCOL",	26,	LANGTAG_OCCAMPI,	NULL},
    {"real",		100,	LANGTAG_GUPPY | 0x00010000,		NULL},
    {"RESCHEDULE",	69,	LANGTAG_OCCAMPI,	NULL},
    {"TRACES",		75,	LANGTAG_OCCAMPI,	NULL},
    {"MOSTNEG",	42,	LANGTAG_OCCAMPI,	NULL},
    {"INT",		7,	LANGTAG_OCCAMPI,	NULL},
    {"BOOL",		6,	LANGTAG_OCCAMPI,	NULL},
    {"TIMES",		56,	LANGTAG_OCCAMPI,	NULL},
    {"MOBILE.CHAN",	57,	LANGTAG_OCCAMPI,	NULL},
    {"AT",		19,	LANGTAG_OCCAMPI,	NULL},
    {"ANY",		60,	LANGTAG_OCCAMPI,	NULL},
    {"CHAN",		16,	LANGTAG_OCCAMPI,	NULL},
    {"INTERLEAVE",	76,	LANGTAG_OCCAMPI,	NULL},
    {"ASSERT",		87,	LANGTAG_OCCAMPI,	NULL},
    {"IF",		32,	LANGTAG_OCCAMPI,	NULL},
    {"PAR",		2,	LANGTAG_OCCAMPI,	NULL},
    {"CHAR",		68,	LANGTAG_OCCAMPI,	NULL},
    {"TIMER",		31,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"if",		129,	LANGTAG_GUPPY | LANGTAG_TRLANG | LANGTAG_AVRASM,	NULL},
    {"val",		98,	LANGTAG_GUPPY,		NULL},
    {"BYTE",		5,	LANGTAG_OCCAMPI,	NULL},
    {"FALSE",		29,	LANGTAG_OCCAMPI,	NULL},
    {"SETPRI",		70,	LANGTAG_OCCAMPI,	NULL},
    {"EXTENDS",	86,	LANGTAG_OCCAMPI,	NULL},
    {"NOT",		55,	LANGTAG_OCCAMPI,	NULL},
    {"FROM",		34,	LANGTAG_OCCAMPI,	NULL},
    {"timer",		127,	LANGTAG_GUPPY,		NULL},
    {"MOBILE.DATA",	58,	LANGTAG_OCCAMPI,	NULL},
    {"BARRIER",	24,	LANGTAG_OCCAMPI,	NULL},
    {"XOR",		79,	LANGTAG_OCCAMPI,	NULL},
    {"TRUE",		28,	LANGTAG_OCCAMPI,	NULL},
    {"CATCH",		82,	LANGTAG_OCCAMPI,	NULL},
    {"GETPRI",		74,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"ASM",		72,	LANGTAG_OCCAMPI,	NULL},
    {"STEP",		45,	LANGTAG_OCCAMPI,	NULL},
    {"TRUNC",		39,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"PRI",		48,	LANGTAG_OCCAMPI,	NULL},
    {"true",		125,	LANGTAG_GUPPY,		NULL},
    {"INT64",		10,	LANGTAG_OCCAMPI,	NULL},
    {"PLACED",		78,	LANGTAG_OCCAMPI,	NULL},
    {"BYTESIN",	73,	LANGTAG_OCCAMPI,	NULL},
    {"ALT",		47,	LANGTAG_OCCAMPI,	NULL},
    {"EXCEPTION",	80,	LANGTAG_OCCAMPI,	NULL},
    {"INT16",		8,	LANGTAG_OCCAMPI,	NULL},
    {"REAL64",		12,	LANGTAG_OCCAMPI,	NULL},
    {"RETYPES",	38,	LANGTAG_OCCAMPI,	NULL},
    {"VAL",		20,	LANGTAG_OCCAMPI,	NULL},
    {"PLUS",		53,	LANGTAG_OCCAMPI,	NULL},
    {"INT32",		9,	LANGTAG_OCCAMPI,	NULL},
    {"SHARED",		67,	LANGTAG_OCCAMPI,	NULL},
    {"INITIAL",	44,	LANGTAG_OCCAMPI,	NULL},
    {"FOR",		35,	LANGTAG_OCCAMPI,	NULL},
    {"SKIP",		0,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"REAL32",		11,	LANGTAG_OCCAMPI,	NULL},
    {"NOEXCEPTIONS",	85,	LANGTAG_OCCAMPI,	NULL},
    {"AND",		51,	LANGTAG_OCCAMPI,	NULL},
    {"char",		103,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL},
    {"define",		96,	LANGTAG_GUPPY,		NULL},
    {"ON",		88,	LANGTAG_OCCAMPI,	NULL},
    {"int",		99,	LANGTAG_GUPPY | 0x00010000,		NULL},
    {"SYNC",		25,	LANGTAG_OCCAMPI,	NULL},
    {"MINUS",		54,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"OR",		52,	LANGTAG_OCCAMPI,	NULL},
    {"pri",		109,	LANGTAG_GUPPY,		NULL},
    {"stop",		95,	LANGTAG_GUPPY,		NULL},
    {"AFTER",		49,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"bytesin",	120,	LANGTAG_GUPPY,		NULL},
    {"alt",		108,	LANGTAG_GUPPY,		NULL},
    {"chan",		97,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"UINT",		90,	LANGTAG_OCCAMPI,	NULL},
    {"VALOF",		36,	LANGTAG_OCCAMPI,	NULL},
    {"return",		122,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL},
    {"VECSPACE",	64,	LANGTAG_OCCAMPI,	NULL},
    {"type",		105,	LANGTAG_GUPPY,		NULL},
    {"throw",		115,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL},
    {"OF",		14,	LANGTAG_OCCAMPI,	NULL},
    {"par",		107,	LANGTAG_GUPPY,		NULL},
    {"enum",		110,	LANGTAG_GUPPY,		NULL},
    {"catch",		116,	LANGTAG_GUPPY,		NULL},
    {"public",		112,	LANGTAG_GUPPY,		NULL},
    {"FINALLY",	84,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"DATA",		17,	LANGTAG_OCCAMPI,	NULL},
    {"after",		128,	LANGTAG_GUPPY,		NULL},
    {"RECORD",		46,	LANGTAG_OCCAMPI,	NULL},
    {"DEFINED",	66,	LANGTAG_OCCAMPI,	NULL},
    {"for",		113,	LANGTAG_GUPPY,		NULL},
    {"case",		123,	LANGTAG_GUPPY,		NULL},
    {"false",		126,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"byte",		101,	LANGTAG_GUPPY,		NULL},
    {"ROUND",		40,	LANGTAG_OCCAMPI,	NULL},
    {"VECTOR",		89,	LANGTAG_OCCAMPI,	NULL},
    {"is",		121,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL},
    {"else",		130,	LANGTAG_GUPPY | LANGTAG_TRLANG | LANGTAG_AVRASM,	NULL},
    {"THROW",		81,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"FUNCTION",	15,	LANGTAG_OCCAMPI,	NULL},
    {"WORKSPACE",	63,	LANGTAG_OCCAMPI,	NULL},
    {"while",		124,	LANGTAG_GUPPY,		NULL},
    {"UINT64",		93,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"TYPE",		18,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"UINT16",		91,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"uint",		111,	LANGTAG_GUPPY | 0x00010000,		NULL},
    {(char*)0,-1,0,NULL},
    {"UINT32",		92,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"end",		119,	LANGTAG_GUPPY,		NULL},
    {"skip",		94,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"seq",		106,	LANGTAG_GUPPY,		NULL},
    {"SIZE",		33,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"subprotocol",	118,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"size",		114,	LANGTAG_GUPPY,		NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"FORK",		22,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL},
    {"SEQ",		3,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL},
    {"WHILE",		50,	LANGTAG_OCCAMPI,	NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {(char*)0,-1,0,NULL}, {(char*)0,-1,0,NULL},
    {"string",		104,	LANGTAG_GUPPY,		NULL}
  };

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
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


