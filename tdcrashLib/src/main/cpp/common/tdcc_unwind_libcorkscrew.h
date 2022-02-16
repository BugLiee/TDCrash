//
// Created by bugliee on 2022/1/18.
//

#if defined(__arm__) || defined(__i386__)

#ifndef TDCRASH_TDCC_UNWIND_LIBCORKSCREW_H
#define TDCRASH_TDCC_UNWIND_LIBCORKSCREW_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

void tdcc_unwind_libcorkscrew_init(void);
size_t tdcc_unwind_libcorkscrew_record(siginfo_t *si, ucontext_t *uc, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_UNWIND_LIBCORKSCREW_H

#endif
