//
// Created by bugliee on 2022/1/11.
//

typedef int make_iso_compilers_happy;

#ifdef __x86_64__

#include <stdio.h>
#include <ucontext.h>
#include <sys/ptrace.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_regs.h"
#include "tacd_memory.h"
#include "tacd_util.h"

#define TACD_REGS_RAX 0
#define TACD_REGS_RDX 1
#define TACD_REGS_RCX 2
#define TACD_REGS_RBX 3
#define TACD_REGS_RSI 4
#define TACD_REGS_RDI 5
#define TACD_REGS_RBP 6
#define TACD_REGS_RSP 7
#define TACD_REGS_R8  8
#define TACD_REGS_R9  9
#define TACD_REGS_R10 10
#define TACD_REGS_R11 11
#define TACD_REGS_R12 12
#define TACD_REGS_R13 13
#define TACD_REGS_R14 14
#define TACD_REGS_R15 15
#define TACD_REGS_RIP 16

#define TACD_REGS_SP  TACD_REGS_RSP
#define TACD_REGS_PC  TACD_REGS_RIP

static tacd_regs_label_t tacd_regs_labels[] =
{
    {TACD_REGS_RAX, "rax"},
    {TACD_REGS_RBX, "rbx"},
    {TACD_REGS_RCX, "rcx"},
    {TACD_REGS_RDX, "rdx"},
    {TACD_REGS_R8,  "r8"},
    {TACD_REGS_R9,  "r9"},
    {TACD_REGS_R10, "r10"},
    {TACD_REGS_R11, "r11"},
    {TACD_REGS_R12, "r12"},
    {TACD_REGS_R13, "r13"},
    {TACD_REGS_R14, "r14"},
    {TACD_REGS_R15, "r15"},
    {TACD_REGS_RDI, "rdi"},
    {TACD_REGS_RSI, "rsi"},
    {TACD_REGS_RBP, "rbp"},
    {TACD_REGS_RSP, "rsp"},
    {TACD_REGS_RIP, "rip"}
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
    self->r[TACD_REGS_RAX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RAX];
    self->r[TACD_REGS_RBX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RBX];
    self->r[TACD_REGS_RCX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RCX];
    self->r[TACD_REGS_RDX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RDX];
    self->r[TACD_REGS_R8]  = (uintptr_t)uc->uc_mcontext.gregs[REG_R8];
    self->r[TACD_REGS_R9]  = (uintptr_t)uc->uc_mcontext.gregs[REG_R9];
    self->r[TACD_REGS_R10] = (uintptr_t)uc->uc_mcontext.gregs[REG_R10];
    self->r[TACD_REGS_R11] = (uintptr_t)uc->uc_mcontext.gregs[REG_R11];
    self->r[TACD_REGS_R12] = (uintptr_t)uc->uc_mcontext.gregs[REG_R12];
    self->r[TACD_REGS_R13] = (uintptr_t)uc->uc_mcontext.gregs[REG_R13];
    self->r[TACD_REGS_R14] = (uintptr_t)uc->uc_mcontext.gregs[REG_R14];
    self->r[TACD_REGS_R15] = (uintptr_t)uc->uc_mcontext.gregs[REG_R15];
    self->r[TACD_REGS_RDI] = (uintptr_t)uc->uc_mcontext.gregs[REG_RDI];
    self->r[TACD_REGS_RSI] = (uintptr_t)uc->uc_mcontext.gregs[REG_RSI];
    self->r[TACD_REGS_RBP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
    self->r[TACD_REGS_RSP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    self->r[TACD_REGS_RIP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
}

void tacd_regs_load_from_ptregs(tacd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    (void)regs_len;
    
    struct pt_regs *ptregs = (struct pt_regs *)regs;

    self->r[TACD_REGS_RAX] = ptregs->rax;
    self->r[TACD_REGS_RBX] = ptregs->rbx;
    self->r[TACD_REGS_RCX] = ptregs->rcx;
    self->r[TACD_REGS_RDX] = ptregs->rdx;
    self->r[TACD_REGS_R8]  = ptregs->r8;
    self->r[TACD_REGS_R9]  = ptregs->r9;
    self->r[TACD_REGS_R10] = ptregs->r10;
    self->r[TACD_REGS_R11] = ptregs->r11;
    self->r[TACD_REGS_R12] = ptregs->r12;
    self->r[TACD_REGS_R13] = ptregs->r13;
    self->r[TACD_REGS_R14] = ptregs->r14;
    self->r[TACD_REGS_R15] = ptregs->r15;
    self->r[TACD_REGS_RDI] = ptregs->rdi;
    self->r[TACD_REGS_RSI] = ptregs->rsi;
    self->r[TACD_REGS_RBP] = ptregs->rbp;
    self->r[TACD_REGS_RSP] = ptregs->rsp;
    self->r[TACD_REGS_RIP] = ptregs->rip;
}

int tacd_regs_record(tacd_regs_t *self, int log_fd)
{
    return tacc_util_write_format(log_fd,
                                 "    rax %016lx  rbx %016lx  rcx %016lx  rdx %016lx\n"
                                 "    r8  %016lx  r9  %016lx  r10 %016lx  r11 %016lx\n"
                                 "    r12 %016lx  r13 %016lx  r14 %016lx  r15 %016lx\n"
                                 "    rdi %016lx  rsi %016lx\n"
                                 "    rbp %016lx  rsp %016lx  rip %016lx\n\n",
                                 self->r[TACD_REGS_RAX], self->r[TACD_REGS_RBX], self->r[TACD_REGS_RCX], self->r[TACD_REGS_RDX],
                                 self->r[TACD_REGS_R8],  self->r[TACD_REGS_R9],  self->r[TACD_REGS_R10], self->r[TACD_REGS_R11],
                                 self->r[TACD_REGS_R12], self->r[TACD_REGS_R13], self->r[TACD_REGS_R14], self->r[TACD_REGS_R15],
                                 self->r[TACD_REGS_RDI], self->r[TACD_REGS_RSI],
                                 self->r[TACD_REGS_RBP], self->r[TACD_REGS_RSP], self->r[TACD_REGS_RIP]);
}

int tacd_regs_try_step_sigreturn(tacd_regs_t *self, uintptr_t rel_pc, tacd_memory_t *memory, pid_t pid)
{
    uint64_t data = 0;
    if(0 != tacd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return TAC_ERRNO_MEM;
    if(0x0f0000000fc0c748 != data) return TAC_ERRNO_NOTFND;

    uint16_t data2 = 0;
    if(0 != tacd_memory_read_fully(memory, rel_pc + 8, &data2, sizeof(data2))) return TAC_ERRNO_MEM;
    if(0x0f05 != data2) return TAC_ERRNO_NOTFND;
    
    // __restore_rt:
    // 0x48 0xc7 0xc0 0x0f 0x00 0x00 0x00   mov $0xf,%rax
    // 0x0f 0x05                            syscall
    // 0x0f                                 nopl 0x0($rax)

    // Read the mcontext data from the stack.
    // sp points to the ucontext data structure, read only the mcontext part.
    ucontext_t uc;
    if(0 != tacd_util_ptrace_read_fully(pid, tacd_regs_get_sp(self) + 0x28, &(uc.uc_mcontext), sizeof(uc.uc_mcontext))) return TAC_ERRNO_MEM;
    tacd_regs_load_from_ucontext(self, &uc);
    
    return 0;
}

uintptr_t tacd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, tacd_memory_t *memory)
{    
    (void)load_bias;
    (void)memory;

    return 0 == rel_pc ? 0 : 1;
}

int tacd_regs_set_pc_from_lr(tacd_regs_t *self, pid_t pid)
{
    // Attempt to get the return address from the top of the stack.
    uintptr_t new_pc;
    if(0 != tacd_util_ptrace_read_fully(pid, tacd_regs_get_sp(self), &new_pc, sizeof(new_pc))) return TAC_ERRNO_MEM;
    
    if(new_pc == tacd_regs_get_pc(self)) return TAC_ERRNO_INVAL;
    
    tacd_regs_set_pc(self, new_pc);
    
    return 0;
}

#endif
