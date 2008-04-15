/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'1,3-4,8,$' */

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

#define TOTAL_KEYWORDS 98
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 15
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 173
/* maximum key range = 172, duplicates = 0 */

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
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174,  45,  15,
        0,  10, 174, 174,  10, 174,  10, 174, 174, 174,
      174, 174, 174, 174, 174,  10,  25,  25,  10,   5,
       15,  35, 174,  60,  40,   5,   0,   5,  35,  80,
       40,  10,  35,   5,   0,  35,  45,  55, 174, 174,
      174, 174, 174, 174, 174,  30, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
      174, 174, 174, 174, 174, 174
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
      case 6:
      case 5:
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
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
    {"ST",		INS_OTHER,	I_ST,		NULL},
    {"MUL",		INS_SECONDARY,	I_MUL,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LD",		INS_OTHER,	I_LD,		NULL},
    {"ALT",		INS_SECONDARY,	I_ALT,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"EQ",		INS_SECONDARY,	I_EQ,		NULL},
    {"SUM",		INS_SECONDARY,	I_SUM,		NULL},
    {"DIST",		INS_SECONDARY,	I_DIST,		NULL},
    {"MWALT",		INS_SECONDARY,	I_MWALT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWALTWT",	INS_SECONDARY,	I_MWALTWT,	NULL},
    {"MWTALTWT",	INS_SECONDARY,	I_MWTALTWT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LB",		INS_SECONDARY,	I_LB,		NULL},
    {"MRELEASE",	INS_SECONDARY,	I_MRELEASE,	NULL},
    {"DISS",		INS_SECONDARY,	I_DISS,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ALTEND",		INS_SECONDARY,	I_ALTEND,	NULL},
    {"SB",		INS_SECONDARY,	I_SB,		NULL},
    {"ADD",		INS_SECONDARY,	I_ADD,		NULL},
    {"ENBT",		INS_SECONDARY,	I_ENBT,		NULL},
    {"ENBT2",		INS_SECONDARY,	I_ENBT2,	NULL},
    {"MALLOC",		INS_SECONDARY,	I_MALLOC,	NULL},
    {"GT",		INS_SECONDARY,	I_GT,		NULL},
    {"NOT",		INS_SECONDARY,	I_NOT,		NULL},
    {"NULL",		INS_SECONDARY,	I_NULL,		NULL},
    {"ENBS2",		INS_SECONDARY,	I_ENBS2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MTCLONE",	INS_SECONDARY,	I_MTCLONE,	NULL},
    {"MWALTEND",	INS_SECONDARY,	I_MWALTEND,	NULL},
    {"ENBS",		INS_SECONDARY,	I_ENBS,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_ALT",	INS_SECONDARY,	I_MWS_ALT,	NULL},
    {"REM",		INS_SECONDARY,	I_REM,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SETERR",		INS_SECONDARY,	I_SETERR,	NULL},
    {"MWS_DIS",	INS_SECONDARY,	I_MWS_DIS,	NULL},
    {"LDC",		INS_PRIMARY,	I_LDC,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_PPILNK",	INS_SECONDARY,	I_MWS_PPILNK,	NULL},
    {"MWS_ALTLOCK",	INS_SECONDARY,	I_MWS_ALTLOCK,	NULL},
    {"LW",		INS_SECONDARY,	I_LW,		NULL},
    {"SUB",		INS_SECONDARY,	I_SUB,		NULL},
    {"DIFF",		INS_SECONDARY,	I_DIFF,		NULL},
    {"ENBC2",		INS_SECONDARY,	I_ENBC2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SW",		INS_SECONDARY,	I_SW,		NULL},
    {"ADC",		INS_PRIMARY,	I_ADC,		NULL},
    {"MOVE",		INS_SECONDARY,	I_MOVE,		NULL},
    {"MWS_ALTEND",	INS_SECONDARY,	I_MWS_ALTEND,	NULL},
    {"MTFREE",		INS_SECONDARY,	I_MTFREE,	NULL},
    {"CJ",		INS_PRIMARY,	I_CJ,		NULL},
    {"FBARINIT",	INS_SECONDARY,	I_FBARINIT,	NULL},
    {"DISC",		INS_SECONDARY,	I_DISC,		NULL},
    {"ALTWT",		INS_SECONDARY,	I_ALTWT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_ENB",	INS_SECONDARY,	I_MWS_ENB,	NULL},
    {"TIN",		INS_SECONDARY,	I_TIN,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWENB",		INS_SECONDARY,	I_MWENB,	NULL},
    {"MWS_PPPAROF",	INS_SECONDARY,	I_MWS_PPPAROF,	NULL},
    {"MWS_PPBASEOF",	INS_SECONDARY,	I_MWS_PPBASEOF,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"J",		INS_PRIMARY, 	I_J,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"OUT",		INS_SECONDARY,	I_OUT,		NULL},
    {"ENBC",		INS_SECONDARY,	I_ENBC,		NULL},
    {"MWDIS",		INS_SECONDARY,	I_MWDIS,	NULL},
    {"JTABLE",		INS_PRIMARY,	I_JTABLE,	NULL},
    {"MWS_PBENROLL",	INS_SECONDARY,	I_MWS_PBENROLL,	NULL},
    {"MWS_PBADJSYNC",	INS_SECONDARY,	I_MWS_PBADJSYNC,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_PBRULNK",	INS_SECONDARY,	I_MWS_PBRULNK,	NULL},
    {"MWS_PBRESIGN",	INS_SECONDARY,	I_MWS_PBRESIGN,	NULL},
    {"MWS_ALTUNLOCK",	INS_SECONDARY,	I_MWS_ALTUNLOCK,NULL},
    {"TRAP",		INS_SECONDARY,	I_TRAP,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"STARTP",		INS_SECONDARY,	I_STARTP,	NULL},
    {"IN",		INS_SECONDARY,	I_IN,		NULL},
    {"MWS_SYNC",	INS_SECONDARY,	I_MWS_SYNC,	NULL},
    {"ENDP",		INS_SECONDARY,	I_ENDP,		NULL},
    {"MWS_ALTPOSTLOCK",INS_SECONDARY,	I_MWS_ALTPOSTLOCK,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LDTIMER",	INS_SECONDARY,	I_LDTIMER,	NULL},
    {"DIV",		INS_SECONDARY,	I_DIV,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MTNEW",		INS_SECONDARY,	I_MTNEW,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"NEG",		INS_SECONDARY,	I_NEG,		NULL},
    {"MWS_BINIT",	INS_SECONDARY,	I_MWS_BINIT,	NULL},
    {"IOR32",		INS_SECONDARY,	I_IOR32,	NULL},
    {"SETPRI",		INS_SECONDARY,	I_SETPRI,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"RESCHEDULE",	INS_SECONDARY,	I_RESCHEDULE,	NULL},
    {"MWS_PBRILNK",	INS_SECONDARY,	I_MWS_PBRILNK,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARSYNC",	INS_SECONDARY,	I_FBARSYNC,	NULL},
    {"IOR8",		INS_SECONDARY,	I_IOR8,		NULL},
    {"BOOLINVERT",	INS_SECONDARY,	I_BOOLINVERT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"POP",		INS_SECONDARY,	I_POP,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOR16",		INS_SECONDARY,	I_IOR16,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"REV",		INS_SECONDARY,	I_REV,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW32",		INS_SECONDARY,	I_IOW32,	NULL},
    {"JCSUB0",		INS_PRIMARY,	I_JCSUB0,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOR",		INS_SECONDARY,	I_IOR,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"CSUB0",		INS_SECONDARY,	I_CSUB0,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW8",		INS_SECONDARY,	I_IOW8,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"GETPRI",		INS_SECONDARY,	I_GETPRI,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"PROD",		INS_SECONDARY,	I_PROD,		NULL},
    {"IOW16",		INS_SECONDARY,	I_IOW16,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARENROLL",	INS_SECONDARY,	I_FBARENROLL,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
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
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARRESIGN",	INS_SECONDARY,	I_FBARRESIGN,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"STOPP",		INS_SECONDARY,	I_STOPP,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW",		INS_SECONDARY,	I_IOW,		NULL}
  };

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
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


