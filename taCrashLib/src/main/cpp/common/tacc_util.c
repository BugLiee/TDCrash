
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/system_properties.h>
#include <android/log.h>
#include "tacc_util.h"
#include "../tacrash/tac_errno.h"
#include "tacc_fmt.h"
#include "tacc_version.h"
#include "tacc_libc_support.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wformat-nonliteral"

#define TACC_UTIL_TIME_FORMAT "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld%02ld"

const char* tacc_util_get_signame(const siginfo_t* si)
{
    switch (si->si_signo)
    {
        case SIGABRT:   return "SIGABRT";
        case SIGBUS:    return "SIGBUS";
        case SIGFPE:    return "SIGFPE";
        case SIGILL:    return "SIGILL";
        case SIGSEGV:   return "SIGSEGV";
        case SIGTRAP:   return "SIGTRAP";
        case SIGSYS:    return "SIGSYS";
        case SIGSTKFLT: return "SIGSTKFLT";
        default:        return "?";
    }
}

const char* tacc_util_get_sigcodename(const siginfo_t* si)
{
    // Try the signal-specific codes...
    switch (si->si_signo) {
        case SIGBUS:
            switch(si->si_code)
            {
                case BUS_ADRALN:    return "BUS_ADRALN";
                case BUS_ADRERR:    return "BUS_ADRERR";
                case BUS_OBJERR:    return "BUS_OBJERR";
                case BUS_MCEERR_AR: return "BUS_MCEERR_AR";
                case BUS_MCEERR_AO: return "BUS_MCEERR_AO";
                default:            break;
            }
            break;
        case SIGFPE:
            switch(si->si_code)
            {
                case FPE_INTDIV:   return "FPE_INTDIV";
                case FPE_INTOVF:   return "FPE_INTOVF";
                case FPE_FLTDIV:   return "FPE_FLTDIV";
                case FPE_FLTOVF:   return "FPE_FLTOVF";
                case FPE_FLTUND:   return "FPE_FLTUND";
                case FPE_FLTRES:   return "FPE_FLTRES";
                case FPE_FLTINV:   return "FPE_FLTINV";
                case FPE_FLTSUB:   return "FPE_FLTSUB";
                default:           break;
            }
            break;
        case SIGILL:
            switch(si->si_code)
            {
                case ILL_ILLOPC:   return "ILL_ILLOPC";
                case ILL_ILLOPN:   return "ILL_ILLOPN";
                case ILL_ILLADR:   return "ILL_ILLADR";
                case ILL_ILLTRP:   return "ILL_ILLTRP";
                case ILL_PRVOPC:   return "ILL_PRVOPC";
                case ILL_PRVREG:   return "ILL_PRVREG";
                case ILL_COPROC:   return "ILL_COPROC";
                case ILL_BADSTK:   return "ILL_BADSTK";
                default:           break;
            }
            break;
        case SIGSEGV:
            switch(si->si_code)
            {
                case SEGV_MAPERR:  return "SEGV_MAPERR";
                case SEGV_ACCERR:  return "SEGV_ACCERR";
                case SEGV_BNDERR:  return "SEGV_BNDERR";
                case SEGV_PKUERR:  return "SEGV_PKUERR";
                default:           break;
            }
            break;
        case SIGTRAP:
            switch(si->si_code)
            {
                case TRAP_BRKPT:   return "TRAP_BRKPT";
                case TRAP_TRACE:   return "TRAP_TRACE";
                case TRAP_BRANCH:  return "TRAP_BRANCH";
                case TRAP_HWBKPT:  return "TRAP_HWBKPT";
                default:           break;
            }
            if((si->si_code & 0xff) == SIGTRAP)
            {
                switch((si->si_code >> 8) & 0xff)
                {
                    case PTRACE_EVENT_FORK:       return "PTRACE_EVENT_FORK";
                    case PTRACE_EVENT_VFORK:      return "PTRACE_EVENT_VFORK";
                    case PTRACE_EVENT_CLONE:      return "PTRACE_EVENT_CLONE";
                    case PTRACE_EVENT_EXEC:       return "PTRACE_EVENT_EXEC";
                    case PTRACE_EVENT_VFORK_DONE: return "PTRACE_EVENT_VFORK_DONE";
                    case PTRACE_EVENT_EXIT:       return "PTRACE_EVENT_EXIT";
                    case PTRACE_EVENT_SECCOMP:    return "PTRACE_EVENT_SECCOMP";
                    case PTRACE_EVENT_STOP:       return "PTRACE_EVENT_STOP";
                    default:                      break;
                }
            }
            break;
        case SIGSYS:
            switch(si->si_code)
            {
                case SYS_SECCOMP: return "SYS_SECCOMP";
                default:          break;
            }
            break;
        default:
            break;
    }

    // Then the other codes...
    switch (si->si_code) {
        case SI_USER:     return "SI_USER";
        case SI_KERNEL:   return "SI_KERNEL";
        case SI_QUEUE:    return "SI_QUEUE";
        case SI_TIMER:    return "SI_TIMER";
        case SI_MESGQ:    return "SI_MESGQ";
        case SI_ASYNCIO:  return "SI_ASYNCIO";
        case SI_SIGIO:    return "SI_SIGIO";
        case SI_TKILL:    return "SI_TKILL";
        case SI_DETHREAD: return "SI_DETHREAD";
    }

    // Then give up...
    return "?";
}

int tacc_util_signal_has_si_addr(const siginfo_t* si)
{
    //manually sent signals won't have si_addr
    if(si->si_code == SI_USER || si->si_code == SI_QUEUE || si->si_code == SI_TKILL) return 0;

    switch (si->si_signo)
    {
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
        case SIGSEGV:
        case SIGTRAP:
            return 1;
        default:
            return 0;
    }
}

int tacc_util_signal_has_sender(const siginfo_t* si, pid_t caller_pid)
{
    return (SI_FROMUSER(si) && (si->si_pid != 0) && (si->si_pid != caller_pid)) ? 1 : 0;
}

int tacc_util_atoi(const char *str, int *i)
{
    //We have to do this job very carefully for some unusual version of stdlib.

    long  val = 0;
    char *endptr = NULL;
    const char *p = str;

    //check
    if(NULL == str || NULL == i) return TAC_ERRNO_INVAL;
    if((*p < '0' || *p > '9') && *p != '-') return TAC_ERRNO_INVAL;
    p++;
    while(*p)
    {
        if(*p < '0' || *p > '9') return TAC_ERRNO_INVAL;
        p++;
    }

    //convert
    errno = 0;
    val = strtol(str, &endptr, 10);

    //check
    if((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0))
        return TAC_ERRNO_INVAL;
    if(endptr == str)
        return TAC_ERRNO_INVAL;
    if(val > INT_MAX || val < INT_MIN)
        return TAC_ERRNO_INVAL;

    //OK
    *i = (int)val;
    return 0;
}

char *tacc_util_trim(char *start)
{
    char *end;

    if(NULL == start) return NULL;

    end = start + strlen(start);
    if(start == end) return start;

    while(start < end && isspace((int)(*start))) start++;
    if(start == end) return start;

    while(start < end && isspace((int)(*(end - 1)))) end--;
    *end = '\0';
    return start;
}

int tacc_util_write(int fd, const char *buf, size_t len)
{
    size_t      nleft;
    ssize_t     nwritten;
    const char *ptr;

    if(fd < 0) return TAC_ERRNO_INVAL;

    ptr   = buf;
    nleft = len;

    while(nleft > 0)
    {
        errno = 0;
        if((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if(nwritten < 0 && errno == EINTR)
                nwritten = 0; /* call write() again */
            else
                return TAC_ERRNO_SYS;    /* error */
        }

        nleft -= (size_t)nwritten;
        ptr   += nwritten;
    }

    return 0;
}

int tacc_util_write_str(int fd, const char *str)
{
    const char *tmp = str;
    size_t      len = 0;

    if(fd < 0) return TAC_ERRNO_INVAL;

    while(*tmp) tmp++;
    len = (size_t)(tmp - str);
    if(0 == len) return 0;

    return tacc_util_write(fd, str, len);
}

int tacc_util_write_format(int fd, const char *format, ...)
{
    va_list ap;
    char    buf[1024];
    ssize_t len;

    if(fd < 0) return TAC_ERRNO_INVAL;

    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if(len <= 0) return 0;

    return tacc_util_write(fd, buf, (size_t)len);
}

int tacc_util_write_format_safe(int fd, const char *format, ...)
{
    va_list ap;
    char    buf[1024];
    size_t  len;

    if(fd < 0) return TAC_ERRNO_INVAL;

    va_start(ap, format);
    len = tacc_fmt_vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    if(0 == len) return 0;

    return tacc_util_write(fd, buf, len);
}

char *tacc_util_gets(char *s, size_t size, int fd)
{
    ssize_t i, nread;
    char    c, *p;

    if(fd < 0 || NULL == s || size < 2) return NULL;

    s[0] = '\0';
    p = s;

    for(i = 0; i < (ssize_t)(size - 1); i++)
    {
        if(1 == (nread = read(fd, &c, 1)))
        {
            *p++ = c;
            if('\n' == c) break;
        }
        else if(0 == nread) //EOF
        {
            break;
        }
        else
        {
            if (errno != EINTR) return NULL;
        }
    }

    *p = '\0';
    return ('\0' == s[0] ? NULL : s);
}

int tacc_util_read_file_line(const char *path, char *buf, size_t len)
{
    int fd = -1;
    int r = 0;

    if(0 > (fd = TACC_UTIL_TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_CLOEXEC))))
    {
        r = TAC_ERRNO_SYS;
        goto end;
    }
    if(NULL == tacc_util_gets(buf, len, fd))
    {
        r = TAC_ERRNO_SYS;
        goto end;
    }

    end:
    if(fd >= 0) close(fd);
    return r;
}

static int tacc_util_get_process_thread_name(const char *path, char *buf, size_t len)
{
    char    tmp[256], *data;
    size_t  data_len, cpy_len;
    int     r;

    //read a line
    if(0 != (r = tacc_util_read_file_line(path, tmp, sizeof(tmp)))) return r;

    //trim
    data = tacc_util_trim(tmp);

    //return data
    if(0 == (data_len = strlen(data))) return TAC_ERRNO_MISSING;
    cpy_len = TACC_UTIL_MIN(len - 1, data_len);
    memcpy(buf, data, cpy_len);
    buf[cpy_len] = '\0';

    return 0;
}

//获取进程名称
void tacc_util_get_process_name(pid_t pid, char *buf, size_t len)
{
    char path[128];

    tacc_fmt_snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);

    if(0 != tacc_util_get_process_thread_name(path, buf, len))
        strncpy(buf, "unknown", len);
}

//获取线程名称
void tacc_util_get_thread_name(pid_t tid, char *buf, size_t len)
{
    char path[128];

    tacc_fmt_snprintf(path, sizeof(path), "/proc/%d/comm", tid);

    if(0 != tacc_util_get_process_thread_name(path, buf, len))
        strncpy(buf, "unknown", len);
}

int tacc_util_record_sub_section_from(int fd, const char *path, const char *title, size_t limit)
{
    FILE   *fp = NULL;
    char    line[512];
    char   *p;
    int     r = 0;
    size_t  n = 0;

    if(NULL == (fp = fopen(path, "r"))) goto end;

    if(0 != (r = tacc_util_write_str(fd, title))) goto end;
    while(NULL != fgets(line, sizeof(line), fp))
    {
        p = tacc_util_trim(line);
        if(strlen(p) > 0)
        {
            n++;
            if(0 == limit || n <= limit)
                if(0 != (r = tacc_util_write_format_safe(fd, "  %s\n", p))) goto end;
        }
    }
    if(limit > 0 && n > limit)
    {
        if(0 != (r = tacc_util_write_str(fd, "  ......\n"))) goto end;
        if(0 != (r = tacc_util_write_format_safe(fd, "  (number of records: %zu)\n", n))) goto end;
    }
    if(0 != (r = tacc_util_write_str(fd, "-\n"))) goto end;

    end:
    if(NULL != fp) fclose(fp);
    return r;
}

static const char *tacc_util_su_pathnames[] =
        {
                "/data/local/su",
                "/data/local/bin/su",
                "/data/local/xbin/su",
                "/system/xbin/su",
                "/system/bin/su",
                "/system/bin/.ext/su",
                "/system/bin/failsafe/su",
                "/system/sd/xbin/su",
                "/system/usr/we-need-root/su",
                "/sbin/su",
                "/su/bin/su"
        };
static int tacc_util_is_root_saved = -1;

int tacc_util_is_root(void)
{
    size_t i;

    if(tacc_util_is_root_saved >= 0) return tacc_util_is_root_saved;

    for(i = 0; i < sizeof(tacc_util_su_pathnames) / sizeof(tacc_util_su_pathnames[0]); i++)
    {
        if(0 == access(tacc_util_su_pathnames[i], F_OK))
        {
            tacc_util_is_root_saved = 1;
            return 1;
        }
    }

    tacc_util_is_root_saved = 0;
    return 0;
}

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
                                const char *build_fingerprint)
{
    time_t       start_sec  = (time_t)(start_time / 1000);
    suseconds_t  start_msec = (time_t)(start_time % 1000);
    struct tm    start_tm;
    time_t       crash_sec   = (time_t)(crash_time / 1000);
    suseconds_t  crash_msec = (time_t)(crash_time % 1000);
    struct tm    crash_tm;

    //convert times
    tacc_libc_support_memset(&start_tm, 0, sizeof(start_tm));
    tacc_libc_support_memset(&crash_tm, 0, sizeof(crash_tm));
    tacc_libc_support_localtime_r(&start_sec, time_zone, &start_tm);
    tacc_libc_support_localtime_r(&crash_sec, time_zone, &crash_tm);

    return tacc_fmt_snprintf(buf, buf_len,
                            TACC_UTIL_TOMB_HEAD
    "Tombstone maker: '"TACRASH_VERSION_STR"'\n"
                                       "Crash type: '%s'\n"
                                       "Start time: '"TACC_UTIL_TIME_FORMAT"'\n"
                                       "Crash time: '"TACC_UTIL_TIME_FORMAT"'\n"
                                       "App ID: '%s'\n"
                                       "App version: '%s'\n"
                                       "Rooted: '%s'\n"
                                       "API level: '%d'\n"
                                       "OS version: '%s'\n"
                                       "Kernel version: '%s'\n"
                                       "ABI list: '%s'\n"
                                       "Manufacturer: '%s'\n"
                                       "Brand: '%s'\n"
                                       "Model: '%s'\n"
                                       "Build fingerprint: '%s'\n"
                                       "ABI: '"TACC_UTIL_ABI_STRING"'\n",
            crash_type,
            start_tm.tm_year + 1900, start_tm.tm_mon + 1, start_tm.tm_mday,
            start_tm.tm_hour, start_tm.tm_min, start_tm.tm_sec, start_msec,
            time_zone < 0 ? '-' : '+', labs(time_zone / 3600), labs(time_zone % 3600),
            crash_tm.tm_year + 1900, crash_tm.tm_mon + 1, crash_tm.tm_mday,
            crash_tm.tm_hour, crash_tm.tm_min, crash_tm.tm_sec, crash_msec,
            time_zone < 0 ? '-' : '+', labs(time_zone / 3600), labs(time_zone % 3600),
            app_id,
            app_version,
            tacc_util_is_root() ? "Yes" : "No",
            api_level,
            os_version,
            kernel_version,
            abi_list,
            manufacturer,
            brand,
            model,
            build_fingerprint);
}

#pragma clang diagnostic pop
