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
#include "tac_common.h"
#include "tac_errno.h"
#include "tacc_fmt.h"
#include "tac_jni.h"
#include "tac_util.h"
#include "../common/tacc_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wcast-align"

#define TAC_COMMON_OPEN_DIR_FLAGS      (O_RDONLY | O_DIRECTORY | O_CLOEXEC)
#define TAC_COMMON_OPEN_FILE_FLAGS     (O_RDWR | O_CLOEXEC)
#define TAC_COMMON_OPEN_NEW_FILE_FLAGS (O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC | O_APPEND)
#define TAC_COMMON_OPEN_NEW_FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) //644

//system info
int           tac_common_api_level         = 0;
char         *tac_common_os_version        = NULL;
char         *tac_common_abi_list          = NULL;
char         *tac_common_manufacturer      = NULL;
char         *tac_common_brand             = NULL;
char         *tac_common_model             = NULL;
char         *tac_common_build_fingerprint = NULL;
char         *tac_common_kernel_version    = NULL;
long          tac_common_time_zone         = 0;

//app info
char         *tac_common_app_id            = NULL;
char         *tac_common_app_version       = NULL;
char         *tac_common_app_lib_dir       = NULL;
char         *tac_common_log_dir           = NULL;

//process info
pid_t         tac_common_process_id        = 0;
char         *tac_common_process_name      = NULL;
uint64_t      tac_common_start_time        = 0;
JavaVM       *tac_common_vm                = NULL;
jclass        tac_common_cb_class          = NULL;
int           tac_common_fd_null           = -1;

//process statue
sig_atomic_t  tac_common_native_crashed    = 0;
sig_atomic_t  tac_common_java_crashed      = 0;

static int    tac_common_crash_prepared_fd = -1;
static int    tac_common_trace_prepared_fd = -1;

//打开文件描述符
static void tac_common_open_prepared_fd(int is_crash)
{
    int fd = (is_crash ? tac_common_crash_prepared_fd : tac_common_trace_prepared_fd);
    if(fd >= 0) return;

    fd = TACC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));

    //分配对应的fd
    if(is_crash)
        tac_common_crash_prepared_fd = fd;
    else
        tac_common_trace_prepared_fd = fd;
}

//关闭对应文件描述符
static int tac_common_close_prepared_fd(int is_crash)
{
    int fd = (is_crash ? tac_common_crash_prepared_fd : tac_common_trace_prepared_fd);
    if(fd < 0) return TAC_ERRNO_FD;

    close(fd);

    if(is_crash)
        tac_common_crash_prepared_fd = -1;
    else
        tac_common_trace_prepared_fd = -1;

    return 0;
}

//设置java虚拟机
void tac_common_set_vm(JavaVM *vm, JNIEnv *env, jclass cls)
{
    tac_common_vm = vm;

    tac_common_cb_class = (*env)->NewGlobalRef(env, cls);
    TAC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(tac_common_cb_class, err);
    return;

    err:
    tac_common_cb_class = NULL;
}


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
#define TAC_COMMON_DUP_STR(v) do {                                       \
        if(NULL == v || 0 == strlen(v))                                 \
            tac_common_##v = "unknown";                                  \
        else                                                            \
        {                                                               \
            if(NULL == (tac_common_##v = strdup(v)))                     \
            {                                                           \
                r = TAC_ERRNO_NOMEM;                                    \
                goto err;                                               \
            }                                                           \
        }                                                               \
    } while (0)
    //释放字符串
#define TAC_COMMON_FREE_STR(v) do {                                      \
        if(NULL != tac_common_##v) {                                     \
            free(tac_common_##v);                                        \
            tac_common_##v = NULL;                                       \
        }                                                               \
    } while (0)

    //save start time
    //获取时间戳
    if(0 != gettimeofday(&tv, NULL)) return TAC_ERRNO_SYS;
    //tv_sec 是秒 tv_usec是微秒即10 -6次方
    //uint64_t 是预定义数据类型标准 ，代表 long 类型
    tac_common_start_time = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)tv.tv_usec / 1000;
//    tac_common_start_time = (uint64_t)(tv.tv_sec) * 1000 * 1000 + (uint64_t)tv.tv_usec;

    //save time zone
    //获取系统时间
    if(NULL == localtime_r((time_t*)(&(tv.tv_sec)), &tm)) return TAC_ERRNO_SYS;
    //时区偏移值
    tac_common_time_zone = tm.tm_gmtoff;

    //save common info
    //赋值
    tac_common_api_level = api_level;
    TAC_COMMON_DUP_STR(os_version);
    TAC_COMMON_DUP_STR(abi_list);
    TAC_COMMON_DUP_STR(manufacturer);
    TAC_COMMON_DUP_STR(brand);
    TAC_COMMON_DUP_STR(model);
    TAC_COMMON_DUP_STR(build_fingerprint);
    TAC_COMMON_DUP_STR(app_id);
    TAC_COMMON_DUP_STR(app_version);
    TAC_COMMON_DUP_STR(app_lib_dir);
    TAC_COMMON_DUP_STR(log_dir);

    //save kernel version
    //获取系统版本信息
    tac_util_get_kernel_version(buf, sizeof(buf));
    kernel_version = buf;
    TAC_COMMON_DUP_STR(kernel_version);

    //save process id and process name
    //获取进程id和name
    tac_common_process_id = getpid();
    tacc_util_get_process_name(tac_common_process_id, buf, sizeof(buf));
    process_name = buf;
    TAC_COMMON_DUP_STR(process_name);

    //to /dev/null
    //可读写打开文件
    if((tac_common_fd_null = TACC_UTIL_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR))) < 0)
    {
        r = TAC_ERRNO_SYS;
        goto err;
    }

    //check or create log directory
    //检查日志文件目录是否存在，无则创建
    if(0 != (r = tac_util_mkdirs(log_dir))) goto err;

    //create prepared FD for FD exhausted case
    //创建文件描述符
    tac_common_open_prepared_fd(1);
    tac_common_open_prepared_fd(0);

    return 0;

    err:
    TAC_COMMON_FREE_STR(os_version);
    TAC_COMMON_FREE_STR(abi_list);
    TAC_COMMON_FREE_STR(manufacturer);
    TAC_COMMON_FREE_STR(brand);
    TAC_COMMON_FREE_STR(model);
    TAC_COMMON_FREE_STR(build_fingerprint);
    TAC_COMMON_FREE_STR(app_id);
    TAC_COMMON_FREE_STR(app_version);
    TAC_COMMON_FREE_STR(app_lib_dir);
    TAC_COMMON_FREE_STR(log_dir);
    TAC_COMMON_FREE_STR(kernel_version);
    TAC_COMMON_FREE_STR(process_name);

    return r;
}

//构建日志
static int tac_common_open_log(int is_crash, uint64_t timestamp,
                              char *pathname, size_t pathname_len, int *from_placeholder)
{
    int                fd = -1;
    char               buf[512];
    char               placeholder_pathname[1024];
    long               n, i;
    tacc_util_dirent_t *ent;

    tacc_fmt_snprintf(pathname, pathname_len, "%s/"TAC_COMMON_LOG_PREFIX"_%"PRIu64"_%s_%s_%s",
            tac_common_log_dir, timestamp, tac_common_app_version, tac_common_process_name,
            is_crash ? TAC_COMMON_LOG_SUFFIX_CRASH : TAC_COMMON_LOG_SUFFIX_TRACE);

    //open dir
    if((fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(tac_common_log_dir, TAC_COMMON_OPEN_DIR_FLAGS))) < 0)
    {
        //try again with the prepared fd
        if(0 != tac_common_close_prepared_fd(is_crash)) goto create_new_file;
        if((fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(tac_common_log_dir, TAC_COMMON_OPEN_DIR_FLAGS))) < 0) goto create_new_file;
    }

    //try to rename a placeholder file and open it
    while((n = syscall(TACC_UTIL_SYSCALL_GETDENTS, fd, buf, sizeof(buf))) > 0)
    {
        for(i = 0; i < n; i += ent->d_reclen)
        {
            ent = (tacc_util_dirent_t *)(buf + i);

            // placeholder_01234567890123456789.clean.tacrash
            // file name length: 45
            if(45 == strlen(ent->d_name) &&
               0 == memcmp(ent->d_name, TAC_COMMON_PLACEHOLDER_PREFIX"_", 12) &&
            0 == memcmp(ent->d_name + 32, TAC_COMMON_PLACEHOLDER_SUFFIX, 13))
            {
                tacc_fmt_snprintf(placeholder_pathname, sizeof(placeholder_pathname), "%s/%s", tac_common_log_dir, ent->d_name);
                if(0 == rename(placeholder_pathname, pathname))
                {
                    close(fd);
                    if(NULL != from_placeholder) *from_placeholder = 1;
                    return TACC_UTIL_TEMP_FAILURE_RETRY(open(pathname, TAC_COMMON_OPEN_FILE_FLAGS));
                }
            }
        }
    }
    close(fd);
    tac_common_open_prepared_fd(is_crash);

    create_new_file:
    if(NULL != from_placeholder) *from_placeholder = 0;

    if((fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(pathname, TAC_COMMON_OPEN_NEW_FILE_FLAGS, TAC_COMMON_OPEN_NEW_FILE_MODE))) >= 0) return fd;

    //try again with the prepared fd
    if(0 != tac_common_close_prepared_fd(is_crash)) return -1;
    return TACC_UTIL_TEMP_FAILURE_RETRY(open(pathname, TAC_COMMON_OPEN_NEW_FILE_FLAGS, TAC_COMMON_OPEN_NEW_FILE_MODE));
}

static void tac_common_close_log(int fd, int is_crash)
{
    close(fd);
    tac_common_open_prepared_fd(is_crash);
}

int tac_common_open_crash_log(char *pathname, size_t pathname_len, int *from_placeholder, uint64_t crash_time)
{
    return tac_common_open_log(1, crash_time, pathname, pathname_len, from_placeholder);
}

int tac_common_open_trace_log(char *pathname, size_t pathname_len, uint64_t trace_time)
{
    return tac_common_open_log(0, trace_time, pathname, pathname_len, NULL);
}

void tac_common_close_crash_log(int fd)
{
    tac_common_close_log(fd, 1);
}

void tac_common_close_trace_log(int fd)
{
    tac_common_close_log(fd, 0);
}

int tac_common_seek_to_content_end(int fd)
{
    uint8_t buf[1024];
    ssize_t readed, n;
    off_t   offset = 0;

    //placeholder file
    if(lseek(fd, 0, SEEK_SET) < 0) goto err;
    while(1)
    {
        readed = TACC_UTIL_TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
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

