//
// Created by bugliee on 2022/1/18.
//

#ifndef TACRASH_TACC_UNWIND_CLANG_H
#define TACRASH_TACC_UNWIND_CLANG_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t tacc_unwind_clang_record(ucontext_t *uc, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif


#endif //TACRASH_TACC_UNWIND_CLANG_H
