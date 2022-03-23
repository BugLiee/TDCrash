//
// Created by bugliee on 2022/1/18.
//

#ifndef TACRASH_TACC_SIGNAL_H
#define TACRASH_TACC_SIGNAL_H 1

#include <stdint.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

int tacc_signal_crash_register(void (*handler)(int, siginfo_t *, void *));
int tacc_signal_crash_unregister(void);
int tacc_signal_crash_ignore(void);
int tacc_signal_crash_queue(siginfo_t* si);

int tacc_signal_trace_register(void (*handler)(int, siginfo_t *, void *));
void tacc_signal_trace_unregister(void);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TACC_SIGNAL_H
