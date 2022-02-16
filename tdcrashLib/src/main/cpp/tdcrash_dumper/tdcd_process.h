//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_THREADS_H
#define TDCD_THREADS_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_process tdcd_process_t;

int tdcd_process_create(tdcd_process_t **self, pid_t pid, pid_t crash_tid, siginfo_t *si, ucontext_t *uc);
size_t tdcd_process_get_number_of_threads(tdcd_process_t *self);

void tdcd_process_suspend_threads(tdcd_process_t *self);
void tdcd_process_resume_threads(tdcd_process_t *self);

int tdcd_process_load_info(tdcd_process_t *self);

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
                       int api_level);

#ifdef __cplusplus
}
#endif

#endif
