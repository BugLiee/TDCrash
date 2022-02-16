//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_UNWIND_H
#define TDCRASH_TDCC_UNWIND_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

void tdcc_unwind_init(int api_level);

size_t tdcc_unwind_get(int api_level, siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_UNWIND_H
