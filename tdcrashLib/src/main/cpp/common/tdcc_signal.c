//
// Created by bugliee on 2022/1/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <sys/syscall.h>
#include <android/log.h>
#include "tdcc_signal.h"
#include "../tdcrash/tdc_errno.h"
#include "tdcc_libc_support.h"

#define TDCC_SIGNAL_CRASH_STACK_SIZE (1024 * 128)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct
{
    int              signum;
    struct sigaction oldact;
} tdcc_signal_crash_info_t;
#pragma clang diagnostic pop

//信号类型，数组
static tdcc_signal_crash_info_t tdcc_signal_crash_info[] =
        {
                {.signum = SIGABRT},
                {.signum = SIGBUS},
                {.signum = SIGFPE},
                {.signum = SIGILL},
                {.signum = SIGSEGV},
                {.signum = SIGTRAP},
                {.signum = SIGSYS},
                {.signum = SIGSTKFLT}
        };

int tdcc_signal_crash_register(void (*handler)(int, siginfo_t *, void *))
{
    stack_t ss;
    if(NULL == (ss.ss_sp = calloc(1, TDCC_SIGNAL_CRASH_STACK_SIZE))) return TDC_ERRNO_NOMEM;
    ss.ss_size  = TDCC_SIGNAL_CRASH_STACK_SIZE;
    ss.ss_flags = 0;
    if(0 != sigaltstack(&ss, NULL)) return TDC_ERRNO_SYS;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_sigaction = handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    size_t i;
    for(i = 0; i < sizeof(tdcc_signal_crash_info) / sizeof(tdcc_signal_crash_info[0]); i++)
        if(0 != sigaction(tdcc_signal_crash_info[i].signum, &act, &(tdcc_signal_crash_info[i].oldact)))
            return TDC_ERRNO_SYS;

    return 0;
}

int tdcc_signal_crash_unregister(void)
{
    int r = 0;
    size_t i;
    for(i = 0; i < sizeof(tdcc_signal_crash_info) / sizeof(tdcc_signal_crash_info[0]); i++)
        if(0 != sigaction(tdcc_signal_crash_info[i].signum, &(tdcc_signal_crash_info[i].oldact), NULL))
            r = TDC_ERRNO_SYS;

    return r;
}

int tdcc_signal_crash_ignore(void)
{
    struct sigaction act;
    tdcc_libc_support_memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_DFL;
    act.sa_flags = SA_RESTART;

    int r = 0;
    size_t i;
    for(i = 0; i < sizeof(tdcc_signal_crash_info) / sizeof(tdcc_signal_crash_info[0]); i++)
        if(0 != sigaction(tdcc_signal_crash_info[i].signum, &act, NULL))
            r = TDC_ERRNO_SYS;

    return r;
}

int tdcc_signal_crash_queue(siginfo_t* si)
{
    if(SIGABRT == si->si_signo || SI_FROMUSER(si))
    {
        if(0 != syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), si->si_signo, si))
            return TDC_ERRNO_SYS;
    }

    return 0;
}

static sigset_t         tdcc_signal_trace_oldset;
static struct sigaction tdcc_signal_trace_oldact;

int tdcc_signal_trace_register(void (*handler)(int, siginfo_t *, void *))
{
    int              r;
    sigset_t         set;
    struct sigaction act;

    //un-block the SIGQUIT mask for current thread, hope this is the main thread
    //解除当前进程主线程对信号SIGQUIT的屏蔽
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    if(0 != (r = pthread_sigmask(SIG_UNBLOCK, &set, &tdcc_signal_trace_oldset))) return r;

    //register new signal handler for SIGQUIT
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_sigaction = handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    if(0 != sigaction(SIGQUIT, &act, &tdcc_signal_trace_oldact))
    {
        pthread_sigmask(SIG_SETMASK, &tdcc_signal_trace_oldset, NULL);
        return TDC_ERRNO_SYS;
    }

    return 0;
}

void tdcc_signal_trace_unregister(void)
{
    pthread_sigmask(SIG_SETMASK, &tdcc_signal_trace_oldset, NULL);
    sigaction(SIGQUIT, &tdcc_signal_trace_oldact, NULL);
}
