//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TAC_FALLBACK_H
#define TACRASH_TAC_FALLBACK_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t tac_fallback_get_emergency(siginfo_t *si,
                                 ucontext_t *uc,
                                 pid_t tid,
                                 uint64_t crash_time,
                                 char *emergency,
                                 size_t emergency_len);

int tac_fallback_record(int log_fd, char *emergency);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TAC_FALLBACK_H
