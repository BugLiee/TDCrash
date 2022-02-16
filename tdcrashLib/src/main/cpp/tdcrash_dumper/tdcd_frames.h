//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_FRAMES_H
#define TDCD_FRAMES_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_regs.h"
#include "tdcd_maps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_frames tdcd_frames_t;

int tdcd_frames_create(tdcd_frames_t **self, tdcd_regs_t *regs, tdcd_maps_t *maps, pid_t pid);
void tdcd_frames_destroy(tdcd_frames_t **self);

int tdcd_frames_record_backtrace(tdcd_frames_t *self, int log_fd);
int tdcd_frames_record_buildid(tdcd_frames_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr);
int tdcd_frames_record_stack(tdcd_frames_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
