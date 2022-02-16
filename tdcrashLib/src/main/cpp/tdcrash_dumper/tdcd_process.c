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
#include <dirent.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include "queue.h"
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcc_b64.h"
#include "tdcc_meminfo.h"
#include "tdcd_log.h"
#include "tdcd_process.h"
#include "tdcd_thread.h"
#include "tdcd_maps.h"
#include "tdcd_regs.h"
#include "tdcd_util.h"
#include "tdcd_sys.h"

typedef struct tdcd_thread_info
{
    tdcd_thread_t t;
    TAILQ_ENTRY(tdcd_thread_info,) link;
} tdcd_thread_info_t;
typedef TAILQ_HEAD(tdcd_thread_info_queue, tdcd_thread_info,) tdcd_thread_info_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tdcd_process
{
    pid_t                    pid;
    char                    *pname;
    pid_t                    crash_tid;
    ucontext_t              *uc;
    siginfo_t               *si;
    tdcd_thread_info_queue_t  thds;
    size_t                   nthds;
    tdcd_maps_t              *maps;
};
#pragma clang diagnostic pop

static int tdcd_process_load_threads(tdcd_process_t *self)
{
    char               buf[128];
    DIR               *dir;
    struct dirent     *ent;
    pid_t              tid;
    tdcd_thread_info_t *thd;

    snprintf(buf, sizeof(buf), "/proc/%d/task", self->pid);
    if(NULL == (dir = opendir(buf))) return TDC_ERRNO_SYS;
    while(NULL != (ent = readdir(dir)))
    {
        if(0 == strcmp(ent->d_name, ".")) continue;
        if(0 == strcmp(ent->d_name, "..")) continue;
        if(0 != tdcc_util_atoi(ent->d_name, &tid)) continue;
        
        if(NULL == (thd = malloc(sizeof(tdcd_thread_info_t)))) return TDC_ERRNO_NOMEM;
        tdcd_thread_init(&(thd->t), self->pid, tid);
        
        TAILQ_INSERT_TAIL(&(self->thds), thd, link);
        self->nthds++;
    }
    closedir(dir);

    return 0;
}

int tdcd_process_create(tdcd_process_t **self, pid_t pid, pid_t crash_tid, siginfo_t *si, ucontext_t *uc)
{
    int                r;
    tdcd_thread_info_t *thd;
    
    if(NULL == (*self = malloc(sizeof(tdcd_process_t)))) return TDC_ERRNO_NOMEM;
    (*self)->pid       = pid;
    (*self)->pname     = NULL;
    (*self)->crash_tid = crash_tid;
    (*self)->si        = si;
    (*self)->uc        = uc;
    (*self)->nthds     = 0;
    TAILQ_INIT(&((*self)->thds));

    if(0 != (r = tdcd_process_load_threads(*self)))
    {
        TDCD_LOG_ERROR("PROCESS: load threads failed, errno=%d", r);
        return r;
    }

    //check if crashed thread existed
    TAILQ_FOREACH(thd, &((*self)->thds), link)
    {
        if(thd->t.tid == (*self)->crash_tid)
            return 0; //OK
    }

    TDCD_LOG_ERROR("PROCESS: crashed thread NOT found");
    return TDC_ERRNO_NOTFND;
}

size_t tdcd_process_get_number_of_threads(tdcd_process_t *self)
{
    return self->nthds;
}

void tdcd_process_suspend_threads(tdcd_process_t *self)
{
    tdcd_thread_info_t *thd;
    TAILQ_FOREACH(thd, &(self->thds), link)
        tdcd_thread_suspend(&(thd->t));
}

void tdcd_process_resume_threads(tdcd_process_t *self)
{
    tdcd_thread_info_t *thd;
    TAILQ_FOREACH(thd, &(self->thds), link)
        tdcd_thread_resume(&(thd->t));
}

int tdcd_process_load_info(tdcd_process_t *self)
{
    int                r;
    tdcd_thread_info_t *thd;
    char               buf[256];
    
    tdcc_util_get_process_name(self->pid, buf, sizeof(buf));
    if(NULL == (self->pname = strdup(buf))) self->pname = "unknown";

    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        //load thread info
        tdcd_thread_load_info(&(thd->t));
        
        //load thread regs
        if(thd->t.tid != self->crash_tid)
            tdcd_thread_load_regs(&(thd->t));
        else
            tdcd_thread_load_regs_from_ucontext(&(thd->t), self->uc);
    }

    //load maps
    if(0 != (r = tdcd_maps_create(&(self->maps), self->pid)))
        TDCD_LOG_ERROR("PROCESS: create maps failed, errno=%d", r);

    return 0;
}

static int tdcd_process_record_signal_info(tdcd_process_t *self, int log_fd)
{
    //fault addr
    char addr_desc[64];
    if(tdcc_util_signal_has_si_addr(self->si))
    {
        void *addr = self->si->si_addr;
        if(self->si->si_signo == SIGILL)
        {
            uint32_t instruction = 0;
            tdcd_util_ptrace_read(self->pid, (uintptr_t)addr, &instruction, sizeof(instruction));
            snprintf(addr_desc, sizeof(addr_desc), "%p (*pc=%#08x)", addr, instruction);
        }
        else
        {
            snprintf(addr_desc, sizeof(addr_desc), "%p", addr);
        }
    }
    else
    {
        snprintf(addr_desc, sizeof(addr_desc), "--------");
    }

    //from
    char sender_desc[64] = "";
    if(tdcc_util_signal_has_sender(self->si, self->pid))
    {
        snprintf(sender_desc, sizeof(sender_desc), " from pid %d, uid %d", self->si->si_pid, self->si->si_uid);
    }

    return tdcc_util_write_format(log_fd, "signal %d (%s), code %d (%s%s), fault addr %s\n",
                                 self->si->si_signo, tdcc_util_get_signame(self->si),
                                 self->si->si_code, tdcc_util_get_sigcodename(self->si),
                                 sender_desc, addr_desc);
}

static int tdcd_process_get_abort_message_29(tdcd_process_t *self, char *buf, size_t buf_len)
{
    //
    // struct abort_msg_t {
    //     size_t size;
    //     char msg[0];
    // };
    //
    // struct magic_abort_msg_t {
    //     uint64_t magic1;
    //     uint64_t magic2;
    //     abort_msg_t msg;
    // };
    //
    // ...
    // size_t size = sizeof(magic_abort_msg_t) + strlen(msg) + 1;
    // ...
    //

    int r;

    //get abort_msg_t *p
    uintptr_t p = tdcd_maps_find_abort_msg(self->maps);
    if(0 == p) return TDC_ERRNO_NOTFND;
    p += (sizeof(uint64_t) * 2);

    //get size
    size_t size = 0;
    if(0 != (r = tdcd_util_ptrace_read_fully(self->pid, p, &size, sizeof(size_t)))) return r;
    if(size < (sizeof(uint64_t) * 2 + sizeof(size_t) + 1 + 1)) return TDC_ERRNO_NOTFND;
    TDCD_LOG_DEBUG("PROCESS: abort_msg, size = %zu", size);

    //get strlen(msg)
    size -= (sizeof(uint64_t) * 2 + sizeof(size_t) + 1);

    //get p->msg
    if(size > buf_len) size = buf_len;
    if(0 != (r = tdcd_util_ptrace_read_fully(self->pid, p + sizeof(size_t), buf, size))) return r;

    return 0;
}

static int tdcd_process_get_abort_message_14(tdcd_process_t *self, char *buf, size_t buf_len)
{
    //
    // struct abort_msg_t {
    //     size_t size;
    //     char msg[0];
    // };
    //
    // abort_msg_t** __abort_message_ptr;
    //
    // ......
    // size_t size = sizeof(abort_msg_t) + strlen(msg) + 1;
    // ......
    //

    int r;

    //get abort_msg_t ***ppp (&__abort_message_ptr)
    uintptr_t ppp = 0;
    ppp = tdcd_maps_find_pc(self->maps, TDCC_UTIL_LIBC, TDCC_UTIL_LIBC_ABORT_MSG_PTR);
    if(0 == ppp) return TDC_ERRNO_NOTFND;
    TDCD_LOG_DEBUG("PROCESS: abort_msg, ppp = %"PRIxPTR, ppp);

    //get abort_msg_t **pp (__abort_message_ptr)
    uintptr_t pp = 0;
    if(0 != (r = tdcd_util_ptrace_read_fully(self->pid, ppp, &pp, sizeof(uintptr_t)))) return r;
    if(0 == pp) return TDC_ERRNO_NOTFND;
    TDCD_LOG_DEBUG("PROCESS: abort_msg, pp = %"PRIxPTR, pp);

    //get abort_msg_t *p (*__abort_message_ptr)
    uintptr_t p = 0;
    if(0 != (r = tdcd_util_ptrace_read_fully(self->pid, pp, &p, sizeof(uintptr_t)))) return r;
    if(0 == p) return TDC_ERRNO_NOTFND;
    TDCD_LOG_DEBUG("PROCESS: abort_msg, p = %"PRIxPTR, p);

    //get p->size
    size_t size = 0;
    if(0 != (r = tdcd_util_ptrace_read_fully(self->pid, p, &size, sizeof(size_t)))) return r;
    if(size < (sizeof(size_t) + 1 + 1)) return TDC_ERRNO_NOTFND;
    TDCD_LOG_DEBUG("PROCESS: abort_msg, size = %zu", size);

    //get strlen(msg)
    size -= (sizeof(size_t) + 1);

    //get p->msg
    if(size > buf_len) size = buf_len;
    if(0 != (r = tdcd_util_ptrace_read_fully(self->pid, p + sizeof(size_t), buf, size))) return r;

    return 0;
}

static int tdcd_process_record_abort_message(tdcd_process_t *self, int log_fd, int api_level)
{
    char msg[256 + 1];
    memset(msg, 0, sizeof(msg));

    if(api_level >= 29)
    {
        if(0 != tdcd_process_get_abort_message_29(self, msg, sizeof(msg) - 1)) return 0;
    }
    else
    {
        if(0 != tdcd_process_get_abort_message_14(self, msg, sizeof(msg) - 1)) return 0;
    }

    //format
    size_t i;
    for(i = 0; i < strlen(msg); i++)
    {
        if(isspace(msg[i]) && ' ' != msg[i])
            msg[i] = ' ';
    }

    //write
    return tdcc_util_write_format(log_fd, "Abort message: '%s'\n", msg);
}

static regex_t *tdcd_process_build_whitelist_regex(char *dump_all_threads_whitelist, size_t *re_cnt)
{
    if(NULL == dump_all_threads_whitelist || 0 == strlen(dump_all_threads_whitelist)) return NULL;

    char *p = dump_all_threads_whitelist;
    size_t cnt = 0;
    while(*p)
    {
        if(*p == '|') cnt++;
        p++;
    }
    cnt += 1;

    regex_t *re = NULL;
    if(NULL == (re = calloc(cnt, sizeof(regex_t)))) return NULL;

    char *tmp;
    char *regex_str = strtok_r(dump_all_threads_whitelist, "|", &tmp);
    char *regex_str_decoded;
    size_t i = 0;
    while(regex_str)
    {
        regex_str_decoded = (char *)tdcc_b64_decode(regex_str, strlen(regex_str), NULL);
        if(0 == regcomp(&(re[i]), regex_str_decoded, REG_EXTENDED | REG_NOSUB))
        {
            TDCD_LOG_DEBUG("PROCESS: compile regex OK: %s", regex_str_decoded);
            i++;
        }
        regex_str = strtok_r(NULL, "|", &tmp);
    }

    if(0 == i)
    {
        free(re);
        return NULL;
    }

    TDCD_LOG_DEBUG("PROCESS: got %zu regex", i);
    *re_cnt = i;
    return re;
}

static int tdcd_process_if_need_dump(char *tname, regex_t *re, size_t re_cnt)
{
    if(NULL == re || 0 == re_cnt) return 1;

    size_t i;
    for(i = 0; i < re_cnt; i++)
        if(0 == regexec(&(re[i]), tname, 0, NULL, 0)) return 1;

    return 0;
}

int tdcd_process_record(tdcd_process_t *self,
                       int log_fd,
                       unsigned int logcat_system_lines,
                       unsigned int logcat_events_lines,
                       unsigned int logcat_main_lines,
                       int dump_elf_hash,
                       int dump_map,
                       int dump_fds,
                       int dump_network_info,
                       int dump_all_threads,
                       unsigned int dump_all_threads_count_max,
                       char *dump_all_threads_whitelist,
                       int api_level)
{
    int                r = 0;
    tdcd_thread_info_t *thd;
    regex_t           *re = NULL;
    size_t             re_cnt = 0;
    unsigned int       thd_dumped = 0;
    int                thd_matched_regex = 0;
    int                thd_ignored_by_limit = 0;
    
    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        if(thd->t.tid == self->crash_tid)
        {
            if(0 != (r = tdcd_thread_record_info(&(thd->t), log_fd, self->pname))) return r;
            if(0 != (r = tdcd_process_record_signal_info(self, log_fd))) return r;
            if(0 != (r = tdcd_process_record_abort_message(self, log_fd, api_level))) return r;
            if(0 != (r = tdcd_thread_record_regs(&(thd->t), log_fd))) return r;
            if(0 == tdcd_thread_load_frames(&(thd->t), self->maps))
            {
                if(0 != (r = tdcd_thread_record_backtrace(&(thd->t), log_fd))) return r;
                if(0 != (r = tdcd_thread_record_buildid(&(thd->t), log_fd, dump_elf_hash, tdcc_util_signal_has_si_addr(self->si) ? (uintptr_t)self->si->si_addr : 0))) return r;
                if(0 != (r = tdcd_thread_record_stack(&(thd->t), log_fd))) return r;
                if(0 != (r = tdcd_thread_record_memory(&(thd->t), log_fd))) return r;
            }
            if(dump_map) if(0 != (r = tdcd_maps_record(self->maps, log_fd))) return r;
            if(0 != (r = tdcc_util_record_logcat(log_fd, self->pid, api_level, logcat_system_lines, logcat_events_lines, logcat_main_lines))) return r;
            if(dump_fds) if(0 != (r = tdcc_util_record_fds(log_fd, self->pid))) return r;
            if(dump_network_info) if(0 != (r = tdcc_util_record_network_info(log_fd, self->pid, api_level))) return r;
            if(0 != (r = tdcc_meminfo_record(log_fd, self->pid))) return r;

            break;
        }
    }
    if(!dump_all_threads) return 0;

    //parse thread name whitelist regex
    re = tdcd_process_build_whitelist_regex(dump_all_threads_whitelist, &re_cnt);

    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        if(thd->t.tid != self->crash_tid)
        {
            //check regex for thread name
            if(NULL != re && re_cnt > 0 && !tdcd_process_if_need_dump(thd->t.tname, re, re_cnt))
            {
                continue;
            }
            thd_matched_regex++;

            //check dump count limit
            if(dump_all_threads_count_max > 0 && thd_dumped >= dump_all_threads_count_max)
            {
                thd_ignored_by_limit++;
                continue;
            }

            if(0 != (r = tdcc_util_write_str(log_fd, TDCC_UTIL_THREAD_SEP))) goto end;
            if(0 != (r = tdcd_thread_record_info(&(thd->t), log_fd, self->pname))) goto end;
            if(0 != (r = tdcd_thread_record_regs(&(thd->t), log_fd))) goto end;
            if(0 == tdcd_thread_load_frames(&(thd->t), self->maps))
            {
                if(0 != (r = tdcd_thread_record_backtrace(&(thd->t), log_fd))) goto end;
                if(0 != (r = tdcd_thread_record_stack(&(thd->t), log_fd))) goto end;
            }
            thd_dumped++;
        }
    }

 end:
    if(self->nthds > 1)
    {
        if(0 == thd_dumped)
            if(0 != (r = tdcc_util_write_str(log_fd, TDCC_UTIL_THREAD_SEP))) goto ret;

        if(0 != (r = tdcc_util_write_format(log_fd, "total threads (exclude the crashed thread): %zu\n", self->nthds - 1))) goto ret;
        if(NULL != re && re_cnt > 0)
            if(0 != (r = tdcc_util_write_format(log_fd, "threads matched whitelist: %d\n", thd_matched_regex))) goto ret;
        if(dump_all_threads_count_max > 0)
            if(0 != (r = tdcc_util_write_format(log_fd, "threads ignored by max count limit: %d\n", thd_ignored_by_limit))) goto ret;
        if(0 != (r = tdcc_util_write_format(log_fd, "dumped threads: %u\n", thd_dumped))) goto ret;
        
        if(0 != (r = tdcc_util_write_str(log_fd, TDCC_UTIL_THREAD_END))) goto ret;
    }
    
 ret:
    return r;
}
