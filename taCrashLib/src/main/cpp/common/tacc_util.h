//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TACC_UTIL_H
#define TACRASH_TACC_UTIL_H 1

#include <stdint.h>
#include <sys/types.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TACC_UTIL_TOMB_HEAD  "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
#define TACC_UTIL_THREAD_END "+++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++ +++\n"


#define TACRASH_DUMPER_FILENAME "libtacrash_dumper.so"

#define TACC_UTIL_CRASH_TYPE_NATIVE "native"
#define TACC_UTIL_CRASH_TYPE_ANR "anr"

#if defined(__arm__)
#define TACC_UTIL_ABI_STRING "arm"
#elif defined(__aarch64__)
#define TACC_UTIL_ABI_STRING "arm64"
#elif defined(__i386__)
#define TACC_UTIL_ABI_STRING "x86"
#elif defined(__x86_64__)
#define TACC_UTIL_ABI_STRING "x86_64"
#else
#define TACC_UTIL_ABI_STRING "unknown"
#endif

#if defined(__LP64__)
#define TACC_UTIL_FMT_ADDR "16"PRIxPTR
#else
#define TACC_UTIL_FMT_ADDR "8"PRIxPTR
#endif

#ifndef __LP64__
#define TACC_UTIL_SYSCALL_GETDENTS SYS_getdents
#else
#define TACC_UTIL_SYSCALL_GETDENTS SYS_getdents64
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct
{
#ifndef __LP64__
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
#else
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
#endif
    char           d_name[1];
} tacc_util_dirent_t;
#pragma clang diagnostic pop

#define TACC_UTIL_MAX(a,b) ({         \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a > _b ? _a : _b; })

#define TACC_UTIL_MIN(a,b) ({         \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a < _b ? _a : _b; })

#define TACC_UTIL_TEMP_FAILURE_RETRY(exp) ({         \
            __typeof__(exp) _rc;                    \
            do {                                    \
                errno = 0;                          \
                _rc = (exp);                        \
            } while (_rc == -1 && errno == EINTR);  \
            _rc; })

#ifndef __LP64__
#define TACC_UTIL_LIBC        "/system/lib/libc.so"
#define TACC_UTIL_LIBCPP      "/system/lib/libc++.so"
#define TACC_UTIL_LIBART      "/system/lib/libart.so"
#define TACC_UTIL_LIBC_APEX   "/apex/com.android.runtime/lib/bionic/libc.so"
#define TACC_UTIL_LIBCPP_APEX "/apex/com.android.runtime/lib/libc++.so"
#define TACC_UTIL_LIBART_APEX_29 "/apex/com.android.runtime/lib/libart.so"
#define TACC_UTIL_LIBART_APEX_30 "/apex/com.android.art/lib/libart.so"
#else
#define TACC_UTIL_LIBC        "/system/lib64/libc.so"
#define TACC_UTIL_LIBCPP      "/system/lib64/libc++.so"
#define TACC_UTIL_LIBART      "/system/lib64/libart.so"
#define TACC_UTIL_LIBC_APEX   "/apex/com.android.runtime/lib64/bionic/libc.so"
#define TACC_UTIL_LIBCPP_APEX "/apex/com.android.runtime/lib64/libc++.so"
#define TACC_UTIL_LIBART_APEX_29 "/apex/com.android.runtime/lib64/libart.so"
#define TACC_UTIL_LIBART_APEX_30 "/apex/com.android.art/lib64/libart.so"
#endif

#define TACC_UTIL_LIBC_ABORT_MSG_PTR      "__abort_message_ptr"
#define TACC_UTIL_LIBC_SET_ABORT_MSG      "android_set_abort_message"
#define TACC_UTIL_LIBCPP_CERR             "_ZNSt3__14cerrE"
#define TACC_UTIL_LIBART_RUNTIME_INSTANCE "_ZN3art7Runtime9instance_E"
#define TACC_UTIL_LIBART_RUNTIME_DUMP     "_ZN3art7Runtime14DumpForSigQuitERNSt3__113basic_ostreamIcNS1_11char_traitsIcEEEE"
#define TACC_UTIL_LIBART_THREAD_CURRENT   "_ZN3art6Thread14CurrentFromGdbEv"
#define TACC_UTIL_LIBART_THREAD_DUMP      "_ZNK3art6Thread13DumpJavaStackERNSt3__113basic_ostreamIcNS1_11char_traitsIcEEEE"
#define TACC_UTIL_LIBART_THREAD_DUMP2     "_ZNK3art6Thread13DumpJavaStackERNSt3__113basic_ostreamIcNS1_11char_traitsIcEEEEbb"
#define TACC_UTIL_LIBART_DBG_SUSPEND      "_ZN3art3Dbg9SuspendVMEv"
#define TACC_UTIL_LIBART_DBG_RESUME       "_ZN3art3Dbg8ResumeVMEv"

typedef void  (*tacc_util_libc_set_abort_message_t)(const char* msg);
typedef void  (*tacc_util_libart_runtime_dump_t)(void *runtime, void *ostream);
typedef void *(*tacc_util_libart_thread_current_t)(void);
typedef void  (*tacc_util_libart_thread_dump_t)(void *thread, void *ostream);
typedef void  (*tacc_util_libart_thread_dump2_t)(void *thread, void *ostream, int check_suspended, int dump_locks);
typedef void  (*tacc_util_libart_dbg_suspend_t)(void);
typedef void  (*tacc_util_libart_dbg_resume_t)(void);

const char* tacc_util_get_signame(const siginfo_t* si);
const char* tacc_util_get_sigcodename(const siginfo_t* si);
int tacc_util_signal_has_si_addr(const siginfo_t* si);
int tacc_util_signal_has_sender(const siginfo_t* si, pid_t caller_pid);

char *tacc_util_trim(char *start);
int tacc_util_atoi(const char *str, int *i);

int tacc_util_write(int fd, const char *buf, size_t len);
int tacc_util_write_str(int fd, const char *str);
int tacc_util_write_format(int fd, const char *format, ...);
int tacc_util_write_format_safe(int fd, const char *format, ...);

char *tacc_util_gets(char *s, size_t size, int fd);
int tacc_util_read_file_line(const char *path, char *buf, size_t len);

void tacc_util_get_process_name(pid_t pid, char *buf, size_t len);
void tacc_util_get_thread_name(pid_t tid, char *buf, size_t len);

int tacc_util_record_sub_section_from(int log_fd, const char *path, const char *title, size_t limit);

int tacc_util_is_root(void);

size_t tacc_util_get_dump_header(char *buf,
                                size_t buf_len,
                                const char *crash_type,
                                long time_zone,
                                uint64_t start_time,
                                uint64_t crash_time,
                                const char *app_id,
                                const char *app_version,
                                int api_level,
                                const char *os_version,
                                const char *kernel_version,
                                const char *abi_list,
                                const char *manufacturer,
                                const char *brand,
                                const char *model,
                                const char *build_fingerprint);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TACC_UTIL_H
