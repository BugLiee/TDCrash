//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_ERRNO_H
#define TDCRASH_TDC_ERRNO_H 1

#include <errno.h>

#define TDC_ERRNO_UNKNOWN  1001
#define TDC_ERRNO_INVAL    1002
#define TDC_ERRNO_NOMEM    1003
#define TDC_ERRNO_NOSPACE  1004
#define TDC_ERRNO_RANGE    1005
#define TDC_ERRNO_NOTFND   1006
#define TDC_ERRNO_MISSING  1007
#define TDC_ERRNO_MEM      1008
#define TDC_ERRNO_DEV      1009
#define TDC_ERRNO_PERM     1010
#define TDC_ERRNO_FORMAT   1011
#define TDC_ERRNO_ILLEGAL  1012
#define TDC_ERRNO_NOTSPT   1013
#define TDC_ERRNO_STATE    1014
#define TDC_ERRNO_JNI      1015
#define TDC_ERRNO_FD       1016

#define TDC_ERRNO_SYS     ((0 != errno) ? errno : TDC_ERRNO_UNKNOWN) //来自系统的error

#endif //TDCRASH_TDC_ERRNO_H
