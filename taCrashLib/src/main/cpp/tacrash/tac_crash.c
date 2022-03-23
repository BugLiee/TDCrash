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
#include "../tacrash/tac_errno.h"
#include "tacc_spot.h"
#include "tacc_util.h"
#include "tacc_unwind.h"
#include "tacc_signal.h"
#include "tacc_b64.h"
#include "../common/tacc_util.h"
#include "../common/tacc_signal.h"
#include "tac_crash.h"
#include "tac_trace.h"
#include "tac_common.h"
#include "tac_dl.h"
#include "tac_util.h"
#include "tac_jni.h"
#include "tac_fallback.h"
#include "tacd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define TAC_CRASH_CALLBACK_METHOD_NAME      "crashCallback"
#define TAC_CRASH_CALLBACK_METHOD_SIGNATURE "(Ljava/lang/String;Ljava/lang/String;ZZLjava/lang/String;)V"
#define TAC_CRASH_EMERGENCY_BUF_LEN         1024
#define TAC_CRASH_ERR_TITLE                 "\n\ntacrash error:\n"

static pthread_mutex_t  tac_crash_mutex   = PTHREAD_MUTEX_INITIALIZER;
static int              tac_crash_rethrow;
static char            *tac_crash_dumper_pathname;
static char            *tac_crash_emergency;

//the log file
static int              tac_crash_prepared_fd = -1;
static int              tac_crash_log_fd  = -1;
static int              tac_crash_log_from_placeholder;
static char             tac_crash_log_pathname[1024] = "\0";

//the crash
static pid_t            tac_crash_tid = 0;
static int              tac_crash_dump_java_stacktrace = 0; //try to dump java stacktrace in java layer
static uint64_t         tac_crash_time = 0;

//callback
static jmethodID        tac_crash_cb_method = NULL;
static pthread_t        tac_crash_cb_thd;
static int              tac_crash_cb_notifier = -1;

//for clone and fork
#ifndef __i386__
#define TAC_CRASH_CHILD_STACK_LEN (16 * 1024)
static void            *tac_crash_child_stack;
#else
static int              tac_crash_child_notifier[2];
#endif

//info passed to the dumper process
static tacc_spot_t       tac_crash_spot;

static int tac_crash_fork(int (*fn)(void *))
{
#ifndef __i386__
    return clone(fn, tac_crash_child_stack, CLONE_VFORK | CLONE_FS | CLONE_UNTRACED, NULL);
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
        TACC_UTIL_TEMP_FAILURE_RETRY(write(tac_crash_child_notifier[1], &msg, sizeof(char)));
        syscall(SYS_close, tac_crash_child_notifier[0]);
        syscall(SYS_close, tac_crash_child_notifier[1]);

        _exit(fn(NULL));
    }
    else
    {
        //parent process ...
        char msg;
        TACC_UTIL_TEMP_FAILURE_RETRY(read(tac_crash_child_notifier[0], &msg, sizeof(char)));
        syscall(SYS_close, tac_crash_child_notifier[0]);
        syscall(SYS_close, tac_crash_child_notifier[1]);

        return dumper_pid;
    }
#endif
}

static int tac_crash_exec_dumper(void *arg)
{
    (void)arg;

    //for fd exhaust
    //keep the log_fd open for writing error msg before execl()
    int i;
    for(i = 0; i < 1024; i++)
        if(i != tac_crash_log_fd)
            syscall(SYS_close, i);

    //hold the fd 0, 1, 2
    errno = 0;
    int devnull = TACC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if(devnull < 0)
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"open /dev/null failed, errno=%d\n\n", errno);
        return 90;
    }
    else if(0 != devnull)
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"/dev/null fd NOT 0, errno=%d\n\n", errno);
        return 91;
    }
    TACC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDOUT_FILENO));
    TACC_UTIL_TEMP_FAILURE_RETRY(dup2(devnull, STDERR_FILENO));

    //create args pipe
    int pipefd[2];
    errno = 0;
    if(0 != pipe2(pipefd, O_CLOEXEC))
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"create args pipe failed, errno=%d\n\n", errno);
        return 92;
    }

    //set args pipe size
    //range: pagesize (4K) ~ /proc/sys/fs/pipe-max-size (1024K)
    int write_len = (int)(sizeof(tacc_spot_t) +
                          tac_crash_spot.log_pathname_len +
                          tac_crash_spot.os_version_len +
                          tac_crash_spot.kernel_version_len +
                          tac_crash_spot.abi_list_len +
                          tac_crash_spot.manufacturer_len +
                          tac_crash_spot.brand_len +
                          tac_crash_spot.model_len +
                          tac_crash_spot.build_fingerprint_len +
                          tac_crash_spot.app_id_len +
                          tac_crash_spot.app_version_len);
    errno = 0;
    if(fcntl(pipefd[1], F_SETPIPE_SZ, write_len) < write_len)
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"set args pipe size failed, errno=%d\n\n", errno);
        return 93;
    }

    //write args to pipe
    struct iovec iovs[11] = {
            {.iov_base = &tac_crash_spot,                      .iov_len = sizeof(tacc_spot_t)},
            {.iov_base = tac_crash_log_pathname,               .iov_len = tac_crash_spot.log_pathname_len},
            {.iov_base = tac_common_os_version,                .iov_len = tac_crash_spot.os_version_len},
            {.iov_base = tac_common_kernel_version,            .iov_len = tac_crash_spot.kernel_version_len},
            {.iov_base = tac_common_abi_list,                  .iov_len = tac_crash_spot.abi_list_len},
            {.iov_base = tac_common_manufacturer,              .iov_len = tac_crash_spot.manufacturer_len},
            {.iov_base = tac_common_brand,                     .iov_len = tac_crash_spot.brand_len},
            {.iov_base = tac_common_model,                     .iov_len = tac_crash_spot.model_len},
            {.iov_base = tac_common_build_fingerprint,         .iov_len = tac_crash_spot.build_fingerprint_len},
            {.iov_base = tac_common_app_id,                    .iov_len = tac_crash_spot.app_id_len},
            {.iov_base = tac_common_app_version,               .iov_len = tac_crash_spot.app_version_len}
    };
    errno = 0;
    ssize_t ret = TACC_UTIL_TEMP_FAILURE_RETRY(writev(pipefd[1], iovs, 11));
    if((ssize_t)write_len != ret)
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"write args to pipe failed, return=%d, errno=%d\n\n", ret, errno);
        return 94;
    }

    //copy the read-side of the args-pipe to stdin (fd: 0)
    TACC_UTIL_TEMP_FAILURE_RETRY(dup2(pipefd[0], STDIN_FILENO));

    syscall(SYS_close, pipefd[0]);
    syscall(SYS_close, pipefd[1]);

    //escape to the dumper process
    errno = 0;
    execl(tac_crash_dumper_pathname, TACRASH_DUMPER_FILENAME, NULL);
    return 100 + errno;
}

static void tac_tacrash_record_java_stacktrace()
{
    JNIEnv                           *env     = NULL;
    tac_dl_t                          *libcpp  = NULL;
    tac_dl_t                          *libart  = NULL;
    tacc_util_libart_thread_current_t  current = NULL;
    tacc_util_libart_thread_dump_t     dump    = NULL;
    tacc_util_libart_thread_dump2_t    dump2   = NULL;
    void                             *cerr    = NULL;
    void                             *thread  = NULL;

    //is this a java thread?
    if(JNI_OK == (*tac_common_vm)->GetEnv(tac_common_vm, (void**)&env, TAC_JNI_VERSION))
        TAC_JNI_CHECK_PENDING_EXCEPTION(end);
    else
        return;
    //yes, this is a java thread
    tac_crash_dump_java_stacktrace = 1;

    //in Dalvik, get java stacktrace on the java layer
    if(tac_common_api_level < 21) return;

    //peek libc++.so
    if(tac_common_api_level >= 29) libcpp = tac_dl_create(TACC_UTIL_LIBCPP_APEX);

    if(NULL == libcpp && NULL == (libcpp = tac_dl_create(TACC_UTIL_LIBCPP))) goto end;
    if(NULL == (cerr = tac_dl_sym(libcpp, TACC_UTIL_LIBCPP_CERR))) goto end;

    //peek libart.so
    if(tac_common_api_level >= 30)
        libart = tac_dl_create(TACC_UTIL_LIBART_APEX_30);
    else if(tac_common_api_level == 29)
        libart = tac_dl_create(TACC_UTIL_LIBART_APEX_29);
    if(NULL == libart && NULL == (libart = tac_dl_create(TACC_UTIL_LIBART))) goto end;
    if(NULL == (current = (tacc_util_libart_thread_current_t)tac_dl_sym(libart, TACC_UTIL_LIBART_THREAD_CURRENT))) goto end;
    if(NULL == (dump = (tacc_util_libart_thread_dump_t)tac_dl_sym(libart, TACC_UTIL_LIBART_THREAD_DUMP)))
    {
#ifndef __i386__
        if(NULL == (dump2 = (tacc_util_libart_thread_dump2_t)tac_dl_sym(libart, TACC_UTIL_LIBART_THREAD_DUMP2))) goto end;
#else
        goto end;
#endif
    }

    //get current thread object
    if(NULL == (thread = current())) goto end;

    //everything seems OK, do not dump java stacktrace again on the java layer
    tac_crash_dump_java_stacktrace = 0;

    //dump java stacktrace
    if(0 != tacc_util_write_str(tac_crash_log_fd, "\n\njava stacktrace:\n")) goto end;
    if(dup2(tac_crash_log_fd, STDERR_FILENO) < 0) goto end;
    if(NULL != dump)
        dump(thread, cerr);
    else if(NULL != dump2)
        dump2(thread, cerr, 0, 0);
    dup2(tac_common_fd_null, STDERR_FILENO);
    tacc_util_write_str(tac_crash_log_fd, "\n");

    end:
    if(NULL != libcpp) tac_dl_destroy(&libcpp);
    if(NULL != libart) tac_dl_destroy(&libart);
}

static void *tac_crash_callback_thread(void *arg)
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
            .version = TAC_JNI_VERSION,
            .name    = "tacrash_crash_cb",
            .group   = NULL
    };
    if(JNI_OK != (*tac_common_vm)->AttachCurrentThread(tac_common_vm, &env, &attach_args)) return NULL;

    //block until native crashed
    if(sizeof(data) != TACC_UTIL_TEMP_FAILURE_RETRY(read(tac_crash_cb_notifier, &data, sizeof(data)))) goto end;

    //prepare callback parameters
    if(NULL == (j_pathname = (*env)->NewStringUTF(env, tac_crash_log_pathname))) goto end;
    if('\0' != tac_crash_emergency[0])
    {
        if(NULL == (j_emergency = (*env)->NewStringUTF(env, tac_crash_emergency))) goto end;
    }
    j_dump_java_stacktrace = (tac_crash_dump_java_stacktrace ? JNI_TRUE : JNI_FALSE);
    if(j_dump_java_stacktrace)
    {
        j_is_main_thread = (tac_common_process_id == tac_crash_tid ? JNI_TRUE : JNI_FALSE);
        if(!j_is_main_thread)
        {
            tacc_util_get_thread_name(tac_crash_tid, c_thread_name, sizeof(c_thread_name));
            if(NULL == (j_thread_name = (*env)->NewStringUTF(env, c_thread_name))) goto end;
        }
    }

    //do callback
    (*env)->CallStaticVoidMethod(env, tac_common_cb_class, tac_crash_cb_method, j_pathname, j_emergency,
                                 j_dump_java_stacktrace, j_is_main_thread, j_thread_name);
    TAC_JNI_IGNORE_PENDING_EXCEPTION();

    end:
    (*tac_common_vm)->DetachCurrentThread(tac_common_vm);
    return NULL;
}

static void tac_crash_callback()
{
    uint64_t data;

    if(tac_crash_cb_notifier < 0 || NULL == tac_common_cb_class || NULL == tac_crash_cb_method) return;

    //wake up the callback thread
    data = 1;
    if(sizeof(data) != TACC_UTIL_TEMP_FAILURE_RETRY(write(tac_crash_cb_notifier, &data, sizeof(data)))) return;

    pthread_join(tac_crash_cb_thd, NULL);
}

static int tac_crash_check_backtrace_valid()
{
    int    fd;
    char   line[512];
    size_t i = 0;
    int    r = 0;

    if((fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(tac_crash_log_pathname, O_RDONLY | O_CLOEXEC))) < 0)
    {
        if(tac_crash_prepared_fd >= 0)
        {
            close(tac_crash_prepared_fd);
            tac_crash_prepared_fd = -1;
        }
        if((fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(tac_crash_log_pathname, O_RDONLY | O_CLOEXEC))) < 0)
            return 0; //failed
    }

    while(NULL != tacc_util_gets(line, sizeof(line), fd))
    {
        if(0 == memcmp(line, "backtrace:\n", 11))
        {
            //check the next line
            if(NULL != tacc_util_gets(line, sizeof(line), fd) && 0 == memcmp(line, "    #00 pc ", 11))
                r = 1; //we found the backtrace
            break;
        }
        if(i++ > 200) //check the top 200 lines at most
            break;
    }

    if(fd >= 0) close(fd);
    return r;
}

static void tac_crash_signal_handler(int sig, siginfo_t *si, void *uc)
{
    struct timespec crash_tp;
    int             restore_orig_ptracer = 0;
    int             restore_orig_dumpable = 0;
    int             orig_dumpable = 0;
    int             dump_ok = 0;

    (void)sig;

    pthread_mutex_lock(&tac_crash_mutex);

    //only once
    if(tac_common_native_crashed) goto exit;
    tac_common_native_crashed = 1;

    //restore the original/default signal handler
    if(tac_crash_rethrow)
    {
        if(0 != tacc_signal_crash_unregister()) goto exit;
    }
    else
    {
        if(0 != tacc_signal_crash_ignore()) goto exit;
    }

    if(TAC_TRACE_DUMP_ON_GOING == tac_trace_dump_status)
    {
        tac_trace_dump_status = TAC_TRACE_DUMP_ART_CRASH;
        TACD_LOG_WARN("meet error sig(%d) while calling ART dump trace\n", sig);
        siglongjmp(jmpenv, 1);
    }

    //save crash time
    clock_gettime(CLOCK_REALTIME, &crash_tp);
    //tv_nsec 纳秒
    tac_crash_time = (uint64_t)(crash_tp.tv_sec) * 1000 + (uint64_t)crash_tp.tv_nsec / 1000000;

    //save crashed thread ID
    tac_crash_tid = gettid();

    //create and open log file
    if((tac_crash_log_fd = tac_common_open_crash_log(tac_crash_log_pathname, sizeof(tac_crash_log_pathname), &tac_crash_log_from_placeholder, tac_crash_time)) < 0) goto end;

    //set dumpable
    orig_dumpable = prctl(PR_GET_DUMPABLE);
    errno = 0;
    if(0 != prctl(PR_SET_DUMPABLE, 1))
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"set dumpable failed, errno=%d\n\n", errno);
        goto end;
    }
    restore_orig_dumpable = 1;

    errno = 0;
    if(0 != prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY))
    {
        if(EINVAL == errno)
        {
            (void)errno;
        }
        else
        {
            tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"set traceable failed, errno=%d\n\n", errno);
            goto end;
        }
    }
    else
    {
        restore_orig_ptracer = 1;
    }

    //set crash spot info
    tac_crash_spot.crash_time = tac_crash_time;
    tac_crash_spot.crash_tid = tac_crash_tid;
    memcpy(&(tac_crash_spot.siginfo), si, sizeof(siginfo_t));
    memcpy(&(tac_crash_spot.ucontext), uc, sizeof(ucontext_t));
    tac_crash_spot.log_pathname_len = strlen(tac_crash_log_pathname);

    //spawn crash dumper process
    errno = 0;
    pid_t dumper_pid = tac_crash_fork(tac_crash_exec_dumper);
    if(-1 == dumper_pid)
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"fork failed, errno=%d\n\n", errno);
        goto end;
    }

    //parent process ...

    //wait the crash dumper process terminated
    errno = 0;
    int status = 0;
    int wait_r = TACC_UTIL_TEMP_FAILURE_RETRY(waitpid(dumper_pid, &status, __WALL));

    //the crash dumper process should have written a lot of logs,
    //so we need to seek to the end of log file
    if(tac_crash_log_from_placeholder)
    {
        if((tac_crash_log_fd = tac_common_seek_to_content_end(tac_crash_log_fd)) < 0) goto end;
    }

    if(-1 == wait_r)
    {
        tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"waitpid failed, errno=%d\n\n", errno);
        goto end;
    }

    //check child process state
    if(!(WIFEXITED(status)) || 0 != WEXITSTATUS(status))
    {
        if(WIFEXITED(status) && 0 != WEXITSTATUS(status))
        {
            //terminated normally, but return / exit / _exit NON-zero
            tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"child terminated normally with non-zero exit status(%d), dumper=%s\n\n", WEXITSTATUS(status), tac_crash_dumper_pathname);
            goto end;
        }
        else if(WIFSIGNALED(status))
        {
            //terminated by a signal
            tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"child terminated by a signal(%d)\n\n", WTERMSIG(status));
            goto end;
        }
        else
        {
            tacc_util_write_format_safe(tac_crash_log_fd, TAC_CRASH_ERR_TITLE"child terminated with other error status(%d), dumper=%s\n\n", status, tac_crash_dumper_pathname);
            goto end;
        }
    }

    //check the backtrace
    if(!tac_crash_check_backtrace_valid()) goto end;

    dump_ok = 1;
    end:
    //restore dumpable
    if(restore_orig_dumpable) prctl(PR_SET_DUMPABLE, orig_dumpable);

    //restore traceable
    if(restore_orig_ptracer) prctl(PR_SET_PTRACER, 0);

    //fallback
    if(!dump_ok)
    {
        tac_fallback_get_emergency(si,
                                  (ucontext_t *)uc,
                                  tac_crash_tid,
                                  tac_crash_time,
                                  tac_crash_emergency,
                                  TAC_CRASH_EMERGENCY_BUF_LEN);

        if(tac_crash_log_fd >= 0)
        {
            if (0 != tac_fallback_record(tac_crash_log_fd, tac_crash_emergency)) {
                close(tac_crash_log_fd);
                tac_crash_log_fd = -1;
            }
        }
    }

    if(tac_crash_log_fd >= 0)
    {
        //record java stacktrace
        tac_tacrash_record_java_stacktrace();

        //we have written all the required information in the native layer, close the FD
        close(tac_crash_log_fd);
        tac_crash_log_fd = -1;
    }
    //JNI callback
    tac_crash_callback();

    if(0 != tacc_signal_crash_queue(si)) goto exit;

    pthread_mutex_unlock(&tac_crash_mutex);
    return;

    exit:
    pthread_mutex_unlock(&tac_crash_mutex);
    _exit(1);
}

static void tac_crash_init_callback(JNIEnv *env)
{
    if(NULL == tac_common_cb_class) return;

    tac_crash_cb_method = (*env)->GetStaticMethodID(env, tac_common_cb_class, TAC_CRASH_CALLBACK_METHOD_NAME, TAC_CRASH_CALLBACK_METHOD_SIGNATURE);
    TAC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(tac_crash_cb_method, err);

    //eventfd and a new thread for callback
    if(0 > (tac_crash_cb_notifier = eventfd(0, EFD_CLOEXEC))) goto err;
    if(0 != pthread_create(&tac_crash_cb_thd, NULL, tac_crash_callback_thread, NULL)) goto err;
    return;

    err:
    tac_crash_cb_method = NULL;
    if(tac_crash_cb_notifier >= 0)
    {
        close(tac_crash_cb_notifier);
        tac_crash_cb_notifier = -1;
    }
}

int tac_crash_init(JNIEnv *env, int rethrow) {

    tac_crash_prepared_fd = TACC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    tac_crash_rethrow = rethrow;
    if (NULL == (tac_crash_emergency = calloc(TAC_CRASH_EMERGENCY_BUF_LEN, 1)))
        return TAC_ERRNO_NOMEM;
    //so文件路径
    if (NULL == (tac_crash_dumper_pathname = tac_util_strdupcat(tac_common_app_lib_dir,
                                                                "/"TACRASH_DUMPER_FILENAME)))
        return TAC_ERRNO_NOMEM;

    //init the local unwinder for fallback mode
    //初始化unwind
    tacc_unwind_init(tac_common_api_level);

    //init for JNI callback
    //初始化回调
    tac_crash_init_callback(env);

    //struct info passed to the dumper process
    memset(&tac_crash_spot, 0, sizeof(tacc_spot_t));
    //赋值
    tac_crash_spot.api_level = tac_common_api_level;
    tac_crash_spot.crash_pid = tac_common_process_id;
    tac_crash_spot.start_time = tac_common_start_time;
    tac_crash_spot.time_zone = tac_common_time_zone;
    tac_crash_spot.os_version_len = strlen(tac_common_os_version);
    tac_crash_spot.kernel_version_len = strlen(tac_common_kernel_version);
    tac_crash_spot.abi_list_len = strlen(tac_common_abi_list);
    tac_crash_spot.manufacturer_len = strlen(tac_common_manufacturer);
    tac_crash_spot.brand_len = strlen(tac_common_brand);
    tac_crash_spot.model_len = strlen(tac_common_model);
    tac_crash_spot.build_fingerprint_len = strlen(tac_common_build_fingerprint);
    tac_crash_spot.app_id_len = strlen(tac_common_app_id);
    tac_crash_spot.app_version_len = strlen(tac_common_app_version);

    //for clone and fork
#ifndef __i386__
    if(NULL == (tac_crash_child_stack = calloc(TAC_CRASH_CHILD_STACK_LEN, 1))) return TAC_ERRNO_NOMEM;
    tac_crash_child_stack = (void *)(((uint8_t *)tac_crash_child_stack) + TAC_CRASH_CHILD_STACK_LEN);
#else
    if (0 != pipe2(tac_crash_child_notifier, O_CLOEXEC)) return TAC_ERRNO_SYS;
#endif

    //register signal handler
    //注册信号处理器
    return tacc_signal_crash_register(tac_crash_signal_handler);
}

#pragma clang diagnostic pop
