//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_THREAD_H
#define TDCD_THREAD_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_regs.h"
#include "tdcd_frames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TDCD_THREAD_STATUS_OK = 0,
    TDCD_THREAD_STATUS_UNKNOWN,
    TDCD_THREAD_STATUS_REGS,
    TDCD_THREAD_STATUS_ATTACH,
    TDCD_THREAD_STATUS_ATTACH_WAIT
} tdcd_thread_status_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tdcd_thread
{
    tdcd_thread_status_t  status;
    pid_t                pid;
    pid_t                tid;
    char                *tname;
    tdcd_regs_t           regs;
    tdcd_frames_t        *frames;
} tdcd_thread_t;
#pragma clang diagnostic pop

void tdcd_thread_init(tdcd_thread_t *self, pid_t pid, pid_t tid);

void tdcd_thread_suspend(tdcd_thread_t *self);
void tdcd_thread_resume(tdcd_thread_t *self);

void tdcd_thread_load_info(tdcd_thread_t *self);
void tdcd_thread_load_regs(tdcd_thread_t *self);
void tdcd_thread_load_regs_from_ucontext(tdcd_thread_t *self, ucontext_t *uc);
int tdcd_thread_load_frames(tdcd_thread_t *self, tdcd_maps_t *maps);

int tdcd_thread_record_info(tdcd_thread_t *self, int log_fd, const char *pname);
int tdcd_thread_record_regs(tdcd_thread_t *self, int log_fd);
int tdcd_thread_record_backtrace(tdcd_thread_t *self, int log_fd);
int tdcd_thread_record_buildid(tdcd_thread_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr);
int tdcd_thread_record_stack(tdcd_thread_t *self, int log_fd);
int tdcd_thread_record_memory(tdcd_thread_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
