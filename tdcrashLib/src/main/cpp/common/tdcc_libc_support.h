//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_LIBC_SUPPORT_H
#define TDCRASH_TDCC_LIBC_SUPPORT_H 1

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// memset(3) is not async-signal-safe, ref: http://boston.conman.org/2016/12/17.1
void *tdcc_libc_support_memset(void *s, int c, size_t n);

// you need to pass timezone through gmtoff
struct tm *tdcc_libc_support_localtime_r(const time_t *timev, long gmtoff, struct tm *result);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_LIBC_SUPPORT_H
