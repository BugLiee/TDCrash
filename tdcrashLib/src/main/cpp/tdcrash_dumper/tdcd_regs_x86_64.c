//
// Created by bugliee on 2022/1/11.
//

typedef int make_iso_compilers_happy;

#ifdef __x86_64__

#include <stdio.h>
#include <ucontext.h>
#include <sys/ptrace.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_regs.h"
#include "tdcd_memory.h"
#include "tdcd_util.h"

#define TDCD_REGS_RAX 0
#define TDCD_REGS_RDX 1
#define TDCD_REGS_RCX 2
#define TDCD_REGS_RBX 3
#define TDCD_REGS_RSI 4
#define TDCD_REGS_RDI 5
#define TDCD_REGS_RBP 6
#define TDCD_REGS_RSP 7
#define TDCD_REGS_R8  8
#define TDCD_REGS_R9  9
#define TDCD_REGS_R10 10
#define TDCD_REGS_R11 11
#define TDCD_REGS_R12 12
#define TDCD_REGS_R13 13
#define TDCD_REGS_R14 14
#define TDCD_REGS_R15 15
#define TDCD_REGS_RIP 16

#define TDCD_REGS_SP  TDCD_REGS_RSP
#define TDCD_REGS_PC  TDCD_REGS_RIP

static tdcd_regs_label_t tdcd_regs_labels[] =
{
    {TDCD_REGS_RAX, "rax"},
    {TDCD_REGS_RBX, "rbx"},
    {TDCD_REGS_RCX, "rcx"},
    {TDCD_REGS_RDX, "rdx"},
    {TDCD_REGS_R8,  "r8"},
    {TDCD_REGS_R9,  "r9"},
    {TDCD_REGS_R10, "r10"},
    {TDCD_REGS_R11, "r11"},
    {TDCD_REGS_R12, "r12"},
    {TDCD_REGS_R13, "r13"},
    {TDCD_REGS_R14, "r14"},
    {TDCD_REGS_R15, "r15"},
    {TDCD_REGS_RDI, "rdi"},
    {TDCD_REGS_RSI, "rsi"},
    {TDCD_REGS_RBP, "rbp"},
    {TDCD_REGS_RSP, "rsp"},
    {TDCD_REGS_RIP, "rip"}
};

void tdcd_regs_get_labels(tdcd_regs_label_t **labels, size_t *labels_count)
{
    *labels = tdcd_regs_labels;
    *labels_count = sizeof(tdcd_regs_labels) / sizeof(tdcd_regs_label_t);
}

uintptr_t tdcd_regs_get_pc(tdcd_regs_t *self)
{
    return self->r[TDCD_REGS_PC];
}

void tdcd_regs_set_pc(tdcd_regs_t *self, uintptr_t pc)
{
    self->r[TDCD_REGS_PC] = pc;
}

uintptr_t tdcd_regs_get_sp(tdcd_regs_t *self)
{
    return self->r[TDCD_REGS_SP];
}

void tdcd_regs_set_sp(tdcd_regs_t *self, uintptr_t sp)
{
    self->r[TDCD_REGS_SP] = sp;
}

void tdcd_regs_load_from_ucontext(tdcd_regs_t *self, ucontext_t *uc)
{
    self->r[TDCD_REGS_RAX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RAX];
    self->r[TDCD_REGS_RBX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RBX];
    self->r[TDCD_REGS_RCX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RCX];
    self->r[TDCD_REGS_RDX] = (uintptr_t)uc->uc_mcontext.gregs[REG_RDX];
    self->r[TDCD_REGS_R8]  = (uintptr_t)uc->uc_mcontext.gregs[REG_R8];
    self->r[TDCD_REGS_R9]  = (uintptr_t)uc->uc_mcontext.gregs[REG_R9];
    self->r[TDCD_REGS_R10] = (uintptr_t)uc->uc_mcontext.gregs[REG_R10];
    self->r[TDCD_REGS_R11] = (uintptr_t)uc->uc_mcontext.gregs[REG_R11];
    self->r[TDCD_REGS_R12] = (uintptr_t)uc->uc_mcontext.gregs[REG_R12];
    self->r[TDCD_REGS_R13] = (uintptr_t)uc->uc_mcontext.gregs[REG_R13];
    self->r[TDCD_REGS_R14] = (uintptr_t)uc->uc_mcontext.gregs[REG_R14];
    self->r[TDCD_REGS_R15] = (uintptr_t)uc->uc_mcontext.gregs[REG_R15];
    self->r[TDCD_REGS_RDI] = (uintptr_t)uc->uc_mcontext.gregs[REG_RDI];
    self->r[TDCD_REGS_RSI] = (uintptr_t)uc->uc_mcontext.gregs[REG_RSI];
    self->r[TDCD_REGS_RBP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
    self->r[TDCD_REGS_RSP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    self->r[TDCD_REGS_RIP] = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
}

void tdcd_regs_load_from_ptregs(tdcd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    (void)regs_len;
    
    struct pt_regs *ptregs = (struct pt_regs *)regs;

    self->r[TDCD_REGS_RAX] = ptregs->rax;
    self->r[TDCD_REGS_RBX] = ptregs->rbx;
    self->r[TDCD_REGS_RCX] = ptregs->rcx;
    self->r[TDCD_REGS_RDX] = ptregs->rdx;
    self->r[TDCD_REGS_R8]  = ptregs->r8;
    self->r[TDCD_REGS_R9]  = ptregs->r9;
    self->r[TDCD_REGS_R10] = ptregs->r10;
    self->r[TDCD_REGS_R11] = ptregs->r11;
    self->r[TDCD_REGS_R12] = ptregs->r12;
    self->r[TDCD_REGS_R13] = ptregs->r13;
    self->r[TDCD_REGS_R14] = ptregs->r14;
    self->r[TDCD_REGS_R15] = ptregs->r15;
    self->r[TDCD_REGS_RDI] = ptregs->rdi;
    self->r[TDCD_REGS_RSI] = ptregs->rsi;
    self->r[TDCD_REGS_RBP] = ptregs->rbp;
    self->r[TDCD_REGS_RSP] = ptregs->rsp;
    self->r[TDCD_REGS_RIP] = ptregs->rip;
}

int tdcd_regs_record(tdcd_regs_t *self, int log_fd)
{
    return tdcc_util_write_format(log_fd,
                                 "    rax %016lx  rbx %016lx  rcx %016lx  rdx %016lx\n"
                                 "    r8  %016lx  r9  %016lx  r10 %016lx  r11 %016lx\n"
                                 "    r12 %016lx  r13 %016lx  r14 %016lx  r15 %016lx\n"
                                 "    rdi %016lx  rsi %016lx\n"
                                 "    rbp %016lx  rsp %016lx  rip %016lx\n\n",
                                 self->r[TDCD_REGS_RAX], self->r[TDCD_REGS_RBX], self->r[TDCD_REGS_RCX], self->r[TDCD_REGS_RDX],
                                 self->r[TDCD_REGS_R8],  self->r[TDCD_REGS_R9],  self->r[TDCD_REGS_R10], self->r[TDCD_REGS_R11],
                                 self->r[TDCD_REGS_R12], self->r[TDCD_REGS_R13], self->r[TDCD_REGS_R14], self->r[TDCD_REGS_R15],
                                 self->r[TDCD_REGS_RDI], self->r[TDCD_REGS_RSI],
                                 self->r[TDCD_REGS_RBP], self->r[TDCD_REGS_RSP], self->r[TDCD_REGS_RIP]);
}

int tdcd_regs_try_step_sigreturn(tdcd_regs_t *self, uintptr_t rel_pc, tdcd_memory_t *memory, pid_t pid)
{
    uint64_t data = 0;
    if(0 != tdcd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return TDC_ERRNO_MEM;
    if(0x0f0000000fc0c748 != data) return TDC_ERRNO_NOTFND;

    uint16_t data2 = 0;
    if(0 != tdcd_memory_read_fully(memory, rel_pc + 8, &data2, sizeof(data2))) return TDC_ERRNO_MEM;
    if(0x0f05 != data2) return TDC_ERRNO_NOTFND;
    
    // __restore_rt:
    // 0x48 0xc7 0xc0 0x0f 0x00 0x00 0x00   mov $0xf,%rax
    // 0x0f 0x05                            syscall
    // 0x0f                                 nopl 0x0($rax)

    // Read the mcontext data from the stack.
    // sp points to the ucontext data structure, read only the mcontext part.
    ucontext_t uc;
    if(0 != tdcd_util_ptrace_read_fully(pid, tdcd_regs_get_sp(self) + 0x28, &(uc.uc_mcontext), sizeof(uc.uc_mcontext))) return TDC_ERRNO_MEM;
    tdcd_regs_load_from_ucontext(self, &uc);
    
    return 0;
}

uintptr_t tdcd_regs_get_adjust_pc(uintptr_t rel_pc, uintptr_t load_bias, tdcd_memory_t *memory)
{    
    (void)load_bias;
    (void)memory;

    return 0 == rel_pc ? 0 : 1;
}

int tdcd_regs_set_pc_from_lr(tdcd_regs_t *self, pid_t pid)
{
    // Attempt to get the return address from the top of the stack.
    uintptr_t new_pc;
    if(0 != tdcd_util_ptrace_read_fully(pid, tdcd_regs_get_sp(self), &new_pc, sizeof(new_pc))) return TDC_ERRNO_MEM;
    
    if(new_pc == tdcd_regs_get_pc(self)) return TDC_ERRNO_INVAL;
    
    tdcd_regs_set_pc(self, new_pc);
    
    return 0;
}

#endif
