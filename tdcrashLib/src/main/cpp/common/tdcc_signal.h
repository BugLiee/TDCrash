//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_SIGNAL_H
#define TDCRASH_TDCC_SIGNAL_H 1

#include <stdint.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

int tdcc_signal_crash_register(void (*handler)(int, siginfo_t *, void *));
int tdcc_signal_crash_unregister(void);
int tdcc_signal_crash_ignore(void);
int tdcc_signal_crash_queue(siginfo_t* si);

int tdcc_signal_trace_register(void (*handler)(int, siginfo_t *, void *));
void tdcc_signal_trace_unregister(void);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_SIGNAL_H
