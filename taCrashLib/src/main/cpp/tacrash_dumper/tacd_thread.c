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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_thread.h"
#include "tacd_frames.h"
#include "tacd_regs.h"
#include "tacd_util.h"
#include "tacd_log.h"

void tacd_thread_init(tacd_thread_t *self, pid_t pid, pid_t tid)
{
    self->status = TACD_THREAD_STATUS_OK;
    self->pid    = pid;
    self->tid    = tid;
    self->tname  = NULL;
    self->frames = NULL;
    memset(&(self->regs), 0, sizeof(self->regs));
}

void tacd_thread_suspend(tacd_thread_t *self)
{
    if(0 != ptrace(PTRACE_ATTACH, self->tid, NULL, NULL))
    {
#if TACD_THREAD_DEBUG
        TACD_LOG_WARN("THREAD: ptrace ATTACH failed, errno=%d", errno);
#endif
        self->status = TACD_THREAD_STATUS_ATTACH;
        return;
    }

    errno = 0;
    while(waitpid(self->tid, NULL, __WALL) < 0)
    {
        if(EINTR != errno)
        {
            ptrace(PTRACE_DETACH, self->tid, NULL, NULL);
#if TACD_THREAD_DEBUG
            TACD_LOG_ERROR("THREAD: waitpid for ptrace ATTACH failed, errno=%d", errno);
#endif
            self->status = TACD_THREAD_STATUS_ATTACH_WAIT;
            return;
        }
        errno = 0;
    }
}

void tacd_thread_resume(tacd_thread_t *self)
{
    ptrace(PTRACE_DETACH, self->tid, NULL, NULL);
}

void tacd_thread_load_info(tacd_thread_t *self)
{
    char buf[64] = "\0";
    
    tacc_util_get_thread_name(self->tid, buf, sizeof(buf));
    if(NULL == (self->tname = strdup(buf))) self->tname = "unknown";
}

void tacd_thread_load_regs(tacd_thread_t *self)
{
    uintptr_t regs[64]; //big enough for all architectures
    size_t    regs_len;
    
#ifdef PTRACE_GETREGS
    if(0 != ptrace(PTRACE_GETREGS, self->tid, NULL, &regs))
    {
        TACD_LOG_ERROR("THREAD: ptrace GETREGS failed, errno=%d", errno);
        self->status = TACD_THREAD_STATUS_REGS;
        return;
    }
    regs_len = TACD_REGS_USER_NUM;
#else
    struct iovec iovec;
    iovec.iov_base = &regs;
    iovec.iov_len = sizeof(regs);
    if(0 != ptrace(PTRACE_GETREGSET, self->tid, (void *)NT_PRSTATUS, &iovec))
    {
        TACD_LOG_ERROR("THREAD: ptrace GETREGSET failed, errno=%d", errno);
        self->status = TACD_THREAD_STATUS_REGS;
        return;
    }
    regs_len = iovec.iov_len / sizeof(uintptr_t);
#endif
    tacd_regs_load_from_ptregs(&(self->regs), regs, regs_len);
}

void tacd_thread_load_regs_from_ucontext(tacd_thread_t *self, ucontext_t *uc)
{
    tacd_regs_load_from_ucontext(&(self->regs), uc);
}

int tacd_thread_load_frames(tacd_thread_t *self, tacd_maps_t *maps)
{
#if TACD_THREAD_DEBUG
    TACD_LOG_DEBUG("THREAD: load frames, tid=%d, tname=%s", self->tid, self->tname);
#endif

    if(TACD_THREAD_STATUS_OK != self->status) return TAC_ERRNO_STATE; //do NOT ignore

    return tacd_frames_create(&(self->frames), &(self->regs), maps, self->pid);
}

int tacd_thread_record_info(tacd_thread_t *self, int log_fd, const char *pname)
{
    return tacc_util_write_format(log_fd, "pid: %d, tid: %d, name: %s  >>> %s <<<\n",
                                 self->pid, self->tid, self->tname, pname);
}

int tacd_thread_record_regs(tacd_thread_t *self, int log_fd)
{
    if(TACD_THREAD_STATUS_OK != self->status) return 0; //ignore
    
    return tacd_regs_record(&(self->regs), log_fd);
}

int tacd_thread_record_backtrace(tacd_thread_t *self, int log_fd)
{
    if(TACD_THREAD_STATUS_OK != self->status) return 0; //ignore

    return tacd_frames_record_backtrace(self->frames, log_fd);
}
