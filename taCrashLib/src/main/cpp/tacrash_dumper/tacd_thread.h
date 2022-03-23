//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_THREAD_H
#define TACD_THREAD_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_regs.h"
#include "tacd_frames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TACD_THREAD_STATUS_OK = 0,
    TACD_THREAD_STATUS_UNKNOWN,
    TACD_THREAD_STATUS_REGS,
    TACD_THREAD_STATUS_ATTACH,
    TACD_THREAD_STATUS_ATTACH_WAIT
} tacd_thread_status_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tacd_thread
{
    tacd_thread_status_t  status;
    pid_t                pid;
    pid_t                tid;
    char                *tname;
    tacd_regs_t           regs;
    tacd_frames_t        *frames;
} tacd_thread_t;
#pragma clang diagnostic pop

void tacd_thread_init(tacd_thread_t *self, pid_t pid, pid_t tid);

void tacd_thread_suspend(tacd_thread_t *self);
void tacd_thread_resume(tacd_thread_t *self);

void tacd_thread_load_info(tacd_thread_t *self);
void tacd_thread_load_regs(tacd_thread_t *self);
void tacd_thread_load_regs_from_ucontext(tacd_thread_t *self, ucontext_t *uc);
int tacd_thread_load_frames(tacd_thread_t *self, tacd_maps_t *maps);

int tacd_thread_record_info(tacd_thread_t *self, int log_fd, const char *pname);
int tacd_thread_record_regs(tacd_thread_t *self, int log_fd);
int tacd_thread_record_backtrace(tacd_thread_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
