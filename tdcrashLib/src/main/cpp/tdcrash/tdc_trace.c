//
// Created by bugliee on 2022/1/11.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <dirent.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <android/log.h>
#include "tdc_errno.h"
#include "tdcc_util.h"
#include "tdcc_signal.h"
#include "tdcc_meminfo.h"
#include "tdcc_version.h"
#include "tdc_trace.h"
#include "tdc_common.h"
#include "tdc_dl.h"
#include "tdc_jni.h"
#include "tdc_util.h"
#include "../common/tdcc_util.h"
#include "tdcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define TDC_TRACE_CALLBACK_METHOD_NAME         "traceCallback"
#define TDC_TRACE_CALLBACK_METHOD_SIGNATURE    "(Ljava/lang/String;Ljava/lang/String;)V"

#define TDC_TRACE_SIGNAL_CATCHER_TID_UNLOAD    (-2)
#define TDC_TRACE_SIGNAL_CATCHER_TID_UNKNOWN   (-1)
#define TDC_TRACE_SIGNAL_CATCHER_THREAD_NAME   "Signal Catcher"
#define TDC_TRACE_SIGNAL_CATCHER_THREAD_SIGBLK 0x1000

static int                              tdc_trace_is_lollipop = 0;
static pid_t                            tdc_trace_signal_catcher_tid = TDC_TRACE_SIGNAL_CATCHER_TID_UNLOAD;

//symbol address in libc++.so and libart.so
static void                            *tdc_trace_libcpp_cerr = NULL;
static void                           **tdc_trace_libart_runtime_instance = NULL;
static tdcc_util_libart_runtime_dump_t   tdc_trace_libart_runtime_dump = NULL;
static tdcc_util_libart_dbg_suspend_t    tdc_trace_libart_dbg_suspend = NULL;
static tdcc_util_libart_dbg_resume_t     tdc_trace_libart_dbg_resume = NULL;
static int                              tdc_trace_symbols_loaded = 0;
static int                              tdc_trace_symbols_status = TDC_ERRNO_NOTFND;

//init parameters
static int                              tdc_trace_rethrow;
static unsigned int                     tdc_trace_logcat_system_lines;
static unsigned int                     tdc_trace_logcat_events_lines;
static unsigned int                     tdc_trace_logcat_main_lines;
static int                              tdc_trace_dump_fds;
static int                              tdc_trace_dump_network_info;

//callback
static jmethodID                        tdc_trace_cb_method = NULL;
static int                              tdc_trace_notifier = -1;

tdc_trace_dump_status_t                  tdc_trace_dump_status = TDC_TRACE_DUMP_NOT_START;
sigjmp_buf                              jmpenv;

static void tdc_trace_load_signal_catcher_tid()
{
    char           buf[256];
    DIR           *dir;
    struct dirent *ent;
    FILE          *f;
    pid_t          tid;
    uint64_t       sigblk;

    tdc_trace_signal_catcher_tid = TDC_TRACE_SIGNAL_CATCHER_TID_UNKNOWN;

    snprintf(buf, sizeof(buf), "/proc/%d/task", tdc_common_process_id);
    if(NULL == (dir = opendir(buf))) return;
    while(NULL != (ent = readdir(dir)))
    {
        //get and check thread id
        if(0 != tdcc_util_atoi(ent->d_name, &tid)) continue;
        if(tid < 0) continue;

        //check thread name
        tdcc_util_get_thread_name(tid, buf, sizeof(buf));
        if(0 != strcmp(buf, TDC_TRACE_SIGNAL_CATCHER_THREAD_NAME)) continue;

        //check signal block masks
        sigblk = 0;
        snprintf(buf, sizeof(buf), "/proc/%d/status", tid);
        if(NULL == (f = fopen(buf, "r"))) break;
        while(fgets(buf, sizeof(buf), f))
        {
            if(1 == sscanf(buf, "SigBlk: %"SCNx64, &sigblk)) break;
        }
        fclose(f);
        if(TDC_TRACE_SIGNAL_CATCHER_THREAD_SIGBLK != sigblk) continue;

        //found it
        tdc_trace_signal_catcher_tid = tid;
        break;
    }
    closedir(dir);
}

static void tdc_trace_send_sigquit()
{
    if(TDC_TRACE_SIGNAL_CATCHER_TID_UNLOAD == tdc_trace_signal_catcher_tid)
        tdc_trace_load_signal_catcher_tid();

    if(tdc_trace_signal_catcher_tid >= 0)
        syscall(SYS_tgkill, tdc_common_process_id, tdc_trace_signal_catcher_tid, SIGQUIT);
}

static int tdc_trace_load_symbols()
{
    tdc_dl_t *libcpp = NULL;
    tdc_dl_t *libart = NULL;

    //only once
    if(tdc_trace_symbols_loaded) return tdc_trace_symbols_status;
    tdc_trace_symbols_loaded = 1;

    if(tdc_common_api_level >= 29) libcpp = tdc_dl_create(TDCC_UTIL_LIBCPP_APEX);
    if(NULL == libcpp && NULL == (libcpp = tdc_dl_create(TDCC_UTIL_LIBCPP))) goto end;
    if(NULL == (tdc_trace_libcpp_cerr = tdc_dl_sym(libcpp, TDCC_UTIL_LIBCPP_CERR))) goto end;

    if(tdc_common_api_level >= 30)
        libart = tdc_dl_create(TDCC_UTIL_LIBART_APEX_30);
    else if(tdc_common_api_level == 29)
        libart = tdc_dl_create(TDCC_UTIL_LIBART_APEX_29);
    if(NULL == libart && NULL == (libart = tdc_dl_create(TDCC_UTIL_LIBART))) goto end;
    if(NULL == (tdc_trace_libart_runtime_instance = (void **)tdc_dl_sym(libart, TDCC_UTIL_LIBART_RUNTIME_INSTANCE))) goto end;
    if(NULL == (tdc_trace_libart_runtime_dump = (tdcc_util_libart_runtime_dump_t)tdc_dl_sym(libart, TDCC_UTIL_LIBART_RUNTIME_DUMP))) goto end;
    if(tdc_trace_is_lollipop)
    {
        if(NULL == (tdc_trace_libart_dbg_suspend = (tdcc_util_libart_dbg_suspend_t)tdc_dl_sym(libart, TDCC_UTIL_LIBART_DBG_SUSPEND))) goto end;
        if(NULL == (tdc_trace_libart_dbg_resume = (tdcc_util_libart_dbg_resume_t)tdc_dl_sym(libart, TDCC_UTIL_LIBART_DBG_RESUME))) goto end;
    }

    //OK
    tdc_trace_symbols_status = 0;

    end:
    if(NULL != libcpp) tdc_dl_destroy(&libcpp);
    if(NULL != libart) tdc_dl_destroy(&libart);
    return tdc_trace_symbols_status;
}

//Not reliable! But try our best to avoid crashes.
static int tdc_trace_check_address_valid()
{
    FILE      *f = NULL;
    char       line[512];
    uintptr_t  start, end;
    int        r_cerr = TDC_ERRNO_INVAL;
    int        r_runtime_instance = TDC_ERRNO_INVAL;
    int        r_runtime_dump = TDC_ERRNO_INVAL;
    int        r_dbg_suspend = TDC_ERRNO_INVAL;
    int        r_dbg_resume = TDC_ERRNO_INVAL;
    int        r = TDC_ERRNO_INVAL;

    if(NULL == (f = fopen("/proc/self/maps", "r"))) return TDC_ERRNO_SYS;
    while(fgets(line, sizeof(line), f))
    {
        if(2 != sscanf(line, "%"SCNxPTR"-%"SCNxPTR" r", &start, &end)) continue;

        if(0 != r_cerr && (uintptr_t)tdc_trace_libcpp_cerr >= start && (uintptr_t)tdc_trace_libcpp_cerr < end)
            r_cerr = 0;
        if(0 != r_runtime_instance && (uintptr_t)tdc_trace_libart_runtime_instance >= start && (uintptr_t)tdc_trace_libart_runtime_instance < end)
            r_runtime_instance = 0;
        if(0 != r_runtime_dump && (uintptr_t)tdc_trace_libart_runtime_dump >= start && (uintptr_t)tdc_trace_libart_runtime_dump < end)
            r_runtime_dump = 0;
        if(tdc_trace_is_lollipop)
        {
            if(0 != r_dbg_suspend && (uintptr_t)tdc_trace_libart_dbg_suspend >= start && (uintptr_t)tdc_trace_libart_dbg_suspend < end)
                r_dbg_suspend = 0;
            if(0 != r_dbg_resume && (uintptr_t)tdc_trace_libart_dbg_resume >= start && (uintptr_t)tdc_trace_libart_dbg_resume < end)
                r_dbg_resume = 0;
        }

        if(0 == r_cerr && 0 == r_runtime_instance && 0 == r_runtime_dump &&
           (!tdc_trace_is_lollipop || (0 == r_dbg_suspend && 0 == r_dbg_resume)))
        {
            r = 0;
            break;
        }
    }
    if(0 != r) goto end;
    if(tdc_common_api_level >= 30) goto end;

    r = TDC_ERRNO_INVAL;
    rewind(f);
    while(fgets(line, sizeof(line), f))
    {
        if(2 != sscanf(line, "%"SCNxPTR"-%"SCNxPTR" r", &start, &end)) continue;

        //The next line of code will cause segmentation fault, sometimes.
        if((uintptr_t)(*tdc_trace_libart_runtime_instance) >= start && (uintptr_t)(*tdc_trace_libart_runtime_instance) < end)
        {
            r = 0;
            break;
        }
    }
    end:
    fclose(f);
    return r;
}

static int tdc_trace_logs_filter(const struct dirent *entry)
{
    size_t len;

    if(DT_REG != entry->d_type) return 0;

    len = strlen(entry->d_name);
    if(len < TDC_COMMON_LOG_NAME_MIN_TRACE) return 0;

    if(0 != memcmp(entry->d_name, TDC_COMMON_LOG_PREFIX"_", TDC_COMMON_LOG_PREFIX_LEN + 1)) return 0;
    if(0 != memcmp(entry->d_name + (len - TDC_COMMON_LOG_SUFFIX_TRACE_LEN), TDC_COMMON_LOG_SUFFIX_TRACE, TDC_COMMON_LOG_SUFFIX_TRACE_LEN)) return 0;

    return 1;
}

static int tdc_trace_logs_clean(void)
{
    struct dirent **entry_list;
    char            pathname[1024];
    int             n, i, r = 0;

    if(0 > (n = scandir(tdc_common_log_dir, &entry_list, tdc_trace_logs_filter, alphasort))) return TDC_ERRNO_SYS;
    for(i = 0; i < n; i++)
    {
        snprintf(pathname, sizeof(pathname), "%s/%s", tdc_common_log_dir, entry_list[i]->d_name);
        if(0 != unlink(pathname)) r = TDC_ERRNO_SYS;
    }
    free(entry_list);
    return r;
}

static int tdc_trace_write_header(int fd, uint64_t trace_time)
{
    int  r;
    char buf[1024];

    tdcc_util_get_dump_header(buf, sizeof(buf),
                             TDCC_UTIL_CRASH_TYPE_ANR,
                             tdc_common_time_zone,
                             tdc_common_start_time,
                             trace_time,
                             tdc_common_app_id,
                             tdc_common_app_version,
                             tdc_common_api_level,
                             tdc_common_os_version,
                             tdc_common_kernel_version,
                             tdc_common_abi_list,
                             tdc_common_manufacturer,
                             tdc_common_brand,
                             tdc_common_model,
                             tdc_common_build_fingerprint);
    if(0 != (r = tdcc_util_write_str(fd, buf))) return r;

    return tdcc_util_write_format(fd, "pid: %d  >>> %s <<<\n\n", tdc_common_process_id, tdc_common_process_name);
}

static void *tdc_trace_dumper(void *arg)
{
    JNIEnv         *env = NULL;
    uint64_t        data;
    uint64_t        trace_time;
    int             fd;
    struct timeval  tv;
    char            pathname[1024];
    jstring         j_pathname;

    (void)arg;

    pthread_detach(pthread_self());

    JavaVMAttachArgs attach_args = {
            .version = TDC_JNI_VERSION,
            .name    = "tdcrash_trace_dp",
            .group   = NULL
    };
    if(JNI_OK != (*tdc_common_vm)->AttachCurrentThread(tdc_common_vm, &env, &attach_args)) goto exit;

    while(1)
    {
        //block here, waiting for sigquit
        TDCC_UTIL_TEMP_FAILURE_RETRY(read(tdc_trace_notifier, &data, sizeof(data)));

        //check if process already crashed
        if(tdc_common_native_crashed || tdc_common_java_crashed) break;

        //trace time
        if(0 != gettimeofday(&tv, NULL)) break;
        trace_time = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)tv.tv_usec / 1000;

        //Keep only one current trace.
        if(0 != tdc_trace_logs_clean()) continue;

        //create and open log file
        if((fd = tdc_common_open_trace_log(pathname, sizeof(pathname), trace_time)) < 0) continue;

        //write header info
        if(0 != tdc_trace_write_header(fd, trace_time)) goto end;

        //write trace info from ART runtime
        if(0 != tdcc_util_write_format(fd, TDCC_UTIL_THREAD_SEP"Cmd line: %s\n", tdc_common_process_name)) goto end;
        if(0 != tdcc_util_write_str(fd, "Mode: ART DumpForSigQuit\n")) goto end;
        if(0 != tdc_trace_load_symbols())
        {
            if(0 != tdcc_util_write_str(fd, "Failed to load symbols.\n")) goto end;
            goto skip;
        }
        if(0 != tdc_trace_check_address_valid())
        {
            if(0 != tdcc_util_write_str(fd, "Failed to check runtime address.\n")) goto end;
            goto skip;
        }
        if(dup2(fd, STDERR_FILENO) < 0)
        {
            if(0 != tdcc_util_write_str(fd, "Failed to duplicate FD.\n")) goto end;
            goto skip;
        }

        tdc_trace_dump_status = TDC_TRACE_DUMP_ON_GOING;
        if(sigsetjmp(jmpenv, 1) == 0)
        {
            if(tdc_trace_is_lollipop)
                tdc_trace_libart_dbg_suspend();
            tdc_trace_libart_runtime_dump(*tdc_trace_libart_runtime_instance, tdc_trace_libcpp_cerr);
            if(tdc_trace_is_lollipop)
                tdc_trace_libart_dbg_resume();
        }
        else
        {
            fflush(NULL);
            TDCD_LOG_WARN("longjmp to skip dumping trace\n");
        }

        dup2(tdc_common_fd_null, STDERR_FILENO);

        skip:
        if(0 != tdcc_util_write_str(fd, "\n"TDCC_UTIL_THREAD_END"\n")) goto end;

        //write other info
        if(0 != tdcc_util_record_logcat(fd, tdc_common_process_id, tdc_common_api_level, tdc_trace_logcat_system_lines, tdc_trace_logcat_events_lines, tdc_trace_logcat_main_lines)) goto end;
        if(tdc_trace_dump_fds)
            if(0 != tdcc_util_record_fds(fd, tdc_common_process_id)) goto end;
        if(tdc_trace_dump_network_info)
            if(0 != tdcc_util_record_network_info(fd, tdc_common_process_id, tdc_common_api_level)) goto end;
        if(0 != tdcc_meminfo_record(fd, tdc_common_process_id)) goto end;

        end:
        //close log file
        tdc_common_close_trace_log(fd);

        //rethrow SIGQUIT to ART Signal Catcher
        if(tdc_trace_rethrow && (TDC_TRACE_DUMP_ART_CRASH != tdc_trace_dump_status)) tdc_trace_send_sigquit();
        tdc_trace_dump_status = TDC_TRACE_DUMP_END;

        //JNI callback
        //Do we need to implement an emergency buffer for disk exhausted?
        if(NULL == tdc_trace_cb_method) continue;
        if(NULL == (j_pathname = (*env)->NewStringUTF(env, pathname))) continue;
        (*env)->CallStaticVoidMethod(env, tdc_common_cb_class, tdc_trace_cb_method, j_pathname, NULL);
        TDC_JNI_IGNORE_PENDING_EXCEPTION();
        (*env)->DeleteLocalRef(env, j_pathname);
    }

    (*tdc_common_vm)->DetachCurrentThread(tdc_common_vm);

    exit:
    tdc_trace_notifier = -1;
    close(tdc_trace_notifier);
    return NULL;
}

static void tdc_trace_handler(int sig, siginfo_t *si, void *uc)
{
    uint64_t data;

    (void)sig;
    (void)si;
    (void)uc;

    if(tdc_trace_notifier >= 0)
    {
        data = 1;
        TDCC_UTIL_TEMP_FAILURE_RETRY(write(tdc_trace_notifier, &data, sizeof(data)));
    }
}

static void tdc_trace_init_callback(JNIEnv *env)
{
    if(NULL == tdc_common_cb_class) return;

    tdc_trace_cb_method = (*env)->GetStaticMethodID(env, tdc_common_cb_class, TDC_TRACE_CALLBACK_METHOD_NAME, TDC_TRACE_CALLBACK_METHOD_SIGNATURE);
    TDC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(tdc_trace_cb_method, err);
    return;

    err:
    tdc_trace_cb_method = NULL;
}

int tdc_trace_init(JNIEnv *env,
                  int rethrow,
                  unsigned int logcat_system_lines,
                  unsigned int logcat_events_lines,
                  unsigned int logcat_main_lines,
                  int dump_fds,
                  int dump_network_info)
{
    int r;
    pthread_t thd;

    //capture SIGQUIT only for ART
    if(tdc_common_api_level < 21) return 0;

    //is Android Lollipop (5.x)?
    tdc_trace_is_lollipop = ((21 == tdc_common_api_level || 22 == tdc_common_api_level) ? 1 : 0);

    tdc_trace_dump_status = TDC_TRACE_DUMP_NOT_START;
    tdc_trace_rethrow = rethrow;
    tdc_trace_logcat_system_lines = logcat_system_lines;
    tdc_trace_logcat_events_lines = logcat_events_lines;
    tdc_trace_logcat_main_lines = logcat_main_lines;
    tdc_trace_dump_fds = dump_fds;
    tdc_trace_dump_network_info = dump_network_info;

    //init for JNI callback
    tdc_trace_init_callback(env);

    //create event FD
    if(0 > (tdc_trace_notifier = eventfd(0, EFD_CLOEXEC))) return TDC_ERRNO_SYS;

    //register signal handler
    if(0 != (r = tdcc_signal_trace_register(tdc_trace_handler))) goto err2;

    //create thread for dump trace
    if(0 != (r = pthread_create(&thd, NULL, tdc_trace_dumper, NULL))) goto err1;

    return 0;

    err1:
    tdcc_signal_trace_unregister();
    err2:
    close(tdc_trace_notifier);
    tdc_trace_notifier = -1;

    return r;
}

#pragma clang diagnostic pop

