//
// Created by bugliee on 2022/1/11.
//

typedef int make_iso_compilers_happy;

#ifdef __i386__

#include <stdio.h>
#include <ucontext.h>
#include <sys/ptrace.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_regs.h"
#include "tacd_memory.h"
#include "tacd_util.h"

#define TACD_REGS_EAX 0
#define TACD_REGS_ECX 1
#define TACD_REGS_EDX 2
#define TACD_REGS_EBX 3
#define TACD_REGS_ESP 4
#define TACD_REGS_EBP 5
#define TACD_REGS_ESI 6
#define TACD_REGS_EDI 7
#define TACD_REGS_EIP 8
//#define TACD_REGS_EFL 9
//#define TACD_REGS_CS  10
//#define TACD_REGS_SS  11
//#define TACD_REGS_DS  12
//#define TACD_REGS_ES  13
//#define TACD_REGS_FS  14
//#define TACD_REGS_GS  15

#define TACD_REGS_SP  TACD_REGS_ESP
#define TACD_REGS_PC  TACD_REGS_EIP

static tacd_regs_label_t tacd_regs_labels[] =
{
    {TACD_REGS_EAX, "eax"},
    {TACD_REGS_EBX, "ebx"},
    {TACD_REGS_ECX, "ecx"},
    {TACD_REGS_EDX, "edx"},
    {TACD_REGS_EDI, "edi"},
    {TACD_REGS_ESI, "esi"},
    {TACD_REGS_EBP, "ebp"},
    {TACD_REGS_ESP, "esp"},
    {TACD_REGS_EIP, "eip"}
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
    self->r[TACD_REGS_EAX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EAX];
    self->r[TACD_REGS_EBX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EBX];
    self->r[TACD_REGS_ECX] = (uintptr_t)uc->uc_mcontext.gregs[REG_ECX];
    self->r[TACD_REGS_EDX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EDX];
    self->r[TACD_REGS_EDI] = (uintptr_t)uc->uc_mcontext.gregs[REG_EDI];
    self->r[TACD_REGS_ESI] = (uintptr_t)uc->uc_mcontext.gregs[REG_ESI];
    self->r[TACD_REGS_EBP] = (uintptr_t)uc->uc_mcontext.gregs[REG_EBP];
    self->r[TACD_REGS_ESP] = (uintptr_t)uc->uc_mcontext.gregs[REG_ESP];
    self->r[TACD_REGS_EIP] = (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
}

void tacd_regs_load_from_ptregs(tacd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    (void)regs_len;
    
    struct pt_regs *ptregs = (struct pt_regs *)regs;
    
    self->r[TACD_REGS_EAX] = (uintptr_t)ptregs->eax;
    self->r[TACD_REGS_EBX] = (uintptr_t)ptregs->ebx;
    self->r[TACD_REGS_ECX] = (uintptr_t)ptregs->ecx;
    self->r[TACD_REGS_EDX] = (uintptr_t)ptregs->edx;
    self->r[TACD_REGS_EDI] = (uintptr_t)ptregs->edi;
    self->r[TACD_REGS_ESI] = (uintptr_t)ptregs->esi;
    self->r[TACD_REGS_EBP] = (uintptr_t)ptregs->ebp;
    self->r[TACD_REGS_ESP] = (uintptr_t)ptregs->esp;
    self->r[TACD_REGS_EIP] = (uintptr_t)ptregs->eip;
}

int tacd_regs_record(tacd_regs_t *self, int log_fd)
{
    return tacc_util_write_format(log_fd,
                                 "    eax %08x  ebx %08x  ecx %08x  edx %08x\n"
                                 "    edi %08x  esi %08x\n"
                                 "    ebp %08x  esp %08x  eip %08x\n\n",
                                 self->r[TACD_REGS_EAX], self->r[TACD_REGS_EBX], self->r[TACD_REGS_ECX], self->r[TACD_REGS_EDX],
                                 self->r[TACD_REGS_EDI], self->r[TACD_REGS_ESI],
                                 self->r[TACD_REGS_EBP], self->r[TACD_REGS_ESP], self->r[TACD_REGS_EIP]);
}

int tacd_regs_try_step_sigreturn(tacd_regs_t *self, uintptr_t rel_pc, tacd_memory_t *memory, pid_t pid)
{
    uint64_t data;
    if(0 != tacd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return TAC_ERRNO_MEM;

    if(0x80cd00000077b858ULL == data)
    {
        // Without SA_SIGINFO set, the return sequence is:
        //
        //   __restore:
        //   0x58                            pop %eax
        //   0xb8 0x77 0x00 0x00 0x00        movl 0x77,%eax
        //   0xcd 0x80                       int 0x80
        //
        // SP points at arguments:
        //   int signum
        //   struct sigcontext (same format as mcontext)
        
        // Only read the portion of the data structure we care about.
        mcontext_t mc;
        if(0 != tacd_util_ptrace_read_fully(pid, tacd_regs_get_sp(self) + 4, &(mc.gregs), sizeof(mc.gregs))) return TAC_ERRNO_MEM;
        self->r[TACD_REGS_EAX] = (uintptr_t)mc.gregs[REG_EAX];
        self->r[TACD_REGS_EBX] = (uintptr_t)mc.gregs[REG_EBX];
        self->r[TACD_REGS_ECX] = (uintptr_t)mc.gregs[REG_ECX];
        self->r[TACD_REGS_EDX] = (uintptr_t)mc.gregs[REG_EDX];
        self->r[TACD_REGS_EBP] = (uintptr_t)mc.gregs[REG_EBP];
        self->r[TACD_REGS_ESP] = (uintptr_t)mc.gregs[REG_ESP];
        self->r[TACD_REGS_EIP] = (uintptr_t)mc.gregs[REG_EIP];
        
        return 0;
    }
    else if(0x0080cd000000adb8ULL == (data & 0x00ffffffffffffffULL))
    {
        // With SA_SIGINFO set, the return sequence is:
        //
        //   __restore_rt:
        //   0xb8 0xad 0x00 0x00 0x00        movl 0xad,%eax
        //   0xcd 0x80                       int 0x80
        //
        // SP points at arguments:
        //   int         signum
        //   siginfo_t  *siginfo
        //   ucontext_t *uc

        // Get the location of the ucontext_t data.
        uintptr_t p_uc;
        if(0 != tacd_util_ptrace_read_fully(pid, tacd_regs_get_sp(self) + 8, &p_uc, sizeof(p_uc))) return TAC_ERRNO_MEM;

        // Only read the portion of the data structure we care about.
        ucontext_t uc;
        if(0 != tacd_util_ptrace_read_fully(pid, p_uc + 0x14, &(uc.uc_mcontext), sizeof(uc.uc_mcontext))) return TAC_ERRNO_MEM;
        tacd_regs_load_from_ucontext(self, &uc);
        
        return 0;
    }

    return TAC_ERRNO_NOTFND;
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
