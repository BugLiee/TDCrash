//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_map.h"
#include "tdcd_memory.h"
#include "tdcd_memory_remote.h"
#include "tdcd_util.h"
#include "tdcd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tdcd_memory_remote
{
    pid_t     pid;
    uintptr_t start;
    size_t    length;
};
#pragma clang diagnostic pop

int tdcd_memory_remote_create(void **obj, tdcd_map_t *map, pid_t pid)
{
    tdcd_memory_remote_t **self = (tdcd_memory_remote_t **)obj;
    
    if(NULL == (*self = malloc(sizeof(tdcd_memory_remote_t)))) return TDC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->start = map->start;
    (*self)->length = (size_t)(map->end - map->start);

    return 0;
}

void tdcd_memory_remote_destroy(void **obj)
{
    tdcd_memory_remote_t **self = (tdcd_memory_remote_t **)obj;
    
    free(*self);
    *self = NULL;
}

size_t tdcd_memory_remote_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    tdcd_memory_remote_t *self = (tdcd_memory_remote_t *)obj;

    if((size_t)addr >= self->length) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t read_length = TDCC_UTIL_MIN(size, self->length - addr);
#pragma clang diagnostic pop
    
    uint64_t read_addr;
    if(__builtin_add_overflow(self->start, addr, &read_addr)) return 0;
    
    return tdcd_util_ptrace_read(self->pid, (uintptr_t)read_addr, dst, read_length);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const tdcd_memory_handlers_t tdcd_memory_remote_handlers = {
    tdcd_memory_remote_destroy,
    tdcd_memory_remote_read
};
#pragma clang diagnostic pop
