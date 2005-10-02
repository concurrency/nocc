/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf  */
/* Computed positions: -k'1,3' */

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
 *	transinstr.gperf -- transputer instructions for nocc
 *	Fred Barnes, 2005  <frmb@kent.ac.uk>
 */
struct TAG_transinstr;

#define TOTAL_KEYWORDS 27
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 10
#define MIN_HASH_VALUE 1
#define MAX_HASH_VALUE 60
/* maximum key range = 60, duplicates = 0 */

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
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 15, 30, 13,  3, 10,
      20,  5, 61, 20,  0, 61,  0,  5, 25, 20,
       0, 61, 10,  5,  0, 61,  0, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61
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
  return hval;
}

static const struct TAG_transinstr wordlist[] =
  {
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"J",		INS_PRIMARY, 	I_J,		NULL},
    {"LT",		INS_SECONDARY,	I_LT,		NULL},
    {"LDL",		INS_PRIMARY,	I_LDL,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"DIV",		INS_SECONDARY,	I_DIV,		NULL},
    {"GT",		INS_SECONDARY,	I_GT,		NULL},
    {"MUL",		INS_SECONDARY,	I_MUL,		NULL},
    {"MOVE",		INS_SECONDARY,	I_MOVE,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SETERR",		INS_SECONDARY,	I_SETERR,	NULL},
    {"EQ",		INS_SECONDARY,	I_EQ,		NULL},
    {"SUM",		INS_SECONDARY,	I_SUM,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"CJ",		INS_PRIMARY,	I_CJ,		NULL},
    {"LDC",		INS_PRIMARY,	I_LDC,		NULL},
    {"ENDP",		INS_SECONDARY,	I_ENDP,		NULL},
    {"REM",		INS_SECONDARY,	I_REM,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ADD",		INS_SECONDARY,	I_ADD,		NULL},
    {"IN",		INS_SECONDARY,	I_IN,		NULL},
    {"OUT",		INS_SECONDARY,	I_OUT,		NULL},
    {"PROD",		INS_SECONDARY,	I_PROD,		NULL},
    {"RESCHEDULE",	INS_SECONDARY,	I_RESCHEDULE,	NULL},
    {"STARTP",		INS_SECONDARY,	I_STARTP,	NULL},
    {"DIFF",		INS_SECONDARY,	I_DIFF,		NULL},
    {"NOT",		INS_SECONDARY,	I_NOT,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"STOPP",		INS_SECONDARY,	I_STOPP,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"NEG",		INS_SECONDARY,	I_NEG,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SUB",		INS_SECONDARY,	I_SUB,		NULL},
    {"RUNP",		INS_SECONDARY,	I_RUNP,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"BOOLINVERT",	INS_SECONDARY,	I_BOOLINVERT,	NULL}
  };

#ifdef __GNUC__
__inline
#endif
const struct TAG_transinstr *
transinstr_lookup_byname (register const char *str, register unsigned int len)
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


