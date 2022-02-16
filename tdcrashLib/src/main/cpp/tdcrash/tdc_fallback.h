//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_FALLBACK_H
#define TDCRASH_TDC_FALLBACK_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t tdc_fallback_get_emergency(siginfo_t *si,
                                 ucontext_t *uc,
                                 pid_t tid,
                                 uint64_t crash_time,
                                 char *emergency,
                                 size_t emergency_len);

int tdc_fallback_record(int log_fd,
                       char *emergency,
                       unsigned int logcat_system_lines,
                       unsigned int logcat_events_lines,
                       unsigned int logcat_main_lines,
                       int dump_fds,
                       int dump_network_info);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDC_FALLBACK_H
