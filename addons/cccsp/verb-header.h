/* BEGIN verb-header.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <cif.h>

#define MReleaseChk(Wptr,P) do { if ((P) != NULL) { MRelease (Wptr, P); P = NULL; } } while (0)

/* END verb-header.h */
