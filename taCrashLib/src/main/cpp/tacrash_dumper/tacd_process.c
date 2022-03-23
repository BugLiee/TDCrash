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
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacc_b64.h"
#include "tacd_log.h"
#include "tacd_process.h"
#include "tacd_thread.h"
#include "tacd_maps.h"
#include "tacd_regs.h"
#include "tacd_util.h"
#include "tacd_sys.h"

typedef struct tacd_thread_info
{
    tacd_thread_t t;
    TAILQ_ENTRY(tacd_thread_info,) link;
} tacd_thread_info_t;
typedef TAILQ_HEAD(tacd_thread_info_queue, tacd_thread_info,) tacd_thread_info_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tacd_process
{
    pid_t                    pid;
    char                    *pname;
    pid_t                    crash_tid;
    ucontext_t              *uc;
    siginfo_t               *si;
    tacd_thread_info_queue_t  thds;
    size_t                   nthds;
    tacd_maps_t              *maps;
};
#pragma clang diagnostic pop

static int tacd_process_load_threads(tacd_process_t *self)
{
    char               buf[128];
    DIR               *dir;
    struct dirent     *ent;
    pid_t              tid;
    tacd_thread_info_t *thd;

    snprintf(buf, sizeof(buf), "/proc/%d/task", self->pid);
    if(NULL == (dir = opendir(buf))) return TAC_ERRNO_SYS;
    while(NULL != (ent = readdir(dir)))
    {
        if(0 == strcmp(ent->d_name, ".")) continue;
        if(0 == strcmp(ent->d_name, "..")) continue;
        if(0 != tacc_util_atoi(ent->d_name, &tid)) continue;
        
        if(NULL == (thd = malloc(sizeof(tacd_thread_info_t)))) return TAC_ERRNO_NOMEM;
        tacd_thread_init(&(thd->t), self->pid, tid);
        
        TAILQ_INSERT_TAIL(&(self->thds), thd, link);
        self->nthds++;
    }
    closedir(dir);

    return 0;
}

int tacd_process_create(tacd_process_t **self, pid_t pid, pid_t crash_tid, siginfo_t *si, ucontext_t *uc)
{
    int                r;
    tacd_thread_info_t *thd;
    
    if(NULL == (*self = malloc(sizeof(tacd_process_t)))) return TAC_ERRNO_NOMEM;
    (*self)->pid       = pid;
    (*self)->pname     = NULL;
    (*self)->crash_tid = crash_tid;
    (*self)->si        = si;
    (*self)->uc        = uc;
    (*self)->nthds     = 0;
    TAILQ_INIT(&((*self)->thds));

    if(0 != (r = tacd_process_load_threads(*self)))
    {
        TACD_LOG_ERROR("PROCESS: load threads failed, errno=%d", r);
        return r;
    }

    //check if crashed thread existed
    TAILQ_FOREACH(thd, &((*self)->thds), link)
    {
        if(thd->t.tid == (*self)->crash_tid)
            return 0; //OK
    }

    TACD_LOG_ERROR("PROCESS: crashed thread NOT found");
    return TAC_ERRNO_NOTFND;
}

size_t tacd_process_get_number_of_threads(tacd_process_t *self)
{
    return self->nthds;
}

void tacd_process_suspend_threads(tacd_process_t *self)
{
    tacd_thread_info_t *thd;
    TAILQ_FOREACH(thd, &(self->thds), link)
        tacd_thread_suspend(&(thd->t));
}

void tacd_process_resume_threads(tacd_process_t *self)
{
    tacd_thread_info_t *thd;
    TAILQ_FOREACH(thd, &(self->thds), link)
        tacd_thread_resume(&(thd->t));
}

int tacd_process_load_info(tacd_process_t *self)
{
    int                r;
    tacd_thread_info_t *thd;
    char               buf[256];
    
    tacc_util_get_process_name(self->pid, buf, sizeof(buf));
    if(NULL == (self->pname = strdup(buf))) self->pname = "unknown";

    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        //load thread info
        tacd_thread_load_info(&(thd->t));
        
        //load thread regs
        if(thd->t.tid != self->crash_tid)
            tacd_thread_load_regs(&(thd->t));
        else
            tacd_thread_load_regs_from_ucontext(&(thd->t), self->uc);
    }

    //load maps
    if(0 != (r = tacd_maps_create(&(self->maps), self->pid)))
        TACD_LOG_ERROR("PROCESS: create maps failed, errno=%d", r);

    return 0;
}

static int tacd_process_record_signal_info(tacd_process_t *self, int log_fd)
{
    //fault addr
    char addr_desc[64];
    if(tacc_util_signal_has_si_addr(self->si))
    {
        void *addr = self->si->si_addr;
        if(self->si->si_signo == SIGILL)
        {
            uint32_t instruction = 0;
            tacd_util_ptrace_read(self->pid, (uintptr_t)addr, &instruction, sizeof(instruction));
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
    if(tacc_util_signal_has_sender(self->si, self->pid))
    {
        snprintf(sender_desc, sizeof(sender_desc), " from pid %d, uid %d", self->si->si_pid, self->si->si_uid);
    }

    return tacc_util_write_format(log_fd, "signal %d (%s), code %d (%s%s), fault addr %s\n",
                                 self->si->si_signo, tacc_util_get_signame(self->si),
                                 self->si->si_code, tacc_util_get_sigcodename(self->si),
                                 sender_desc, addr_desc);
}

static int tacd_process_get_abort_message_29(tacd_process_t *self, char *buf, size_t buf_len)
{
    int r;

    //get abort_msg_t *p
    uintptr_t p = tacd_maps_find_abort_msg(self->maps);
    if(0 == p) return TAC_ERRNO_NOTFND;
    p += (sizeof(uint64_t) * 2);

    //get size
    size_t size = 0;
    if(0 != (r = tacd_util_ptrace_read_fully(self->pid, p, &size, sizeof(size_t)))) return r;
    if(size < (sizeof(uint64_t) * 2 + sizeof(size_t) + 1 + 1)) return TAC_ERRNO_NOTFND;
    TACD_LOG_DEBUG("PROCESS: abort_msg, size = %zu", size);

    //get strlen(msg)
    size -= (sizeof(uint64_t) * 2 + sizeof(size_t) + 1);

    //get p->msg
    if(size > buf_len) size = buf_len;
    if(0 != (r = tacd_util_ptrace_read_fully(self->pid, p + sizeof(size_t), buf, size))) return r;

    return 0;
}

static int tacd_process_get_abort_message_14(tacd_process_t *self, char *buf, size_t buf_len)
{
    int r;

    //get abort_msg_t ***ppp (&__abort_message_ptr)
    uintptr_t ppp = 0;
    ppp = tacd_maps_find_pc(self->maps, TACC_UTIL_LIBC, TACC_UTIL_LIBC_ABORT_MSG_PTR);
    if(0 == ppp) return TAC_ERRNO_NOTFND;
    TACD_LOG_DEBUG("PROCESS: abort_msg, ppp = %"PRIxPTR, ppp);

    //get abort_msg_t **pp (__abort_message_ptr)
    uintptr_t pp = 0;
    if(0 != (r = tacd_util_ptrace_read_fully(self->pid, ppp, &pp, sizeof(uintptr_t)))) return r;
    if(0 == pp) return TAC_ERRNO_NOTFND;
    TACD_LOG_DEBUG("PROCESS: abort_msg, pp = %"PRIxPTR, pp);

    //get abort_msg_t *p (*__abort_message_ptr)
    uintptr_t p = 0;
    if(0 != (r = tacd_util_ptrace_read_fully(self->pid, pp, &p, sizeof(uintptr_t)))) return r;
    if(0 == p) return TAC_ERRNO_NOTFND;
    TACD_LOG_DEBUG("PROCESS: abort_msg, p = %"PRIxPTR, p);

    //get p->size
    size_t size = 0;
    if(0 != (r = tacd_util_ptrace_read_fully(self->pid, p, &size, sizeof(size_t)))) return r;
    if(size < (sizeof(size_t) + 1 + 1)) return TAC_ERRNO_NOTFND;
    TACD_LOG_DEBUG("PROCESS: abort_msg, size = %zu", size);

    //get strlen(msg)
    size -= (sizeof(size_t) + 1);

    //get p->msg
    if(size > buf_len) size = buf_len;
    if(0 != (r = tacd_util_ptrace_read_fully(self->pid, p + sizeof(size_t), buf, size))) return r;

    return 0;
}

static int tacd_process_record_abort_message(tacd_process_t *self, int log_fd, int api_level)
{
    char msg[256 + 1];
    memset(msg, 0, sizeof(msg));

    if(api_level >= 29)
    {
        if(0 != tacd_process_get_abort_message_29(self, msg, sizeof(msg) - 1)) return 0;
    }
    else
    {
        if(0 != tacd_process_get_abort_message_14(self, msg, sizeof(msg) - 1)) return 0;
    }

    //format
    size_t i;
    for(i = 0; i < strlen(msg); i++)
    {
        if(isspace(msg[i]) && ' ' != msg[i])
            msg[i] = ' ';
    }

    //write
    return tacc_util_write_format(log_fd, "Abort message: '%s'\n", msg);
}

int tacd_process_record(tacd_process_t *self,int log_fd,int api_level)
{
    int                r = 0;
    tacd_thread_info_t *thd;

    TAILQ_FOREACH(thd, &(self->thds), link)
    {
        if(thd->t.tid == self->crash_tid)
        {
            if(0 != (r = tacd_thread_record_info(&(thd->t), log_fd, self->pname))) return r;
            if(0 != (r = tacd_process_record_signal_info(self, log_fd))) return r;
            if(0 != (r = tacd_process_record_abort_message(self, log_fd, api_level))) return r;
            if(0 != (r = tacd_thread_record_regs(&(thd->t), log_fd))) return r;
            if(0 == tacd_thread_load_frames(&(thd->t), self->maps))
            {
                if(0 != (r = tacd_thread_record_backtrace(&(thd->t), log_fd))) return r;
            }
            break;
        }
    }

    if(0 != (r = tacc_util_write_str(log_fd, TACC_UTIL_THREAD_END))) goto ret;

 ret:
    return r;
}
