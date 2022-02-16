//
// Created by bugliee on 2022/1/11.
//

#ifdef __arm__

#ifndef TDCD_ARM_EXIDX_H
#define TDCD_ARM_EXIDX_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_regs.h"
#include "tdcd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

int tdcd_arm_exidx_step(tdcd_regs_t *regs, tdcd_memory_t *memory, pid_t pid,
                       size_t exidx_offset, size_t exidx_size, uintptr_t load_bias,
                       uintptr_t pc, int *finished);

#ifdef __cplusplus
}
#endif

#endif

#endif
