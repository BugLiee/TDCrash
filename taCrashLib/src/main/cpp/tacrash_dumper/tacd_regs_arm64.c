//
// Created by bugliee on 2022/1/11.
//

typedef int make_iso_compilers_happy;

#ifdef __aarch64__

#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_regs.h"
#include "tacd_memory.h"
#include "tacd_util.h"

#define TACD_REGS_X0  0
#define TACD_REGS_X1  1
#define TACD_REGS_X2  2
#define TACD_REGS_X3  3
#define TACD_REGS_X4  4
#define TACD_REGS_X5  5
#define TACD_REGS_X6  6
#define TACD_REGS_X7  7
#define TACD_REGS_X8  8
#define TACD_REGS_X9  9
#define TACD_REGS_X10 10
#define TACD_REGS_X11 11
#define TACD_REGS_X12 12
#define TACD_REGS_X13 13
#define TACD_REGS_X14 14
#define TACD_REGS_X15 15
#define TACD_REGS_X16 16
#define TACD_REGS_X17 17
#define TACD_REGS_X18 18
#define TACD_REGS_X19 19
#define TACD_REGS_X20 20
#define TACD_REGS_X21 21
#define TACD_REGS_X22 22
#define TACD_REGS_X23 23
#define TACD_REGS_X24 24
#define TACD_REGS_X25 25
#define TACD_REGS_X26 26
#define TACD_REGS_X27 27
#define TACD_REGS_X28 28
#define TACD_REGS_X29 29
#define TACD_REGS_LR  30
#define TACD_REGS_SP  31
#define TACD_REGS_PC  32

static tacd_regs_label_t tacd_regs_labels[] =
{
    {TACD_REGS_X0,  "x0"},
    {TACD_REGS_X1,  "x1"},
    {TACD_REGS_X2,  "x2"},
    {TACD_REGS_X3,  "x3"},
    {TACD_REGS_X4,  "x4"},
    {TACD_REGS_X5,  "x5"},
    {TACD_REGS_X6,  "x6"},
    {TACD_REGS_X7,  "x7"},
    {TACD_REGS_X8,  "x8"},
    {TACD_REGS_X9,  "x9"},
    {TACD_REGS_X10, "x10"},
    {TACD_REGS_X11, "x11"},
    {TACD_REGS_X12, "x12"},
    {TACD_REGS_X13, "x13"},
    {TACD_REGS_X14, "x14"},
    {TACD_REGS_X15, "x15"},
    {TACD_REGS_X16, "x16"},
    {TACD_REGS_X17, "x17"},
    {TACD_REGS_X18, "x18"},
    {TACD_REGS_X19, "x19"},
    {TACD_REGS_X20, "x20"},
    {TACD_REGS_X21, "x21"},
    {TACD_REGS_X22, "x22"},
    {TACD_REGS_X23, "x23"},
    {TACD_REGS_X24, "x24"},
    {TACD_REGS_X25, "x25"},
    {TACD_REGS_X26, "x26"},
    {TACD_REGS_X27, "x27"},
    {TACD_REGS_X28, "x28"},
    {TACD_REGS_X29, "x29"},
    {TACD_REGS_SP,  "sp"},
    {TACD_REGS_LR,  "lr"},
    {TACD_REGS_PC,  "pc"}
};

void tacd_regs_get_labels(tacd_regs_label_t **labels, size_t *labels_count)
{
    *labels = tacd_regs_labels;
    *labels_count = sizeof(tacd_regs_labels) / sizeof(tacd_regs_label_t);
}

uintptr_t tacd_regs_get_pc(tacd_regs_t *self)
{
    return self->r[TACD_REGS_PC];
}

void tacd_regs_set_pc(tacd_regs_t *self, uintptr_t pc)
{
    self->r[TACD_REGS_PC] = pc;
}

uintptr_t tacd_regs_get_sp(tacd_regs_t *self)
{
    return self->r[TACD_REGS_SP];
}

void tacd_regs_set_sp(tacd_regs_t *self, uintptr_t sp)
{
    self->r[TACD_REGS_SP] = sp;
}

void tacd_regs_load_from_ucontext(tacd_regs_t *self, ucontext_t *uc)
{
    self->r[TACD_REGS_X0]  = uc->uc_mcontext.regs[0];
    self->r[TACD_REGS_X1]  = uc->uc_mcontext.regs[1];
    self->r[TACD_REGS_X2]  = uc->uc_mcontext.regs[2];
    self->r[TACD_REGS_X3]  = uc->uc_mcontext.regs[3];
    self->r[TACD_REGS_X4]  = uc->uc_mcontext.regs[4];
    self->r[TACD_REGS_X5]  = uc->uc_mcontext.regs[5];
    self->r[TACD_REGS_X6]  = uc->uc_mcontext.regs[6];
    self->r[TACD_REGS_X7]  = uc->uc_mcontext.regs[7];
    self->r[TACD_REGS_X8]  = uc->uc_mcontext.regs[8];
    self->r[TACD_REGS_X9]  = uc->uc_mcontext.regs[9];
    self->r[TACD_REGS_X10] = uc->uc_mcontext.regs[10];
    self->r[TACD_REGS_X11] = uc->uc_mcontext.regs[11];
    self->r[TACD_REGS_X12] = uc->uc_mcontext.regs[12];
    self->r[TACD_REGS_X13] = uc->uc_mcontext.regs[13];
    self->r[TACD_REGS_X14] = uc->uc_mcontext.regs[14];
    self->r[TACD_REGS_X15] = uc->uc_mcontext.regs[15];
    self->r[TACD_REGS_X16] = uc->uc_mcontext.regs[16];
    self->r[TACD_REGS_X17] = uc->uc_mcontext.regs[17];
    self->r[TACD_REGS_X18] = uc->uc_mcontext.regs[18];
    self->r[TACD_REGS_X19] = uc->uc_mcontext.regs[19];
    self->r[TACD_REGS_X20] = uc->uc_mcontext.regs[20];
    self->r[TACD_REGS_X21] = uc->uc_mcontext.regs[21];
    self->r[TACD_REGS_X22] = uc->uc_mcontext.regs[22];
    self->r[TACD_REGS_X23] = uc->uc_mcontext.regs[23];
    self->r[TACD_REGS_X24] = uc->uc_mcontext.regs[24];
    self->r[TACD_REGS_X25] = uc->uc_mcontext.regs[25];
    self->r[TACD_REGS_X26] = uc->uc_mcontext.regs[26];
    self->r[TACD_REGS_X27] = uc->uc_mcontext.regs[27];
    self->r[TACD_REGS_X28] = uc->uc_mcontext.regs[28];
    self->r[TACD_REGS_X29] = uc->uc_mcontext.regs[29];
    self->r[TACD_REGS_LR]  = uc->uc_mcontext.regs[30];
    self->r[TACD_REGS_SP]  = uc->uc_mcontext.sp;
    self->r[TACD_REGS_PC]  = uc->uc_mcontext.pc;
}

void tacd_regs_load_from_ptregs(tacd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    if(regs_len > TACD_REGS_USER_NUM) regs_len = TACD_REGS_USER_NUM;
    memcpy(&(self->r), regs, sizeof(uintptr_t) * regs_len);
}

int tacd_regs_record(tacd_regs_t *self, int log_fd)
{
    return tacc_util_write_format(log_fd,
                                 "    x0  %016lx  x1  %016lx  x2  %016lx  x3  %016lx\n"
                                 "    x4  %016lx  x5  %016lx  x6  %016lx  x7  %016lx\n"
                                 "    x8  %016lx  x9  %016lx  x10 %016lx  x11 %016lx\n"
                                 "    x12 %016lx  x13 %016lx  x14 %016lx  x15 %016lx\n"
                                 "    x16 %016lx  x17 %016lx  x18 %016lx  x19 %016lx\n"
                                 "    x20 %016lx  x21 %016lx  x22 %016lx  x23 %016lx\n"
                                 "    x24 %016lx  x25 %016lx  x26 %016lx  x27 %016lx\n"
                                 "    x28 %016lx  x29 %016lx\n"
                                 "    sp  %016lx  lr  %016lx  pc  %016lx\n\n",
                                 self->r[TACD_REGS_X0],  self->r[TACD_REGS_X1],  self->r[TACD_REGS_X2],  self->r[TACD_REGS_X3],
                                 self->r[TACD_REGS_X4],  self->r[TACD_REGS_X5],  self->r[TACD_REGS_X6],  self->r[TACD_REGS_X7],
                                 self->r[TACD_REGS_X8],  self->r[TACD_REGS_X9],  self->r[TACD_REGS_X10], self->r[TACD_REGS_X11],
                                 self->r[TACD_REGS_X12], self->r[TACD_REGS_X13], self->r[TACD_REGS_X14], self->r[TACD_REGS_X15],
                                 self->r[TACD_REGS_X16], self->r[TACD_REGS_X17], self->r[TACD_REGS_X18], self->r[TACD_REGS_X19],
                                 self->r[TACD_REGS_X20], self->r[TACD_REGS_X21], self->r[TACD_REGS_X22], self->r[TACD_REGS_X23],
                                 self->r[TACD_REGS_X24], self->r[TACD_REGS_X25], self->r[TACD_REGS_X26], self->r[TACD_REGS_X27],
                                 self->r[TACD_REGS_X28], self->r[TACD_REGS_X29],
                                 self->r[TACD_REGS_SP],  self->r[TACD_REGS_LR],  self->r[TACD_REGS_PC]);
}

int tacd_regs_try_step_sigreturn(tacd_regs_t *self, uintptr_t rel_pc, tacd_memory_t *memory, pid_t pid)
{
    uint64_t  data;
    uintptr_t offset;

    if(0 != tacd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return TAC_ERRNO_MEM;

    // Look for the kernel sigreturn function.
    // __kernel_rt_sigreturn:
    // 0xd2801168     mov x8, #0x8b
    // 0xd4000001     svc #0x0
    if (0xd4000001d2801168ULL != data) return TAC_ERRNO_NOTFND;

    // SP + sizeof(siginfo_t) + uc_mcontext offset + X0 offset.
    offset = self->r[TACD_REGS_SP] + 0x80 + 0xb0 + 0x08;
    if(0 != tacd_util_ptrace_read_fully(pid, offset, self, sizeof(uint64_t) * TACD_REGS_MACHINE_NUM)) return TAC_ERRNO_MEM;
    
    return 0;
}

uintptr_t tacd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, tacd_memory_t *memory)
{
    (void)load_bias;
    (void)memory;

    return rel_pc < 4 ? 0 : 4;
}

int tacd_regs_set_pc_from_lr(tacd_regs_t *self, pid_t pid)
{
    (void)pid;

    if(self->r[TACD_REGS_PC] == self->r[TACD_REGS_LR])
        return TAC_ERRNO_INVAL;

    self->r[TACD_REGS_PC] = self->r[TACD_REGS_LR];
    return 0;
}

#endif
