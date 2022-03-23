//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_FRAMES_H
#define TACD_FRAMES_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_regs.h"
#include "tacd_maps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_frames tacd_frames_t;

int tacd_frames_create(tacd_frames_t **self, tacd_regs_t *regs, tacd_maps_t *maps, pid_t pid);

int tacd_frames_record_backtrace(tacd_frames_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif
