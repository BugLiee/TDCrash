//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TAC_ERRNO_H
#define TACRASH_TAC_ERRNO_H 1

#include <errno.h>

#define TAC_ERRNO_UNKNOWN  1001
#define TAC_ERRNO_INVAL    1002
#define TAC_ERRNO_NOMEM    1003
#define TAC_ERRNO_NOSPACE  1004
#define TAC_ERRNO_RANGE    1005
#define TAC_ERRNO_NOTFND   1006
#define TAC_ERRNO_MISSING  1007
#define TAC_ERRNO_MEM      1008
#define TAC_ERRNO_DEV      1009
#define TAC_ERRNO_PERM     1010
#define TAC_ERRNO_FORMAT   1011
#define TAC_ERRNO_ILLEGAL  1012
#define TAC_ERRNO_NOTSPT   1013
#define TAC_ERRNO_STATE    1014
#define TAC_ERRNO_JNI      1015
#define TAC_ERRNO_FD       1016

#define TAC_ERRNO_SYS     ((0 != errno) ? errno : TAC_ERRNO_UNKNOWN) //来自系统的error

#endif //TACRASH_TAC_ERRNO_H
