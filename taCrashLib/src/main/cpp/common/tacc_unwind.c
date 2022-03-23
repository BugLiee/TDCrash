//
// Created by bugliee on 2022/1/18.
//

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include <android/log.h>
#include "tacc_unwind.h"
#include "tacc_unwind_libcorkscrew.h"
#include "tacc_unwind_libunwind.h"
#include "tacc_unwind_clang.h"

//根据系统api等级初始化对应的so
void tacc_unwind_init(int api_level)
{
#if defined(__arm__) || defined(__i386__)
    if(api_level >= 16 && api_level <= 20)
    {
        tacc_unwind_libcorkscrew_init();
    }
#endif

    if(api_level >= 21 && api_level <= 23)
    {
        tacc_unwind_libunwind_init();
    }
}

size_t tacc_unwind_get(int api_level, siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len)
{
    size_t buf_used;

#if defined(__arm__) || defined(__i386__)
    if(api_level >= 16 && api_level <= 20)
    {
        if(0 == (buf_used = tacc_unwind_libcorkscrew_record(si, uc, buf, buf_len))) goto bottom;
        return buf_used;
    }
#else
    (void)si;
#endif

    if(api_level >= 21 && api_level <= 23)
    {
        if(0 == (buf_used = tacc_unwind_libunwind_record(uc, buf, buf_len))) goto bottom;
        return buf_used;
    }

    bottom:
    return tacc_unwind_clang_record(uc, buf, buf_len);
}


