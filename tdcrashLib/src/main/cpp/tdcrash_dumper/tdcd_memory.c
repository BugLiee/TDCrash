//
// Created by bugliee on 2022/1/11.
//

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcd_map.h"
#include "tdcd_memory.h"
#include "tdcd_memory_file.h"
#include "tdcd_memory_buf.h"
#include "tdcd_memory_remote.h"

extern const tdcd_memory_handlers_t tdcd_memory_buf_handlers;
extern const tdcd_memory_handlers_t tdcd_memory_file_handlers;
extern const tdcd_memory_handlers_t tdcd_memory_remote_handlers;

struct tdcd_memory
{
    void                        *obj;
    const tdcd_memory_handlers_t *handlers;
};

int tdcd_memory_create(tdcd_memory_t **self, void *map_obj, pid_t pid, void *maps_obj)
{
    tdcd_map_t  *map  = (tdcd_map_t *)map_obj;
    tdcd_maps_t *maps = (tdcd_maps_t *)maps_obj;
    
    if(map->end <= map->start) return TDC_ERRNO_INVAL;
    if(map->flags & TDCD_MAP_PORT_DEVICE) return TDC_ERRNO_DEV;
    
    if(NULL == (*self = malloc(sizeof(tdcd_memory_t)))) return TDC_ERRNO_NOMEM;
    
    //try memory from file
    (*self)->handlers = &tdcd_memory_file_handlers;
    if(0 == tdcd_memory_file_create(&((*self)->obj), *self, map, maps)) return 0;

    //try memory from remote ptrace
    //Sometimes, data only exists in the remote process's memory such as vdso data on x86.
    if(!(map->flags & PROT_READ)) return TDC_ERRNO_PERM;
    (*self)->handlers = &tdcd_memory_remote_handlers;
    if(0 == tdcd_memory_remote_create(&((*self)->obj), map, pid)) return 0;
    (void)pid;

    free(*self);
    return TDC_ERRNO_MEM;
}

//for ELF header info unzipped from .gnu_debugdata in the local memory
int tdcd_memory_create_from_buf(tdcd_memory_t **self, uint8_t *buf, size_t len)
{
    if(NULL == (*self = malloc(sizeof(tdcd_memory_t)))) return TDC_ERRNO_NOMEM;
    (*self)->handlers = &tdcd_memory_buf_handlers;
    if(0 == tdcd_memory_buf_create(&((*self)->obj), buf, len)) return 0;

    free(*self);
    return TDC_ERRNO_MEM;
}

void tdcd_memory_destroy(tdcd_memory_t **self)
{
    (*self)->handlers->destroy(&((*self)->obj));
    free(*self);
    *self = NULL;
}

size_t tdcd_memory_read(tdcd_memory_t *self, uintptr_t addr, void *dst, size_t size)
{
    return self->handlers->read(self->obj, addr, dst, size);
}

int tdcd_memory_read_fully(tdcd_memory_t *self, uintptr_t addr, void* dst, size_t size)
{
    size_t rc = self->handlers->read(self->obj, addr, dst, size);
    return rc == size ? 0 : TDC_ERRNO_MISSING;
}

int tdcd_memory_read_string(tdcd_memory_t *self, uintptr_t addr, char *dst, size_t size, size_t max_read)
{
    char   value;
    size_t i = 0;
    int    r;
    
    while(i < size && i < max_read)
    {
        if(0 != (r = tdcd_memory_read_fully(self, addr, &value, sizeof(value)))) return r;
        dst[i] = value;
        if('\0' == value) return 0;
        addr++;
        i++;
    }
    return TDC_ERRNO_NOSPACE;
}

int tdcd_memory_read_uleb128(tdcd_memory_t *self, uintptr_t addr, uint64_t *dst, size_t *size)
{
    uint64_t cur_value = 0;
    uint64_t shift = 0;
    uint8_t  byte;
    int      r;
    
    if(NULL != size) *size = 0;
    
    do
    {
        if(0 != (r = tdcd_memory_read_fully(self, addr, &byte, 1))) return r;
        addr += 1;
        if(NULL != size) *size += 1;
        cur_value += ((uint64_t)(byte & 0x7f) << shift);
        shift += 7;
    }while(byte & 0x80);
    
    *dst = cur_value;
    return 0;
}

int tdcd_memory_read_sleb128(tdcd_memory_t *self, uintptr_t addr, int64_t *dst, size_t *size)
{
    uint64_t cur_value = 0;
    uint64_t shift = 0;
    uint8_t  byte;
    int      r;

    if(NULL != size) *size = 0;
    
    do
    {
        if(0 != (r = tdcd_memory_read_fully(self, addr, &byte, 1))) return r;
        addr += 1;
        if(NULL != size) *size += 1;
        cur_value += ((uint64_t)(byte & 0x7f) << shift);
        shift += 7;
    }while(byte & 0x80);

    if(byte & 0x40)
        cur_value |= ((uint64_t)(-1) << shift);
    
    *dst = (int64_t)cur_value;
    return 0;    
}
