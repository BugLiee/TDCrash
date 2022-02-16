//
// Created by bugliee on 2022/1/11.
//

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#define _GNU_SOURCE
#pragma clang diagnostic pop

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <android/log.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_spot.h"
#include "tdcc_util.h"
#include "tdcc_unwind.h"
#include "tdcc_signal.h"
#include "tdcc_b64.h"
#include "../common/tdcc_util.h"
#include "../common/tdcc_signal.h"
#include "tdc_crash.h"
#include "tdc_trace.h"
#include "tdc_common.h"
#include "tdc_dl.h"
#include "tdc_util.h"
#include "tdc_jni.h"
#include "tdc_fallback.h"
#include "tdcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define TDC_CRASH_CALLBACK_METHOD_NAME      "crashCallback"
#define TDC_CRASH_CALLBACK_METHOD_SIGNATURE "(Ljava/lang/String;Ljava/lang/String;ZZLjava/lang/String;)V"
#define TDC_CRASH_EMERGENCY_BUF_LEN         (30 * 1024)
#define TDC_CRASH_ERR_TITLE                 "\n\ntdcrash error:\n"

static pthread_mutex_t  tdc_crash_mutex   = PTHREAD_MUTEX_INITIALIZER;
static int              tdc_crash_rethrow;
static char            *tdc_crash_dumper_pathname;
static char            *tdc_crash_emergency;

//the log file
static int              tdc_crash_prepared_fd = -1;
static int              tdc_crash_log_fd  = -1;
static int              tdc_crash_log_from_placeholder;
static char             tdc_crash_log_pathname[1024] = "\0";

//the crash
static pid_t            tdc_crash_tid = 0;
static int              tdc_crash_dump_java_stacktrace = 0; //try to dump java stacktrace in java layer
static uint64_t         tdc_crash_time = 0;

//callback
static jmethodID        tdc_crash_cb_method = NULL;
static pthread_t        tdc_crash_cb_thd;
static int              tdc_crash_cb_notifier = -1;

//for clone and fork
#ifndef __i386__
#define TDC_CRASH_CHILD_STACK_LEN (16 * 1024)
static void            *tdc_crash_child_stack;
#else
static int              tdc_crash_child_notifier[2];
#endif

//info passed to the dumper process
static tdcc_spot_t       tdc_crash_spot;
static char            *tdc_crash_dump_all_threads_whitelist = NULL;

static int tdc_crash_fork(int (*fn)(void *))
{
#ifndef __i386__
    return clone(fn, tdc_crash_child_stack, CLONE_VFORK | CLONE_FS | CLONE_UNTRACED, NULL);
#else
    pid_t dumper_pid = fork();
    if(-1 == dumper_pid)
    {
        return -1;
    }
    else if(0 == dumper_pid)
    {
        //child process ...
        char msg = 'a';
        TDCC_UTIL_TEMP_FAILURE_RETRY(write(tdc_crash_child_notifier[1], &msg, sizeof(char)));
        syscall(SYS_close, tdc_crash_child_notifier[0]);
        syscall(SYS_close, tdc_crash_child_notifier[1]);

        _exit(fn(NULL));
    }
    else
    {
        //parent process ...
        char msg;
        TDCC_UTIL_TEMP_FAILURE_RETRY(read(tdc_crash_child_notifier[0], &msg, sizeof(char)));
        syscall(SYS_close, tdc_crash_child_notifier[0]);
        syscall(SYS_close, tdc_crash_child_notifier[1]);

        return dumper_pid;
    }
#endif
}

static int tdc_crash_exec_dumper(void *arg)
{
    (void)arg;

    //for fd exhaust
    //keep the log_fd open for writing error msg before execl()
    int i;
    for(i = 0; i < 1024; i++)
        if(i != tdc_crash_log_fd)
            syscall(SYS_close, i);

    //hold the fd 0, 1, 2
    errno = 0;
    int devnull = TDCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if(devnull < 0)
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"open /dev/null failed, errno=%d\n\n", errno);
        return 90;
    }
    else if(0 != devnull)
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"/dev/null fd NOT 0, errno=%d\n\n", errno);
        return 91;
    }
    TDCC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDOUT_FILENO));
    TDCC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDERR_FILENO));

    //create args pipe
    int pipefd[2];
    errno = 0;
    if(0 != pipe2(pipefd, O_CLOEXEC))
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"create args pipe failed, errno=%d\n\n", errno);
        return 92;
    }

    //set args pipe size
    //range: pagesize (4K) ~ /proc/sys/fs/pipe-max-size (1024K)
    int write_len = (int)(sizeof(tdcc_spot_t) +
                          tdc_crash_spot.log_pathname_len +
                          tdc_crash_spot.os_version_len +
                          tdc_crash_spot.kernel_version_len +
                          tdc_crash_spot.abi_list_len +
                          tdc_crash_spot.manufacturer_len +
                          tdc_crash_spot.brand_len +
                          tdc_crash_spot.model_len +
                          tdc_crash_spot.build_fingerprint_len +
                          tdc_crash_spot.app_id_len +
                          tdc_crash_spot.app_version_len +
                          tdc_crash_spot.dump_all_threads_whitelist_len);
    errno = 0;
    if(fcntl(pipefd[1], F_SETPIPE_SZ, write_len) < write_len)
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"set args pipe size failed, errno=%d\n\n", errno);
        return 93;
    }

    //write args to pipe
    struct iovec iovs[12] = {
            {.iov_base = &tdc_crash_spot,                      .iov_len = sizeof(tdcc_spot_t)},
            {.iov_base = tdc_crash_log_pathname,               .iov_len = tdc_crash_spot.log_pathname_len},
            {.iov_base = tdc_common_os_version,                .iov_len = tdc_crash_spot.os_version_len},
            {.iov_base = tdc_common_kernel_version,            .iov_len = tdc_crash_spot.kernel_version_len},
            {.iov_base = tdc_common_abi_list,                  .iov_len = tdc_crash_spot.abi_list_len},
            {.iov_base = tdc_common_manufacturer,              .iov_len = tdc_crash_spot.manufacturer_len},
            {.iov_base = tdc_common_brand,                     .iov_len = tdc_crash_spot.brand_len},
            {.iov_base = tdc_common_model,                     .iov_len = tdc_crash_spot.model_len},
            {.iov_base = tdc_common_build_fingerprint,         .iov_len = tdc_crash_spot.build_fingerprint_len},
            {.iov_base = tdc_common_app_id,                    .iov_len = tdc_crash_spot.app_id_len},
            {.iov_base = tdc_common_app_version,               .iov_len = tdc_crash_spot.app_version_len},
            {.iov_base = tdc_crash_dump_all_threads_whitelist, .iov_len = tdc_crash_spot.dump_all_threads_whitelist_len}
    };
    int iovs_cnt = (0 == tdc_crash_spot.dump_all_threads_whitelist_len ? 11 : 12);
    errno = 0;
    ssize_t ret = TDCC_UTIL_TEMP_FAILURE_RETRY(writev(pipefd[1], iovs, iovs_cnt));
    if((ssize_t)write_len != ret)
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"write args to pipe failed, return=%d, errno=%d\n\n", ret, errno);
        return 94;
    }

    //copy the read-side of the args-pipe to stdin (fd: 0)
    TDCC_UTIL_TEMP_FAILURE_RETRY(dup2(pipefd[0], STDIN_FILENO));

    syscall(SYS_close, pipefd[0]);
    syscall(SYS_close, pipefd[1]);

    //escape to the dumper process
    errno = 0;
    execl(tdc_crash_dumper_pathname, TDCRASH_DUMPER_FILENAME, NULL);
    return 100 + errno;
}

static void tdc_tdcrash_record_java_stacktrace()
{
    JNIEnv                           *env     = NULL;
    tdc_dl_t                          *libcpp  = NULL;
    tdc_dl_t                          *libart  = NULL;
    tdcc_util_libart_thread_current_t  current = NULL;
    tdcc_util_libart_thread_dump_t     dump    = NULL;
    tdcc_util_libart_thread_dump2_t    dump2   = NULL;
    void                             *cerr    = NULL;
    void                             *thread  = NULL;

    //is this a java thread?
    if(JNI_OK == (*tdc_common_vm)->GetEnv(tdc_common_vm, (void**)&env, TDC_JNI_VERSION))
        TDC_JNI_CHECK_PENDING_EXCEPTION(end);
    else
        return;
    //yes, this is a java thread
    tdc_crash_dump_java_stacktrace = 1;

    //in Dalvik, get java stacktrace on the java layer
    if(tdc_common_api_level < 21) return;

    //peek libc++.so
    if(tdc_common_api_level >= 29) libcpp = tdc_dl_create(TDCC_UTIL_LIBCPP_APEX);

    if(NULL == libcpp && NULL == (libcpp = tdc_dl_create(TDCC_UTIL_LIBCPP))) goto end;
    if(NULL == (cerr = tdc_dl_sym(libcpp, TDCC_UTIL_LIBCPP_CERR))) goto end;

    //peek libart.so
    if(tdc_common_api_level >= 30)
        libart = tdc_dl_create(TDCC_UTIL_LIBART_APEX_30);
    else if(tdc_common_api_level == 29)
        libart = tdc_dl_create(TDCC_UTIL_LIBART_APEX_29);
    if(NULL == libart && NULL == (libart = tdc_dl_create(TDCC_UTIL_LIBART))) goto end;
    if(NULL == (current = (tdcc_util_libart_thread_current_t)tdc_dl_sym(libart, TDCC_UTIL_LIBART_THREAD_CURRENT))) goto end;
    if(NULL == (dump = (tdcc_util_libart_thread_dump_t)tdc_dl_sym(libart, TDCC_UTIL_LIBART_THREAD_DUMP)))
    {
#ifndef __i386__
        if(NULL == (dump2 = (tdcc_util_libart_thread_dump2_t)tdc_dl_sym(libart, TDCC_UTIL_LIBART_THREAD_DUMP2))) goto end;
#else
        goto end;
#endif
    }

    //get current thread object
    if(NULL == (thread = current())) goto end;

    //everything seems OK, do not dump java stacktrace again on the java layer
    tdc_crash_dump_java_stacktrace = 0;

    //dump java stacktrace
    if(0 != tdcc_util_write_str(tdc_crash_log_fd, "\n\njava stacktrace:\n")) goto end;
    if(dup2(tdc_crash_log_fd, STDERR_FILENO) < 0) goto end;
    if(NULL != dump)
        dump(thread, cerr);
    else if(NULL != dump2)
        dump2(thread, cerr, 0, 0);
    dup2(tdc_common_fd_null, STDERR_FILENO);
    tdcc_util_write_str(tdc_crash_log_fd, "\n");

    end:
    if(NULL != libcpp) tdc_dl_destroy(&libcpp);
    if(NULL != libart) tdc_dl_destroy(&libart);
}

static void *tdc_crash_callback_thread(void *arg)
{
    JNIEnv   *env = NULL;
    uint64_t  data = 0;
    jstring   j_pathname  = NULL;
    jstring   j_emergency = NULL;
    jboolean  j_dump_java_stacktrace = JNI_FALSE;
    jboolean  j_is_main_thread = JNI_FALSE;
    jstring   j_thread_name = NULL;
    char      c_thread_name[16] = "\0";

    (void)arg;

    JavaVMAttachArgs attach_args = {
            .version = TDC_JNI_VERSION,
            .name    = "tdcrash_crash_cb",
            .group   = NULL
    };
    if(JNI_OK != (*tdc_common_vm)->AttachCurrentThread(tdc_common_vm, &env, &attach_args)) return NULL;

    //block until native crashed
    if(sizeof(data) != TDCC_UTIL_TEMP_FAILURE_RETRY(read(tdc_crash_cb_notifier, &data, sizeof(data)))) goto end;

    //prepare callback parameters
    if(NULL == (j_pathname = (*env)->NewStringUTF(env, tdc_crash_log_pathname))) goto end;
    if('\0' != tdc_crash_emergency[0])
    {
        if(NULL == (j_emergency = (*env)->NewStringUTF(env, tdc_crash_emergency))) goto end;
    }
    j_dump_java_stacktrace = (tdc_crash_dump_java_stacktrace ? JNI_TRUE : JNI_FALSE);
    if(j_dump_java_stacktrace)
    {
        j_is_main_thread = (tdc_common_process_id == tdc_crash_tid ? JNI_TRUE : JNI_FALSE);
        if(!j_is_main_thread)
        {
            tdcc_util_get_thread_name(tdc_crash_tid, c_thread_name, sizeof(c_thread_name));
            if(NULL == (j_thread_name = (*env)->NewStringUTF(env, c_thread_name))) goto end;
        }
    }

    //do callback
    (*env)->CallStaticVoidMethod(env, tdc_common_cb_class, tdc_crash_cb_method, j_pathname, j_emergency,
                                 j_dump_java_stacktrace, j_is_main_thread, j_thread_name);
    TDC_JNI_IGNORE_PENDING_EXCEPTION();

    end:
    (*tdc_common_vm)->DetachCurrentThread(tdc_common_vm);
    return NULL;
}

static void tdc_crash_callback()
{
    uint64_t data;

    if(tdc_crash_cb_notifier < 0 || NULL == tdc_common_cb_class || NULL == tdc_crash_cb_method) return;

    //wake up the callback thread
    data = 1;
    if(sizeof(data) != TDCC_UTIL_TEMP_FAILURE_RETRY(write(tdc_crash_cb_notifier, &data, sizeof(data)))) return;

    pthread_join(tdc_crash_cb_thd, NULL);
}

static int tdc_crash_check_backtrace_valid()
{
    int    fd;
    char   line[512];
    size_t i = 0;
    int    r = 0;

    if((fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(tdc_crash_log_pathname, O_RDONLY | O_CLOEXEC))) < 0)
    {
        if(tdc_crash_prepared_fd >= 0)
        {
            close(tdc_crash_prepared_fd);
            tdc_crash_prepared_fd = -1;
        }
        if((fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(tdc_crash_log_pathname, O_RDONLY | O_CLOEXEC))) < 0)
            return 0; //failed
    }

    while(NULL != tdcc_util_gets(line, sizeof(line), fd))
    {
        if(0 == memcmp(line, "backtrace:\n", 11))
        {
            //check the next line
            if(NULL != tdcc_util_gets(line, sizeof(line), fd) && 0 == memcmp(line, "    #00 pc ", 11))
                r = 1; //we found the backtrace
            break;
        }
        if(i++ > 200) //check the top 200 lines at most
            break;
    }

    if(fd >= 0) close(fd);
    return r;
}

static void tdc_crash_signal_handler(int sig, siginfo_t *si, void *uc)
{
    struct timespec crash_tp;
    int             restore_orig_ptracer = 0;
    int             restore_orig_dumpable = 0;
    int             orig_dumpable = 0;
    int             dump_ok = 0;

    (void)sig;

    pthread_mutex_lock(&tdc_crash_mutex);

    //only once
    if(tdc_common_native_crashed) goto exit;
    tdc_common_native_crashed = 1;

    //restore the original/default signal handler
    if(tdc_crash_rethrow)
    {
        if(0 != tdcc_signal_crash_unregister()) goto exit;
    }
    else
    {
        if(0 != tdcc_signal_crash_ignore()) goto exit;
    }

    if(TDC_TRACE_DUMP_ON_GOING == tdc_trace_dump_status)
    {
        tdc_trace_dump_status = TDC_TRACE_DUMP_ART_CRASH;
        TDCD_LOG_WARN("meet error sig(%d) while calling ART dump trace\n", sig);
        siglongjmp(jmpenv, 1);
    }

    //save crash time
    clock_gettime(CLOCK_REALTIME, &crash_tp);
    //tv_nsec 纳秒
    tdc_crash_time = (uint64_t)(crash_tp.tv_sec) * 1000 + (uint64_t)crash_tp.tv_nsec / 1000000;

    //save crashed thread ID
    tdc_crash_tid = gettid();

    //create and open log file
    if((tdc_crash_log_fd = tdc_common_open_crash_log(tdc_crash_log_pathname, sizeof(tdc_crash_log_pathname), &tdc_crash_log_from_placeholder, tdc_crash_time)) < 0) goto end;

    //check privilege-restricting mode
    //https://www.kernel.org/doc/Documentation/prctl/no_new_privs.txt
    //errno = 0;
    //if(1 == prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0))
    //{
    //    tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"get NO_NEW_PRIVS failed, errno=%d\n\n", errno);
    //    goto end;
    //}

    //set dumpable
    orig_dumpable = prctl(PR_GET_DUMPABLE);
    errno = 0;
    if(0 != prctl(PR_SET_DUMPABLE, 1))
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"set dumpable failed, errno=%d\n\n", errno);
        goto end;
    }
    restore_orig_dumpable = 1;

    //set traceable (disable the ptrace restrictions introduced by Yama)
    //https://www.kernel.org/doc/Documentation/security/Yama.txt
    errno = 0;
    if(0 != prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY))
    {
        if(EINVAL == errno)
        {
            //this kernel does not support PR_SET_PTRACER_ANY, or Yama is not enabled
            //errno = errno;
            (void)errno;
        }
        else
        {
            tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"set traceable failed, errno=%d\n\n", errno);
            goto end;
        }
    }
    else
    {
        restore_orig_ptracer = 1;
    }

    //set crash spot info
    tdc_crash_spot.crash_time = tdc_crash_time;
    tdc_crash_spot.crash_tid = tdc_crash_tid;
    memcpy(&(tdc_crash_spot.siginfo), si, sizeof(siginfo_t));
    memcpy(&(tdc_crash_spot.ucontext), uc, sizeof(ucontext_t));
    tdc_crash_spot.log_pathname_len = strlen(tdc_crash_log_pathname);

    //spawn crash dumper process
    errno = 0;
    pid_t dumper_pid = tdc_crash_fork(tdc_crash_exec_dumper);
    if(-1 == dumper_pid)
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"fork failed, errno=%d\n\n", errno);
        goto end;
    }

    //parent process ...

    //wait the crash dumper process terminated
    errno = 0;
    int status = 0;
    int wait_r = TDCC_UTIL_TEMP_FAILURE_RETRY(waitpid(dumper_pid, &status, __WALL));

    //the crash dumper process should have written a lot of logs,
    //so we need to seek to the end of log file
    if(tdc_crash_log_from_placeholder)
    {
        if((tdc_crash_log_fd = tdc_common_seek_to_content_end(tdc_crash_log_fd)) < 0) goto end;
    }

    if(-1 == wait_r)
    {
        tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"waitpid failed, errno=%d\n\n", errno);
        goto end;
    }

    //check child process state
    if(!(WIFEXITED(status)) || 0 != WEXITSTATUS(status))
    {
        if(WIFEXITED(status) && 0 != WEXITSTATUS(status))
        {
            //terminated normally, but return / exit / _exit NON-zero
            tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"child terminated normally with non-zero exit status(%d), dumper=%s\n\n", WEXITSTATUS(status), tdc_crash_dumper_pathname);
            goto end;
        }
        else if(WIFSIGNALED(status))
        {
            //terminated by a signal
            tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"child terminated by a signal(%d)\n\n", WTERMSIG(status));
            goto end;
        }
        else
        {
            tdcc_util_write_format_safe(tdc_crash_log_fd, TDC_CRASH_ERR_TITLE"child terminated with other error status(%d), dumper=%s\n\n", status, tdc_crash_dumper_pathname);
            goto end;
        }
    }

    //check the backtrace
    if(!tdc_crash_check_backtrace_valid()) goto end;

    dump_ok = 1;
    end:
    //restore dumpable
    if(restore_orig_dumpable) prctl(PR_SET_DUMPABLE, orig_dumpable);

    //restore traceable
    if(restore_orig_ptracer) prctl(PR_SET_PTRACER, 0);

    //fallback
    if(!dump_ok)
    {
        tdc_fallback_get_emergency(si,
                                  (ucontext_t *)uc,
                                  tdc_crash_tid,
                                  tdc_crash_time,
                                  tdc_crash_emergency,
                                  TDC_CRASH_EMERGENCY_BUF_LEN);

        if(tdc_crash_log_fd >= 0)
        {
            if(0 != tdc_fallback_record(tdc_crash_log_fd,
                                       tdc_crash_emergency,
                                       tdc_crash_spot.logcat_system_lines,
                                       tdc_crash_spot.logcat_events_lines,
                                       tdc_crash_spot.logcat_main_lines,
                                       tdc_crash_spot.dump_fds,
                                       tdc_crash_spot.dump_network_info))
            {
                close(tdc_crash_log_fd);
                tdc_crash_log_fd = -1;
            }
        }
    }

    if(tdc_crash_log_fd >= 0)
    {
        //record java stacktrace
        tdc_tdcrash_record_java_stacktrace();

        //we have written all the required information in the native layer, close the FD
        close(tdc_crash_log_fd);
        tdc_crash_log_fd = -1;
    }
    //JNI callback
    tdc_crash_callback();

    if(0 != tdcc_signal_crash_queue(si)) goto exit;

    pthread_mutex_unlock(&tdc_crash_mutex);
    return;

    exit:
    pthread_mutex_unlock(&tdc_crash_mutex);
    _exit(1);
}

static void tdc_crash_init_dump_all_threads_whitelist(const char **whitelist, size_t whitelist_len)
{
    size_t  i, len;
    size_t  encoded_len, total_encoded_len = 0, cur_encoded_len = 0;
    char   *total_encoded_whitelist, *tmp;

    if(NULL == whitelist || 0 == whitelist_len) return;

    //get total encoded length
    for(i = 0; i < whitelist_len; i++)
    {
        if(NULL == whitelist[i]) continue;
        len = strlen(whitelist[i]);
        if(0 == len) continue;
        total_encoded_len += tdcc_b64_encode_max_len(len);
    }
    if(0 == total_encoded_len) return;
    total_encoded_len += whitelist_len; //separator ('|')
    total_encoded_len += 1; //terminating null byte ('\0')

    //alloc encode buffer
    if(NULL == (total_encoded_whitelist = calloc(1, total_encoded_len))) return;

    //to base64 encode each whitelist item
    for(i = 0; i < whitelist_len; i++)
    {
        if(NULL == whitelist[i]) continue;
        len = strlen(whitelist[i]);
        if(0 == len) continue;

        if(NULL != (tmp = tdcc_b64_encode((const uint8_t *)(whitelist[i]), len, &encoded_len)))
        {
            if(cur_encoded_len + encoded_len + 1 >= total_encoded_len) return; //impossible

            memcpy(total_encoded_whitelist + cur_encoded_len, tmp, encoded_len);
            cur_encoded_len += encoded_len;

            memcpy(total_encoded_whitelist + cur_encoded_len, "|", 1);
            cur_encoded_len += 1;

            free(tmp);
        }
    }

    if(cur_encoded_len > 0 && '|' == total_encoded_whitelist[cur_encoded_len - 1])
    {
        total_encoded_whitelist[cur_encoded_len - 1] = '\0';
        cur_encoded_len -= 1;
    }

    if(0 == cur_encoded_len)
    {
        free(total_encoded_whitelist);
        return;
    }

    tdc_crash_spot.dump_all_threads_whitelist_len = cur_encoded_len;
    tdc_crash_dump_all_threads_whitelist = total_encoded_whitelist;
}

static void tdc_crash_init_callback(JNIEnv *env)
{
    if(NULL == tdc_common_cb_class) return;

    tdc_crash_cb_method = (*env)->GetStaticMethodID(env, tdc_common_cb_class, TDC_CRASH_CALLBACK_METHOD_NAME, TDC_CRASH_CALLBACK_METHOD_SIGNATURE);
    TDC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(tdc_crash_cb_method, err);

    //eventfd and a new thread for callback
    if(0 > (tdc_crash_cb_notifier = eventfd(0, EFD_CLOEXEC))) goto err;
    if(0 != pthread_create(&tdc_crash_cb_thd, NULL, tdc_crash_callback_thread, NULL)) goto err;
    return;

    err:
    tdc_crash_cb_method = NULL;
    if(tdc_crash_cb_notifier >= 0)
    {
        close(tdc_crash_cb_notifier);
        tdc_crash_cb_notifier = -1;
    }
}

int tdc_crash_init(JNIEnv *env,
                  int rethrow,
                  unsigned int logcat_system_lines,
                  unsigned int logcat_events_lines,
                  unsigned int logcat_main_lines,
                  int dump_elf_hash,
                  int dump_map,
                  int dump_fds,
                  int dump_network_info,
                  int dump_all_threads,
                  unsigned int dump_all_threads_count_max,
                  const char **dump_all_threads_whitelist,
                  size_t dump_all_threads_whitelist_len)
{
    tdc_crash_prepared_fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    tdc_crash_rethrow = rethrow;
    if(NULL == (tdc_crash_emergency = calloc(TDC_CRASH_EMERGENCY_BUF_LEN, 1))) return TDC_ERRNO_NOMEM;
    //so文件路径
    if(NULL == (tdc_crash_dumper_pathname = tdc_util_strdupcat(tdc_common_app_lib_dir, "/"TDCRASH_DUMPER_FILENAME))) return TDC_ERRNO_NOMEM;

    //init the local unwinder for fallback mode
    //初始化unwind
    tdcc_unwind_init(tdc_common_api_level);

    //init for JNI callback
    //初始化回调
    tdc_crash_init_callback(env);

    //struct info passed to the dumper process
    //使用0填充tdcc_spot_t的size个位数到tdc_crash_spot
    memset(&tdc_crash_spot, 0, sizeof(tdcc_spot_t));
    //赋值
    tdc_crash_spot.api_level = tdc_common_api_level;
    tdc_crash_spot.crash_pid = tdc_common_process_id;
    tdc_crash_spot.start_time = tdc_common_start_time;
    tdc_crash_spot.time_zone = tdc_common_time_zone;
    tdc_crash_spot.logcat_system_lines = logcat_system_lines;
    tdc_crash_spot.logcat_events_lines = logcat_events_lines;
    tdc_crash_spot.logcat_main_lines = logcat_main_lines;
    tdc_crash_spot.dump_elf_hash = dump_elf_hash;
    tdc_crash_spot.dump_map = dump_map;
    tdc_crash_spot.dump_fds = dump_fds;
    tdc_crash_spot.dump_network_info = dump_network_info;
    tdc_crash_spot.dump_all_threads = dump_all_threads;
    tdc_crash_spot.dump_all_threads_count_max = dump_all_threads_count_max;
    tdc_crash_spot.os_version_len = strlen(tdc_common_os_version);
    tdc_crash_spot.kernel_version_len = strlen(tdc_common_kernel_version);
    tdc_crash_spot.abi_list_len = strlen(tdc_common_abi_list);
    tdc_crash_spot.manufacturer_len = strlen(tdc_common_manufacturer);
    tdc_crash_spot.brand_len = strlen(tdc_common_brand);
    tdc_crash_spot.model_len = strlen(tdc_common_model);
    tdc_crash_spot.build_fingerprint_len = strlen(tdc_common_build_fingerprint);
    tdc_crash_spot.app_id_len = strlen(tdc_common_app_id);
    tdc_crash_spot.app_version_len = strlen(tdc_common_app_version);
    tdc_crash_init_dump_all_threads_whitelist(dump_all_threads_whitelist, dump_all_threads_whitelist_len);

    //for clone and fork
#ifndef __i386__
    if(NULL == (tdc_crash_child_stack = calloc(TDC_CRASH_CHILD_STACK_LEN, 1))) return TDC_ERRNO_NOMEM;
    tdc_crash_child_stack = (void *)(((uint8_t *)tdc_crash_child_stack) + TDC_CRASH_CHILD_STACK_LEN);
#else
    if(0 != pipe2(tdc_crash_child_notifier, O_CLOEXEC)) return TDC_ERRNO_SYS;
#endif

    //register signal handler
    //注册信号处理器
    return tdcc_signal_crash_register(tdc_crash_signal_handler);
}

#pragma clang diagnostic pop
