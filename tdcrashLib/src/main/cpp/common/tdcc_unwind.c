//
// Created by bugliee on 2022/1/18.
//

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include <android/log.h>
#include "tdcc_unwind.h"
#include "tdcc_unwind_libcorkscrew.h"
#include "tdcc_unwind_libunwind.h"
#include "tdcc_unwind_clang.h"

//根据系统api等级初始化对应的so
void tdcc_unwind_init(int api_level)
{
#if defined(__arm__) || defined(__i386__)
    if(api_level >= 16 && api_level <= 20)
    {
        tdcc_unwind_libcorkscrew_init();
    }
#endif

    if(api_level >= 21 && api_level <= 23)
    {
        tdcc_unwind_libunwind_init();
    }
}

size_t tdcc_unwind_get(int api_level, siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len)
{
    size_t buf_used;

#if defined(__arm__) || defined(__i386__)
    if(api_level >= 16 && api_level <= 20)
    {
        if(0 == (buf_used = tdcc_unwind_libcorkscrew_record(si, uc, buf, buf_len))) goto bottom;
        return buf_used;
    }
#else
    (void)si;
#endif

    if(api_level >= 21 && api_level <= 23)
    {
        if(0 == (buf_used = tdcc_unwind_libunwind_record(uc, buf, buf_len))) goto bottom;
        return buf_used;
    }

    bottom:
    return tdcc_unwind_clang_record(uc, buf, buf_len);
}


