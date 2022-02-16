//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_UNWIND_LIBUNWIND_H
#define TDCRASH_TDCC_UNWIND_LIBUNWIND_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

void tdcc_unwind_libunwind_init(void);
size_t tdcc_unwind_libunwind_record(ucontext_t *uc, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_UNWIND_LIBUNWIND_H
