//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ucontext.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include <android/log.h>
#include "queue.h"
#include "../tacrash/tac_errno.h"
#include "tacc_signal.h"
#include "tacc_unwind.h"
#include "tacc_util.h"
#include "tacc_spot.h"
#include "tacd_log.h"
#include "tacd_process.h"
#include "tacd_sys.h"
#include "tacd_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static int                    tacd_core_handled      = 0;
static int                    tacd_core_log_fd       = -1;
static tacd_process_t         *tacd_core_proc         = NULL;

static tacc_spot_t             tacd_core_spot;
static char                  *tacd_core_log_pathname      = NULL;
static char                  *tacd_core_os_version        = NULL;
static char                  *tacd_core_kernel_version    = NULL;
static char                  *tacd_core_abi_list          = NULL;
static char                  *tacd_core_manufacturer      = NULL;
static char                  *tacd_core_brand             = NULL;
static char                  *tacd_core_model             = NULL;
static char                  *tacd_core_build_fingerprint = NULL;
static char                  *tacd_core_app_id            = NULL;
static char                  *tacd_core_app_version       = NULL;

static int tacd_core_read_stdin(void *buf, size_t len)
{
    size_t  nread = 0;
    ssize_t n;

    while(len - nread > 0)
    {
        n = TACC_UTIL_TEMP_FAILURE_RETRY(read(STDIN_FILENO, (void *)((uint8_t *)buf + nread), len - nread));
        if(n <= 0) return TAC_ERRNO_SYS;
        nread += (size_t)n;
    }
    
    return 0;
}

static int tacd_core_read_stdin_extra(char **buf, size_t len)
{
    if(0 == len) return TAC_ERRNO_INVAL;
    if(NULL == ((*buf) = (char *)calloc(1, len + 1))) return TAC_ERRNO_NOMEM;
    return tacd_core_read_stdin((void *)(*buf), len);
}

static int tacd_core_read_args()
{
    int r;
    
    if(0 != (r = tacd_core_read_stdin((void *)&tacd_core_spot, sizeof(tacc_spot_t)))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_log_pathname, tacd_core_spot.log_pathname_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_os_version, tacd_core_spot.os_version_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_kernel_version, tacd_core_spot.kernel_version_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_abi_list, tacd_core_spot.abi_list_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_manufacturer, tacd_core_spot.manufacturer_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_brand, tacd_core_spot.brand_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_model, tacd_core_spot.model_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_build_fingerprint, tacd_core_spot.build_fingerprint_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_app_id, tacd_core_spot.app_id_len))) return r;
    if(0 != (r = tacd_core_read_stdin_extra(&tacd_core_app_version, tacd_core_spot.app_version_len))) return r;

    return 0;
}

static void tacd_core_signal_handler(int sig, siginfo_t *si, void *uc)
{
    char buf[2048] = "\0";
    size_t len;

    (void)sig;

    //only once
    if(tacd_core_handled) _exit(200);
    tacd_core_handled = 1;

    //restore the signal handler
    if(0 != tacc_signal_crash_unregister()) _exit(10);

    if(tacd_core_log_fd >= 0)
    {
        //dump signal, code, backtrace
        if(0 != tacc_util_write_format_safe(tacd_core_log_fd,
                                           "\n\n"
                                           "tacrash error debug:\n"
                                           "dumper has crashed (signal: %d, code: %d)\n",
                                           si->si_signo, si->si_code)) goto end;
        if(0 < (len = tacc_unwind_get(tacd_core_spot.api_level, si, uc, buf, sizeof(buf))))
            tacc_util_write(tacd_core_log_fd, buf, len);

    end:
        tacc_util_write_str(tacd_core_log_fd, "\n\n");
    }

    tacc_signal_crash_queue(si);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    //don't leave a zombie process
    alarm(30);

    //read args from stdin
    if(0 != tacd_core_read_args()) exit(1);

    //open log file
    if(0 > (tacd_core_log_fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(tacd_core_log_pathname, O_WRONLY | O_CLOEXEC)))) exit(2);

    //register signal handler for catching self-crashing
    tacc_unwind_init(tacd_core_spot.api_level);
    tacc_signal_crash_register(tacd_core_signal_handler);

    //create process object
    if(0 != tacd_process_create(&tacd_core_proc,
                               tacd_core_spot.crash_pid,
                               tacd_core_spot.crash_tid,
                               &(tacd_core_spot.siginfo),
                               &(tacd_core_spot.ucontext))) exit(3);

    //suspend all threads in the process
    tacd_process_suspend_threads(tacd_core_proc);

    //load process info
    if(0 != tacd_process_load_info(tacd_core_proc)) exit(4);

    //record system info
    if(0 != tacd_sys_record(tacd_core_log_fd,
                           tacd_core_spot.time_zone,
                           tacd_core_spot.start_time,
                           tacd_core_spot.crash_time,
                           tacd_core_app_id,
                           tacd_core_app_version,
                           tacd_core_spot.api_level,
                           tacd_core_os_version,
                           tacd_core_kernel_version,
                           tacd_core_abi_list,
                           tacd_core_manufacturer,
                           tacd_core_brand,
                           tacd_core_model,
                           tacd_core_build_fingerprint)) exit(5);

    //record process info
    if (0 != tacd_process_record(tacd_core_proc, tacd_core_log_fd, tacd_core_spot.api_level))
        exit(6);

    //resume all threads in the process
    tacd_process_resume_threads(tacd_core_proc);

#if TACD_CORE_DEBUG
    TACD_LOG_DEBUG("CORE: done");
#endif
    return 0;
}

#pragma clang diagnostic pop
