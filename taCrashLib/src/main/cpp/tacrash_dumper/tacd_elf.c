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
#include "../tacrash/tac_errno.h"
#include "tacd_elf.h"
#include "tacd_elf_interface.h"
#include "tacd_memory.h"
#include "tacd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tacd_elf
{
    pid_t                pid;
    tacd_memory_t        *memory;
    uintptr_t            load_bias;
    tacd_elf_interface_t *interface;
    tacd_elf_interface_t *gnu_interface;
    int                  gnu_interface_created;
};
#pragma clang diagnostic pop

int tacd_elf_is_valid(tacd_memory_t *memory)
{
    if(NULL == memory) return 0;

    uint8_t e_ident[SELFMAG + 1];
    if(0 != tacd_memory_read_fully(memory, 0, e_ident, SELFMAG)) return 0;
    if(0 != memcmp(e_ident, ELFMAG, SELFMAG)) return 0;

    uint8_t class_type;
    if(0 != tacd_memory_read_fully(memory, EI_CLASS, &class_type, 1)) return 0;
#if defined(__LP64__)
    if(ELFCLASS64 != class_type) return 0;
#else
    if(ELFCLASS32 != class_type) return 0;
#endif

    return 1;
}

size_t tacd_elf_get_max_size(tacd_memory_t *memory)
{
    ElfW(Ehdr) ehdr;

    if(0 != tacd_memory_read_fully(memory, 0, &ehdr, sizeof(ehdr))) return 0;
    if(0 == ehdr.e_shnum) return 0;
    
    return ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum;
}

int tacd_elf_create(tacd_elf_t **self, pid_t pid, tacd_memory_t *memory)
{
    int r;
    
    if(NULL == (*self = calloc(1, sizeof(tacd_elf_t)))) return TAC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->memory = memory;

    //create ELF interface, save load bias
    if(0 != (r = tacd_elf_interface_create(&((*self)->interface), pid, memory, &((*self)->load_bias))))
    {
        free(*self);
        return r;
    }

    return 0;
}

uintptr_t tacd_elf_get_load_bias(tacd_elf_t *self)
{
    return self->load_bias;
}

tacd_memory_t *tacd_elf_get_memory(tacd_elf_t *self)
{
    return self->memory;
}

int tacd_elf_step(tacd_elf_t *self, uintptr_t rel_pc, uintptr_t step_pc, tacd_regs_t *regs, int *finished, int *sigreturn)
{
    *finished = 0;
    *sigreturn = 0;
    
    //try sigreturn
    if(rel_pc >= self->load_bias)
    {
        if(0 == tacd_regs_try_step_sigreturn(regs, rel_pc - self->load_bias, self->memory, self->pid))
        {
            *finished  = 0;
            *sigreturn = 1;
#if TACD_ELF_DEBUG
            TACD_LOG_DEBUG("ELF: step by sigreturn OK, rel_pc=%"PRIxPTR", finished=0", rel_pc);
#endif
            return 0;
        }
    }

    //try DWARF (.debug_frame and .eh_frame)
    if(0 == tacd_elf_interface_dwarf_step(self->interface, step_pc, regs, finished)) return 0;

    //create GNU interface (only once)
    if(NULL == self->gnu_interface && 0 == self->gnu_interface_created)
    {
        self->gnu_interface_created = 1;
        self->gnu_interface = tacd_elf_interface_gnu_create(self->interface);
    }

    //try DWARF (.debug_frame and .eh_frame) in GNU interface
    if(NULL != self->gnu_interface)
        if(0 == tacd_elf_interface_dwarf_step(self->gnu_interface, step_pc, regs, finished)) return 0;

    //try .ARM.exidx
#ifdef __arm__
    if(0 == tacd_elf_interface_arm_exidx_step(self->interface, step_pc, regs, finished)) return 0;
#endif

#if TACD_ELF_DEBUG
    TACD_LOG_ERROR("ELF: step FAILED, rel_pc=%"PRIxPTR", step_pc=%"PRIxPTR, rel_pc, step_pc);
#endif
    return TAC_ERRNO_MISSING;
}

int tacd_elf_get_function_info(tacd_elf_t *self, uintptr_t addr, char **name, size_t *name_offset)
{
    int r;

    //try ELF interface
    if(0 == (r = tacd_elf_interface_get_function_info(self->interface, addr, name, name_offset))) return 0;

    //create GNU interface (only once)
    if(NULL == self->gnu_interface && 0 == self->gnu_interface_created)
    {
        self->gnu_interface_created = 1;
        self->gnu_interface = tacd_elf_interface_gnu_create(self->interface);
    }
    
    //try GNU interface
    if(NULL != self->gnu_interface)
        if(0 == (r = tacd_elf_interface_get_function_info(self->gnu_interface, addr, name, name_offset))) return 0;

    return r;
}

int tacd_elf_get_symbol_addr(tacd_elf_t *self, const char *name, uintptr_t *addr)
{
    return tacd_elf_interface_get_symbol_addr(self->interface, name, addr);
}

int tacd_elf_get_build_id(tacd_elf_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret)
{
    return tacd_elf_interface_get_build_id(self->interface, build_id, build_id_len, build_id_len_ret);
}

char *tacd_elf_get_so_name(tacd_elf_t *self)
{
    return tacd_elf_interface_get_so_name(self->interface);
}
