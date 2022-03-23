//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_map.h"
#include "tacd_memory.h"
#include "tacd_memory_remote.h"
#include "tacd_util.h"
#include "tacd_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tacd_memory_remote
{
    pid_t     pid;
    uintptr_t start;
    size_t    length;
};
#pragma clang diagnostic pop

int tacd_memory_remote_create(void **obj, tacd_map_t *map, pid_t pid)
{
    tacd_memory_remote_t **self = (tacd_memory_remote_t **)obj;
    
    if(NULL == (*self = malloc(sizeof(tacd_memory_remote_t)))) return TAC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->start = map->start;
    (*self)->length = (size_t)(map->end - map->start);

    return 0;
}

void tacd_memory_remote_destroy(void **obj)
{
    tacd_memory_remote_t **self = (tacd_memory_remote_t **)obj;
    
    free(*self);
    *self = NULL;
}

size_t tacd_memory_remote_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    tacd_memory_remote_t *self = (tacd_memory_remote_t *)obj;

    if((size_t)addr >= self->length) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t read_length = TACC_UTIL_MIN(size, self->length - addr);
#pragma clang diagnostic pop
    
    uint64_t read_addr;
    if(__builtin_add_overflow(self->start, addr, &read_addr)) return 0;
    
    return tacd_util_ptrace_read(self->pid, (uintptr_t)read_addr, dst, read_length);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const tacd_memory_handlers_t tacd_memory_remote_handlers = {
    tacd_memory_remote_destroy,
    tacd_memory_remote_read
};
#pragma clang diagnostic pop
