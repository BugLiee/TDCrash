//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_REGS_H
#define TACD_REGS_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include "tacd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__arm__)
#define TACD_REGS_USER_NUM    18
#define TACD_REGS_MACHINE_NUM 16
#elif defined(__aarch64__)
#define TACD_REGS_USER_NUM    34
#define TACD_REGS_MACHINE_NUM 33
#elif defined(__i386__)
#define TACD_REGS_USER_NUM    17
#define TACD_REGS_MACHINE_NUM 16
#elif defined(__x86_64__)
#define TACD_REGS_USER_NUM    27
#define TACD_REGS_MACHINE_NUM 17
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
    uintptr_t r[TACD_REGS_USER_NUM];
} tacd_regs_t;
#pragma clang diagnostic pop

uintptr_t tacd_regs_get_pc(tacd_regs_t *self);
void tacd_regs_set_pc(tacd_regs_t *self, uintptr_t pc);

uintptr_t tacd_regs_get_sp(tacd_regs_t *self);
void tacd_regs_set_sp(tacd_regs_t *self, uintptr_t sp);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
    uint8_t     idx;
    const char *name;
} tacd_regs_label_t;
#pragma clang diagnostic pop

void tacd_regs_get_labels(tacd_regs_label_t **labels, size_t *labels_count);

void tacd_regs_load_from_ucontext(tacd_regs_t *self, ucontext_t *uc);
void tacd_regs_load_from_ptregs(tacd_regs_t *self, uintptr_t *regs, size_t regs_len);

int tacd_regs_record(tacd_regs_t *self, int log_fd);

int tacd_regs_try_step_sigreturn(tacd_regs_t *self, uintptr_t rel_pc, tacd_memory_t *memory, pid_t pid);

uintptr_t tacd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, tacd_memory_t *memory);

int tacd_regs_set_pc_from_lr(tacd_regs_t *self, pid_t pid);

#ifdef __cplusplus
}
#endif

#endif
