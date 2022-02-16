//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>
#include <sys/types.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcd_elf.h"
#include "tdcd_elf_interface.h"
#include "tdcd_memory.h"
#include "tdcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tdcd_elf
{
    pid_t                pid;
    tdcd_memory_t        *memory;
    uintptr_t            load_bias;
    tdcd_elf_interface_t *interface;
    tdcd_elf_interface_t *gnu_interface;
    int                  gnu_interface_created;
};
#pragma clang diagnostic pop

int tdcd_elf_is_valid(tdcd_memory_t *memory)
{
    if(NULL == memory) return 0;

    uint8_t e_ident[SELFMAG + 1];
    if(0 != tdcd_memory_read_fully(memory, 0, e_ident, SELFMAG)) return 0;
    if(0 != memcmp(e_ident, ELFMAG, SELFMAG)) return 0;

    uint8_t class_type;
    if(0 != tdcd_memory_read_fully(memory, EI_CLASS, &class_type, 1)) return 0;
#if defined(__LP64__)
    if(ELFCLASS64 != class_type) return 0;
#else
    if(ELFCLASS32 != class_type) return 0;
#endif

    return 1;
}

size_t tdcd_elf_get_max_size(tdcd_memory_t *memory)
{
    ElfW(Ehdr) ehdr;

    if(0 != tdcd_memory_read_fully(memory, 0, &ehdr, sizeof(ehdr))) return 0;
    if(0 == ehdr.e_shnum) return 0;
    
    return ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum;
}

int tdcd_elf_create(tdcd_elf_t **self, pid_t pid, tdcd_memory_t *memory)
{
    int r;
    
    if(NULL == (*self = calloc(1, sizeof(tdcd_elf_t)))) return TDC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->memory = memory;

    //create ELF interface, save load bias
    if(0 != (r = tdcd_elf_interface_create(&((*self)->interface), pid, memory, &((*self)->load_bias))))
    {
        free(*self);
        return r;
    }

    return 0;
}

uintptr_t tdcd_elf_get_load_bias(tdcd_elf_t *self)
{
    return self->load_bias;
}

tdcd_memory_t *tdcd_elf_get_memory(tdcd_elf_t *self)
{
    return self->memory;
}

int tdcd_elf_step(tdcd_elf_t *self, uintptr_t rel_pc, uintptr_t step_pc, tdcd_regs_t *regs, int *finished, int *sigreturn)
{
    *finished = 0;
    *sigreturn = 0;
    
    //try sigreturn
    if(rel_pc >= self->load_bias)
    {
        if(0 == tdcd_regs_try_step_sigreturn(regs, rel_pc - self->load_bias, self->memory, self->pid))
        {
            *finished  = 0;
            *sigreturn = 1;
#if TDCD_ELF_DEBUG
            TDCD_LOG_DEBUG("ELF: step by sigreturn OK, rel_pc=%"PRIxPTR", finished=0", rel_pc);
#endif
            return 0;
        }
    }

    //try DWARF (.debug_frame and .eh_frame)
    if(0 == tdcd_elf_interface_dwarf_step(self->interface, step_pc, regs, finished)) return 0;

    //create GNU interface (only once)
    if(NULL == self->gnu_interface && 0 == self->gnu_interface_created)
    {
        self->gnu_interface_created = 1;
        self->gnu_interface = tdcd_elf_interface_gnu_create(self->interface);
    }

    //try DWARF (.debug_frame and .eh_frame) in GNU interface
    if(NULL != self->gnu_interface)
        if(0 == tdcd_elf_interface_dwarf_step(self->gnu_interface, step_pc, regs, finished)) return 0;

    //try .ARM.exidx
#ifdef __arm__
    if(0 == tdcd_elf_interface_arm_exidx_step(self->interface, step_pc, regs, finished)) return 0;
#endif

#if TDCD_ELF_DEBUG
    TDCD_LOG_ERROR("ELF: step FAILED, rel_pc=%"PRIxPTR", step_pc=%"PRIxPTR, rel_pc, step_pc);
#endif
    return TDC_ERRNO_MISSING;
}

int tdcd_elf_get_function_info(tdcd_elf_t *self, uintptr_t addr, char **name, size_t *name_offset)
{
    int r;

    //try ELF interface
    if(0 == (r = tdcd_elf_interface_get_function_info(self->interface, addr, name, name_offset))) return 0;

    //create GNU interface (only once)
    if(NULL == self->gnu_interface && 0 == self->gnu_interface_created)
    {
        self->gnu_interface_created = 1;
        self->gnu_interface = tdcd_elf_interface_gnu_create(self->interface);
    }
    
    //try GNU interface
    if(NULL != self->gnu_interface)
        if(0 == (r = tdcd_elf_interface_get_function_info(self->gnu_interface, addr, name, name_offset))) return 0;

    return r;
}

int tdcd_elf_get_symbol_addr(tdcd_elf_t *self, const char *name, uintptr_t *addr)
{
    return tdcd_elf_interface_get_symbol_addr(self->interface, name, addr);
}

int tdcd_elf_get_build_id(tdcd_elf_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret)
{
    return tdcd_elf_interface_get_build_id(self->interface, build_id, build_id_len, build_id_len_ret);
}

char *tdcd_elf_get_so_name(tdcd_elf_t *self)
{
    return tdcd_elf_interface_get_so_name(self->interface);
}
