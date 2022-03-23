//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TAC_COMMON_H
#define TACRASH_TAC_COMMON_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

// log filename format:
// tombstone_01234567890123456789_appversion__processname.native.tacrash
// tombstone_01234567890123456789_appversion__processname.trace.tacrash
// placeholder_01234567890123456789.clean.tacrash
#define TAC_COMMON_LOG_PREFIX           "tombstone"
#define TAC_COMMON_LOG_PREFIX_LEN       9
#define TAC_COMMON_LOG_SUFFIX_CRASH     ".native.tacrash"
#define TAC_COMMON_LOG_SUFFIX_TRACE     ".trace.tacrash"
#define TAC_COMMON_LOG_SUFFIX_TRACE_LEN 13
#define TAC_COMMON_LOG_NAME_MIN_TRACE   (9 + 1 + 20 + 1 + 2 + 13)
#define TAC_COMMON_PLACEHOLDER_PREFIX   "placeholder"
#define TAC_COMMON_PLACEHOLDER_SUFFIX   ".clean.tacrash"

//system info
//extern 标识需要在外部使用的全局变量 .c 里面也有
extern int           tac_common_api_level;
extern char         *tac_common_os_version;
extern char         *tac_common_abi_list;
extern char         *tac_common_manufacturer;
extern char         *tac_common_brand;
extern char         *tac_common_model;
extern char         *tac_common_build_fingerprint;
extern char         *tac_common_kernel_version;
extern long          tac_common_time_zone;

//app info
extern char         *tac_common_app_id;
extern char         *tac_common_app_version;
extern char         *tac_common_app_lib_dir;
extern char         *tac_common_log_dir;

//process info
extern pid_t         tac_common_process_id;
extern char         *tac_common_process_name;
extern uint64_t      tac_common_start_time;
extern JavaVM       *tac_common_vm;
extern jclass        tac_common_cb_class;
extern int           tac_common_fd_null;

//process statue
extern sig_atomic_t  tac_common_native_crashed;
extern sig_atomic_t  tac_common_java_crashed;

void tac_common_set_vm(JavaVM *vm, JNIEnv *env, jclass cls);

int tac_common_init(int         api_level,
                   const char *os_version,
                   const char *abi_list,
                   const char *manufacturer,
                   const char *brand,
                   const char *model,
                   const char *build_fingerprint,
                   const char *app_id,
                   const char *app_version,
                   const char *app_lib_dir,
                   const char *log_dir);

int tac_common_open_crash_log(char *pathname, size_t pathname_len, int *from_placeholder, uint64_t crash_time);
int tac_common_open_trace_log(char *pathname, size_t pathname_len, uint64_t trace_time);
void tac_common_close_crash_log(int fd);
void tac_common_close_trace_log(int fd);
int tac_common_seek_to_content_end(int fd);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TAC_COMMON_H
