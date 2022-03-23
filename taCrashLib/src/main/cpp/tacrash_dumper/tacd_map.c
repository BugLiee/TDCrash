//
// Created by bugliee on 2022/1/11.
//

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_map.h"
#include "tacd_util.h"
#include "tacd_log.h"

int tacd_map_init(tacd_map_t *self, uintptr_t start, uintptr_t end, size_t offset,
                 const char * flags, const char *name)
{
    self->start  = start;
    self->end    = end;
    self->offset = offset;
    
    self->flags  = PROT_NONE;
    if(flags[0] == 'r') self->flags |= PROT_READ;
    if(flags[1] == 'w') self->flags |= PROT_WRITE;
    if(flags[2] == 'x') self->flags |= PROT_EXEC;
    
    if(NULL == name || '\0' == name[0])
    {
        self->name = NULL;
    }
    else
    {
        if(0 == strncmp(name, "/dev/", 5) && 0 != strncmp(name + 5, "ashmem/", 7))
            self->flags |= TACD_MAP_PORT_DEVICE;
        
        if(NULL == (self->name = strdup(name))) return TAC_ERRNO_NOMEM;
    }

    self->elf = NULL;
    self->elf_loaded = 0;
    self->elf_offset = 0;
    self->elf_start_offset = 0;

    return 0;
}

void tacd_map_uninit(tacd_map_t *self)
{
    free(self->name);
    self->name = NULL;
}

tacd_elf_t *tacd_map_get_elf(tacd_map_t *self, pid_t pid, void *maps_obj)
{
    tacd_memory_t *memory = NULL;
    tacd_elf_t    *elf = NULL;

    if(NULL == self->elf && 0 == self->elf_loaded)
    {
        self->elf_loaded = 1;
        
        if(0 != tacd_memory_create(&memory, self, pid, maps_obj)) return NULL;

        if(0 != tacd_elf_create(&elf, pid, memory)) return NULL;
        
        self->elf = elf;
    }

    return self->elf;
}

uintptr_t tacd_map_get_rel_pc(tacd_map_t *self, uintptr_t abs_pc, pid_t pid, void *maps_obj)
{
    tacd_elf_t *elf = tacd_map_get_elf(self, pid, maps_obj);
    uintptr_t load_bias = (NULL == elf ? 0 : tacd_elf_get_load_bias(elf));
    
    return abs_pc - (self->start - self->elf_offset - load_bias);
}

uintptr_t tacd_map_get_abs_pc(tacd_map_t *self, uintptr_t rel_pc, pid_t pid, void *maps_obj)
{
    tacd_elf_t *elf = tacd_map_get_elf(self, pid, maps_obj);
    uintptr_t load_bias = (NULL == elf ? 0 : tacd_elf_get_load_bias(elf));
    
    return (self->start - self->elf_offset - load_bias) + rel_pc;
}
