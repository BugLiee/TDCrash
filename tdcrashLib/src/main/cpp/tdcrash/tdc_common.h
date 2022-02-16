//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_COMMON_H
#define TDCRASH_TDC_COMMON_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

// log filename format:
// tombstone_01234567890123456789_appversion__processname.native.tdcrash
// tombstone_01234567890123456789_appversion__processname.trace.tdcrash
// placeholder_01234567890123456789.clean.tdcrash
#define TDC_COMMON_LOG_PREFIX           "tombstone"
#define TDC_COMMON_LOG_PREFIX_LEN       9
#define TDC_COMMON_LOG_SUFFIX_CRASH     ".native.tdcrash"
#define TDC_COMMON_LOG_SUFFIX_TRACE     ".trace.tdcrash"
#define TDC_COMMON_LOG_SUFFIX_TRACE_LEN 13
#define TDC_COMMON_LOG_NAME_MIN_TRACE   (9 + 1 + 20 + 1 + 2 + 13)
#define TDC_COMMON_PLACEHOLDER_PREFIX   "placeholder"
#define TDC_COMMON_PLACEHOLDER_SUFFIX   ".clean.tdcrash"

//system info
//extern 标识需要在外部使用的全局变量 .c 里面也有
extern int           tdc_common_api_level;
extern char         *tdc_common_os_version;
extern char         *tdc_common_abi_list;
extern char         *tdc_common_manufacturer;
extern char         *tdc_common_brand;
extern char         *tdc_common_model;
extern char         *tdc_common_build_fingerprint;
extern char         *tdc_common_kernel_version;
extern long          tdc_common_time_zone;

//app info
extern char         *tdc_common_app_id;
extern char         *tdc_common_app_version;
extern char         *tdc_common_app_lib_dir;
extern char         *tdc_common_log_dir;

//process info
extern pid_t         tdc_common_process_id;
extern char         *tdc_common_process_name;
extern uint64_t      tdc_common_start_time;
extern JavaVM       *tdc_common_vm;
extern jclass        tdc_common_cb_class;
extern int           tdc_common_fd_null;

//process statue
extern sig_atomic_t  tdc_common_native_crashed;
extern sig_atomic_t  tdc_common_java_crashed;

void tdc_common_set_vm(JavaVM *vm, JNIEnv *env, jclass cls);

int tdc_common_init(int         api_level,
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

int tdc_common_open_crash_log(char *pathname, size_t pathname_len, int *from_placeholder, uint64_t crash_time);
int tdc_common_open_trace_log(char *pathname, size_t pathname_len, uint64_t trace_time);
void tdc_common_close_crash_log(int fd);
void tdc_common_close_trace_log(int fd);
int tdc_common_seek_to_content_end(int fd);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDC_COMMON_H
