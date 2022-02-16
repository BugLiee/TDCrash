//
// Created by bugliee on 2022/1/11.
//

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <jni.h>
#include "tdc_common.h"
#include "tdc_errno.h"
#include "tdcc_fmt.h"
#include "tdc_jni.h"
#include "tdc_util.h"
#include "../common/tdcc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wcast-align"

#define TDC_COMMON_OPEN_DIR_FLAGS      (O_RDONLY | O_DIRECTORY | O_CLOEXEC)
#define TDC_COMMON_OPEN_FILE_FLAGS     (O_RDWR | O_CLOEXEC)
#define TDC_COMMON_OPEN_NEW_FILE_FLAGS (O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC | O_APPEND)
#define TDC_COMMON_OPEN_NEW_FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) //644

//system info
int           tdc_common_api_level         = 0;
char         *tdc_common_os_version        = NULL;
char         *tdc_common_abi_list          = NULL;
char         *tdc_common_manufacturer      = NULL;
char         *tdc_common_brand             = NULL;
char         *tdc_common_model             = NULL;
char         *tdc_common_build_fingerprint = NULL;
char         *tdc_common_kernel_version    = NULL;
long          tdc_common_time_zone         = 0;

//app info
char         *tdc_common_app_id            = NULL;
char         *tdc_common_app_version       = NULL;
char         *tdc_common_app_lib_dir       = NULL;
char         *tdc_common_log_dir           = NULL;

//process info
pid_t         tdc_common_process_id        = 0;
char         *tdc_common_process_name      = NULL;
uint64_t      tdc_common_start_time        = 0;
JavaVM       *tdc_common_vm                = NULL;
jclass        tdc_common_cb_class          = NULL;
int           tdc_common_fd_null           = -1;

//process statue
sig_atomic_t  tdc_common_native_crashed    = 0;
sig_atomic_t  tdc_common_java_crashed      = 0;

static int    tdc_common_crash_prepared_fd = -1;
static int    tdc_common_trace_prepared_fd = -1;

//打开文件描述符
static void tdc_common_open_prepared_fd(int is_crash)
{
    int fd = (is_crash ? tdc_common_crash_prepared_fd : tdc_common_trace_prepared_fd);
    if(fd >= 0) return;

    fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));

    //分配对应的fd
    if(is_crash)
        tdc_common_crash_prepared_fd = fd;
    else
        tdc_common_trace_prepared_fd = fd;
}

//关闭对应文件描述符
static int tdc_common_close_prepared_fd(int is_crash)
{
    int fd = (is_crash ? tdc_common_crash_prepared_fd : tdc_common_trace_prepared_fd);
    if(fd < 0) return TDC_ERRNO_FD;

    close(fd);

    if(is_crash)
        tdc_common_crash_prepared_fd = -1;
    else
        tdc_common_trace_prepared_fd = -1;

    return 0;
}

//设置java虚拟机
void tdc_common_set_vm(JavaVM *vm, JNIEnv *env, jclass cls)
{
    tdc_common_vm = vm;

    tdc_common_cb_class = (*env)->NewGlobalRef(env, cls);
    TDC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(tdc_common_cb_class, err);
    return;

    err:
    tdc_common_cb_class = NULL;
}


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
                   const char *log_dir)
{
    int             r = 0;
    struct timeval  tv; //时间结构体
    struct tm       tm; //时间结构体
    char            buf[256];
    char           *kernel_version;
    char           *process_name;

    //##相当于拼接两个字符串  单个#是转换为字符串变量
    //strdup()字符串拷贝函数，和free()成对出现，这里如果结果为NULL，走err:之后通过free()释放内存
#define TDC_COMMON_DUP_STR(v) do {                                       \
        if(NULL == v || 0 == strlen(v))                                 \
            tdc_common_##v = "unknown";                                  \
        else                                                            \
        {                                                               \
            if(NULL == (tdc_common_##v = strdup(v)))                     \
            {                                                           \
                r = TDC_ERRNO_NOMEM;                                    \
                goto err;                                               \
            }                                                           \
        }                                                               \
    } while (0)
    //释放字符串
#define TDC_COMMON_FREE_STR(v) do {                                      \
        if(NULL != tdc_common_##v) {                                     \
            free(tdc_common_##v);                                        \
            tdc_common_##v = NULL;                                       \
        }                                                               \
    } while (0)

    //save start time
    //获取时间戳
    if(0 != gettimeofday(&tv, NULL)) return TDC_ERRNO_SYS;
    //tv_sec 是秒 tv_usec是微秒即10 -6次方
    //uint64_t 是预定义数据类型标准 ，代表 long 类型
    tdc_common_start_time = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)tv.tv_usec / 1000;
//    tdc_common_start_time = (uint64_t)(tv.tv_sec) * 1000 * 1000 + (uint64_t)tv.tv_usec;

    //save time zone
    //获取系统时间
    if(NULL == localtime_r((time_t*)(&(tv.tv_sec)), &tm)) return TDC_ERRNO_SYS;
    //时区偏移值
    tdc_common_time_zone = tm.tm_gmtoff;

    //save common info
    //赋值
    tdc_common_api_level = api_level;
    TDC_COMMON_DUP_STR(os_version);
    TDC_COMMON_DUP_STR(abi_list);
    TDC_COMMON_DUP_STR(manufacturer);
    TDC_COMMON_DUP_STR(brand);
    TDC_COMMON_DUP_STR(model);
    TDC_COMMON_DUP_STR(build_fingerprint);
    TDC_COMMON_DUP_STR(app_id);
    TDC_COMMON_DUP_STR(app_version);
    TDC_COMMON_DUP_STR(app_lib_dir);
    TDC_COMMON_DUP_STR(log_dir);

    //save kernel version
    //获取系统版本信息
    tdc_util_get_kernel_version(buf, sizeof(buf));
    kernel_version = buf;
    TDC_COMMON_DUP_STR(kernel_version);

    //save process id and process name
    //获取进程id和name
    tdc_common_process_id = getpid();
    tdcc_util_get_process_name(tdc_common_process_id, buf, sizeof(buf));
    process_name = buf;
    TDC_COMMON_DUP_STR(process_name);

    //to /dev/null
    //可读写打开文件
    if((tdc_common_fd_null = TDCC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR))) < 0)
    {
        r = TDC_ERRNO_SYS;
        goto err;
    }

    //check or create log directory
    //检查日志文件目录是否存在，无则创建
    if(0 != (r = tdc_util_mkdirs(log_dir))) goto err;

    //create prepared FD for FD exhausted case
    //创建文件描述符
    tdc_common_open_prepared_fd(1);
    tdc_common_open_prepared_fd(0);

    return 0;

    err:
    TDC_COMMON_FREE_STR(os_version);
    TDC_COMMON_FREE_STR(abi_list);
    TDC_COMMON_FREE_STR(manufacturer);
    TDC_COMMON_FREE_STR(brand);
    TDC_COMMON_FREE_STR(model);
    TDC_COMMON_FREE_STR(build_fingerprint);
    TDC_COMMON_FREE_STR(app_id);
    TDC_COMMON_FREE_STR(app_version);
    TDC_COMMON_FREE_STR(app_lib_dir);
    TDC_COMMON_FREE_STR(log_dir);
    TDC_COMMON_FREE_STR(kernel_version);
    TDC_COMMON_FREE_STR(process_name);

    return r;
}

//构建日志
static int tdc_common_open_log(int is_crash, uint64_t timestamp,
                              char *pathname, size_t pathname_len, int *from_placeholder)
{
    int                fd = -1;
    char               buf[512];
    char               placeholder_pathname[1024];
    long               n, i;
    tdcc_util_dirent_t *ent;

    tdcc_fmt_snprintf(pathname, pathname_len, "%s/"TDC_COMMON_LOG_PREFIX"_%"PRIu64"_%s_%s_%s",
            tdc_common_log_dir, timestamp, tdc_common_app_version, tdc_common_process_name,
            is_crash ? TDC_COMMON_LOG_SUFFIX_CRASH : TDC_COMMON_LOG_SUFFIX_TRACE);

    //open dir
    if((fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(tdc_common_log_dir, TDC_COMMON_OPEN_DIR_FLAGS))) < 0)
    {
        //try again with the prepared fd
        if(0 != tdc_common_close_prepared_fd(is_crash)) goto create_new_file;
        if((fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(tdc_common_log_dir, TDC_COMMON_OPEN_DIR_FLAGS))) < 0) goto create_new_file;
    }

    //try to rename a placeholder file and open it
    while((n = syscall(TDCC_UTIL_SYSCALL_GETDENTS, fd, buf, sizeof(buf))) > 0)
    {
        for(i = 0; i < n; i += ent->d_reclen)
        {
            ent = (tdcc_util_dirent_t *)(buf + i);

            // placeholder_01234567890123456789.clean.tdcrash
            // file name length: 45
            if(45 == strlen(ent->d_name) &&
               0 == memcmp(ent->d_name, TDC_COMMON_PLACEHOLDER_PREFIX"_", 12) &&
            0 == memcmp(ent->d_name + 32, TDC_COMMON_PLACEHOLDER_SUFFIX, 13))
            {
                tdcc_fmt_snprintf(placeholder_pathname, sizeof(placeholder_pathname), "%s/%s", tdc_common_log_dir, ent->d_name);
                if(0 == rename(placeholder_pathname, pathname))
                {
                    close(fd);
                    if(NULL != from_placeholder) *from_placeholder = 1;
                    return TDCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, TDC_COMMON_OPEN_FILE_FLAGS));
                }
            }
        }
    }
    close(fd);
    tdc_common_open_prepared_fd(is_crash);

    create_new_file:
    if(NULL != from_placeholder) *from_placeholder = 0;

    if((fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, TDC_COMMON_OPEN_NEW_FILE_FLAGS, TDC_COMMON_OPEN_NEW_FILE_MODE))) >= 0) return fd;

    //try again with the prepared fd
    if(0 != tdc_common_close_prepared_fd(is_crash)) return -1;
    return TDCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, TDC_COMMON_OPEN_NEW_FILE_FLAGS, TDC_COMMON_OPEN_NEW_FILE_MODE));
}

static void tdc_common_close_log(int fd, int is_crash)
{
    close(fd);
    tdc_common_open_prepared_fd(is_crash);
}

int tdc_common_open_crash_log(char *pathname, size_t pathname_len, int *from_placeholder, uint64_t crash_time)
{
    return tdc_common_open_log(1, crash_time, pathname, pathname_len, from_placeholder);
}

int tdc_common_open_trace_log(char *pathname, size_t pathname_len, uint64_t trace_time)
{
    return tdc_common_open_log(0, trace_time, pathname, pathname_len, NULL);
}

void tdc_common_close_crash_log(int fd)
{
    tdc_common_close_log(fd, 1);
}

void tdc_common_close_trace_log(int fd)
{
    tdc_common_close_log(fd, 0);
}

int tdc_common_seek_to_content_end(int fd)
{
    uint8_t buf[1024];
    ssize_t readed, n;
    off_t   offset = 0;

    //placeholder file
    if(lseek(fd, 0, SEEK_SET) < 0) goto err;
    while(1)
    {
        readed = TDCC_UTIL_TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
        if(readed < 0)
        {
            goto err;
        }
        else if(0 == readed)
        {
            if(lseek(fd, 0, SEEK_END) < 0) goto err;
            return fd;
        }
        else
        {
            for(n = readed; n > 0; n--)
            {
                if(0 != buf[n - 1]) break;
            }
            offset += (off_t)n;
            if(n < readed)
            {
                if(lseek(fd, offset, SEEK_SET) < 0) goto err;
                return fd;
            }
        }
    }

    err:
    close(fd);
    return -1;
}

#pragma clang diagnostic pop

