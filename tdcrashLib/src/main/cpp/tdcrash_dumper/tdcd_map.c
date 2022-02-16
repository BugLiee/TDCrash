//
// Created by bugliee on 2022/1/11.
//

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_map.h"
#include "tdcd_util.h"
#include "tdcd_log.h"

int tdcd_map_init(tdcd_map_t *self, uintptr_t start, uintptr_t end, size_t offset,
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
            self->flags |= TDCD_MAP_PORT_DEVICE;
        
        if(NULL == (self->name = strdup(name))) return TDC_ERRNO_NOMEM;
    }

    self->elf = NULL;
    self->elf_loaded = 0;
    self->elf_offset = 0;
    self->elf_start_offset = 0;

    return 0;
}

void tdcd_map_uninit(tdcd_map_t *self)
{
    free(self->name);
    self->name = NULL;
}

tdcd_elf_t *tdcd_map_get_elf(tdcd_map_t *self, pid_t pid, void *maps_obj)
{
    tdcd_memory_t *memory = NULL;
    tdcd_elf_t    *elf = NULL;

    if(NULL == self->elf && 0 == self->elf_loaded)
    {
        self->elf_loaded = 1;
        
        if(0 != tdcd_memory_create(&memory, self, pid, maps_obj)) return NULL;

        if(0 != tdcd_elf_create(&elf, pid, memory)) return NULL;
        
        self->elf = elf;
    }

    return self->elf;
}

uintptr_t tdcd_map_get_rel_pc(tdcd_map_t *self, uintptr_t abs_pc, pid_t pid, void *maps_obj)
{
    tdcd_elf_t *elf = tdcd_map_get_elf(self, pid, maps_obj);
    uintptr_t load_bias = (NULL == elf ? 0 : tdcd_elf_get_load_bias(elf));
    
    return abs_pc - (self->start - self->elf_offset - load_bias);
}

uintptr_t tdcd_map_get_abs_pc(tdcd_map_t *self, uintptr_t rel_pc, pid_t pid, void *maps_obj)
{
    tdcd_elf_t *elf = tdcd_map_get_elf(self, pid, maps_obj);
    uintptr_t load_bias = (NULL == elf ? 0 : tdcd_elf_get_load_bias(elf));
    
    return (self->start - self->elf_offset - load_bias) + rel_pc;
}
