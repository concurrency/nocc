/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: /usr/bin/gperf  */
/* Computed positions: -k'1,3-4,6,8,$' */

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
 *	Fred Barnes, 2005-2016  <frmb@kent.ac.uk>
 */
struct TAG_transinstr;

#define TOTAL_KEYWORDS 170
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 15
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 419
/* maximum key range = 418, duplicates = 0 */

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
  static const unsigned short asso_values[] =
    {
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420,  25, 130,
       70,  45,   0, 420,   5, 420,  15, 420, 420, 420,
      420, 420, 420, 420, 420,  30,  30,  90,  10,  30,
        0,  65,  20, 100,   5,  45,   0,  60,   5,  65,
       75,   5,  75,  20,   0,  25, 145,  70,   5,   0,
        0, 420, 420, 420, 420,  60, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420, 420, 420, 420, 420,
      420, 420, 420, 420, 420, 420
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
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
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"NOT",		INS_SECONDARY,	I_NOT,		NULL},
    {"NULL",		INS_SECONDARY,	I_NULL,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"J",		INS_PRIMARY, 	I_J,		NULL},
    {"LD",		INS_OTHER,	I_LD,		NULL},
    {"TIN",		INS_SECONDARY,	I_TIN,		NULL},
    {"DIFF",		INS_SECONDARY,	I_DIFF,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDALL",	INS_SECONDARY,	I_FPLDALL,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ST",		INS_OTHER,	I_ST,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPSTALL",	INS_SECONDARY,	I_FPSTALL,	NULL},
    {"FPLDNLSN",	INS_SECONDARY,	I_FPLDNLSN,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPSQRT",		INS_SECONDARY,	I_FPSQRT,	NULL},
    {"LB",		INS_SECONDARY,	I_LB,		NULL},
    {"ALT",		INS_SECONDARY,	I_ALT,		NULL},
    {"DIST",		INS_SECONDARY,	I_DIST,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDNLADDSN",	INS_SECONDARY,	I_FPLDNLADDSN,	NULL},
    {"EQ",		INS_SECONDARY,	I_EQ,		NULL},
    {"FPSTNLSN",	INS_SECONDARY,	I_FPSTNLSN,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPEQ",		INS_SECONDARY,	I_FPEQ,		NULL},
    {"FPNAN",		INS_SECONDARY,	I_FPNAN,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDTEST",	INS_SECONDARY,	I_FPLDTEST,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDNLMULSN",	INS_SECONDARY,	I_FPLDNLMULSN,	NULL},
    {"SB",		INS_SECONDARY,	I_SB,		NULL},
    {"ADD",		INS_SECONDARY,	I_ADD,		NULL},
    {"UMUL",		INS_SECONDARY,	I_UMUL,		NULL},
    {"FPADD",		INS_SECONDARY,	I_FPADD,	NULL},
    {"SETAFF",		INS_SECONDARY,	I_SETAFF,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPSTTEST",	INS_SECONDARY,	I_FPSTTEST,	NULL},
    {"UADD",		INS_SECONDARY,	I_UADD,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDNLADDDB",	INS_SECONDARY,	I_FPLDNLADDDB,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MUL",		INS_SECONDARY,	I_MUL,		NULL},
    {"ENBT",		INS_SECONDARY,	I_ENBT,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"GT",		INS_SECONDARY,	I_GT,		NULL},
    {"OUT",		INS_SECONDARY,	I_OUT,		NULL},
    {"FPGT",		INS_SECONDARY,	I_FPGT,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LW",		INS_SECONDARY,	I_LW,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"DISS",		INS_SECONDARY,	I_DISS,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDNLMULDB",	INS_SECONDARY,	I_FPLDNLMULDB,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDNLDB",	INS_SECONDARY,	I_FPLDNLDB,	NULL},
    {"FPRZ",		INS_SECONDARY,	I_FPRZ,		NULL},
    {"FPSUB",		INS_SECONDARY,	I_FPSUB,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SUB",		INS_SECONDARY,	I_SUB,		NULL},
    {"FPADDDBSN",	INS_SECONDARY,	I_FPADDDBSN,	NULL},
    {"FPABS",		INS_SECONDARY,	I_FPABS,	NULL},
    {"ALTEND",		INS_SECONDARY,	I_ALTEND,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPSTNLDB",	INS_SECONDARY,	I_FPSTNLDB,	NULL},
    {"FPRN",		INS_SECONDARY,	I_FPRN,		NULL},
    {"FPMUL",		INS_SECONDARY,	I_FPMUL,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SW",		INS_SECONDARY,	I_SW,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWALT",		INS_SECONDARY,	I_MWALT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"CJ",		INS_PRIMARY,	I_CJ,		NULL},
    {"MWTALTWT",	INS_SECONDARY,	I_MWTALTWT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"GETAFF",		INS_SECONDARY,	I_GETAFF,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ENBS",		INS_SECONDARY,	I_ENBS,		NULL},
    {"ALTWT",		INS_SECONDARY,	I_ALTWT,	NULL},
    {"JCSUB0",		INS_PRIMARY,	I_JCSUB0,	NULL},
    {"IN",		INS_SECONDARY,	I_IN,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPINT",		INS_SECONDARY,	I_FPINT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"USUB",		INS_SECONDARY,	I_USUB,		NULL},
    {"FPDUP",		INS_SECONDARY,	I_FPDUP,	NULL},
    {"FPNOTFINITE",	INS_SECONDARY,	I_FPNOTFINITE,	NULL},
    {"FPENTRY",	INS_SECONDARY,	I_FPENTRY,	NULL},
    {"FBARINIT",	INS_SECONDARY,	I_FBARINIT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDZEROSN",	INS_SECONDARY,	I_FPLDZEROSN,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLDNLSNI",	INS_SECONDARY,	I_FPLDNLSNI,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SHUTDOWN",	INS_SECONDARY,	I_SHUTDOWN,	NULL},
    {"FPGE",		INS_SECONDARY,	I_FPGE,		NULL},
    {"MWENB",		INS_SECONDARY,	I_MWENB,	NULL},
    {"JTABLE",		INS_PRIMARY,	I_JTABLE,	NULL},
    {"OUTBYTE",	INS_SECONDARY,	I_OUTBYTE,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPLG",		INS_SECONDARY,	I_FPLG,		NULL},
    {"ENBT2",		INS_SECONDARY,	I_ENBT2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"NEG",		INS_SECONDARY,	I_NEG,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"BOOLINVERT",	INS_SECONDARY,	I_BOOLINVERT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SUM",		INS_SECONDARY,	I_SUM,		NULL},
    {"FPSTNLI32",	INS_SECONDARY,	I_FPSTNLI32,	NULL},
    {"FPLDZERODB",	INS_SECONDARY,	I_FPLDZERODB,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_ALT",	INS_SECONDARY,	I_MWS_ALT,	NULL},
    {"MWALTEND",	INS_SECONDARY,	I_MWALTEND,	NULL},
    {"FPLDNLDBI",	INS_SECONDARY,	I_FPLDNLDBI,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ENBS2",		INS_SECONDARY,	I_ENBS2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPB32TOR64",	INS_SECONDARY,	I_FPB32TOR64,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"SMALLER",	INS_SECONDARY,	I_SMALLER,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"PROD",		INS_SECONDARY,	I_PROD,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWALTWT",	INS_SECONDARY,	I_MWALTWT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPREM",		INS_SECONDARY,	I_FPREM,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"CSUB0",		INS_SECONDARY,	I_CSUB0,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"UREM",		INS_SECONDARY,	I_UREM,		NULL},
    {"UPROD",		INS_SECONDARY,	I_UPROD,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_ENB",	INS_SECONDARY,	I_MWS_ENB,	NULL},
    {"LDC",		INS_PRIMARY,	I_LDC,		NULL},
    {"TRAP",		INS_SECONDARY,	I_TRAP,		NULL},
    {"FBARENROLL",	INS_SECONDARY,	I_FBARENROLL,	NULL},
    {"GETPAS",		INS_SECONDARY,	I_GETPAS,	NULL},
    {"MWS_PBENROLL",	INS_SECONDARY,	I_MWS_PBENROLL,	NULL},
    {"MRELEASE",	INS_SECONDARY,	I_MRELEASE,	NULL},
    {"FPTESTERR",	INS_SECONDARY,	I_FPTESTERR,	NULL},
    {"MWS_ALTEND",	INS_SECONDARY,	I_MWS_ALTEND,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MTCLONE",	INS_SECONDARY,	I_MTCLONE,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ENDP",		INS_SECONDARY,	I_ENDP,		NULL},
    {"MWDIS",		INS_SECONDARY,	I_MWDIS,	NULL},
    {"MWS_ALTLOCK",	INS_SECONDARY,	I_MWS_ALTLOCK,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"REM",		INS_SECONDARY,	I_REM,		NULL},
    {"FPRM",		INS_SECONDARY,	I_FPRM,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW8",		INS_SECONDARY,	I_IOW8,		NULL},
    {"FPR32TOR64",	INS_SECONDARY,	I_FPR32TOR64,	NULL},
    {"SETERR",		INS_SECONDARY,	I_SETERR,	NULL},
    {"FPRANGE",	INS_SECONDARY,	I_FPRANGE,	NULL},
    {"FPENTRY3",	INS_SECONDARY,	I_FPENTRY3,	NULL},
    {"IOR8",		INS_SECONDARY,	I_IOR8,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"LDTIMER",	INS_SECONDARY,	I_LDTIMER,	NULL},
    {"ADC",		INS_PRIMARY,	I_ADC,		NULL},
    {"DISC",		INS_SECONDARY,	I_DISC,		NULL},
    {"FPEXPDEC32",	INS_SECONDARY,	I_FPEXPDEC32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_PBRESIGN",	INS_SECONDARY,	I_MWS_PBRESIGN,	NULL},
    {"FPCHKI64",	INS_SECONDARY,	I_FPCHKI64,	NULL},
    {"FPORDERED",	INS_SECONDARY,	I_FPORDERED,	NULL},
    {"FPPOP",		INS_SECONDARY,	I_FPPOP,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_ALTUNLOCK",	INS_SECONDARY,	I_MWS_ALTUNLOCK,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"ENBC2",		INS_SECONDARY,	I_ENBC2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"OUTWORD",	INS_SECONDARY,	I_OUTWORD,	NULL},
    {"POP",		INS_SECONDARY,	I_POP,		NULL},
    {"FPRP",		INS_SECONDARY,	I_FPRP,		NULL},
    {"FPI32TOR64",	INS_SECONDARY,	I_FPI32TOR64,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"RUNP",		INS_SECONDARY,	I_RUNP,		NULL},
    {"FPR64TOR32",	INS_SECONDARY,	I_FPR64TOR32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"GREATER",	INS_SECONDARY,	I_GREATER,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"STOPP",		INS_SECONDARY,	I_STOPP,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW",		INS_SECONDARY,	I_IOW,		NULL},
    {"ENBC",		INS_SECONDARY,	I_ENBC,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MALLOC",		INS_SECONDARY,	I_MALLOC,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARRESIGN",	INS_SECONDARY,	I_FBARRESIGN,	NULL},
    {"MWS_PBRULNK",	INS_SECONDARY,	I_MWS_PBRULNK,	NULL},
    {"MTALLOC",	INS_SECONDARY,	I_MTALLOC,	NULL},
    {"IOR",		INS_SECONDARY,	I_IOR,		NULL},
    {"MTRELEASE",	INS_SECONDARY,	I_MTRELEASE,	NULL},
    {"FPREV",		INS_SECONDARY,	I_FPREV,	NULL},
    {"MWS_PPPAROF",	INS_SECONDARY,	I_MWS_PPPAROF,	NULL},
    {"MWS_PPBASEOF",	INS_SECONDARY,	I_MWS_PPBASEOF,	NULL},
    {"FPENTRY2",	INS_SECONDARY,	I_FPENTRY2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPDIV",		INS_SECONDARY,	I_FPDIV,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPMULBY2",	INS_SECONDARY,	I_FPMULBY2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_DIS",	INS_SECONDARY,	I_MWS_DIS,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MOVE",		INS_SECONDARY,	I_MOVE,		NULL},
    {"MWS_PPILNK",	INS_SECONDARY,	I_MWS_PPILNK,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_ALTPOSTLOCK",INS_SECONDARY,	I_MWS_ALTPOSTLOCK,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"RESCHEDULE",	INS_SECONDARY,	I_RESCHEDULE,	NULL},
    {"STARTP",		INS_SECONDARY,	I_STARTP,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_PBADJSYNC",	INS_SECONDARY,	I_MWS_PBADJSYNC,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPDIVBY2",	INS_SECONDARY,	I_FPDIVBY2,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW32",		INS_SECONDARY,	I_IOW32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FBARSYNC",	INS_SECONDARY,	I_FBARSYNC,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOR32",		INS_SECONDARY,	I_IOR32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPCHKERR",	INS_SECONDARY,	I_FPCHKERR,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPI32TOR32",	INS_SECONDARY,	I_FPI32TOR32,	NULL},
    {"SETPRI",		INS_SECONDARY,	I_SETPRI,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"DIV",		INS_SECONDARY,	I_DIV,		NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPEXPINC32",	INS_SECONDARY,	I_FPEXPINC32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOW16",		INS_SECONDARY,	I_IOW16,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"IOR16",		INS_SECONDARY,	I_IOR16,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPRTOI32",	INS_SECONDARY,	I_FPRTOI32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_PBRILNK",	INS_SECONDARY,	I_MWS_PBRILNK,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_SYNC",	INS_SECONDARY,	I_MWS_SYNC,	NULL},
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
    {"GETPRI",		INS_SECONDARY,	I_GETPRI,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"MWS_BINIT",	INS_SECONDARY,	I_MWS_BINIT,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"FPCHKI32",	INS_SECONDARY,	I_FPCHKI32,	NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {(char*)0,INS_INVALID,I_INVALID,NULL},
    {"REV",		INS_SECONDARY,	I_REV,		NULL},
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
    {"UDIV",		INS_SECONDARY,	I_UDIV,		NULL}
  };

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
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


