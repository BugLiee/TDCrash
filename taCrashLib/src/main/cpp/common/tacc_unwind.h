//
// Created by bugliee on 2022/1/18.
//

#ifndef TACRASH_TACC_UNWIND_H
#define TACRASH_TACC_UNWIND_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

void tacc_unwind_init(int api_level);

size_t tacc_unwind_get(int api_level, siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TACC_UNWIND_H
