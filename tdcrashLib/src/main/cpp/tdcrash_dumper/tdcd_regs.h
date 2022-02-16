//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_REGS_H
#define TDCD_REGS_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include "tdcd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__arm__)
#define TDCD_REGS_USER_NUM    18
#define TDCD_REGS_MACHINE_NUM 16
#elif defined(__aarch64__)
#define TDCD_REGS_USER_NUM    34
#define TDCD_REGS_MACHINE_NUM 33
#elif defined(__i386__)
#define TDCD_REGS_USER_NUM    17
#define TDCD_REGS_MACHINE_NUM 16
#elif defined(__x86_64__)
#define TDCD_REGS_USER_NUM    27
#define TDCD_REGS_MACHINE_NUM 17
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
    uintptr_t r[TDCD_REGS_USER_NUM];
} tdcd_regs_t;
#pragma clang diagnostic pop

uintptr_t tdcd_regs_get_pc(tdcd_regs_t *self);
void tdcd_regs_set_pc(tdcd_regs_t *self, uintptr_t pc);

uintptr_t tdcd_regs_get_sp(tdcd_regs_t *self);
void tdcd_regs_set_sp(tdcd_regs_t *self, uintptr_t sp);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
    uint8_t     idx;
    const char *name;
} tdcd_regs_label_t;
#pragma clang diagnostic pop

void tdcd_regs_get_labels(tdcd_regs_label_t **labels, size_t *labels_count);

void tdcd_regs_load_from_ucontext(tdcd_regs_t *self, ucontext_t *uc);
void tdcd_regs_load_from_ptregs(tdcd_regs_t *self, uintptr_t *regs, size_t regs_len);

int tdcd_regs_record(tdcd_regs_t *self, int log_fd);

int tdcd_regs_try_step_sigreturn(tdcd_regs_t *self, uintptr_t rel_pc, tdcd_memory_t *memory, pid_t pid);

uintptr_t tdcd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, tdcd_memory_t *memory);

int tdcd_regs_set_pc_from_lr(tdcd_regs_t *self, pid_t pid);

#ifdef __cplusplus
}
#endif

#endif
