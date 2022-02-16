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
#include "../tdcrash/tdc_errno.h"
#include "tdcc_signal.h"
#include "tdcc_unwind.h"
#include "tdcc_util.h"
#include "tdcc_spot.h"
#include "tdcd_log.h"
#include "tdcd_process.h"
#include "tdcd_sys.h"
#include "tdcd_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static int                    tdcd_core_handled      = 0;
static int                    tdcd_core_log_fd       = -1;
static tdcd_process_t         *tdcd_core_proc         = NULL;

static tdcc_spot_t             tdcd_core_spot;
static char                  *tdcd_core_log_pathname      = NULL;
static char                  *tdcd_core_os_version        = NULL;
static char                  *tdcd_core_kernel_version    = NULL;
static char                  *tdcd_core_abi_list          = NULL;
static char                  *tdcd_core_manufacturer      = NULL;
static char                  *tdcd_core_brand             = NULL;
static char                  *tdcd_core_model             = NULL;
static char                  *tdcd_core_build_fingerprint = NULL;
static char                  *tdcd_core_app_id            = NULL;
static char                  *tdcd_core_app_version       = NULL;
static char                  *tdcd_core_dump_all_threads_whitelist = NULL;

static int tdcd_core_read_stdin(void *buf, size_t len)
{
    size_t  nread = 0;
    ssize_t n;

    while(len - nread > 0)
    {
        n = TDCC_UTIL_TEMP_FAILURE_RETRY(read(STDIN_FILENO, (void *)((uint8_t *)buf + nread), len - nread));
        if(n <= 0) return TDC_ERRNO_SYS;
        nread += (size_t)n;
    }
    
    return 0;
}

static int tdcd_core_read_stdin_extra(char **buf, size_t len)
{
    if(0 == len) return TDC_ERRNO_INVAL;
    if(NULL == ((*buf) = (char *)calloc(1, len + 1))) return TDC_ERRNO_NOMEM;
    return tdcd_core_read_stdin((void *)(*buf), len);
}

static int tdcd_core_read_args()
{
    int r;
    
    if(0 != (r = tdcd_core_read_stdin((void *)&tdcd_core_spot, sizeof(tdcc_spot_t)))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_log_pathname, tdcd_core_spot.log_pathname_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_os_version, tdcd_core_spot.os_version_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_kernel_version, tdcd_core_spot.kernel_version_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_abi_list, tdcd_core_spot.abi_list_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_manufacturer, tdcd_core_spot.manufacturer_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_brand, tdcd_core_spot.brand_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_model, tdcd_core_spot.model_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_build_fingerprint, tdcd_core_spot.build_fingerprint_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_app_id, tdcd_core_spot.app_id_len))) return r;
    if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_app_version, tdcd_core_spot.app_version_len))) return r;
    if(tdcd_core_spot.dump_all_threads_whitelist_len > 0)
        if(0 != (r = tdcd_core_read_stdin_extra(&tdcd_core_dump_all_threads_whitelist, tdcd_core_spot.dump_all_threads_whitelist_len))) return r;
    
    return 0;
}

static void tdcd_core_signal_handler(int sig, siginfo_t *si, void *uc)
{
    char buf[2048] = "\0";
    size_t len;

    (void)sig;

    //only once
    if(tdcd_core_handled) _exit(200);
    tdcd_core_handled = 1;

    //restore the signal handler
    if(0 != tdcc_signal_crash_unregister()) _exit(10);

    if(tdcd_core_log_fd >= 0)
    {
        //dump signal, code, backtrace
        if(0 != tdcc_util_write_format_safe(tdcd_core_log_fd,
                                           "\n\n"
                                           "tdcrash error debug:\n"
                                           "dumper has crashed (signal: %d, code: %d)\n",
                                           si->si_signo, si->si_code)) goto end;
        if(0 < (len = tdcc_unwind_get(tdcd_core_spot.api_level, si, uc, buf, sizeof(buf))))
            tdcc_util_write(tdcd_core_log_fd, buf, len);

    end:
        tdcc_util_write_str(tdcd_core_log_fd, "\n\n");
    }

    tdcc_signal_crash_queue(si);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    //don't leave a zombie process
    alarm(30);

    //read args from stdin
    if(0 != tdcd_core_read_args()) exit(1);

    //open log file
    if(0 > (tdcd_core_log_fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(tdcd_core_log_pathname, O_WRONLY | O_CLOEXEC)))) exit(2);

    //register signal handler for catching self-crashing
    tdcc_unwind_init(tdcd_core_spot.api_level);
    tdcc_signal_crash_register(tdcd_core_signal_handler);

    //create process object
    if(0 != tdcd_process_create(&tdcd_core_proc,
                               tdcd_core_spot.crash_pid,
                               tdcd_core_spot.crash_tid,
                               &(tdcd_core_spot.siginfo),
                               &(tdcd_core_spot.ucontext))) exit(3);

    //suspend all threads in the process
    tdcd_process_suspend_threads(tdcd_core_proc);

    //load process info
    if(0 != tdcd_process_load_info(tdcd_core_proc)) exit(4);

    //record system info
    if(0 != tdcd_sys_record(tdcd_core_log_fd,
                           tdcd_core_spot.time_zone,
                           tdcd_core_spot.start_time,
                           tdcd_core_spot.crash_time,
                           tdcd_core_app_id,
                           tdcd_core_app_version,
                           tdcd_core_spot.api_level,
                           tdcd_core_os_version,
                           tdcd_core_kernel_version,
                           tdcd_core_abi_list,
                           tdcd_core_manufacturer,
                           tdcd_core_brand,
                           tdcd_core_model,
                           tdcd_core_build_fingerprint)) exit(5);

    //record process info
    if(0 != tdcd_process_record(tdcd_core_proc,
                               tdcd_core_log_fd,
                               tdcd_core_spot.logcat_system_lines,
                               tdcd_core_spot.logcat_events_lines,
                               tdcd_core_spot.logcat_main_lines,
                               tdcd_core_spot.dump_elf_hash,
                               tdcd_core_spot.dump_map,
                               tdcd_core_spot.dump_fds,
                               tdcd_core_spot.dump_network_info,
                               tdcd_core_spot.dump_all_threads,
                               tdcd_core_spot.dump_all_threads_count_max,
                               tdcd_core_dump_all_threads_whitelist,
                               tdcd_core_spot.api_level)) exit(6);

    //resume all threads in the process
    tdcd_process_resume_threads(tdcd_core_proc);

#if TDCD_CORE_DEBUG
    TDCD_LOG_DEBUG("CORE: done");
#endif
    return 0;
}

#pragma clang diagnostic pop
