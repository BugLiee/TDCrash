//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_THREADS_H
#define TACD_THREADS_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_process tacd_process_t;

int tacd_process_create(tacd_process_t **self, pid_t pid, pid_t crash_tid, siginfo_t *si, ucontext_t *uc);
size_t tacd_process_get_number_of_threads(tacd_process_t *self);

void tacd_process_suspend_threads(tacd_process_t *self);
void tacd_process_resume_threads(tacd_process_t *self);

int tacd_process_load_info(tacd_process_t *self);

int tacd_process_record(tacd_process_t *self, int log_fd, int api_level);

#ifdef __cplusplus
}
#endif

#endif
