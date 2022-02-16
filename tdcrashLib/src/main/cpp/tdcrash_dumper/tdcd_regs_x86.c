//
// Created by bugliee on 2022/1/11.
//

typedef int make_iso_compilers_happy;

#ifdef __i386__

#include <stdio.h>
#include <ucontext.h>
#include <sys/ptrace.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_regs.h"
#include "tdcd_memory.h"
#include "tdcd_util.h"

#define TDCD_REGS_EAX 0
#define TDCD_REGS_ECX 1
#define TDCD_REGS_EDX 2
#define TDCD_REGS_EBX 3
#define TDCD_REGS_ESP 4
#define TDCD_REGS_EBP 5
#define TDCD_REGS_ESI 6
#define TDCD_REGS_EDI 7
#define TDCD_REGS_EIP 8
//#define TDCD_REGS_EFL 9
//#define TDCD_REGS_CS  10
//#define TDCD_REGS_SS  11
//#define TDCD_REGS_DS  12
//#define TDCD_REGS_ES  13
//#define TDCD_REGS_FS  14
//#define TDCD_REGS_GS  15

#define TDCD_REGS_SP  TDCD_REGS_ESP
#define TDCD_REGS_PC  TDCD_REGS_EIP

static tdcd_regs_label_t tdcd_regs_labels[] =
{
    {TDCD_REGS_EAX, "eax"},
    {TDCD_REGS_EBX, "ebx"},
    {TDCD_REGS_ECX, "ecx"},
    {TDCD_REGS_EDX, "edx"},
    {TDCD_REGS_EDI, "edi"},
    {TDCD_REGS_ESI, "esi"},
    {TDCD_REGS_EBP, "ebp"},
    {TDCD_REGS_ESP, "esp"},
    {TDCD_REGS_EIP, "eip"}
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
    self->r[TDCD_REGS_EAX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EAX];
    self->r[TDCD_REGS_EBX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EBX];
    self->r[TDCD_REGS_ECX] = (uintptr_t)uc->uc_mcontext.gregs[REG_ECX];
    self->r[TDCD_REGS_EDX] = (uintptr_t)uc->uc_mcontext.gregs[REG_EDX];
    self->r[TDCD_REGS_EDI] = (uintptr_t)uc->uc_mcontext.gregs[REG_EDI];
    self->r[TDCD_REGS_ESI] = (uintptr_t)uc->uc_mcontext.gregs[REG_ESI];
    self->r[TDCD_REGS_EBP] = (uintptr_t)uc->uc_mcontext.gregs[REG_EBP];
    self->r[TDCD_REGS_ESP] = (uintptr_t)uc->uc_mcontext.gregs[REG_ESP];
    self->r[TDCD_REGS_EIP] = (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
}

void tdcd_regs_load_from_ptregs(tdcd_regs_t *self, uintptr_t *regs, size_t regs_len)
{
    (void)regs_len;
    
    struct pt_regs *ptregs = (struct pt_regs *)regs;
    
    self->r[TDCD_REGS_EAX] = (uintptr_t)ptregs->eax;
    self->r[TDCD_REGS_EBX] = (uintptr_t)ptregs->ebx;
    self->r[TDCD_REGS_ECX] = (uintptr_t)ptregs->ecx;
    self->r[TDCD_REGS_EDX] = (uintptr_t)ptregs->edx;
    self->r[TDCD_REGS_EDI] = (uintptr_t)ptregs->edi;
    self->r[TDCD_REGS_ESI] = (uintptr_t)ptregs->esi;
    self->r[TDCD_REGS_EBP] = (uintptr_t)ptregs->ebp;
    self->r[TDCD_REGS_ESP] = (uintptr_t)ptregs->esp;
    self->r[TDCD_REGS_EIP] = (uintptr_t)ptregs->eip;
}

int tdcd_regs_record(tdcd_regs_t *self, int log_fd)
{
    return tdcc_util_write_format(log_fd,
                                 "    eax %08x  ebx %08x  ecx %08x  edx %08x\n"
                                 "    edi %08x  esi %08x\n"
                                 "    ebp %08x  esp %08x  eip %08x\n\n",
                                 self->r[TDCD_REGS_EAX], self->r[TDCD_REGS_EBX], self->r[TDCD_REGS_ECX], self->r[TDCD_REGS_EDX],
                                 self->r[TDCD_REGS_EDI], self->r[TDCD_REGS_ESI],
                                 self->r[TDCD_REGS_EBP], self->r[TDCD_REGS_ESP], self->r[TDCD_REGS_EIP]);
}

int tdcd_regs_try_step_sigreturn(tdcd_regs_t *self, uintptr_t rel_pc, tdcd_memory_t *memory, pid_t pid)
{
    uint64_t data;
    if(0 != tdcd_memory_read_fully(memory, rel_pc, &data, sizeof(data))) return TDC_ERRNO_MEM;

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
        if(0 != tdcd_util_ptrace_read_fully(pid, tdcd_regs_get_sp(self) + 4, &(mc.gregs), sizeof(mc.gregs))) return TDC_ERRNO_MEM;
        self->r[TDCD_REGS_EAX] = (uintptr_t)mc.gregs[REG_EAX];
        self->r[TDCD_REGS_EBX] = (uintptr_t)mc.gregs[REG_EBX];
        self->r[TDCD_REGS_ECX] = (uintptr_t)mc.gregs[REG_ECX];
        self->r[TDCD_REGS_EDX] = (uintptr_t)mc.gregs[REG_EDX];
        self->r[TDCD_REGS_EBP] = (uintptr_t)mc.gregs[REG_EBP];
        self->r[TDCD_REGS_ESP] = (uintptr_t)mc.gregs[REG_ESP];
        self->r[TDCD_REGS_EIP] = (uintptr_t)mc.gregs[REG_EIP];
        
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
        if(0 != tdcd_util_ptrace_read_fully(pid, tdcd_regs_get_sp(self) + 8, &p_uc, sizeof(p_uc))) return TDC_ERRNO_MEM;

        // Only read the portion of the data structure we care about.
        ucontext_t uc;
        if(0 != tdcd_util_ptrace_read_fully(pid, p_uc + 0x14, &(uc.uc_mcontext), sizeof(uc.uc_mcontext))) return TDC_ERRNO_MEM;
        tdcd_regs_load_from_ucontext(self, &uc);
        
        return 0;
    }

    return TDC_ERRNO_NOTFND;
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
