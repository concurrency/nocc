/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf  */
/* Computed positions: -k'1,$' */

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

#define TOTAL_KEYWORDS 64
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 10
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 125
/* maximum key range = 124, duplicates = 0 */

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
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126,  70,  35,  10,  40,   5,
       70,  10, 126,   0,  50, 126,   0,  20,  45,   5,
       30,  50,  70,  15,   0, 126,   0,   5, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126
    };
  return len + asso_values[(unsigned char)str[len - 1]] + asso_values[(unsigned char)str[0]];
}

static const struct TAG_transinstr wordlist[] =
  {
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LT",		INS_SECONDARY,	I_LT,		NULL},
    {"LDL",		INS_PRIMARY,	I_LDL,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"TALTWT",		INS_SECONDARY,	I_TALTWT,	NULL},
    {"LW",		INS_SECONDARY,	I_LW,		NULL},
    {"OUT",		INS_SECONDARY,	I_OUT,		NULL},
    {"ENBT",		INS_SECONDARY,	I_ENBT,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"GT",		INS_SECONDARY,	I_GT,		NULL},
    {"LDC",		INS_PRIMARY,	I_LDC,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"GETPRI",		INS_SECONDARY,	I_GETPRI,	NULL},
    {"ST",		INS_OTHER,	I_ST,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ENBC",		INS_SECONDARY,	I_ENBC,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SETPRI",		INS_SECONDARY,	I_SETPRI,	NULL},
    {"SW",		INS_SECONDARY,	I_SW,		NULL},
    {"MUL",		INS_SECONDARY,	I_MUL,		NULL},
    {"ENBS",		INS_SECONDARY,	I_ENBS,		NULL},
    {"MWALT",		INS_SECONDARY,	I_MWALT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWALTWT",	INS_SECONDARY,	I_MWALTWT,	NULL},
    {"MWTALTWT",	INS_SECONDARY,	I_MWTALTWT,	NULL},
    {"MOVE",		INS_SECONDARY,	I_MOVE,		NULL},
    {"MTNEW",		INS_SECONDARY,	I_MTNEW,	NULL},
    {"MTFREE",		INS_SECONDARY,	I_MTFREE,	NULL},
    {"MTCLONE",	INS_SECONDARY,	I_MTCLONE,	NULL},
    {"MRELEASE",	INS_SECONDARY,	I_MRELEASE,	NULL},
    {"TRAP",		INS_SECONDARY,	I_TRAP,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MALLOC",		INS_SECONDARY,	I_MALLOC,	NULL},
    {"LB",		INS_SECONDARY,	I_LB,		NULL},
    {"SUM",		INS_SECONDARY,	I_SUM,		NULL},
    {"ENDP",		INS_SECONDARY,	I_ENDP,		NULL},
    {"MWDIS",		INS_SECONDARY,	I_MWDIS,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LD",		INS_OTHER,	I_LD,		NULL},
    {"DIV",		INS_SECONDARY,	I_DIV,		NULL},
    {"DIST",		INS_SECONDARY,	I_DIST,		NULL},
    {"BOOLINVERT",	INS_SECONDARY,	I_BOOLINVERT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IN",		INS_SECONDARY,	I_IN,		NULL},
    {"NOT",		INS_SECONDARY,	I_NOT,		NULL},
    {"NULL",		INS_SECONDARY,	I_NULL,		NULL},
    {"STOPP",		INS_SECONDARY,	I_STOPP,	NULL},
    {"STARTP",		INS_SECONDARY,	I_STARTP,	NULL},
    {"SB",		INS_SECONDARY,	I_SB,		NULL},
    {"SUB",		INS_SECONDARY,	I_SUB,		NULL},
    {"DISC",		INS_SECONDARY,	I_DISC,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"EQ",		INS_SECONDARY,	I_EQ,		NULL},
    {"NEG",		INS_SECONDARY,	I_NEG,		NULL},
    {"DISS",		INS_SECONDARY,	I_DISS,		NULL},
    {"MWENB",		INS_SECONDARY,	I_MWENB,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"CJ",		INS_PRIMARY,	I_CJ,		NULL},
    {"POP",		INS_SECONDARY,	I_POP,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWALTEND",	INS_SECONDARY,	I_MWALTEND,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ALT",		INS_SECONDARY,	I_ALT,		NULL},
    {"PROD",		INS_SECONDARY,	I_PROD,		NULL},
    {"ALTWT",		INS_SECONDARY,	I_ALTWT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARINIT",	INS_SECONDARY,	I_FBARINIT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARENROLL",	INS_SECONDARY,	I_FBARENROLL,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ADC",		INS_PRIMARY,	I_ADC,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"RESCHEDULE",	INS_SECONDARY,	I_RESCHEDULE,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARSYNC",	INS_SECONDARY,	I_FBARSYNC,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SETERR",		INS_SECONDARY,	I_SETERR,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"REM",		INS_SECONDARY,	I_REM,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"J",		INS_PRIMARY, 	I_J,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"RUNP",		INS_SECONDARY,	I_RUNP,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ADD",		INS_SECONDARY,	I_ADD,		NULL},
    {"DIFF",		INS_SECONDARY,	I_DIFF,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ALTEND",		INS_SECONDARY,	I_ALTEND,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARRESIGN",	INS_SECONDARY,	I_FBARRESIGN,	NULL}
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


